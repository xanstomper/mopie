#ifndef __MOPIE_GLYPHS_H__
#define __MOPIE_GLYPHS_H__
#include <TFT_eSPI.h>
#include <globals.h>

// Vector icon glyphs — drawn with TFT primitives, scale to any size.
// No bitmaps, no quality loss, crisp at any cell size.

enum MopieGlyph {
    GLYPH_WIFI, GLYPH_BLE, GLYPH_RF, GLYPH_IR, GLYPH_GPS, GLYPH_FILES,
    GLYPH_CLOCK, GLYPH_OTHERS, GLYPH_CONFIG, GLYPH_CONNECT, GLYPH_ETH,
    GLYPH_FM, GLYPH_LORA, GLYPH_NRF, GLYPH_RFID, GLYPH_SCRIPTS, GLYPH_AI
};

inline void drawMopieGlyph(int g, int cx, int cy, int s) {
    const uint16_t P  = bruceConfig.priColor;
    const uint16_t A  = DEFAULT_ACCENT;
    const uint16_t W  = TFT_WHITE;
    const uint16_t BG = bruceConfig.bgColor;
    const int h = s / 2;
    int lw = s / 14; if (lw < 1) lw = 1; if (lw > 3) lw = 3;

    switch (g) {
    case GLYPH_WIFI: {
        // TFT_eSPI drawArc: 0 deg = 6 o'clock (BOTTOM), 90 = 9 o'clock, 180 = TOP, 270 = 3 o'clock.
        // WiFi: dot low-center, arcs sweep 90 -> 270 (through 180) = fan over the TOP.
        int ax = cx, ay = cy + h * 2 / 5;
        for (int k = 0; k < 3; k++) {
            int r = h * 3 / 10 + k * h * 3 / 10;   // h*3/10, h*3/5, h*9/10
            uint16_t c = (k == 2) ? A : P;
            tft.drawArc(ax, ay, r, r - lw - 1, 90, 270, c, BG);
        }
        tft.fillCircle(ax, ay, lw + 2, A);
        break;
    }
    case GLYPH_BLE: {
        int x0 = cx - h * 3 / 8;
        tft.drawWideLine(x0, cy - h, x0, cy + h, lw + 1, P, P);              // stem
        tft.drawWideLine(x0, cy - h, cx + h * 5 / 8, cy + h / 5, lw, P, P);  // diag TL->R
        tft.drawWideLine(cx + h * 5 / 8, cy - h / 5, x0, cy + h, lw, P, P);  // diag R->BL
        tft.drawWideLine(x0, cy, cx + h * 5 / 8, cy - h * 4 / 5, lw, A, A);  // accent rays
        tft.drawWideLine(x0, cy, cx + h * 5 / 8, cy + h * 4 / 5, lw, A, A);
        break;
    }
    case GLYPH_RF: {
        int ty = cy - h * 3 / 4;
        tft.drawWideLine(cx, ty + h / 4, cx, cy + h, lw + 1, P, P);          // mast
        tft.drawWideLine(cx - h * 3 / 5, cy + h, cx, cy + h / 5, lw, P, P);  // legs
        tft.drawWideLine(cx + h * 3 / 5, cy + h, cx, cy + h / 5, lw, P, P);
        tft.fillCircle(cx, ty, lw + 2, A);                                   // top
        tft.drawArc(cx, ty, h * 9 / 10, h * 9 / 10 - lw, 130, 230, P, BG);   // waves
        tft.drawArc(cx, ty, h * 6 / 5, h * 6 / 5 - lw, 140, 220, A, BG);
        tft.drawFastHLine(cx - h * 3 / 4, cy + h, s * 3 / 4, A);             // ground
        break;
    }
    case GLYPH_IR: {
        tft.drawRoundRect(cx - h * 3 / 4, cy - h, s * 3 / 4, s, 4, P);
        for (int k = -1; k <= 1; k++)
            tft.drawFastHLine(cx - h * 2 / 5, cy + k * h / 2, h * 4 / 5, A);
        break;
    }
    case GLYPH_GPS: {
        int cy0 = cy - h / 5;
        tft.fillCircle(cx, cy0, h * 4 / 5, P);
        tft.fillCircle(cx, cy0, h * 2 / 5, BG);
        tft.fillCircle(cx, cy0, h / 5, A);
        tft.fillTriangle(cx - h * 11 / 20, cy0 + h * 3 / 5, cx + h * 11 / 20, cy0 + h * 3 / 5,
                         cx, cy + h, P);
        break;
    }
    case GLYPH_FILES: {
        tft.fillRect(cx - h, cy - h * 3 / 5, s, h * 6 / 5, P);
        tft.fillRect(cx - h, cy - h, h * 7 / 10, h * 2 / 5, P);              // tab
        tft.drawFastHLine(cx - h * 3 / 4, cy, h * 3 / 2, A);
        tft.drawFastHLine(cx - h * 3 / 4, cy + h * 2 / 5, h * 3 / 2, A);
        break;
    }
    case GLYPH_CLOCK: {
        tft.drawCircle(cx, cy, h - 1, P);
        tft.drawCircle(cx, cy, h - 2, P);
        tft.drawWideLine(cx, cy, cx, cy - h * 3 / 5, lw, A, A);              // minute
        tft.drawWideLine(cx, cy, cx + h * 2 / 5, cy + h / 5, lw, W, W);      // hour
        tft.fillCircle(cx, cy, lw, A);
        break;
    }
    case GLYPH_OTHERS: {
        int q = h * 2 / 5, r = h * 3 / 10;
        tft.fillRoundRect(cx - q - r, cy - q - r, r * 2, r * 2, 2, P);
        tft.fillRoundRect(cx + q - r, cy - q - r, r * 2, r * 2, 2, P);
        tft.fillRoundRect(cx - q - r, cy + q - r, r * 2, r * 2, 2, P);
        tft.fillRoundRect(cx + q - r, cy + q - r, r * 2, r * 2, 2, A);
        break;
    }
    case GLYPH_CONFIG: {
        for (int k = -1; k <= 1; k++)
            tft.drawFastHLine(cx - h, cy + k * h * 3 / 5, s, P);
        int kx[3] = {cx - h / 2, cx + h / 6, cx - h / 6};
        for (int k = 0; k < 3; k++)
            tft.fillCircle(kx[k], cy + (k - 1) * h * 3 / 5, lw + 2, A);
        break;
    }
    case GLYPH_CONNECT: {
        tft.fillCircle(cx - h * 3 / 5, cy, lw + 2, P);
        tft.fillCircle(cx + h * 1 / 2, cy - h * 3 / 5, lw + 2, P);
        tft.fillCircle(cx + h * 1 / 2, cy + h * 3 / 5, lw + 2, P);
        tft.drawWideLine(cx - h * 2 / 5, cy, cx + h * 3 / 10, cy - h * 2 / 5, lw, A, A);
        tft.drawWideLine(cx - h * 2 / 5, cy, cx + h * 3 / 10, cy + h * 2 / 5, lw, A, A);
        tft.drawWideLine(cx + h * 1 / 2, cy - h * 2 / 5, cx + h * 1 / 2, cy + h * 2 / 5, lw, A, A);
        break;
    }
    case GLYPH_ETH: {
        tft.drawRoundRect(cx - h * 3 / 4, cy - h * 11 / 20, s * 3 / 4, h * 11 / 10, 3, P);
        for (int k = 0; k < 4; k++)
            tft.drawFastVLine(cx - h / 2 + k * h / 3, cy - h / 4, h / 2, A);
        break;
    }
    case GLYPH_FM: {
        int ny = cy + h * 2 / 5;
        tft.fillCircle(cx - h * 2 / 5, ny, h * 3 / 10, P);
        tft.drawWideLine(cx + h / 10, ny, cx + h / 10, cy - h * 3 / 4, lw + 1, P, P);
        tft.drawArc(cx - h * 2 / 5, ny, h * 11 / 10, h * 11 / 10 - lw, 195, 345, A, BG);
        break;
    }
    case GLYPH_LORA: {
        // Compact chevrons: max half-width h*2/5, apex rise h/4, all inside the s-box.
        // Chevron k spans y: cy+h*3/10-k*h/4 down to apex; widest chevron edges stay
        // within cx±h*2/5 and every y within cy-h*3/5..cy+h*3/10. Safe for any lw<=3.
        for (int k = 0; k < 3; k++) {
            int spread = h * (2 + k) * 2 / 15;       // h*4/15, h*6/15, h*8/15 (< h*3/5)
            int yy = cy + h * 3 / 10 - k * (h * 3 / 10);
            uint16_t c = (k == 2) ? A : P;
            tft.drawWideLine(cx - spread, yy, cx, yy - h / 4, lw, c, c);
            tft.drawWideLine(cx, yy - h / 4, cx + spread, yy, lw, c, c);
        }
        break;
    }
    case GLYPH_NRF: {
        tft.drawRoundRect(cx - h * 3 / 5, cy - h * 1 / 5, h * 6 / 5, h * 4 / 5, 2, P);
        tft.drawWideLine(cx, cy - h * 1 / 5, cx, cy - h * 9 / 10, lw + 1, P, P);
        tft.fillCircle(cx, cy - h, lw + 1, A);
        for (int k = 0; k < 3; k++)
            tft.drawFastHLine(cx - h * 2 / 5, cy + h / 20 + k * h / 4, h * 4 / 5, A);
        break;
    }
    case GLYPH_RFID: {
        tft.drawRoundRect(cx - h * 4 / 5, cy - h * 3 / 5, s * 4 / 5, h * 6 / 5, 3, P);
        int dx = cx - h * 2 / 5, dy = cy;
        tft.fillCircle(dx, dy, lw + 1, A);
        tft.drawArc(dx, dy, h * 4 / 5, h * 4 / 5 - lw, -70, 70, P, BG);
        tft.drawArc(dx, dy, h * 6 / 5, h * 6 / 5 - lw, -70, 70, A, BG);
        break;
    }
    case GLYPH_SCRIPTS: {
        tft.drawRoundRect(cx - h * 3 / 4, cy - h * 3 / 5, s * 3 / 4, h * 6 / 5, 3, P);
        tft.drawWideLine(cx - h / 2, cy - h / 3, cx - h / 6, cy, lw, A, A);
        tft.drawWideLine(cx - h / 6, cy, cx - h / 2, cy + h / 3, lw, A, A);
        tft.drawFastHLine(cx, cy + h / 3, h / 2, A);
        break;
    }
    case GLYPH_AI: {
        // neural node: brain-spark — center node + 4 satellite nodes + links
        tft.fillCircle(cx, cy, h * 3 / 10, P);
        const int off = h * 3 / 5;
        tft.fillCircle(cx - off, cy - off / 2, h / 5, A);
        tft.fillCircle(cx + off, cy - off / 2, h / 5, A);
        tft.fillCircle(cx - off, cy + off / 2, h / 5, A);
        tft.fillCircle(cx + off, cy + off / 2, h / 5, A);
        tft.drawWideLine(cx - off, cy - off / 2, cx, cy, lw, A, A);
        tft.drawWideLine(cx + off, cy - off / 2, cx, cy, lw, A, A);
        tft.drawWideLine(cx - off, cy + off / 2, cx, cy, lw, A, A);
        tft.drawWideLine(cx + off, cy + off / 2, cx, cy, lw, A, A);
        break;
    }
    default: break;
    }
}

#endif
