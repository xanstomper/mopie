#ifndef __MOPIE_ICONS_H__
#define __MOPIE_ICONS_H__
#include <TFT_eSPI.h>
#include <globals.h>

#include "mopie_icons.h"   // ICON_WIFI, ICON_BLE, ... (auto-generated bitmap data)

// Mopie pixel-art icon renderer
// 24x24 grid, 2-bit packed (4px/byte): 0=off, 1=cyan accent, 2=primary blue, 3=white
// Colors are resolved at draw time from the live theme so icons follow the UI.

inline void drawMopieIcon(const uint8_t *icon, int cx, int cy, float scale) {
    const int SZ = 24;
    // Fixed 2px per art pixel: 48x48 icon. Caller positions it.
    int px = 2;
    if (px < 1) px = 1;
    int total = SZ * px;
    int x0 = cx - total / 2;
    int y0 = cy - total / 2;

    uint16_t colAccent = mopieConfig.priColor;      // v2: 1=red primary
    uint16_t colPri    = DEFAULT_ACCENT;            // v2: 2=amber accent
    uint16_t colWhite  = TFT_WHITE;

    for (int gy = 0; gy < SZ; gy++) {
        for (int gx = 0; gx < SZ; gx++) {
            int byteIdx = (gy * SZ + gx) >> 2;      // 4 px per byte
            int shift   = 6 - ((gy * SZ + gx) & 3) * 2;
            uint8_t v   = (icon[byteIdx] >> shift) & 0x03;
            if (v == 0) continue;
            uint16_t c = (v == 1) ? colAccent : (v == 2) ? colPri : colWhite;
            tft.fillRect(x0 + gx * px, y0 + gy * px, px, px, c);
        }
    }
}
#endif
