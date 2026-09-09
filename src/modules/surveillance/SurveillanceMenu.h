#ifndef __SURVEILLANCE_MENU_H__
#define __SURVEILLANCE_MENU_H__

#include <MenuItemInterface.h>
#include "surveillance_engine.h"

class SurveillanceMenu : public MenuItemInterface {
public:
    SurveillanceMenu() : MenuItemInterface("Surveillance") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    bool hasTheme() override { return false; }
    const String& themePath() override { static String empty = ""; return empty; }

    static void runRadarHud();
    static void runThreatScanner();
    static void showThreatDetails(const Surveillance::DetectedThreat& threat);
    static void showThreatLog();
};

#endif // __SURVEILLANCE_MENU_H__
