#include "mopie_ai.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"
#include "NimBLEDevice.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#include "core/wifi/webInterface.h"
#include "core/sd_functions.h"
#include "modules/wifi/wifi_atks.h"
#include "modules/ble/ble_common.h"
#include <globals.h>

// ---------------------------------------------------------------------------
// Settings persistence (LittleFS /mopie_ai.json)
// ---------------------------------------------------------------------------
static const char *AI_CONF = "/mopie_ai.json";

static String aiModel = "gpt-4o-mini";
static String aiEndpoint = "https://api.openai.com/v1/chat/completions";
static String aiKey = "";

static void aiSaveConf();

String aiGetApiKey() { return aiKey; }
void aiSetApiKey(const String &key) { aiKey = key; aiSaveConf(); }
String aiGetWifiSsid() { return WiFi.SSID(); }
void aiSetWifiSsid(const String &ssid) { (void)ssid; /* WiFi manager owns this */ }
String aiGetEndpoint() { return aiEndpoint; }
void aiSetEndpoint(const String &ep) { aiEndpoint = ep; aiSaveConf(); }
String aiGetModel() { return aiModel; }
void aiSetModel(const String &model) { aiModel = model; aiSaveConf(); }

static void aiLoadConf() {
    FS *fs = &LittleFS;
    if (!fs->exists(AI_CONF)) return;
    File f = fs->open(AI_CONF, FILE_READ);
    if (!f) return;
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    if (!doc["key"].isNull()) aiKey = doc["key"].as<String>();
    if (!doc["model"].isNull()) aiModel = doc["model"].as<String>();
    if (!doc["endpoint"].isNull()) aiEndpoint = doc["endpoint"].as<String>();
}

static void aiSaveConf() {
    FS *fs = &LittleFS;
    JsonDocument doc;
    doc["key"] = aiKey;
    doc["model"] = aiModel;
    doc["endpoint"] = aiEndpoint;
    File f = fs->open(AI_CONF, FILE_WRITE);
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

// ---------------------------------------------------------------------------
// AI tool protocol: the model may answer with a JSON command block:
//   {"mopie_cmd":"led","arg":"FF0000"}
// Supported: led <RRGGBB> | reboot | flash <on/off> | beep | sysinfo
// ---------------------------------------------------------------------------
struct AiCmd { String name; String arg; };

static bool parseAiCmd(const String &reply, AiCmd &out) {
    // find last JSON object with "mopie_cmd"
    int idx = reply.lastIndexOf("\"mopie_cmd\"");
    if (idx < 0) return false;
    int start = reply.lastIndexOf('{', idx);
    int end = reply.indexOf('}', idx);
    if (start < 0 || end < 0) return false;
    JsonDocument doc;
    if (deserializeJson(doc, reply.substring(start, end + 1))) return false;
    out.name = doc["mopie_cmd"].as<String>();
    out.arg = doc["arg"].as<String>();
    return out.name.length() > 0;
}

static bool execAiCmd(const AiCmd &cmd, String &result) {
    if (cmd.name == "led") {
#ifdef HAS_RGB_LED
        uint32_t rgb = strtoul(cmd.arg.c_str(), nullptr, 16);
        bruceConfig.ledColor = rgb;
        beginLed();
        setLedColor(rgb);
        result = "LED set to " + cmd.arg;
#else
        result = "no LED on this board";
#endif
    } else if (cmd.name == "reboot") {
        result = "rebooting";
        ESP.restart();
    } else if (cmd.name == "flash") {
        // front LED blink on boards w/o RGB — just report
        result = cmd.arg == "on" ? "flash on" : "flash off";
    } else if (cmd.name == "beep") {
#if defined(BUZZ_PIN)
        tone(BUZZ_PIN, 880, 120);
        result = "beeped";
#else
        result = "no buzzer";
#endif
    } else if (cmd.name == "sysinfo") {
        result = "heap=" + String(ESP.getFreeHeap()) + " bat=" + String(getBattery()) + "%";
    } else if (cmd.name == "wifi_scan") {
        // AI-triggered WiFi scan, returns AP list the model can reason about
        int n = WiFi.scanNetworks(false, true, false, 400);
        String out = String(n) + " nets: ";
        for (int i = 0; i < n && i < 6; i++) {
            out += WiFi.SSID(i).isEmpty() ? "[hidden]" : WiFi.SSID(i).substring(0, 10);
            out += "(" + String(WiFi.RSSI(i)) + "dB" +
                   String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? ",open) " : ") ");
        }
        if (n > 6) out += "+" + String(n - 6) + " more";
        WiFi.scanDelete();
        result = out;
    } else if (cmd.name == "ble_scan") {
        // AI-triggered BLE device count + names
        NimBLEScan *pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(3, false);
        NimBLEScanResults res = pScan->getResults();
        String out = String(res.getCount()) + " ble devs: ";
        for (int i = 0; i < (int)res.getCount() && i < 5; i++) {
            String nm = String(res.getDevice(i)->getName().c_str());
            out += (nm.length() ? nm : String(res.getDevice(i)->getAddress().toString().c_str()).substring(0, 8)) + " ";
        }
        pScan->stop();
        result = out;
    } else if (cmd.name == "deauth") {
        // one-shot deauth burst against most recent scan result on channel arg (default 1)
        uint8_t ch = cmd.arg.length() ? (uint8_t)atoi(cmd.arg.c_str()) : 1;
        if (ch < 1 || ch > 13) ch = 1;
        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        uint8_t frame[26];
        memcpy(frame, deauth_frame_default, 26);
        for (int b = 0; b < 10; b++) {
            frame[24] = b;
            send_raw_frame(frame, 26);
            delayMicroseconds(500);
        }
        esp_wifi_set_promiscuous(false);
        result = "10 deauth frames sent on ch" + String(ch);
    } else if (cmd.name == "troll_beacon") {
        // fake AP with the AI-chosen SSID
        String ssid = cmd.arg.length() ? cmd.arg.substring(0, 20) : "MOPIE_NET";
        if (!wifiConnected) WiFi.mode(WIFI_MODE_NULL);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        uint8_t beacon[128];
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
        for (int r = 0; r < 20; r++) { send_raw_frame(beacon, p); delay(30); }
        esp_wifi_set_promiscuous(false);
        result = "broadcast SSID '" + ssid + "' x20";
    } else if (cmd.name == "battery") {
        result = String(getBattery()) + "%";
    } else if (cmd.name == "uptime") {
        unsigned long s = millis() / 1000;
        result = String(s / 3600) + "h" + String((s % 3600) / 60) + "m" + String(s % 60) + "s";
    } else {
        result = "unknown command";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Chat completion call (OpenAI-compatible)
// ---------------------------------------------------------------------------
struct ChatMsg { String role; String content; };
static std::vector<ChatMsg> history;

static bool aiChatBlocking(const String &userMsg, String &reply) {
    if (!WiFi.isConnected()) return false;

    // build messages array
    JsonDocument doc;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] =
        "You are Mopie AI running on an ESP32 pentest device (M5StickC Plus). "
        "Be terse: max 60 words. You can control the device by ending your reply "
        "with a JSON command object. Commands: "
        "{\"mopie_cmd\":\"led\",\"arg\":\"RRGGBB\"}, {\"mopie_cmd\":\"beep\"}, "
        "{\"mopie_cmd\":\"sysinfo\"}, {\"mopie_cmd\":\"battery\"}, "
        "{\"mopie_cmd\":\"uptime\"}, {\"mopie_cmd\":\"wifi_scan\"}, "
        "{\"mopie_cmd\":\"ble_scan\"}, {\"mopie_cmd\":\"deauth\",\"arg\":\"1-13\"}, "
        "{\"mopie_cmd\":\"troll_beacon\",\"arg\":\"SSID\"}, {\"mopie_cmd\":\"reboot\"}. "
        "wifi_scan/ble_scan return real results you can analyze. "
        "Only use a command when the user asks for a device action.";
    for (auto &m : history) {
        JsonObject o = msgs.add<JsonObject>();
        o["role"] = m.role;
        o["content"] = m.content;
    }
    JsonObject u = msgs.add<JsonObject>();
    u["role"] = "user";
    u["content"] = userMsg;

    doc["model"] = aiModel;
    doc["max_tokens"] = 150;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(aiEndpoint);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + aiKey);
    http.setTimeout(20000);
    int code = http.POST(body);
    if (code != 200) {
        http.end();
        reply = "HTTP " + String(code);
        return false;
    }
    String resp = http.getString();
    http.end();

    JsonDocument rdoc;
    if (deserializeJson(rdoc, resp)) {
        reply = "bad JSON";
        return false;
    }
    reply = rdoc["choices"][0]["message"]["content"].as<String>();
    return true;
}

// ---------------------------------------------------------------------------
// Blocking network primitives — ONLY call from aiJobTask (big stack), never
// directly from AsyncWebServer callbacks.
// ---------------------------------------------------------------------------
static String aiFetchModelsBlocking() {
    if (!WiFi.isConnected()) return "{\"models\":[],\"error\":\"no wifi\"}";

    // M5StickC Plus has only ~520KB RAM (no PSRAM). The web dashboard + TLS +
    // AsyncWebServer already eat a lot; a huge /v1/models response can exhaust
    // the heap and hardfault/reboot. Gate on heap and cap the buffer.
    if (ESP.getFreeHeap() < 300000) {
        Serial.printf("[ai] models: low heap %d, skipping\n", ESP.getFreeHeap());
        return "{\"models\":[],\"error\":\"low heap " + String(ESP.getFreeHeap()) + "\"}";
    }

    String base = aiEndpoint;
    int cut = base.lastIndexOf("/chat/completions");
    if (cut > 0) base = base.substring(0, cut);

    HTTPClient http;
    http.begin(base + "/models");
    http.addHeader("Authorization", "Bearer " + aiKey);
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return "{\"models\":[],\"error\":\"HTTP " + String(code) + "\"}";
    }
    // Cap body read to 60KB — enough for hundreds of model ids, not enough to OOM
    String resp = http.getString();
    http.end();
    if (resp.length() > 60000) resp = resp.substring(0, 60000);

    JsonDocument in;
    if (deserializeJson(in, resp)) {
        return "{\"models\":[],\"error\":\"bad json\"}";
    }
    JsonDocument out;
    JsonArray arr = out["models"].to<JsonArray>();
    JsonArray src = in["data"].isNull() ? in.as<JsonArray>() : in["data"].as<JsonArray>();
    int n = 0;
    for (JsonObject m : src) {
        if (!m["id"].isNull() && n < 60) { arr.add(m["id"].as<String>()); n++; }
    }
    String body;
    serializeJson(out, body);
    return body;
}

// ---------------------------------------------------------------------------
// Blocking-network-call worker.
// HTTPClient TLS from the AsyncWebServer callback crashes (async_tcp task has a
// tiny stack). These helpers marshal the request to a dedicated task with a
// big stack, wait, and return the result. Safe from ANY task.
// ---------------------------------------------------------------------------
struct AiJob {
    enum Type { CHAT, MODELS } type;
    String q;
    // outputs
    String out;         // reply text (CHAT) or models JSON (MODELS)
    volatile bool done;
    volatile bool ok;
};

static void aiJobTask(void *p) {
    AiJob *job = (AiJob *)p;
    if (job->type == AiJob::CHAT) {
        job->ok = aiChatBlocking(job->q, job->out);
    } else {
        job->out = aiFetchModelsBlocking();
        job->ok = job->out.indexOf("\"error\"") < 0;
    }
    job->done = true;
    vTaskDelete(nullptr);
}

static bool aiRunJob(AiJob::Type type, const String &q, String &out) {
    if (!WiFi.isConnected()) { out = "no wifi"; return false; }
    // Heap guard: don't even try TLS if we're cramped (M5StickC Plus = ~520KB no PSRAM)
    if (ESP.getFreeHeap() < 140000) {
        Serial.printf("[ai] aiRunJob: low heap %d, skipping\n", ESP.getFreeHeap());
        out = "low heap " + String(ESP.getFreeHeap());
        return false;
    }
    AiJob job = {type, q, "", false, false};
    if (xTaskCreate(aiJobTask, "ai_job", 16384, &job, 5, nullptr) != pdPASS) {
        out = "task spawn failed";
        return false;
    }
    unsigned long start = millis();
    while (!job.done && millis() - start < 25000) {
        delay(50);
        // keep UI/web drivers alive while we wait
        vTaskDelay(1);
    }
    if (!job.done) { out = "timeout"; return false; }
    out = job.out;
    return job.ok;
}

// ---------------------------------------------------------------------------
// Web dashboard support (async-safe wrappers)
// ---------------------------------------------------------------------------
String aiFetchModels() {
    String out;
    aiRunJob(AiJob::MODELS, "", out);
    return out;
}

bool aiChatWeb(const String &q, String &reply, String &execResult) {
    execResult = "";
    if (!aiRunJob(AiJob::CHAT, q, reply)) return false;

    history.push_back({"user", q});
    history.push_back({"assistant", reply});
    if (history.size() > 8) history.erase(history.begin());

    AiCmd cmd;
    if (parseAiCmd(reply, cmd)) {
        String result;
        execAiCmd(cmd, result);
        execResult = cmd.name + ": " + result;
    }
    return true;
}

// ---------------------------------------------------------------------------
// UI helpers
// ---------------------------------------------------------------------------
static void aiDrawHeader(const char *title) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.drawCentreString(title, tftWidth / 2, 4, 1);
    tft.drawFastHLine(0, 14, tftWidth, bruceConfig.priColor);
}

static void aiWrapPrint(const String &txt, int startY) {
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.setTextSize(1);
    int y = startY;
    String line = "";
    for (int i = 0; i < (int)txt.length(); i++) {
        line += txt[i];
        if (line.length() >= 38 || txt[i] == '\n') {
            tft.drawString(line, 4, y, 1);
            y += 12;
            line = "";
            if (y > tftHeight - 24) { y = startY; }  // crude scroll: restart
        }
    }
    if (line.length()) tft.drawString(line, 4, y, 1);
}

// ---------------------------------------------------------------------------
// Submenus
// ---------------------------------------------------------------------------
static void aiSetupMenu() {
    while (true) {
        std::vector<Option> opts = {
            {"WiFi Connect", []() { wifiConnectMenu(WIFI_STA); }},
            {"API Key", []() {
                String k = keyboard(aiKey, 64, "API Key");
                if (k.length() > 0 && k != "\x1B") {
                    aiKey = k;
                    aiSaveConf();
                    displaySuccess("Key saved", true);
                }
            }},
            {"Model", []() {
                String m = keyboard(aiModel, 32, "Model name");
                if (m.length() > 0 && m != "\x1B") {
                    aiModel = m;
                    aiSaveConf();
                }
            }},
            {"Endpoint", []() {
                String e = keyboard(aiEndpoint, 96, "API endpoint URL");
                if (e.length() > 0 && e != "\x1B") {
                    aiEndpoint = e;
                    aiSaveConf();
                }
            }},
            {"Status", []() {
                aiDrawHeader("AI STATUS");
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.drawString("WiFi: " + String(WiFi.isConnected() ? "ON " + WiFi.localIP().toString() : "OFF"), 4, 22, 1);
                tft.drawString("Key: " + String(aiKey.length() ? "set (" + String(aiKey.length()) + " chars)" : "NOT SET"), 4, 34, 1);
                tft.drawString("Model: " + aiModel, 4, 46, 1);
                tft.drawString("EP: " + aiEndpoint.substring(0, 30), 4, 58, 1);
                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
                tft.drawCentreString("Browser: http://" + WiFi.localIP().toString() + "/ai", tftWidth/2, tftHeight - 22, 1);
                while (!check(AnyKeyPress)) delay(50);
            }},
            {"Back", []() {}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "AI Setup");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}

static void aiChatMenu() {
    if (!WiFi.isConnected()) {
        displayWarning("Connect WiFi first", true);
        return;
    }
    if (aiKey.length() == 0) {
        displayWarning("Set API key first", true);
        return;
    }
    history.clear();
    aiDrawHeader("MOPIE AI CHAT");

    while (true) {
        tft.fillRect(0, tftHeight - 14, tftWidth, 14, 0x2004);
        tft.setTextColor(TFT_WHITE, 0x2004);
        tft.setTextSize(1);
        tft.drawCentreString("M5:ask  PWR:exit", tftWidth / 2, tftHeight - 11, 1);

        while (!check(SelPress)) {
            if (check(EscPress)) return;
            delay(20);
        }

        // get prompt
        String q = keyboard("", 120, "Ask Mopie AI");
        if (q.length() == 0 || q == "\x1B") continue;

        aiDrawHeader("MOPIE AI");
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.setTextSize(1);
        tft.drawString("You: " + q.substring(0, 36), 4, 20, 1);
        tft.drawCentreString("thinking...", tftWidth / 2, tftHeight / 2, 1);

        String reply;
        if (aiChatBlocking(q, reply)) {
            history.push_back({"user", q});
            history.push_back({"assistant", reply});
            if (history.size() > 8) history.erase(history.begin());  // cap context

            // redraw area
            tft.fillRect(0, 16, tftWidth, tftHeight - 30, bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawString("You:", 4, 18, 1);
            aiWrapPrint(q, 30);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawString("AI:", 4, 54, 1);
            aiWrapPrint(reply, 66);

            // check for device command in reply
            AiCmd cmd;
            if (parseAiCmd(reply, cmd)) {
                String result;
                execAiCmd(cmd, result);
                tft.setTextColor(DEFAULT_ACCENT, bruceConfig.bgColor);
                tft.drawString("[exec] " + cmd.name + ": " + result, 4, tftHeight - 30, 1);
            }
        } else {
            displayError(reply, true);
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
void mopie_ai_setup() {
    aiLoadConf();
    while (true) {
        std::vector<Option> opts = {
            {"Chat", []() { aiChatMenu(); }},
            {"Web Dashboard", []() {
                // One-shot: host the /ai dashboard. Prefer STA (your router IP),
                // wait for a real IP, else fall back to AP mode (MopieNet, 192.168.4.1)
                // so the dashboard is ALWAYS reachable — never 0.0.0.0.
                Serial.printf("[ai] WebDashboard: freeHeap=%d\n", ESP.getFreeHeap());
                bool useAP = false;
                if (!WiFi.isConnected()) {
                    stopBLEStack();   // free heap for WiFi+web on 520KB chip
                    vTaskDelay(150);
                    aiDrawHeader("AI WEB SETUP");
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                    tft.drawCentreString("Connecting to WiFi...", tftWidth / 2, tftHeight / 2, 1);
                    wifiConnectMenu(WIFI_STA);
                    // wait up to 10s for a real IP (DHCP), not just association
                    unsigned long t0 = millis();
                    while ((!WiFi.isConnected() || WiFi.localIP().toString() == "0.0.0.0")
                           && millis() - t0 < 10000) {
                        delay(100);
                    }
                    if (!WiFi.isConnected() || WiFi.localIP().toString() == "0.0.0.0") {
                        // no router IP → fall back to access point (always 192.168.4.1)
                        useAP = true;
                        aiDrawHeader("NO ROUTER");
                        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                        tft.drawCentreString("Using AP: MopieNet/mopienet", tftWidth / 2, tftHeight / 2, 1);
                        tft.drawCentreString("open http://192.168.4.1/ai", tftWidth / 2, tftHeight / 2 + 14, 1);
                        delay(1500);
                    } else {
                        Serial.printf("[ai] connected: freeHeap=%d ip=%s\n",
                                      ESP.getFreeHeap(), WiFi.localIP().toString().c_str());
                    }
                } else if (WiFi.localIP().toString() == "0.0.0.0") {
                    useAP = true;   // claimed connected but no IP — serve from AP instead
                }
                // start host + register /ai routes (STA, or AP if we fell back)
                startWebUi(useAP ? WIFI_AP : WIFI_STA);   // "Run in background" keeps it alive
            }},
            {"Setup", []() { aiSetupMenu(); }},
            {"Back", []() {}},
        };
        int sel = loopOptions(opts, MENU_TYPE_SUBMENU, "Mopie AI");
        if (sel == -1 || sel == (int)opts.size() - 1) return;
    }
}
