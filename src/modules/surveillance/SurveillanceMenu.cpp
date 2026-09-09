#include "SurveillanceMenu.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <globals.h>
#include <math.h>

static const char* FILTER_NAMES[] = {
    "All Threats",
    "Flock ALPR Only",
    "Police MDT Only",
    "Bodycam Only",
    "ALPR Cameras",
    "Municipal CCTV",
    "Drones RID Only"
};

static const Surveillance::ThreatCategory FILTER_CATS[] = {
    Surveillance::CAT_UNKNOWN,
    Surveillance::CAT_FLOCK_ALPR,
    Surveillance::CAT_POLICE_VEHICLE,
    Surveillance::CAT_POLICE_BODYCAM,
    Surveillance::CAT_OTHER_ALPR,
    Surveillance::CAT_SURVEILLANCE_CAM,
    Surveillance::CAT_DRONE_UAV
};

static uint8_t currentFilterIdx = 0;

void SurveillanceMenu::optionsMenu() {
    auto& engine = Surveillance::SurveillanceEngine::getInstance();

    while (true) {
        options.clear();
        options.push_back({"Tactical Radar", [=]() { runRadarHud(); }});
        options.push_back({"Live Sniffer", [=]() { runThreatScanner(); }});

        String filterStr = "Filter: " + String(FILTER_NAMES[currentFilterIdx]);
        options.push_back({filterStr, [=, &engine]() {
            currentFilterIdx = (currentFilterIdx + 1) % 7;
            engine.activeFilter = FILTER_CATS[currentFilterIdx];
        }});

        String logStr = "Threat Log (" + String(engine.getThreatCount()) + ")";
        options.push_back({logStr, [=]() { showThreatLog(); }});

        String soundStr = "Audio Chime: " + String(engine.isAudioAlert() ? "ON" : "OFF");
        options.push_back({soundStr, [=, &engine]() {
            engine.setAudioAlert(!engine.isAudioAlert());
        }});

        String ledStr = "LED Flash: " + String(engine.isLedAlert() ? "ON" : "OFF");
        options.push_back({ledStr, [=, &engine]() {
            engine.setLedAlert(!engine.isLedAlert());
        }});

        options.push_back({"Clear Log", [=, &engine]() {
            engine.clearThreats();
            displayInfo("Threat Log Cleared", true);
        }});

        addOptionToMainMenu();
        int res = loopOptions(options, MENU_TYPE_SUBMENU, "Surveillance");
        if (res < 0 || returnToMenu) break;
    }
}

void SurveillanceMenu::drawIcon(float scale) {
    clearIconArea();

    int radius = scale * 26;
    if (radius < 10) radius = 10;

    // Draw concentric radar circles
    tft.drawCircle(iconCenterX, iconCenterY, radius, mopieConfig.priColor);
    tft.drawCircle(iconCenterX, iconCenterY, radius * 2 / 3, mopieConfig.secColor);
    tft.drawCircle(iconCenterX, iconCenterY, radius / 3, mopieConfig.secColor);

    // Crosshairs
    tft.drawFastHLine(iconCenterX - radius - 4, iconCenterY, (radius + 4) * 2, mopieConfig.secColor);
    tft.drawFastVLine(iconCenterX, iconCenterY - radius - 4, (radius + 4) * 2, mopieConfig.secColor);

    // Target blip (Red)
    int blipX = iconCenterX + radius / 2;
    int blipY = iconCenterY - radius / 3;
    tft.fillCircle(blipX, blipY, scale * 3.5, TFT_RED);
    tft.drawCircle(blipX, blipY, scale * 5.5, TFT_RED);
}

void SurveillanceMenu::runRadarHud() {
    auto& engine = Surveillance::SurveillanceEngine::getInstance();
    engine.startScan(FILTER_CATS[currentFilterIdx]);

    tft.fillScreen(TFT_BLACK);

    const int radarCenterX = 68;
    const int radarCenterY = tftHeight / 2;
    const int maxRadius = 56;
    float sweepAngle = 0.0f;
    unsigned long lastUiUpdate = 0;
    int selectedThreatIdx = -1;

    while (true) {
        InputHandler();
        engine.update();

        if (check(EscPress)) {
            break;
        }

        if (millis() - lastUiUpdate > 35) { // ~30 FPS HUD
            lastUiUpdate = millis();
            sweepAngle += 0.08f;
            if (sweepAngle >= 6.28318f) sweepAngle -= 6.28318f;

            // Radar background and grid
            tft.drawCircle(radarCenterX, radarCenterY, maxRadius, 0x03E0);       // Dark green outer
            tft.drawCircle(radarCenterX, radarCenterY, maxRadius * 2 / 3, 0x0260);
            tft.drawCircle(radarCenterX, radarCenterY, maxRadius / 3, 0x01E0);
            tft.drawFastHLine(radarCenterX - maxRadius, radarCenterY, maxRadius * 2, 0x01E0);
            tft.drawFastVLine(radarCenterX, radarCenterY - maxRadius, maxRadius * 2, 0x01E0);

            // Rotating sweep line
            int sweepX = radarCenterX + (int)(cosf(sweepAngle) * maxRadius);
            int sweepY = radarCenterY + (int)(sinf(sweepAngle) * maxRadius);
            tft.drawLine(radarCenterX, radarCenterY, sweepX, sweepY, 0x07E0);

            // Draw status header
            tft.fillRect(0, 0, tftWidth, 14, 0x0841);
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, 0x0841);
            tft.setCursor(4, 3);
            tft.printf("RADAR [%s] TGT:%u", FILTER_NAMES[currentFilterIdx], engine.getThreatCount());

            // Render threats on radar
            const auto& threats = engine.getThreats();
            for (size_t i = 0; i < threats.size(); i++) {
                const auto& t = threats[i];
                // Hash MAC to get consistent pseudo-angle
                uint32_t macHash = 0;
                for (size_t c = 0; c < t.mac.length(); c++) macHash = (macHash * 31) + t.mac[c];
                float angle = (float)(macHash % 360) * (3.14159f / 180.0f);

                // Distance maps to radar radius (0 to 100m -> 0 to maxRadius)
                float distClamped = t.estimatedDistanceMeters;
                if (distClamped > 100.0f) distClamped = 100.0f;
                float r = (distClamped / 100.0f) * (maxRadius - 4) + 4.0f;

                int bx = radarCenterX + (int)(cosf(angle) * r);
                int by = radarCenterY + (int)(sinf(angle) * r);

                uint16_t color = Surveillance::categoryColor(t.category);
                tft.fillCircle(bx, by, 3, color);
                tft.drawCircle(bx, by, 5, color);
            }

            // Right side tactical telemetry panel (X: 132 to 240)
            const int infoX = 132;
            const int infoW = tftWidth - infoX;
            tft.fillRect(infoX, 16, infoW, tftHeight - 16, 0x0000);
            tft.drawFastVLine(infoX - 2, 14, tftHeight - 14, 0x03E0);

            if (threats.empty()) {
                tft.setTextColor(0x07E0, TFT_BLACK);
                tft.setCursor(infoX + 2, 22);
                tft.print("SCANNING...");
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.setCursor(infoX + 2, 36);
                tft.print("2.4G & BLE");
                tft.setCursor(infoX + 2, 48);
                tft.print("PROMISCUOUS");
                tft.setCursor(infoX + 2, 60);
                tft.print("AIRWAVES OK");
            } else {
                // Show latest or strongest threat
                const auto& top = threats.back();
                uint16_t catCol = Surveillance::categoryColor(top.category);

                tft.setTextColor(catCol, TFT_BLACK);
                tft.setCursor(infoX + 2, 18);
                tft.print(Surveillance::categoryName(top.category));

                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setCursor(infoX + 2, 30);
                String shortName = top.name.substring(0, 15);
                tft.print(shortName);

                tft.setTextColor(0x07E0, TFT_BLACK);
                tft.setCursor(infoX + 2, 42);
                tft.printf("DIST: ~%.1fm", top.estimatedDistanceMeters);

                tft.setCursor(infoX + 2, 54);
                tft.printf("RSSI: %ddBm", top.rssi);

                // RSSI Bar
                int barW = map(constrain(top.rssi, -95, -30), -95, -30, 2, infoW - 8);
                tft.drawRect(infoX + 2, 66, infoW - 6, 6, 0x03E0);
                tft.fillRect(infoX + 3, 67, barW, 4, catCol);

                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.setCursor(infoX + 2, 76);
                String shortMac = top.mac.substring(0, 14);
                tft.print(shortMac);

                tft.setCursor(infoX + 2, 88);
                tft.printf("HITS: %u", top.hits);

                tft.setTextColor(TFT_CYAN, TFT_BLACK);
                tft.setCursor(infoX + 2, 102);
                tft.print("SEL: DETAIL");
            }

            // Bottom prompt
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(4, tftHeight - 10);
            tft.print("PWR/ESC: EXIT");

            // Handle inspection
            if (check(SelPress) && !threats.empty()) {
                showThreatDetails(threats.back());
                tft.fillScreen(TFT_BLACK);
            }
        }
        delay(1);
    }

    engine.stopScan();
    tft.fillScreen(mopieConfig.bgColor);
}

void SurveillanceMenu::runThreatScanner() {
    auto& engine = Surveillance::SurveillanceEngine::getInstance();
    engine.startScan(FILTER_CATS[currentFilterIdx]);

    tft.fillScreen(TFT_BLACK);
    unsigned long lastRender = 0;

    while (true) {
        InputHandler();
        engine.update();

        if (check(EscPress)) break;

        if (millis() - lastRender > 100) {
            lastRender = millis();

            // Header
            tft.fillRect(0, 0, tftWidth, 16, 0x0841);
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, 0x0841);
            tft.setCursor(4, 4);
            tft.printf("LIVE SNIFFER | THREATS: %u", engine.getThreatCount());

            const auto& threats = engine.getThreats();
            int startIdx = (threats.size() > 4) ? (threats.size() - 4) : 0;
            int yPos = 18;

            if (threats.empty()) {
                tft.fillRect(0, 18, tftWidth, tftHeight - 30, TFT_BLACK);
                tft.setTextColor(0x07E0, TFT_BLACK);
                tft.setCursor(20, 50);
                tft.print("SNIFFING 802.11 & BLE PACKETS...");
                tft.setCursor(30, 65);
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.print("No surveillance targets nearby");
            } else {
                for (size_t i = startIdx; i < threats.size(); i++) {
                    const auto& t = threats[i];
                    uint16_t catColor = Surveillance::categoryColor(t.category);

                    tft.fillRect(2, yPos, tftWidth - 4, 26, 0x10A2);
                    tft.drawRect(2, yPos, tftWidth - 4, 26, catColor);

                    // Line 1: Category & Name
                    tft.setTextColor(catColor, 0x10A2);
                    tft.setCursor(6, yPos + 3);
                    tft.printf("[%s] ", Surveillance::categoryName(t.category));
                    tft.setTextColor(TFT_WHITE, 0x10A2);
                    tft.print(t.name.substring(0, 18));

                    // Line 2: MAC, RSSI, Distance
                    tft.setTextColor(TFT_CYAN, 0x10A2);
                    tft.setCursor(6, yPos + 14);
                    tft.printf("%s  %ddBm  ~%.1fm  #%u", t.mac.c_str(), t.rssi, t.estimatedDistanceMeters, t.hits);

                    yPos += 28;
                    if (yPos + 26 > tftHeight - 12) break;
                }
            }

            // Footer
            tft.fillRect(0, tftHeight - 12, tftWidth, 12, TFT_BLACK);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(4, tftHeight - 10);
            tft.print("PWR/ESC: STOP | SEL: VIEW LOG");

            if (check(SelPress) && !threats.empty()) {
                showThreatLog();
                tft.fillScreen(TFT_BLACK);
            }
        }
        delay(1);
    }

    engine.stopScan();
    tft.fillScreen(mopieConfig.bgColor);
}

void SurveillanceMenu::showThreatDetails(const Surveillance::DetectedThreat& threat) {
    tft.fillScreen(TFT_BLACK);
    uint16_t catColor = Surveillance::categoryColor(threat.category);

    // Title bar
    tft.fillRect(0, 0, tftWidth, 18, catColor);
    tft.setTextColor(TFT_BLACK, catColor);
    tft.setTextSize(1);
    tft.setCursor(4, 5);
    tft.printf("THREAT: %s", Surveillance::categoryName(threat.category));

    int y = 24;
    auto printRow = [&](const char* label, const String& val, uint16_t col) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setCursor(6, y);
        tft.print(label);
        tft.setTextColor(col, TFT_BLACK);
        tft.setCursor(75, y);
        tft.print(val);
        y += 12;
    };

    printRow("Name:", threat.name, TFT_WHITE);
    printRow("Vendor:", threat.vendor, catColor);
    printRow("BSSID/MAC:", threat.mac, TFT_CYAN);
    printRow("Channel:", (threat.channel == 0) ? "BLE Adv" : String(threat.channel), TFT_YELLOW);
    printRow("RSSI:", String(threat.rssi) + " dBm", (threat.rssi > -60) ? TFT_RED : TFT_GREEN);
    printRow("Est. Dist:", String(threat.estimatedDistanceMeters, 1) + " meters", TFT_ORANGE);
    printRow("Packets:", String(threat.hits) + " hits", TFT_WHITE);

    // Proximity bar
    y += 4;
    tft.drawRect(6, y, tftWidth - 12, 10, catColor);
    int barLen = map(constrain(threat.rssi, -95, -30), -95, -30, 2, tftWidth - 16);
    tft.fillRect(8, y + 2, barLen, 6, catColor);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(6, tftHeight - 12);
    tft.print("Press ANY button to return");

    while (true) {
        InputHandler();
        if (check(AnyKeyPress) || check(SelPress) || check(EscPress)) break;
        delay(20);
    }
}

void SurveillanceMenu::showThreatLog() {
    auto& engine = Surveillance::SurveillanceEngine::getInstance();
    const auto& threats = engine.getThreats();

    if (threats.empty()) {
        displayInfo("No threats logged", true);
        return;
    }

    while (true) {
        options.clear();
        for (size_t i = 0; i < threats.size(); i++) {
            const auto& t = threats[i];
            String label = "[" + String(Surveillance::categoryName(t.category)).substring(0, 5) + "] " +
                           t.name.substring(0, 12) + " (" + String(t.rssi) + "dBm)";
            options.push_back({label, [&, i]() {
                showThreatDetails(threats[i]);
            }});
        }
        options.push_back({"Back", [=]() {}});

        int res = loopOptions(options, MENU_TYPE_SUBMENU, "Threat Log");
        if (res < 0 || res >= (int)threats.size()) break;
    }
}
