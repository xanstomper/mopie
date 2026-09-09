#ifndef __AI_MENU_H__
#define __AI_MENU_H__

#include <MenuItemInterface.h>
#include "core/mopie_glyphs.h"
#include "../../modules/ai/mopie_ai.h"

class AiMenu : public MenuItemInterface {
public:
    AiMenu() : MenuItemInterface("AI") {}

    void optionsMenu(void) override { mopie_ai_setup(); }

    void drawIcon(float scale = 1) override {
        clearIconArea();
        // neural net glyph centered (same shape as GLYPH_AI)
        int cX = iconCenterX, cY = iconCenterY;
        int r = 10 * scale;
        tft.fillCircle(cX, cY, r * 3 / 10 + 2, bruceConfig.priColor);
        const int off = r * 3 / 5;
        tft.fillCircle(cX - off, cY - off / 2, r / 5 + 1, DEFAULT_ACCENT);
        tft.fillCircle(cX + off, cY - off / 2, r / 5 + 1, DEFAULT_ACCENT);
        tft.fillCircle(cX - off, cY + off / 2, r / 5 + 1, DEFAULT_ACCENT);
        tft.fillCircle(cX + off, cY + off / 2, r / 5 + 1, DEFAULT_ACCENT);
        tft.drawWideLine(cX - off, cY - off / 2, cX, cY, 2, DEFAULT_ACCENT, DEFAULT_ACCENT);
        tft.drawWideLine(cX + off, cY - off / 2, cX, cY, 2, DEFAULT_ACCENT, DEFAULT_ACCENT);
        tft.drawWideLine(cX - off, cY + off / 2, cX, cY, 2, DEFAULT_ACCENT, DEFAULT_ACCENT);
        tft.drawWideLine(cX + off, cY + off / 2, cX, cY, 2, DEFAULT_ACCENT, DEFAULT_ACCENT);
    }

    void drawIconAt(int cx, int cy, float scale) override {
        drawMopieGlyph(GLYPH_AI, cx, cy, (int)(30 * scale));
    }

    bool hasTheme() override { return false; }
    const String& themePath() override {
        static String empty = "";
        return empty;
    }
};

#endif
