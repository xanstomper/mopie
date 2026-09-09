#include "ClockMenu.h"
#include "core/mopie_icon_draw.h"
#include "core/display.h"
#include "core/settings.h"
#include "modules/others/timer.h"

void ClockMenu::optionsMenu() {
    while (!returnToMenu) {
        runClockLoop(true);

        // If ESC is pressed on the watch, it exits
        if (returnToMenu) break;

        // OK pressed, show submenu
        showSubMenu();

        // If "Exit" is pressed in the submenu, it exits
        if (returnToMenu) break;
    }
}

void ClockMenu::showSubMenu() {
    options = {
        {"Timer",         [=]() { Timer(); }            },
        {"Back to Clock", [=]() {}                      },
        {"Exit",          [=]() { returnToMenu = true; }}
        // Add more options here
    };

    delay(200);
    loopOptions(options);
}

void ClockMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_CLOCK, iconCenterX, iconCenterY, scale);
}
void ClockMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_CLOCK, cx, cy, scale);
}

