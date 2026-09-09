#ifndef __RFID_MENU_H__
#define __RFID_MENU_H__

#include <MenuItemInterface.h>

class RFIDMenu : public MenuItemInterface {
public:
    RFIDMenu() : MenuItemInterface("RFID") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    void drawIconAt(int cx, int cy, float scale);
    bool hasTheme() { return mopieConfig.theme.rfid; }
    const String& themePath() override { return mopieConfig.theme.paths.rfid; }

private:
    void configMenu(void);
};

#endif
