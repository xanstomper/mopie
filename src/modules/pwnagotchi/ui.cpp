/*
Thanks to thoses developers for their projects:
* @7h30th3r0n3 : https://github.com/7h30th3r0n3/Evil-M5Core2 and https://github.com/7h30th3r0n3/PwnGridSpam
* @viniciusbo : https://github.com/viniciusbo/m5-palnagotchi
* @sduenasg : https://github.com/sduenasg/pio_palnagotchi

Thanks to @bmorcelli for his help doing a better code.
*/
#if !defined(LITE_VERSION)
#include "ui.h"
#include "../wifi/sniffer.h"

#define ROW_SIZE 40
#define PADDING 10

int32_t display_w;
int32_t display_h;
int32_t canvas_h;
int32_t canvas_center_x;
int32_t canvas_top_h;
int32_t canvas_bot_h;
int32_t canvas_peers_menu_h;
int32_t canvas_peers_menu_w;

uint8_t menu_current_cmd = 0;
uint8_t menu_current_opt = 0;

void initUi() {
    tft.setTextSize(1);
    tft.fillScreen(mopieConfig.bgColor);
    tft.setTextColor(mopieConfig.priColor);

    display_w = tftWidth;
    display_h = tftHeight;
    canvas_h = display_h * .8;
    canvas_center_x = display_w / 2;
    canvas_top_h = display_h * .1;
    canvas_bot_h = display_h * .9;
    canvas_peers_menu_h = display_h * .8;
    canvas_peers_menu_w = display_w * .8;
}

String getRssiBars(signed int rssi) {
    String rssi_bars = "";

    if (rssi != -1000) {
        if (rssi >= -67) {
            rssi_bars = "[####]";
        } else if (rssi >= -70) {
            rssi_bars = "[### ]";
        } else if (rssi >= -80) {
            rssi_bars = "[##  ]";
        } else {
            rssi_bars = "[#   ]";
        }
    }

    return rssi_bars;
}

void drawTime() {
    tft.drawPixel(0, 0, 0);
    tft.fillRect(80, 0, display_w, canvas_top_h - 3, mopieConfig.bgColor);
    tft.setTextDatum(TR_DATUM);
    unsigned long ellapsed = millis() / 1000;
    int8_t h = ellapsed / 3600;
    int sr = ellapsed % 3600;
    int8_t m = sr / 60;
    int8_t s = sr % 60;
    char right_str[50] = "UPS 0%  UP 00:00:00";
    sprintf(right_str, "UPS %i%% UP %02d:%02d:%02d", getBattery(), h, m, s);
    tft.drawString(right_str, display_w, 3);
}

void drawFooterData(uint8_t friends_run, uint8_t friends_tot, const String &last_friend_name, signed int rssi) {
    tft.drawPixel(0, 0, 0);
    tft.fillRect(0, canvas_bot_h + 1, display_w - 50, canvas_bot_h + 10, mopieConfig.bgColor);
    tft.setTextSize(1);
    tft.setTextColor(mopieConfig.priColor);
    tft.setTextDatum(TL_DATUM);

    String rssi_bars = getRssiBars(rssi);
    String stats = "FRND 0 (0)";
    if (friends_run > 0) {
        stats = "FRND " + String(friends_run) + " (" + String(friends_tot) + ")" + " [" +
                last_friend_name.substring(0, 13) + "] " + rssi_bars;
    }

    tft.drawString(stats, 0, canvas_bot_h + 5);
}

void updateUi(bool show_toolbars) {
    uint8_t mood_id = getCurrentMoodId();
    String mood_face = getCurrentMoodFace();
    String mood_phrase = getCurrentMoodPhrase();
    bool mood_broken = isCurrentMoodBroken();

    // Draw header and footer
    if (show_toolbars) {
        drawTopCanvas();
        if (tftHeight > 150) drawTime();
        drawFooterData(
            getPwngridRunTotalPeers(),
            getPwngridTotalPeers(),
            getPwngridLastFriendName(),
            getPwngridClosestRssi()
        );
    }

    // Draw mood
    drawMood(mood_face, mood_phrase, mood_broken);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

void drawTopCanvas() {
    // prepare canvas
    tft.setTextSize(1);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.setTextDatum(TL_DATUM);
    char buffer[32];
    sprintf(buffer, "CH %02d, HS %d", ch, num_HS);
    // draw screen
    tft.drawPixel(0, 0, 0);
    tft.fillRect(0, 0, display_w, canvas_top_h, mopieConfig.bgColor);
    tft.drawString("DETECTIVE MOPIE", 0, 3);
    tft.setTextColor(mopieConfig.secColor, mopieConfig.bgColor);
    tft.drawString(buffer, 108, 3);

    // channel scan animation: 13 segment bar = real AP density per channel,
    // with a moving sweep highlight so it reads as a live radar sweep.
    const int barY = canvas_top_h - 4;
    const int segW = 6, segH = 12, gap = 1;
    static uint8_t sweep = 0;              // advancing highlight
    long scanMs = millis();
    long phase = scanMs / 220;             // sweep step every 220ms
    sweep = phase;                         // just cycles as an int
    int chCount[14] = {0};
    for (const auto &b : registeredBeacons) {
        if (b.channel >= 1 && b.channel <= 13) chCount[b.channel]++;
    }
    int maxC = 1;
    for (int i = 1; i <= 13; i++) if (chCount[i] > maxC) maxC = chCount[i];
    for (int i = 0; i < 13; i++) {
        int x = 2 + i * (segW + gap);
        int cn = chCount[i + 1];           // APs on channel i+1
        int hh = 2 + (cn * (segH - 3)) / maxC;   // density-proportional height
        if (cn == 0) hh = 3;
        uint16_t c = mopieConfig.bgColor;
        if (cn > 0) c = mopieConfig.secColor;                    // active channel
        if (i + 1 == ch) c = mopieConfig.priColor;               // current channel
        if ((int)(sweep % 13) == i) c = mopieConfig.secColor;    // sweep highlight
        tft.fillRect(x, barY - hh, segW, hh, c);
    }
    tft.drawLine(0, canvas_top_h - 1, display_w, canvas_top_h - 1, mopieConfig.priColor);
}

void drawHandshakeCaught() {
    // quick "handshake locked" flash animation
    const int cx = display_w / 2, cy = canvas_h / 3;
    uint16_t c = mopieConfig.priColor;
    for (int k = 0; k < 2; k++) {
        tft.drawCircle(cx, cy, 18 + k * 2, c);
        tft.drawCircle(cx, cy, 24 + k * 2, mopieConfig.secColor);
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, mopieConfig.bgColor);
    tft.drawCentreString("HS LOCKED", cx, cy + 30, 1);
    // handshake zigzag between two dots
    tft.drawWideLine(cx - 30, cy + 44, cx - 10, cy + 40, 1, c, c);
    tft.drawWideLine(cx - 10, cy + 40, cx + 10, cy + 48, 1, mopieConfig.secColor, mopieConfig.secColor);
    tft.drawWideLine(cx + 10, cy + 48, cx + 30, cy + 44, 1, c, c);
    tft.fillCircle(cx - 30, cy + 44, 2, c);
    tft.fillCircle(cx + 30, cy + 44, 2, c);
}

void drawBottomCanvas() {
    // prepare canvas
    tft.setTextSize(1);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.setTextDatum(TR_DATUM);
    // draw screen
    tft.drawPixel(0, 0, 0);
    tft.fillRect(0, canvas_bot_h, display_w, canvas_bot_h + 10, mopieConfig.bgColor);
    tft.drawString("NOT AI", display_w, canvas_bot_h + 5);
    tft.drawLine(0, canvas_bot_h, display_w, canvas_bot_h, mopieConfig.priColor);
}

void drawMood(const String &face, const String &phrase, bool broken) {
    // prepare canvas
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.setTextSize(FG + 1);
    tft.setTextDatum(MC_DATUM);
    // draw screen
    tft.drawPixel(0, 0, 0);
    tft.fillRect(0, canvas_top_h + 10, display_w, canvas_bot_h - 40, mopieConfig.bgColor);
    tft.drawCentreString(face, canvas_center_x, canvas_h / 3, SMOOTH_FONT);
    // prepare canvas
    tft.setTextDatum(BC_DATUM);
    tft.setTextSize(1);
    // draw screen
    tft.drawPixel(0, 0, 0);
    tft.drawCentreString(phrase, canvas_center_x, canvas_h - 30, SMOOTH_FONT);
}
#endif
