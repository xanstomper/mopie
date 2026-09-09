#ifndef __DISPLAY_TACTICAL_H__
#define __DISPLAY_TACTICAL_H__

#include <Arduino.h>
#include <vector>
#include "display.h"

namespace TacticalUI {

enum CyberTheme : uint8_t {
    THEME_MOPIE_RED   = 0,
    THEME_MOPIE_BLUE  = 1,
    THEME_MATRIX_LIME = 2,
    THEME_CYBER_CYAN  = 3,
    THEME_SOLAR_AMBER = 4,
    THEME_NEON_VIOLET = 5,
    THEME_COUNT       = 6
};

bool isEnabled();
void setEnabled(bool enable);

void drawStatusBar();
void drawSubmenu(int index, std::vector<Option>& options, const char* title);
Opt_Coord drawOptions(
    int index, std::vector<Option>& options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor, bool firstRender
);

void applyTheme(CyberTheme theme);
void cycleTheme();
const char* getThemeName(CyberTheme theme);
CyberTheme getCurrentTheme();

} // namespace TacticalUI

#endif // __DISPLAY_TACTICAL_H__
