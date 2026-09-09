#ifndef __SURVEILLANCE_SIGNATURES_H__
#define __SURVEILLANCE_SIGNATURES_H__

#include <Arduino.h>

namespace Surveillance {

enum ThreatCategory : uint8_t {
    CAT_FLOCK_ALPR       = 0,
    CAT_POLICE_VEHICLE   = 1,
    CAT_POLICE_BODYCAM   = 2,
    CAT_OTHER_ALPR       = 3,
    CAT_SURVEILLANCE_CAM = 4,
    CAT_DRONE_UAV        = 5,
    CAT_UNKNOWN          = 6
};

inline const char* categoryName(ThreatCategory cat) {
    switch (cat) {
        case CAT_FLOCK_ALPR:       return "FLOCK ALPR";
        case CAT_POLICE_VEHICLE:   return "POLICE MDT";
        case CAT_POLICE_BODYCAM:   return "BODYCAM";
        case CAT_OTHER_ALPR:       return "ALPR CAM";
        case CAT_SURVEILLANCE_CAM: return "CCTV CAM";
        case CAT_DRONE_UAV:        return "DRONE RID";
        default:                   return "UNKNOWN";
    }
}

inline uint16_t categoryColor(ThreatCategory cat) {
    switch (cat) {
        case CAT_FLOCK_ALPR:       return 0xF800; // Red
        case CAT_POLICE_VEHICLE:   return 0x07FF; // Cyan
        case CAT_POLICE_BODYCAM:   return 0x001F; // Blue
        case CAT_OTHER_ALPR:       return 0xFD20; // Orange
        case CAT_SURVEILLANCE_CAM: return 0x07E0; // Green
        case CAT_DRONE_UAV:        return 0xFFE0; // Yellow
        default:                   return 0xFFFF; // White
    }
}

struct OuiEntry {
    const char* oui;
    ThreatCategory category;
    const char* vendor;
    const char* defaultName;
};

static const OuiEntry OUI_DATABASE[] = {
    // Flock Safety ALPR Cameras
    {"70:c9:4e", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"3c:91:80", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"d8:f3:bc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Sparrow ALPR"},
    {"80:30:49", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"b8:35:32", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"14:5a:fc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Penguin ALPR"},
    {"74:4c:a1", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"08:3a:88", CAT_FLOCK_ALPR, "Flock Safety", "Flock Safety Cam"},
    {"9c:2f:9d", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"c0:35:32", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"94:08:53", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"e4:aa:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"f4:6a:dd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"e0:0a:f6", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"24:b2:b9", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"00:f4:8d", CAT_FLOCK_ALPR, "Flock Safety", "Flock Safety ALPR"},
    {"d0:39:57", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"e8:d0:fc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"e0:4f:43", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"b8:1e:a4", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"70:08:94", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"58:8e:81", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"ec:1b:bd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"3c:71:bf", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"58:00:e3", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"90:35:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"5c:93:a2", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"64:6e:69", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"48:27:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"a4:cf:12", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"14:b5:cd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},
    {"82:6b:f2", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR"},

    // Police Cruisers & Mobile Data Terminals (MDT)
    {"00:0f:7d", CAT_POLICE_VEHICLE, "Sierra Wireless", "Police Cruiser MP70"},
    {"00:14:3a", CAT_POLICE_VEHICLE, "Sierra Wireless", "Police Cruiser AirLink"},
    {"00:a0:f8", CAT_POLICE_VEHICLE, "Sierra Wireless", "Police Cruiser MG90"},
    {"34:e6:ad", CAT_POLICE_VEHICLE, "Sierra Wireless", "Police Cruiser AirLink"},
    {"4c:3b:74", CAT_POLICE_VEHICLE, "Sierra Wireless", "Police Cruiser MP70"},
    {"00:30:44", CAT_POLICE_VEHICLE, "Cradlepoint", "Police Cruiser IBR900"},
    {"00:1e:2a", CAT_POLICE_VEHICLE, "Cradlepoint", "Police Cruiser IBR1700"},
    {"c0:06:c3", CAT_POLICE_VEHICLE, "Cradlepoint", "Police Cruiser Router"},
    {"f8:18:97", CAT_POLICE_VEHICLE, "Cradlepoint", "Police Cruiser R1900"},

    // Police Body Cameras & In-Car Video
    {"00:25:df", CAT_POLICE_BODYCAM, "Axon Enterprise", "Axon Body 3/4 Cam"},
    {"00:1b:2f", CAT_POLICE_BODYCAM, "Motorola WatchGuard", "Motorola 4RE / V300"},
    {"00:19:a9", CAT_POLICE_BODYCAM, "L3 Mobile-Vision", "Mobile-Vision BWV"},

    // Other Commercial & Municipal ALPR Systems
    {"00:0c:e5", CAT_OTHER_ALPR, "Motorola Solutions", "Vigilant Fixed ALPR"},
    {"00:1a:2b", CAT_OTHER_ALPR, "Genetec", "AutoVu SharpX ALPR"},
    {"00:80:f4", CAT_OTHER_ALPR, "Verra Mobility", "Verra Traffic Cam"},

    // Surveillance & Traffic CCTV Cameras
    {"00:40:8c", CAT_SURVEILLANCE_CAM, "Axis Comm", "Axis Municipal CCTV"},
    {"ac:cc:8e", CAT_SURVEILLANCE_CAM, "Axis Comm", "Axis PTZ Camera"},
    {"b8:a4:4f", CAT_SURVEILLANCE_CAM, "Axis Comm", "Axis Traffic Cam"},
    {"00:09:18", CAT_SURVEILLANCE_CAM, "Hanwha Techwin", "Hanwha Wisenet CCTV"},
    {"38:af:29", CAT_SURVEILLANCE_CAM, "Dahua Tech", "Dahua Traffic Cam"},
    {"44:19:b6", CAT_SURVEILLANCE_CAM, "Hikvision", "Hikvision CCTV"},

    // Drone Remote ID (FAA & Global)
    {"60:60:1f", CAT_DRONE_UAV, "DJI", "DJI Drone Remote ID"},
    {"48:10:e5", CAT_DRONE_UAV, "DJI", "DJI Matrice / Mavic"},
    {"90:35:6e", CAT_DRONE_UAV, "Skydio", "Skydio 2+ / X2 Drone"},
    {"bc:33:ac", CAT_DRONE_UAV, "Autel Robotics", "Autel EVO Drone"}
};
static constexpr size_t OUI_ENTRY_COUNT = sizeof(OUI_DATABASE) / sizeof(OUI_DATABASE[0]);

// BLE Manufacturer IDs
static constexpr uint16_t BLE_MFR_FLOCK = 0x09C8; // Flock Safety
static constexpr uint16_t BLE_MFR_AXON  = 0x0283; // Axon Enterprise (TASER)

// BLE Service UUIDs
static const char* BLE_AXON_SIGNALS[] = {"fe01", "fe02", "fe57"};
static constexpr size_t BLE_AXON_COUNT = sizeof(BLE_AXON_SIGNALS) / sizeof(BLE_AXON_SIGNALS[0]);
static const char* BLE_DRONE_RID_UUID = "fffa";

// String matches
static const char* FLOCK_NAME_PATTERNS[] = {
    "FLOCK", "FS EXT BATTERY", "FS-CAM", "FLOCK-SAFETY", "PENGUIN", "PIGVISION"
};
static constexpr size_t FLOCK_NAME_COUNT = sizeof(FLOCK_NAME_PATTERNS) / sizeof(FLOCK_NAME_PATTERNS[0]);

static const char* POLICE_NAME_PATTERNS[] = {
    "AXON", "SIGNAL", "AB3", "AB4", "FLEET3", "TASER", "MDT", "CRUISER"
};
static constexpr size_t POLICE_NAME_COUNT = sizeof(POLICE_NAME_PATTERNS) / sizeof(POLICE_NAME_PATTERNS[0]);

static const char* DRONE_NAME_PATTERNS[] = {
    "RID-", "DRONE", "DJI-", "SKYDIO", "AUTEL"
};
static constexpr size_t DRONE_NAME_COUNT = sizeof(DRONE_NAME_PATTERNS) / sizeof(DRONE_NAME_PATTERNS[0]);

} // namespace Surveillance

#endif // __SURVEILLANCE_SIGNATURES_H__
