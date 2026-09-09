#ifndef __NRF24_MENU_H__
#define __NRF24_MENU_H__

#include <MenuItemInterface.h>

class NRF24Menu : public MenuItemInterface {
public:
    NRF24Menu() : MenuItemInterface("NRF24") {}

    void optionsMenu(void);
    void configMenu(void);
    void drawIcon(float scale);
    void drawIconAt(int cx, int cy, float scale);
    bool hasTheme() { return mopieConfig.theme.nrf; }
    const String& themePath() override { return mopieConfig.theme.paths.nrf; }
};

#endif
