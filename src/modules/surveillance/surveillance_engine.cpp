#include "surveillance_engine.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <math.h>

namespace Surveillance {

// Global callback bridge for WiFi promiscuous packets
static void wifiPromiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 24) return;

    uint8_t frameControl = payload[0];
    uint8_t subType = (frameControl >> 4) & 0x0F;

    // Beacon (0x08) or Probe Response (0x05)
    if (subType == 0x08 || subType == 0x05) {
        const uint8_t* bssid = &payload[16];
        int8_t rssi = pkt->rx_ctrl.rssi;
        uint8_t channel = pkt->rx_ctrl.channel;

        char ssid[33] = {0};
        int pos = 36; // Skip 24-byte header + 12-byte fixed parameters
        while (pos + 2 <= len) {
            uint8_t tagNumber = payload[pos];
            uint8_t tagLength = payload[pos + 1];
            if (pos + 2 + tagLength > len) break;
            if (tagNumber == 0 && tagLength <= 32) {
                memcpy(ssid, &payload[pos + 2], tagLength);
                ssid[tagLength] = '\0';
                break;
            }
            pos += 2 + tagLength;
        }

        SurveillanceEngine::getInstance().processWifiFrame(bssid, ssid, rssi, channel);
    }
}

class BleScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        String mac = advertisedDevice->getAddress().toString().c_str();
        String name = advertisedDevice->getName().c_str();
        int8_t rssi = advertisedDevice->getRSSI();
        uint16_t mfrId = 0;

        if (advertisedDevice->haveManufacturerData()) {
            std::string mfr = advertisedDevice->getManufacturerData();
            if (mfr.length() >= 2) {
                mfrId = (uint8_t)mfr[0] | ((uint8_t)mfr[1] << 8);
            }
        }

        String serviceUuid = "";
        if (advertisedDevice->haveServiceUUID()) {
            serviceUuid = advertisedDevice->getServiceUUID().toString().c_str();
        }

        SurveillanceEngine::getInstance().processBleAdv(mac, name, rssi, mfrId, serviceUuid);
    }
};

static BleScanCallbacks bleScanCallbacks;

SurveillanceEngine::SurveillanceEngine() {}

SurveillanceEngine& SurveillanceEngine::getInstance() {
    static SurveillanceEngine instance;
    return instance;
}

void SurveillanceEngine::init() {
#ifdef BUZZ_PIN
    pinMode(BUZZ_PIN, OUTPUT);
    digitalWrite(BUZZ_PIN, LOW);
#endif
#ifdef G10
    pinMode(G10, OUTPUT);
    digitalWrite(G10, HIGH); // Off for active-low LED
#endif
}

void SurveillanceEngine::clearThreats() {
    threats.clear();
    latestThreat = nullptr;
    newAlert = false;
}

void SurveillanceEngine::startScan(ThreatCategory filter) {
    activeFilter = filter;
    init();
    clearThreats();

    // Start WiFi Promiscuous Sniffer
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(wifiPromiscuousCallback);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    currentChannel = 1;
    lastChannelHop = millis();

    // Start BLE scan
    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("WhereDaFlock");
    }
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(&bleScanCallbacks);
    pScan->setActiveScan(true);
    pScan->setInterval(97);
    pScan->setWindow(65);
    pScan->start(0, false);

    isScanning = true;
}

void SurveillanceEngine::stopScan() {
    if (!isScanning) return;
    esp_wifi_set_promiscuous(false);
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan && pScan->isScanning()) {
        pScan->stop();
    }
    isScanning = false;
}

void SurveillanceEngine::update() {
    if (!isScanning) return;

    // Channel hopping every 150ms
    if (millis() - lastChannelHop > 150) {
        lastChannelHop = millis();
        currentChannel = (currentChannel % 13) + 1;
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    }
}

float SurveillanceEngine::calculateDistance(int8_t rssi) {
    if (rssi >= 0) return 0.1f;
    // Log-distance path loss formula: d = 10 ^ ((A - RSSI) / (10 * n))
    // Reference power at 1 meter A = -45 dBm, path loss exponent n = 2.4
    float A = -45.0f;
    float n = 2.4f;
    float exp = (A - (float)rssi) / (10.0f * n);
    float dist = powf(10.0f, exp);
    if (dist < 0.2f) dist = 0.2f;
    if (dist > 250.0f) dist = 250.0f;
    return dist;
}

bool SurveillanceEngine::matchOui(const String& mac, OuiEntry& outEntry) {
    String cleanMac = mac;
    cleanMac.toLowerCase();
    if (cleanMac.length() < 8) return false;
    String prefix = cleanMac.substring(0, 8); // e.g. "70:c9:4e"

    for (size_t i = 0; i < OUI_ENTRY_COUNT; i++) {
        if (prefix.equalsIgnoreCase(OUI_DATABASE[i].oui)) {
            outEntry = OUI_DATABASE[i];
            return true;
        }
    }
    return false;
}

void SurveillanceEngine::triggerAlert(const DetectedThreat& threat) {
    newAlert = true;
    latestThreat = &threat;

    // Hardware buzzer feedback (GPIO 2)
#ifdef BUZZ_PIN
    if (soundAlertEnabled) {
        tone(BUZZ_PIN, 2400, 30);
    }
#endif

    // Hardware LED pulse (GPIO 10)
#ifdef G10
    if (ledAlertEnabled) {
        digitalWrite(G10, LOW);  // Active LOW on M5Stick
        delay(10);
        digitalWrite(G10, HIGH);
    }
#endif
}

bool SurveillanceEngine::processWifiFrame(const uint8_t* bssid, const char* ssid, int8_t rssi, uint8_t channel) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    String mac = String(macStr);
    String ssidStr = String(ssid);
    String upperSsid = ssidStr;
    upperSsid.toUpperCase();

    ThreatCategory detectedCat = CAT_UNKNOWN;
    String vendor = "";
    String defaultName = "";

    OuiEntry matchedOui;
    if (matchOui(mac, matchedOui)) {
        detectedCat = matchedOui.category;
        vendor = matchedOui.vendor;
        defaultName = matchedOui.defaultName;
    }

    // Name-based matching
    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < FLOCK_NAME_COUNT; i++) {
            if (upperSsid.indexOf(FLOCK_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_FLOCK_ALPR;
                vendor = "Flock Safety";
                defaultName = "Flock SSID Beacon";
                break;
            }
        }
    }

    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < POLICE_NAME_COUNT; i++) {
            if (upperSsid.indexOf(POLICE_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_POLICE_VEHICLE;
                vendor = "Police MDT/Fleet";
                defaultName = "Police Wireless";
                break;
            }
        }
    }

    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < DRONE_NAME_COUNT; i++) {
            if (upperSsid.indexOf(DRONE_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_DRONE_UAV;
                vendor = "Drone RID";
                defaultName = "FAA Drone ID";
                break;
            }
        }
    }

    if (detectedCat == CAT_UNKNOWN) return false;
    if (activeFilter != CAT_UNKNOWN && activeFilter != detectedCat) return false;

    // Check if threat already exists in list
    for (auto& t : threats) {
        if (t.mac.equalsIgnoreCase(mac)) {
            t.rssi = rssi;
            t.channel = channel;
            t.lastSeen = millis();
            t.hits++;
            t.estimatedDistanceMeters = calculateDistance(rssi);
            if (ssidStr.length() > 0 && t.name.length() == 0) {
                t.name = ssidStr;
            }
            return true;
        }
    }

    // Add new threat
    DetectedThreat dt;
    dt.mac = mac;
    dt.name = (ssidStr.length() > 0) ? ssidStr : defaultName;
    dt.vendor = vendor;
    dt.category = detectedCat;
    dt.rssi = rssi;
    dt.channel = channel;
    dt.lastSeen = millis();
    dt.hits = 1;
    dt.estimatedDistanceMeters = calculateDistance(rssi);

    threats.push_back(dt);
    triggerAlert(threats.back());
    return true;
}

bool SurveillanceEngine::processBleAdv(const String& mac, const String& name, int8_t rssi, uint16_t mfrId, const String& serviceUuid) {
    ThreatCategory detectedCat = CAT_UNKNOWN;
    String vendor = "";
    String defaultName = "";
    String upperName = name;
    upperName.toUpperCase();
    String lowerUuid = serviceUuid;
    lowerUuid.toLowerCase();

    // Check MFR IDs
    if (mfrId == BLE_MFR_FLOCK) {
        detectedCat = CAT_FLOCK_ALPR;
        vendor = "Flock Safety";
        defaultName = "Flock BLE Beacon";
    } else if (mfrId == BLE_MFR_AXON) {
        detectedCat = CAT_POLICE_BODYCAM;
        vendor = "Axon Enterprise";
        defaultName = "Axon Bodycam Signal";
    }

    // Check UUIDs
    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < BLE_AXON_COUNT; i++) {
            if (lowerUuid.indexOf(BLE_AXON_SIGNALS[i]) >= 0) {
                detectedCat = CAT_POLICE_BODYCAM;
                vendor = "Axon Fleet";
                defaultName = "Axon Signal Trigger";
                break;
            }
        }
        if (lowerUuid.indexOf(BLE_DRONE_RID_UUID) >= 0) {
            detectedCat = CAT_DRONE_UAV;
            vendor = "ASTM Drone";
            defaultName = "Drone Remote ID";
        }
    }

    // Check Name substrings
    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < FLOCK_NAME_COUNT; i++) {
            if (upperName.indexOf(FLOCK_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_FLOCK_ALPR;
                vendor = "Flock Safety";
                defaultName = "Flock BLE Beacon";
                break;
            }
        }
    }

    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < POLICE_NAME_COUNT; i++) {
            if (upperName.indexOf(POLICE_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_POLICE_BODYCAM;
                vendor = "Police Axon";
                defaultName = "Axon Bodycam";
                break;
            }
        }
    }

    if (detectedCat == CAT_UNKNOWN) {
        for (size_t i = 0; i < DRONE_NAME_COUNT; i++) {
            if (upperName.indexOf(DRONE_NAME_PATTERNS[i]) >= 0) {
                detectedCat = CAT_DRONE_UAV;
                vendor = "Drone Remote ID";
                defaultName = "UAV Broadcast";
                break;
            }
        }
    }

    // Also check OUI for BLE random/public address
    if (detectedCat == CAT_UNKNOWN) {
        OuiEntry matchedOui;
        if (matchOui(mac, matchedOui)) {
            detectedCat = matchedOui.category;
            vendor = matchedOui.vendor;
            defaultName = matchedOui.defaultName;
        }
    }

    if (detectedCat == CAT_UNKNOWN) return false;
    if (activeFilter != CAT_UNKNOWN && activeFilter != detectedCat) return false;

    for (auto& t : threats) {
        if (t.mac.equalsIgnoreCase(mac)) {
            t.rssi = rssi;
            t.lastSeen = millis();
            t.hits++;
            t.estimatedDistanceMeters = calculateDistance(rssi);
            if (name.length() > 0 && t.name.length() == 0) {
                t.name = name;
            }
            return true;
        }
    }

    DetectedThreat dt;
    dt.mac = mac;
    dt.name = (name.length() > 0) ? name : defaultName;
    dt.vendor = vendor;
    dt.category = detectedCat;
    dt.rssi = rssi;
    dt.channel = 0; // BLE
    dt.lastSeen = millis();
    dt.hits = 1;
    dt.estimatedDistanceMeters = calculateDistance(rssi);

    threats.push_back(dt);
    triggerAlert(threats.back());
    return true;
}

} // namespace Surveillance
