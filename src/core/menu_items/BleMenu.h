#ifndef __BLE_MENU_H__
#define __BLE_MENU_H__

#include <MenuItemInterface.h>

class BleMenu : public MenuItemInterface {
public:
    BleMenu() : MenuItemInterface("BLE") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    void drawIconAt(int cx, int cy, float scale);
    bool hasTheme() { return mopieConfig.theme.ble; }
    const String& themePath() override { return mopieConfig.theme.paths.ble; }

private:
    void configMenu(void);
    void setBleNameMenu(void);
};

#endif
