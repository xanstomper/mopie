#ifndef __SURVEILLANCE_ENGINE_H__
#define __SURVEILLANCE_ENGINE_H__

#include <Arduino.h>
#include <vector>
#include "surveillance_signatures.h"

namespace Surveillance {

struct DetectedThreat {
    String mac;
    String name;
    String vendor;
    ThreatCategory category;
    int8_t rssi;
    uint8_t channel;
    uint32_t lastSeen;
    uint16_t hits;
    float estimatedDistanceMeters;
};

class SurveillanceEngine {
public:
    static SurveillanceEngine& getInstance();

    void init();
    void startScan(ThreatCategory filter = CAT_UNKNOWN);
    void stopScan();
    void update();

    // Process incoming sightings
    bool processWifiFrame(const uint8_t* bssid, const char* ssid, int8_t rssi, uint8_t channel);
    bool processBleAdv(const String& mac, const String& name, int8_t rssi, uint16_t mfrId, const String& serviceUuid);

    const std::vector<DetectedThreat>& getThreats() const { return threats; }
    size_t getThreatCount() const { return threats.size(); }
    bool hasNewAlert() { bool a = newAlert; newAlert = false; return a; }
    const DetectedThreat* getLatestThreat() const { return latestThreat; }
    void clearThreats();

    void setAudioAlert(bool enable) { soundAlertEnabled = enable; }
    bool isAudioAlert() const { return soundAlertEnabled; }
    void setLedAlert(bool enable) { ledAlertEnabled = enable; }
    bool isLedAlert() const { return ledAlertEnabled; }

    ThreatCategory activeFilter = CAT_UNKNOWN;

private:
    SurveillanceEngine();
    std::vector<DetectedThreat> threats;
    bool isScanning = false;
    bool newAlert = false;
    const DetectedThreat* latestThreat = nullptr;
    bool soundAlertEnabled = true;
    bool ledAlertEnabled = true;
    unsigned long lastChannelHop = 0;
    uint8_t currentChannel = 1;

    void triggerAlert(const DetectedThreat& threat);
    float calculateDistance(int8_t rssi);
    bool matchOui(const String& mac, OuiEntry& outEntry);
};

} // namespace Surveillance

#endif // __SURVEILLANCE_ENGINE_H__
