#include "GpsMenu.h"
#include "core/mopie_icon_draw.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/gps/gps_tracker.h"
#include "modules/gps/wardriving.h"
#include <math.h>

void GpsMenu::optionsMenu() {
    options = {
        {"Wardriving",  [this]() { wardrivingMenu(); }},
#if !defined(LITE_VERSION)
        {"GPS Tracker", [=]() { GPSTracker(); }       },
#endif
        {"Config",      [this]() { configMenu(); }    },
    };
    addOptionToMainMenu();

    String txt = "GPS (" + String(mopieConfigPins.gpsBaudrate) + " bps)";
    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
}

void GpsMenu::wardrivingMenu() {
    options = {
        {"Scan WiFi Networks", []() { Wardriving(true, false); }},
        {"Scan BLE Devices",   []() { Wardriving(false, true); }},
        {"Scan Both",          []() { Wardriving(true, true); } },
        {"Back",               [this]() { optionsMenu(); }      },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Wardriving");
}
void GpsMenu::configMenu() {
    options = {
        {"Baudrate", setGpsBaudrateMenu                                 },
        {"GPS Pins", [=]() { setUARTPinsMenu(mopieConfigPins.gps_bus); }},
        {"Back",     [this]() { optionsMenu(); }                        },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "GPS Config");
}

void GpsMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_GPS, iconCenterX, iconCenterY, scale);
}
void GpsMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_GPS, cx, cy, scale);
}

