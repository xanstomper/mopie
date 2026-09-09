#ifndef __CLOCK_MENU_H__
#define __CLOCK_MENU_H__

#include <MenuItemInterface.h>

class ClockMenu : public MenuItemInterface {
public:
    ClockMenu() : MenuItemInterface("Clock") {}

    void optionsMenu(void);
    void showSubMenu(void);
    void drawIcon(float scale);
    void drawIconAt(int cx, int cy, float scale);
    bool hasTheme() { return mopieConfig.theme.clock; }
    const String& themePath() override { return mopieConfig.theme.paths.clock; }
};

#endif
