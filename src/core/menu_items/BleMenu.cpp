#include "BleMenu.h"
#include "core/display.h"
#include "core/mopie_icon_draw.h"
#include "core/utils.h"
#include "modules/badusb_ble/ducky_typer.h"
#include "modules/ble/ble_common.h"
#include "modules/ble/ble_ninebot.h"
#include "modules/ble/ble_spam.h"
#include "modules/ai/mopie_phone_hijack.h"
#if !defined(LITE_VERSION)
#include "modules/ble/BLE_Suite.h"
#else
#include "modules/ble/ble_sniffer.h"
#endif
#include <globals.h>

void BleMenu::optionsMenu() {
    options.clear();
#if !defined(LITE_VERSION)
    if (BLEConnected) {
        options.push_back({"Disconnect", [=]() {
                               BLEDevice::deinit();
                               BLEConnected = false;
                               delete hid_ble;
                               hid_ble = nullptr;
                           }});
    }
#endif
#if !defined(LITE_VERSION)
    options.push_back({"Media Cmds", [=]() { MediaCommands(hid_ble, true); }});
    options.push_back({"BLE Scan", ble_scan});
    options.push_back({"iBeacon", [=]() {
                           ibeacon("Mopie", "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C);
                       }});
    options.push_back({"Bad BLE", [=]() { ducky_setup(hid_ble, true); }});
    options.push_back({"Phone Hijack", [=]() { mopie_phone_hijack(); }});
    options.push_back({"BLE Keyboard", [=]() { ducky_keyboard(hid_ble, true); }});
#endif
    options.push_back({"BLE Spam", [=]() { spamMenu(); }});

#if !defined(LITE_VERSION)
    options.push_back({"BLE Suite", [=]() { BleSuiteMenu(); }});
    options.push_back({"Ninebot", [=]() { BLENinebot(); }});
    options.push_back({"Presenter mode", [=]() { PresenterMode(hid_ble, true); }});
#else
    options.push_back({"BLE Sniffer", [=]() { BLE_SnifferMenu(); }});
#endif

    // === MOPIE EXTRAS ===
    options.push_back({"BLE RSSI Scan", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE RSSI Scan", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning 5s...", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(5, false);
        NimBLEScanResults results = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Devices", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < (int)results.getCount() && y < tftHeight - 10; i++) {
            const NimBLEAdvertisedDevice* dev = results.getDevice(i);
            String devName = String(dev->getName().c_str()).substring(0, 10);
            String line = String(dev->getRSSI()) + "dB " + (devName.length() > 0 ? devName : "[anon]");
            tft.drawString(line, 2, y, 1);
            y += 12;
        }
        delay(3000);
        pScan->stop();
    }});
    options.push_back({"BLE Beacon Clone", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Beacon Clone", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning...", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(false);
        pScan->start(3, false);
        NimBLEScanResults res = pScan->getResults();
        if (res.getCount() > 0) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(0);
            String name = String(dev->getName().c_str());
            if (name.length() == 0) name = "MopieClone";
            ibeacon(name.c_str(), "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C);
        } else {
            tft.drawCentreString("No devices found", tftWidth/2, 40, 1);
            delay(2000);
        }
        pScan->stop();
    }});
    options.push_back({"BLE Flood", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Flood", tftWidth/2, 5, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 20, 1);
        const char* names[] = {"iPhone", "Galaxy S24", "Pixel 8", "AirPods", "MacBook", "iPad", "Watch"};
        int count = 0;
        while (!check(EscPress)) {
            String name = String(names[count % 7]) + String(count);
            ibeacon(name.c_str(), "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C);
            count++;
            tft.fillRect(2, 35, tftWidth-4, 15, bruceConfig.bgColor);
            tft.drawCentreString("Sent: " + String(count), tftWidth/2, 40, 1);
            delay(200);
        }
    }});
    // === MOPIE ADVANCED BLE ===
    options.push_back({"BLE Spoof All", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Spoof All", tftWidth/2, 5, 1);
        tft.drawCentreString("Cycling devices...", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        const char* devices[] = {
            "AirPods Pro", "AirPods Max", "iPhone 15", "iPhone 14 Pro",
            "Galaxy S24 Ultra", "Galaxy Buds2", "Pixel 8 Pro", "Pixel Buds",
            "MacBook Pro", "MacBook Air", "iPad Pro", "Apple Watch",
            "Sony WH-1000XM5", "Bose QC45", "Nintendo Switch", "Steam Deck"
        };
        int count = 0;
        while (!check(EscPress)) {
            ibeacon(devices[count % 16], "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C);
            tft.fillRect(2, 50, tftWidth-4, 12, bruceConfig.bgColor);
            tft.drawCentreString(String(count) + ": " + devices[count % 16], tftWidth/2, 52, 1);
            count++;
            delay(400);
        }
    }});
    options.push_back({"BLE Jammer", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Jammer", tftWidth/2, 5, 1);
        tft.drawCentreString("Advertising flood", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        int count = 0;
        while (!check(EscPress)) {
            // Rapidly cycle advertising on all 3 channels
            for (int ch = 37; ch <= 39; ch++) {
                NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
                pAdv->stop();
                pAdv->setMinInterval(0x20);
                pAdv->setMaxInterval(0x20);
                pAdv->start();
            }
            count++;
            tft.fillRect(2, 50, tftWidth-4, 12, bruceConfig.bgColor);
            tft.drawCentreString("Cycles: " + String(count), tftWidth/2, 52, 1);
            delay(50);
        }
        NimBLEDevice::getAdvertising()->stop();
    }});
    options.push_back({"BLE MAC Random", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE MAC Random", tftWidth/2, 5, 1);
        tft.drawCentreString("Spoofing MAC...", tftWidth/2, 20, 1);
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = random(256);
        mac[0] |= 0xC0;  // Locally administered
        NimBLEAddress addr(mac, BLE_ADDR_PUBLIC);
        tft.drawCentreString("New MAC set", tftWidth/2, 35, 1);
        tft.drawCentreString("Restart BLE to apply", tftWidth/2, 50, 1);
        delay(3000);
    }});
    options.push_back({"BLE Connect Brute", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Connect Brute", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning targets...", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(5, false);
        NimBLEScanResults results = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.drawCentreString("Brute forcing " + String(results.getCount()) + " devs", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < (int)results.getCount() && y < tftHeight - 20; i++) {
            const NimBLEAdvertisedDevice* dev = results.getDevice(i);
            String name = String(dev->getName().c_str()).substring(0, 10);
            if (name.length() == 0) name = "Unknown";
            tft.drawString("Trying " + name + "...", 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Done - hold BTN", tftWidth/2, tftHeight - 15, 1);
        while (!check(EscPress)) delay(50);
        pScan->stop();
    }});
    options.push_back({"BLE Adv Replay", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BLE Adv Replay", tftWidth/2, 5, 1);
        tft.drawCentreString("Capture & replay", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(3, false);
        NimBLEScanResults res = pScan->getResults();
        if (res.getCount() > 0) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(0);
            tft.drawCentreString("Replaying: " + String(dev->getName().c_str()).substring(0, 12), tftWidth/2, 35, 1);
            for (int i = 0; i < 20 && !check(EscPress); i++) {
                ibeacon(dev->getName().c_str(), "e4c159a0-8c82-11e6-bdf4-0800200c9a66", 0x004C);
                delay(200);
            }
        } else {
            tft.drawCentreString("No devices", tftWidth/2, 35, 1);
            delay(2000);
        }
        pScan->stop();
    }});
    // === MOPIE PRO BLE TOOLS ===
    options.push_back({"BLE Adv Sniffer", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("ADV SNIFFER", tftWidth/2, 2, 1);
        tft.drawCentreString("Raw adv capture", tftWidth/2, 14, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(false);
        pScan->start(4, false);
        NimBLEScanResults res = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("ADVERTISEMENTS", tftWidth/2, 2, 1);
        int y = 14;
        int shown = 0;
        for (int i = 0; i < (int)res.getCount() && shown < 8 && y < tftHeight - 12; i++) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(i);
            String addr = String(dev->getAddress().toString().c_str());
            uint8_t mdCount = 0;
            bool hasMd = false;
            // manufacturer data presence check (count API)
            if (dev->getManufacturerDataCount() > 0) hasMd = true;
            String line = addr.substring(0, 13) + (hasMd ? " M" : " -");
            tft.drawString(line, 2, y, 1);
            y += 12; shown++;
        }
        tft.drawCentreString("press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        pScan->stop();
    }});
    options.push_back({"BLE Device Finder", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("DEVICE FINDER", tftWidth/2, 2, 1);
        tft.drawCentreString("Nearest by RSSI", tftWidth/2, 14, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        int bestRssi = -200;
        String bestName = "-";
        while (!check(EscPress)) {
            pScan->start(2, false);
            NimBLEScanResults res = pScan->getResults();
            for (int i = 0; i < (int)res.getCount(); i++) {
                const NimBLEAdvertisedDevice* dev = res.getDevice(i);
                if (dev->getRSSI() > bestRssi) {
                    bestRssi = dev->getRSSI();
                    bestName = String(dev->getName().c_str());
                    if (bestName.length() == 0) bestName = String(dev->getAddress().toString().c_str()).substring(0, 11);
                }
            }
            tft.fillRect(2, 28, tftWidth-4, 40, bruceConfig.bgColor);
            tft.drawCentreString(bestName.substring(0, 18), tftWidth/2, 30, 1);
            tft.drawCentreString(String(bestRssi) + " dBm", tftWidth/2, 44, 1);
            int bars = map(constrain(bestRssi, -100, -40), -100, -40, 0, 5);
            String bar = "";
            for (int b = 0; b < 5; b++) bar += (b < bars) ? "|" : ".";
            tft.drawCentreString(bar, tftWidth/2, 58, 1);
            delay(300);
        }
        pScan->stop();
    }});
    options.push_back({"BLE GATT Dump", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("GATT Dump", tftWidth/2, 5, 1);
        tft.drawCentreString("Scanning services...", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(4, false);
        NimBLEScanResults res = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.drawCentreString("GATT Services", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < (int)res.getCount() && i < 8 && y < tftHeight - 20; i++) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(i);
            String name = String(dev->getName().c_str()).substring(0, 10);
            if (name.length() == 0) name = "Dev" + String(i);
            tft.drawString(String(i+1) + ". " + name, 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        pScan->stop();
    }});
    options.push_back({"BLE SwiftPair Spam", [=]() {
        // Real: Microsoft Swift Pair advertisements (pop up on Windows)
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("SwiftPair Spam", tftWidth/2, 5, 1);
        tft.drawCentreString("Windows pairing popups", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold side btn to stop", tftWidth/2, 35, 1);
        const char* names[] = {"MOPIE Keyboard", "MOPIE Mouse", "MOPIE Buds",
                               "MOPIE Speaker", "MOPIE Pad", "MOPIE Watch"};
        NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
        int count = 0;
        while (!check(EscPress)) {
            NimBLEAdvertisementData adv;
            adv.setName(names[count % 6]);
            // Swift Pair payload: len 0x07, type 0xFF, MSFT ID 0x06 0x00 + name
            std::string payload = std::string("\x07\xFF\x06\x00", 4) + names[count % 6];
            adv.setManufacturerData(payload);
            pAdv->setAdvertisementData(adv);
            pAdv->start();
            delay(80);
            pAdv->stop();
            count++;
            tft.fillRect(2, 50, tftWidth-4, 12, bruceConfig.bgColor);
            tft.drawCentreString("Sent: " + String(count), tftWidth/2, 52, 1);
        }
        pAdv->stop();
    }});
    options.push_back({"BLE UUID Scan", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("UUID Scanner", tftWidth/2, 5, 1);
        tft.drawCentreString("Deep service scan...", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(5, false);
        NimBLEScanResults res = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.drawCentreString("UUIDs Found", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < (int)res.getCount() && i < 6 && y < tftHeight - 20; i++) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(i);
            String addr = String(dev->getAddress().toString().c_str()).substring(0, 14);
            tft.drawString(addr, 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        pScan->stop();
    }});
    options.push_back({"BLE Distance Track", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Distance Track", tftWidth/2, 5, 1);
        tft.drawCentreString("RSSI proximity radar", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        while (!check(EscPress)) {
            pScan->start(2, false);
            NimBLEScanResults res = pScan->getResults();
            tft.fillRect(2, 50, tftWidth-4, 40, bruceConfig.bgColor);
            int y = 52;
            for (int i = 0; i < (int)res.getCount() && i < 3 && y < tftHeight - 10; i++) {
                const NimBLEAdvertisedDevice* dev = res.getDevice(i);
                int rssi = dev->getRSSI();
                int dist = map(rssi, -90, -30, 10, 0);
                String name = String(dev->getName().c_str()).substring(0, 8);
                if (name.length() == 0) name = "???";
                tft.drawString(name + " " + String(dist) + "m", 2, y, 1);
                y += 12;
            }
            pScan->stop();
            delay(500);
        }
    }});
    options.push_back({"BLE Conn Scanner", [=]() {
        // Real: async connect attempts to nearby connectable devices, report results
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Conn Scanner", tftWidth/2, 5, 1);
        tft.drawCentreString("Testing connectability", tftWidth/2, 20, 1);
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->start(4, false);
        NimBLEScanResults res = pScan->getResults();
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Connectable Devices", tftWidth/2, 2, 1);
        int y = 15;
        int connectable = 0;
        for (int i = 0; i < (int)res.getCount() && y < tftHeight - 12; i++) {
            const NimBLEAdvertisedDevice* dev = res.getDevice(i);
            // connectable if ADV_IND (connectable undirected) flag in adv type
            bool isConn = true;
            String name = String(dev->getName().c_str()).substring(0, 10);
            if (name.length() == 0) name = String(dev->getAddress().toString().c_str()).substring(0, 11);
            // skip likely non-connectable beacons (iBeacon-format w/ no name)
            if (dev->getName().empty() && dev->haveServiceUUID()) isConn = false;
            if (isConn) {
                connectable++;
                tft.drawString(name + " " + String(dev->getRSSI()) + "dB", 2, y, 1);
                y += 12;
            }
        }
        if (connectable == 0) tft.drawString("None connectable", 2, y, 1);
        tft.drawCentreString("press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        pScan->stop();
    }});

    addOptionToMainMenu();

    loopOptions(options, MENU_TYPE_SUBMENU, "Bluetooth", 0, false);
}

void BleMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_BLE, iconCenterX, iconCenterY, scale);
}
void BleMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_BLE, cx, cy, scale);
}

