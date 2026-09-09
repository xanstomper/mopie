#include "boot_animation.h"
#include <globals.h>
#include <TFT_eSPI.h>

extern tft_logger tft;

static void drawHexLocal(int cx, int cy, int r, uint16_t color);

// Mopie boot — crimson tactical intro:
// 1) radar ring materializes w/ rotating sweep
// 2) glitch bars tear across
// 3) hex core locks in, wordmark stamps
// 4) heartbeat double-pulse, chime, out

void playMopieBootAnimation() {
    const uint16_t BG    = 0x0000;
    const uint16_t RED   = 0xF986;   // crimson
    const uint16_t DEEP  = 0x8C41;   // rust
    const uint16_t AMBER = 0xFDA0;   // accent
    const uint16_t DARK  = 0x2004;   // maroon-black

    const int cx = tftWidth / 2;
    const int cy = tftHeight / 2 - 6;

    tft.fillScreen(BG);

    // --- Phase 1: radar rings + sweep (~900ms) ---
    for (int ring = 0; ring < 3; ring++) {
        int r = 14 + ring * 12;
        // draw ring as dashes
        for (int a = 0; a < 360; a += 12) {
            float a1 = a * PI / 180.0f;
            float a2 = (a + 6) * PI / 180.0f;
            tft.drawLine(
                cx + r * cosf(a1), cy + r * sinf(a1),
                cx + r * cosf(a2), cy + r * sinf(a2),
                (ring == 1) ? DEEP : DARK
            );
        }
        delay(60);
    }
    // rotating sweep, fading trail
    for (int sweep = 0; sweep < 3; sweep++) {
        for (int a = 0; a < 360; a += 8) {
            float a1 = a * PI / 180.0f;
            tft.drawLine(cx, cy, cx + 44 * cosf(a1), cy + 44 * sinf(a1), RED);
            // trail
            float t1 = (a - 12) * PI / 180.0f;
            tft.drawLine(cx, cy, cx + 44 * cosf(t1), cy + 44 * sinf(t1), DEEP);
            float t2 = (a - 24) * PI / 180.0f;
            tft.drawLine(cx, cy, cx + 34 * cosf(t2), cy + 34 * sinf(t2), DARK);
            delay(4);
            // erase behind: each trail line erased at its own length
            tft.drawLine(cx, cy, cx + 44 * cosf(t1), cy + 44 * sinf(t1), BG);
            tft.drawLine(cx, cy, cx + 34 * cosf(t2), cy + 34 * sinf(t2), BG);
        }
    }

    // --- Phase 2: glitch bars (~300ms) ---
    for (int g = 0; g < 7; g++) {
        int gy = random(tftHeight);
        int gh = 3 + random(8);
        int gx = random(-30, 10);
        tft.fillRect(gx, gy, tftWidth + 20, gh, (g & 1) ? RED : DEEP);
        delay(35);
        tft.fillRect(gx, gy, tftWidth + 15, gh, BG);
    }

    // --- Phase 3: hex lock + wordmark (~700ms) ---
    tft.fillRect(0, cy - 46, tftWidth, 92, BG);
    // quick scale-in hexes
    for (int r = 4; r <= 26; r += 4) {
        drawHexLocal(cx, cy, r, RED);
        delay(22);
    }
    drawHexLocal(cx, cy, 31, DEEP);
    drawHexLocal(cx, cy, 33, DARK);

    // stamp wordmark
    tft.setTextColor(AMBER, BG);
    tft.drawCentreString("MOPIE", cx, cy + 20, 2);
    tft.setTextColor(DEEP, BG);
#ifdef MOPIE_VERSION
    tft.drawCentreString("v" MOPIE_VERSION " TACTICAL", cx, cy + 38, 1);
#else
    tft.drawCentreString("TACTICAL", cx, cy + 38, 1);
#endif
    delay(350);

    // --- Phase 4: heartbeat double-pulse (~500ms) ---
    for (int beat = 0; beat < 2; beat++) {
        drawHexLocal(cx, cy, 36, RED);
        drawHexLocal(cx, cy, 39, DEEP);
        delay(90);
        drawHexLocal(cx, cy, 36, BG);
        drawHexLocal(cx, cy, 39, BG);
        delay(140);
    }

    // --- Chime: Pokémon-style sparkle (E5-A5-E6) ---
#if defined(BUZZ_PIN)
    tone(BUZZ_PIN, 659, 90);   delay(95);
    tone(BUZZ_PIN, 880, 90);   delay(95);
    tone(BUZZ_PIN, 1319, 140); delay(150);
    noTone(BUZZ_PIN);
#endif

    tft.fillScreen(BG);
}

static void drawHexLocal(int cx, int cy, int r, uint16_t color) {
    for (int i = 0; i < 6; i++) {
        float a1 = (60 * i - 90) * PI / 180.0f;
        float a3 = (60 * (i + 1) - 90) * PI / 180.0f;
        tft.drawLine(
            cx + (int)(r * cosf(a1)), cy + (int)(r * sinf(a1)),
            cx + (int)(r * cosf(a3)), cy + (int)(r * sinf(a3)),
            color
        );
    }
}
