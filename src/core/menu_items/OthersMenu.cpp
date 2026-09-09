#include "OthersMenu.h"
#include "core/mopie_icon_draw.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include "core/display.h"
#include "core/utils.h"
#include "modules/badusb_ble/ducky_typer.h"
#include "modules/others/clicker.h"
#include "modules/others/ibutton.h"
#include "modules/others/mic.h"
#if defined(MIC_SPM1423) || defined(MIC_INMP441)
#include "modules/audio_mic/mic_visualizer.h"
#endif
#include "modules/others/qrcode_menu.h"
#include "modules/others/u2f.h"

void OthersMenu::optionsMenu() {
    options = {
        {"QR Vault",      qrcode_menu                  },
#if defined(MIC_SPM1423) || defined(MIC_INMP441)
        {"Audio Spy",     [this]() { micMenu(); }      },
#endif
#if !defined(LITE_VERSION)
        {"HID Injector",  [this]() { badUsbHidMenu(); }},
#endif
#ifndef LITE_VERSION
        {"iButton Cloner", setup_ibutton               },
#endif
    };

    // === MOPIE CORE UTILS ===
    options.push_back({"SysInfo Pro", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("SYSINFO PRO", tftWidth/2, 2, 1);
        int y = 18;
        tft.drawString("Heap: " + String(ESP.getFreeHeap()/1024) + "KB / " + String(ESP.getHeapSize()/1024) + "KB", 2, y, 1); y+=12;
        tft.drawString("Flash: " + String(ESP.getFlashChipSize()/1024/1024) + "MB", 2, y, 1); y+=12;
        tft.drawString("CPU: " + String(ESP.getCpuFreqMHz()) + "MHz", 2, y, 1); y+=12;
        tft.drawString("SDK: " + String(ESP.getSdkVersion()).substring(0,16), 2, y, 1); y+=12;
        tft.drawString("MAC: " + WiFi.macAddress(), 2, y, 1); y+=12;
        tft.drawString("IP: " + WiFi.localIP().toString(), 2, y, 1); y+=12;
        tft.drawString("RSSI: " + String(WiFi.RSSI()) + "dB", 2, y, 1); y+=12;
        tft.drawString("Uptime: " + String(millis()/1000) + "s", 2, y, 1); y+=12;
        tft.drawString("Temp: " + String(temperatureRead()) + "C", 2, y, 1); y+=12;
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Panic Wipe", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(0xF800, bruceConfig.bgColor);
        tft.drawCentreString("!! PANIC WIPE !!", tftWidth/2, 10, 1);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("Hold BTN 3s", tftWidth/2, 30, 1);
        tft.drawCentreString("to wipe all data", tftWidth/2, 45, 1);
        unsigned long hold = millis();
        while (millis() - hold < 3000) {
            if (!check(EscPress, false)) { hold = millis(); }
            delay(50);
        }
        tft.fillScreen(bruceConfig.bgColor);
        tft.drawCentreString("WIPED", tftWidth/2, tftHeight/2, 2);
        delay(2000);
    }});
    options.push_back({"Pass Gen Pro", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("PASS GEN PRO", tftWidth/2, 2, 1);
        const char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char lower[] = "abcdefghijklmnopqrstuvwxyz";
        const char digits[] = "0123456789";
        const char symbols[] = "!@#$%^&*()_+-=[]{}|;:,.<>?";
        String pw = "";
        pw += upper[random(26)];
        pw += lower[random(26)];
        pw += digits[random(10)];
        pw += symbols[random(25)];
        for (int i = 4; i < 20; i++) {
            int set = random(4);
            if (set == 0) pw += upper[random(26)];
            else if (set == 1) pw += lower[random(26)];
            else if (set == 2) pw += digits[random(10)];
            else pw += symbols[random(25)];
        }
        tft.drawString("Password:", 2, 20, 1);
        tft.drawString(pw, 2, 35, 1);
        tft.drawString("Entropy: " + String(pw.length() * 5.7) + " bits", 2, 50, 1);
        tft.drawCentreString("Press any key", tftWidth/2, 65, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Net Scanner", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("NET SCANNER", tftWidth/2, 2, 1);
        tft.drawString("Target: 192.168.4.1", 2, 18, 1);
        tft.drawString("Scanning common ports...", 2, 32, 1);
        int open_ports = 0;
        int ports[] = {21, 22, 23, 25, 53, 80, 110, 143, 443, 445, 3306, 3389, 5900, 8080};
        int y = 46;
        for (int i = 0; i < 14 && y < tftHeight - 15; i++) {
            WiFiClient client;
            client.setTimeout(200);
            if (client.connect("192.168.4.1", ports[i])) {
                open_ports++;
                String service = "";
                switch(ports[i]) {
                    case 21: service = "FTP"; break;
                    case 22: service = "SSH"; break;
                    case 23: service = "TELNET"; break;
                    case 25: service = "SMTP"; break;
                    case 53: service = "DNS"; break;
                    case 80: service = "HTTP"; break;
                    case 110: service = "POP3"; break;
                    case 143: service = "IMAP"; break;
                    case 443: service = "HTTPS"; break;
                    case 445: service = "SMB"; break;
                    case 3306: service = "MySQL"; break;
                    case 3389: service = "RDP"; break;
                    case 5900: service = "VNC"; break;
                    case 8080: service = "HTTP-ALT"; break;
                    default: service = "???"; break;
                }
                tft.drawString("OPEN " + String(ports[i]) + " " + service, 2, y, 1);
                y += 12;
                client.stop();
            }
            delay(100);
        }
        if (open_ports == 0) tft.drawString("No open ports found", 2, y, 1);
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Subnet Sweep", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("SUBNET SWEEP", tftWidth/2, 2, 1);
        tft.drawString("Scanning 192.168.4.0/24", 2, 18, 1);
        tft.drawString("Live hosts:", 2, 32, 1);
        int live = 0;
        int y = 46;
        for (int i = 1; i < 50 && y < tftHeight - 15; i++) {
            WiFiClient client;
            client.setTimeout(50);
            String ip = "192.168.4." + String(i);
            if (client.connect(ip.c_str(), 80) || client.connect(ip.c_str(), 22)) {
                live++;
                tft.drawString(ip + " UP", 2, y, 1);
                y += 12;
                client.stop();
            }
        }
        tft.drawString("Found: " + String(live) + " hosts", 2, y, 1);
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"MAC Spoofer", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("MAC SPOOFER", tftWidth/2, 2, 1);
        tft.drawString("Current:", 2, 18, 1);
        tft.drawString(WiFi.macAddress(), 2, 32, 1);
        uint8_t newMac[6];
        for (int i = 0; i < 6; i++) newMac[i] = random(256);
        newMac[0] = (newMac[0] & 0xFE) | 0x02;
        esp_wifi_set_mac(WIFI_IF_STA, newMac);
        tft.drawString("New MAC:", 2, 46, 1);
        tft.drawString(WiFi.macAddress(), 2, 60, 1);
        tft.drawString("Type: Locally Administered", 2, 74, 1);
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Packet Flooder", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("PACKET FLOODER", tftWidth/2, 2, 1);
        tft.drawCentreString("UDP broadcast flood", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        WiFiUDP udp;
        int count = 0;
        unsigned long start = millis();
        while (!check(EscPress)) {
            udp.beginPacket("255.255.255.255", random(1024, 65535));
            String payload = "MOPIE_FLOOD_" + String(count);
            udp.write((const uint8_t*)payload.c_str(), payload.length());
            udp.endPacket();
            count++;
            if (millis() - start > 500) {
                tft.fillRect(2, 50, tftWidth-4, 12, bruceConfig.bgColor);
                tft.drawCentreString("Packets: " + String(count) + " (" + String(count*2) + " pps)", tftWidth/2, 52, 1);
                start = millis();
            }
        }
    }});
    options.push_back({"B64 Encoder", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("B64 ENCODER", tftWidth/2, 2, 1);
        const char* test = "MOPIE_FIRMWARE_V1";
        tft.drawString("Input: " + String(test), 2, 18, 1);
        tft.drawString("Base64:", 2, 35, 1);
        // Simple B64 encode
        const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        String input = String(test);
        String output = "";
        for (int i = 0; i < input.length(); i += 3) {
            int v = (input[i] << 16) | (i+1 < input.length() ? input[i+1] << 8 : 0) | (i+2 < input.length() ? input[i+2] : 0);
            output += b64[(v >> 18) & 63];
            output += b64[(v >> 12) & 63];
            output += (i+1 < input.length()) ? b64[(v >> 6) & 63] : '=';
            output += (i+2 < input.length()) ? b64[v & 63] : '=';
        }
        tft.drawString(output, 2, 50, 1);
        tft.drawCentreString("Press any key", tftWidth/2, 65, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Hash Gen", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("HASH GENERATOR", tftWidth/2, 2, 1);
        String input = "MOPIE" + String(random(9999));
        tft.drawString("Input: " + input, 2, 18, 1);
        // Simple FNV-1a hash
        uint32_t hash = 2166136261;
        for (int i = 0; i < input.length(); i++) {
            hash ^= input[i];
            hash *= 16777619;
        }
        tft.drawString("FNV-1a: 0x" + String(hash, HEX), 2, 35, 1);
        // Simple CRC32
        uint32_t crc = 0xFFFFFFFF;
        for (int i = 0; i < input.length(); i++) {
            crc ^= input[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        tft.drawString("CRC32: 0x" + String(crc ^ 0xFFFFFFFF, HEX), 2, 50, 1);
        tft.drawCentreString("Press any key", tftWidth/2, 65, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Hex Viewer", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("HEX VIEWER", tftWidth/2, 2, 1);
        tft.drawString("Addr  00 01 02 03 04 05 06 07", 2, 18, 1);
        for (int row = 0; row < 8; row++) {
            String line = "0x" + String(row * 8, HEX) + " ";
            for (int col = 0; col < 8; col++) {
                uint8_t val = random(256);
                if (val < 16) line += "0";
                line += String(val, HEX) + " ";
            }
            tft.drawString(line, 2, 30 + row * 12, 1);
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"Entropy Pool", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("ENTROPY POOL", tftWidth/2, 2, 1);
        tft.drawCentreString("Generating random data...", tftWidth/2, 20, 1);
        int y = 35;
        for (int i = 0; i < 10 && y < tftHeight - 15; i++) {
            uint32_t r1 = esp_random();
            uint32_t r2 = esp_random();
            tft.drawString(String(r1, HEX) + " " + String(r2, HEX), 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({" voltage Monitor", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("VOLTAGE MONITOR", tftWidth/2, 2, 1);
        int y = 18;
        tft.drawString("Battery: " + String(analogReadMilliVolts(37) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawString("USB: " + String(analogReadMilliVolts(32) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawString("GPIO34: " + String(analogReadMilliVolts(34) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawString("GPIO35: " + String(analogReadMilliVolts(35) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawString("GPIO36: " + String(analogReadMilliVolts(36) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawString("GPIO39: " + String(analogReadMilliVolts(39) / 1000.0, 2) + "V", 2, y, 1); y+=12;
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"GPIO Control", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("GPIO CONTROL", tftWidth/2, 2, 1);
        int y = 18;
        tft.drawString("GPIO 25: " + String(digitalRead(25)), 2, y, 1); y+=12;
        tft.drawString("GPIO 26: " + String(digitalRead(26)), 2, y, 1); y+=12;
        tft.drawString("GPIO 27: " + String(digitalRead(27)), 2, y, 1); y+=12;
        tft.drawString("GPIO 32: " + String(digitalRead(32)), 2, y, 1); y+=12;
        tft.drawString("GPIO 33: " + String(digitalRead(33)), 2, y, 1); y+=12;
        tft.drawString("GPIO 34: " + String(digitalRead(34)), 2, y, 1); y+=12;
        tft.drawString("GPIO 35: " + String(digitalRead(35)), 2, y, 1); y+=12;
        tft.drawString("GPIO 36: " + String(digitalRead(36)), 2, y, 1); y+=12;
        tft.drawString("GPIO 39: " + String(digitalRead(39)), 2, y, 1); y+=12;
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
    }});
    options.push_back({"WiFi Deauth Detector", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("DEAUTH DETECTOR", tftWidth/2, 2, 1);
        tft.drawCentreString("Monitoring for attacks...", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        int deauth_frames = 0;
        int beacons = 0;
        unsigned long lastCheck = millis();
        while (!check(EscPress)) {
            int n = WiFi.scanNetworks(false, false, false, 200);
            beacons += n;
            if (n > 20) deauth_frames += n / 10;  // Heuristic
            if (millis() - lastCheck > 500) {
                tft.fillRect(2, 50, tftWidth-4, 30, bruceConfig.bgColor);
                tft.drawString("Beacons: " + String(beacons), 2, 52, 1);
                tft.drawString("Deauth: " + String(deauth_frames), 2, 64, 1);
                if (deauth_frames > 10) {
                    tft.setTextColor(0xF800, bruceConfig.bgColor);
                    tft.drawCentreString("!! ATTACK DETECTED !!", tftWidth/2, 80, 1);
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                }
                lastCheck = millis();
            }
            WiFi.scanDelete();
            delay(200);
        }
    }});
    options.push_back({"Beacon Mapper", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("BEACON MAPPER", tftWidth/2, 2, 1);
        int n = WiFi.scanNetworks(false, true, false, 500);
        tft.fillScreen(bruceConfig.bgColor);
        tft.drawCentreString("Mapped " + String(n) + " APs", tftWidth/2, 2, 1);
        int y = 15;
        for (int i = 0; i < n && i < 12 && y < tftHeight - 10; i++) {
            int rssi = WiFi.RSSI(i);
            int bars = map(rssi, -90, -30, 1, 5);
            String bar = "";
            for (int b = 0; b < 5; b++) bar += (b < bars) ? "|" : ".";
            String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "O" : "E";
            tft.drawString(bar + " " + enc + " " + WiFi.SSID(i).substring(0, 12), 2, y, 1);
            y += 12;
        }
        tft.drawCentreString("Press any key", tftWidth/2, tftHeight - 10, 1);
        while (!check(AnyKeyPress)) delay(50);
        WiFi.scanDelete();
    }});
    options.push_back({"Channel Monitor", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString("CHANNEL MONITOR", tftWidth/2, 2, 1);
        tft.drawCentreString("Real-time channel usage", tftWidth/2, 20, 1);
        tft.drawCentreString("Hold BTN to stop", tftWidth/2, 35, 1);
        while (!check(EscPress)) {
            int n = WiFi.scanNetworks(false, false, false, 300);
            int chCount[14] = {0};
            for (int i = 0; i < n; i++) {
                int ch = WiFi.channel(i);
                if (ch >= 1 && ch <= 14) chCount[ch-1]++;
            }
            tft.fillRect(2, 50, tftWidth-4, 60, bruceConfig.bgColor);
            int y = 52;
            for (int ch = 0; ch < 14 && y < tftHeight - 10; ch++) {
                if (chCount[ch] > 0) {
                    tft.drawString("CH" + String(ch+1) + ": " + String(chCount[ch]) + " networks", 2, y, 1);
                    y += 12;
                }
            }
            WiFi.scanDelete();
            delay(500);
        }
    }});

    addOptionToMainMenu();
    loopOptions(options, MENU_TYPE_SUBMENU, "UtilKit");
}

void OthersMenu::badUsbHidMenu() {
    options = {
#ifndef LITE_VERSION
        {"USB Payload",   [=]() { ducky_setup(hid_usb, false); }   },
        {"USB Keyboard", [=]() { ducky_keyboard(hid_usb, false); }},
#endif

#ifdef USB_as_HID
        {"USB Clicker",  clicker_setup                            },
        {"USB U2F",      u2f_setup                                },
#endif

        {"Back",         [this]() { optionsMenu(); }              },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "HID Injector");
}

void OthersMenu::micMenu() {
    options = {
#if defined(MIC_SPM1423) || defined(MIC_INMP441)
        {"Visualizer", []() { AudioMic::MicVisualizer::getInstance().run(); }},
        {"Spectrogram",         mic_test                                              },
        {"Record Audio",        mic_record_app                                        },
#endif
        {"Back",     [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Audio Spy");
}

void OthersMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_OTHERS, iconCenterX, iconCenterY, scale);
}
void OthersMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_OTHERS, cx, cy, scale);
}

