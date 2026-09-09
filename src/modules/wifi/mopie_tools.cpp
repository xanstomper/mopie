#include "mopie_tools.h"
#include "mopie_tx.h"
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