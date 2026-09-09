#include "display_tactical.h"
#include "utils.h"
#include <globals.h>
#include <WiFi.h>

#if defined(HAS_SURVEILLANCE)
#include "modules/surveillance/surveillance_engine.h"
#endif

// Forward declaration from board interface if available
extern float getBatteryVoltage() __attribute__((weak));
float getBatteryVoltage() { return 0.0f; }

namespace TacticalUI {

static bool tacticalEnabled = true;
static CyberTheme activeTheme = THEME_MOPIE_RED;

static const struct {
    const char* name;
    uint16_t pri;
    uint16_t sec;
    uint16_t bg;
} CYBER_THEMES[] = {
    {"Mopie Red",    0xF986, 0x8C41, 0x0000},  // crimson default
    {"Mopie Blue",   0x051F, 0x024F, 0x0000},  // vivid azure
    {"Matrix Lime",  0x07E0, 0x03E0, 0x0000},
    {"Cyber Cyan",   0x07FF, 0x02D7, 0x0000},
    {"Solar Amber",  0xFDA0, 0x93C0, 0x0000},
    {"Neon Violet",  0xD01F, 0x680D, 0x0000}
};

bool isEnabled() {
#if defined(M5STICK_TACTICAL_UI)
    return tacticalEnabled;
#else
    return false;
#endif
}

void setEnabled(bool enable) {
    tacticalEnabled = enable;
}

CyberTheme getCurrentTheme() {
    return activeTheme;
}

const char* getThemeName(CyberTheme theme) {
    if (theme >= THEME_COUNT) return "Custom";
    return CYBER_THEMES[theme].name;
}

void applyTheme(CyberTheme theme) {
    if (theme >= THEME_COUNT) return;
    activeTheme = theme;
    bruceConfig.priColor = CYBER_THEMES[theme].pri;
    bruceConfig.secColor = CYBER_THEMES[theme].sec;
    bruceConfig.bgColor  = CYBER_THEMES[theme].bg;
}

void cycleTheme() {
    CyberTheme next = (CyberTheme)((activeTheme + 1) % THEME_COUNT);
    applyTheme(next);
}

void drawStatusBar() {
    // 15px compact status bar — dark navy fill, blue base line, corner brackets
    tft.fillRect(0, 0, tftWidth, 15, 0x2004);          // near-black navy
    tft.drawFastHLine(0, 15, tftWidth, bruceConfig.priColor);

    tft.setTextSize(1);
    tft.setTextColor(bruceConfig.priColor, 0x2004);
    tft.setCursor(4, 4);

    // Clock or Device branding
    if (clock_set) {
#if defined(HAS_RTC)
        updateTimeStr(_rtc.getTimeStruct());
#else
        updateTimeStr(rtc.getTimeStruct());
#endif
        tft.print(timeStr);
    } else {
        tft.print("MOPIE");
    }

    // Active subsystem badges — filled squares with letter
    int badgeX = 58;
    auto drawBadge = [&](const char* tag, bool active, uint16_t col) {
        if (!active) return;
        tft.fillRect(badgeX, 2, 13, 11, col);
        tft.setTextColor(TFT_WHITE, col);
        tft.setCursor(badgeX + 3, 4);
        tft.print(tag);
        badgeX += 16;
    };

    drawBadge("W", (WiFi.getMode() != 0), 0x8C41);
    drawBadge("B", BLEConnected, 0xF986);
#if defined(HAS_SURVEILLANCE)
    drawBadge("!", (Surveillance::SurveillanceEngine::getInstance().getThreatCount() > 0), TFT_RED);
#endif
    drawBadge("M", sdcardMounted, 0x4208);   // dim orange

    // Battery & Voltage telemetry on right edge
    uint8_t batPct = getBattery();
    float batVolt = getBatteryVoltage();
    bool charging = isCharging();

    char batBuf[20];
    if (batVolt > 2.5f) {
        snprintf(batBuf, sizeof(batBuf), "%s%u%% %.2fV", charging ? "+" : "", batPct, batVolt);
    } else {
        snprintf(batBuf, sizeof(batBuf), "%s%u%%", charging ? "+" : "", batPct);
    }

    int batTextW = strlen(batBuf) * 6;
    int batX = tftWidth - batTextW - 18;
    if (batX < 110) batX = 110;

    uint16_t batCol = TFT_GREEN;
    if (batPct < 20) batCol = TFT_RED;
    else if (batPct < 50) batCol = TFT_YELLOW;

    tft.setTextColor(batCol, 0x2004);
    tft.setCursor(batX, 4);
    tft.print(batBuf);

    // horizontal bar battery (segmented, not a box)
    int iconX = tftWidth - 14;
    int iconY = 4;
    int segs = 4;
    for (int s = 0; s < segs; s++) {
        int sx = iconX + s * 3;
        int on = map(constrain(batPct, 0, 100), 0, 100, 0, segs);
        tft.fillRect(sx, iconY, 2, 7, (s < on) ? batCol : 0x2104);
    }
    tft.fillRect(iconX - 2, iconY + 2, 1, 3, batCol); // tip
}

void drawSubmenu(int index, std::vector<Option>& options, const char* title) {
    drawStatusBar();

    const int total = options.size();
    if (total == 0) return;

    // Viewport: header(16-30) + 4 rows + bottom hint bar
    const int VISIBLE_ROWS = 4;
    const int ROW_HEIGHT = 21;
    const int START_Y = 34;
    const int ROW_WIDTH = tftWidth - 8; // 232px on 240px screen

    // Calculate scrolling window start
    int windowStart = 0;
    if (index >= VISIBLE_ROWS) {
        windowStart = index - VISIBLE_ROWS + 1;
    }
    if (windowStart + VISIBLE_ROWS > total) {
        windowStart = (total >= VISIBLE_ROWS) ? (total - VISIBLE_ROWS) : 0;
    }

    // Header: title left, count right, red rule underneath
    {
        tft.fillRect(0, 16, tftWidth, 15, 0x2004);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, 0x2004);
        String hdr = String(title);
        hdr.toUpperCase();
        tft.setCursor(6, 20);
        tft.print(hdr.substring(0, 22));
        tft.setTextColor(bruceConfig.priColor, 0x2004);
        char cnt[12];
        snprintf(cnt, sizeof(cnt), "%d", total);
        tft.setCursor(tftWidth - 6 - strlen(cnt) * 6, 20);
        tft.print(cnt);
        tft.drawFastHLine(0, 30, tftWidth, bruceConfig.priColor);
    }

    for (int r = 0; r < VISIBLE_ROWS; r++) {
        int optIdx = windowStart + r;
        int y = START_Y + r * ROW_HEIGHT;

        if (optIdx >= total) {
            tft.fillRect(4, y, ROW_WIDTH, ROW_HEIGHT, bruceConfig.bgColor);
            continue;
        }

        bool isSelected = (optIdx == index);
        const auto& opt = options[optIdx];

        if (isSelected) {
            // Full-red inverted bar, white text, left notch
            tft.fillRect(6, y + 1, ROW_WIDTH - 10, ROW_HEIGHT - 3, bruceConfig.priColor);
            tft.fillRect(2, y + 5, 4, ROW_HEIGHT - 11, bruceConfig.priColor);  // notch
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, bruceConfig.priColor);
            tft.setCursor(14, y + 6);
            int maxChars = (ROW_WIDTH - 24) / 6;
            tft.print(opt.label.substring(0, maxChars));

            // Index e.g. 3/12 right-aligned inside bar
            char numBuf[10];
            snprintf(numBuf, sizeof(numBuf), "%d/%d", optIdx + 1, total);
            tft.setTextColor(TFT_WHITE, bruceConfig.priColor);
            tft.setCursor(ROW_WIDTH - 10 - strlen(numBuf) * 6, y + 6);
            tft.print(numBuf);
        } else {
            // Normal row: light text, subtle hairline
            tft.fillRect(4, y, ROW_WIDTH - 6, ROW_HEIGHT, bruceConfig.bgColor);
            tft.setTextSize(1);
            tft.setTextColor(opt.enabled ? TFT_LIGHTGREY : TFT_DARKGREY, bruceConfig.bgColor);
            tft.setCursor(14, y + 6);
            int maxChars = (ROW_WIDTH - 24) / 6;
            tft.print(opt.label.substring(0, maxChars));
            tft.drawFastHLine(8, y + ROW_HEIGHT - 2, ROW_WIDTH - 24, 0x3000); // faint hairline
        }
    }

    // Bottom hint bar — control legend
    {
        int y = tftHeight - 13;
        tft.fillRect(0, y, tftWidth, 13, 0x2004);
        tft.drawFastHLine(0, y, tftWidth, bruceConfig.secColor);
        tft.setTextSize(1);
        tft.setTextColor(bruceConfig.secColor, 0x2004);
        tft.setCursor(6, y + 3);
        tft.print("M5:sel SIDE:up/dn PWR:back");
    }

    // Proportional vertical scrollbar track on right edge
    const int trackX = tftWidth - 4;
    const int trackY = START_Y;
    const int trackH = VISIBLE_ROWS * ROW_HEIGHT;
    tft.fillRect(trackX, trackY, 3, trackH, 0x0841);

    if (total > 0) {
        int thumbH = (VISIBLE_ROWS * trackH) / total;
        if (thumbH < 6) thumbH = 6;
        if (thumbH > trackH) thumbH = trackH;

        int thumbY = trackY + (windowStart * (trackH - thumbH)) / ((total > VISIBLE_ROWS) ? (total - VISIBLE_ROWS) : 1);
        tft.fillRect(trackX, thumbY, 3, thumbH, bruceConfig.priColor);
    }
}

Opt_Coord drawOptions(
    int index, std::vector<Option>& options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor, bool firstRender
) {
    TacticalUI::drawSubmenu(index, options, "Menu");
    Opt_Coord coord;
    coord.x = 18;
    coord.y = 34 + (index % 4) * 21 + 6;
    coord.size = 32;
    coord.fgcolor = fgcolor;
    coord.bgcolor = bgcolor;
    return coord;
}

} // namespace TacticalUI
