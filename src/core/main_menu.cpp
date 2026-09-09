#include "main_menu.h"
#include "display.h"
#include "mykeyboard.h"
#include "mopie_glyphs.h"
#include "utils.h"
#include <globals.h>

// Glyph id per menu class — matches registration order below
static int glyphForName(const String &name) {
    if (name == "WiFi") return GLYPH_WIFI;
    if (name == "BLE") return GLYPH_BLE;
    if (name == "RF") return GLYPH_RF;
    if (name == "IR") return GLYPH_IR;
    if (name == "GPS") return GLYPH_GPS;
    if (name == "Files") return GLYPH_FILES;
    if (name == "Clock") return GLYPH_CLOCK;
    if (name == "Others") return GLYPH_OTHERS;
    if (name == "Config") return GLYPH_CONFIG;
    if (name == "Connect") return GLYPH_CONNECT;
    if (name == "Ethernet") return GLYPH_ETH;
    if (name == "FM") return GLYPH_FM;
    if (name == "LoRa") return GLYPH_LORA;
    if (name == "NRF24") return GLYPH_NRF;
    if (name == "RFID") return GLYPH_RFID;
    if (name == "Scripts") return GLYPH_SCRIPTS;
    if (name == "AI") return GLYPH_AI;
    if (name == "Microphone") return GLYPH_OTHERS;
    return GLYPH_OTHERS;
}

MainMenu::MainMenu() {
    _menuItems = {
#if defined(HAS_SURVEILLANCE)
        &surveillanceMenu,
#endif
        &wifiMenu,
        &bleMenu,
        &rfMenu,
        &nrf24Menu,
#if !defined(LITE_VERSION)
        &loraMenu,
#endif
#if defined(FM_SI4713) && !defined(LITE_VERSION)
        &fmMenu,
#endif
        &irMenu,
#if !defined(LITE_VERSION)
        &ethernetMenu,
#endif
        &gpsMenu,
        &rfidMenu,
        &fileMenu,
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
        &scriptsMenu,
#endif
        &clockMenu,
        &othersMenu,
        &configMenu,
        &aiMenu,
    };

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

void MainMenu::begin(void) {
    returnToMenu = false;

    struct GridItem { String name; int glyph; MenuItemInterface *item; };
    std::vector<GridItem> items;
    std::vector<String> disabled = mopieConfig.disabledMenus;
    for (int i = 0; i < _totalItems; i++) {
        String itemName = _menuItems[i]->getName();
        if (find(disabled.begin(), disabled.end(), itemName) == disabled.end()) {
            items.push_back({itemName, glyphForName(itemName), _menuItems[i]});
        }
    }
    const int total = items.size();
    if (total == 0) return;

    // ---- Paged grid: 4 cols x 2 rows = 8 per page ----
    const int COLS = 4;
    const int ROWS = 2;
    const int PER_PAGE = COLS * ROWS;
    const int pages = (total + PER_PAGE - 1) / PER_PAGE;

    // Layout: status bar 0-15, header 16-31, grid 34-118, label 122-135
    const int GRID_X = 2, GRID_Y = 34;
    const int CELL_W = (tftWidth - 4) / COLS;
    const int CELL_H = 42;                     // 2 rows fit in 84px
    // Glyph sized to the cell: smaller of width/height minus padding, clamped
    const int ICON_S = (CELL_W < CELL_H ? CELL_W : CELL_H) - 12;

    int sel = _currentIndex;
    if (sel >= total) sel = 0;

    bool redrawAll = true;

    auto cellXY = [&](int i, int &x, int &y) {
        int page = i / PER_PAGE;
        int idx = i % PER_PAGE;
        int col = idx % COLS;
        int row = idx / COLS;
        x = GRID_X + col * CELL_W;
        y = GRID_Y + row * CELL_H;
    };

    auto drawCell = [&](int i, bool selected) {
        int x, y; cellXY(i, x, y);
        int cx = x + CELL_W / 2;
        int cy = y + CELL_H / 2;

        // clear cell
        tft.fillRect(x, y, CELL_W - 1, CELL_H - 1, mopieConfig.bgColor);

        // selection backdrop + bracket corners
        if (selected) {
            tft.fillRect(x + 1, y + 1, CELL_W - 3, CELL_H - 3, 0x3000);  // dark red tint
            const int L = 5;
            uint16_t c = mopieConfig.priColor;
            tft.drawFastHLine(x + 2, y + 2, L, c);              tft.drawFastVLine(x + 2, y + 2, L, c);
            tft.drawFastHLine(x + CELL_W - 3 - L, y + 2, L, c); tft.drawFastVLine(x + CELL_W - 3, y + 2, L, c);
            tft.drawFastHLine(x + 2, y + CELL_H - 3, L, c);     tft.drawFastVLine(x + 2, y + CELL_H - 3 - L, L, c);
            tft.drawFastHLine(x + CELL_W - 3 - L, y + CELL_H - 3, L, c); tft.drawFastVLine(x + CELL_W - 3, y + CELL_H - 3 - L, L, c);
        }

        drawMopieGlyph(items[i].glyph, cx, cy, ICON_S);
    };

    auto drawLabel = [&](int i) {
        tft.fillRect(0, tftHeight - 13, tftWidth, 13, 0x2004);
        tft.drawFastHLine(0, tftHeight - 13, tftWidth, mopieConfig.priColor);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, 0x2004);
        tft.drawCentreString(items[i].name.substring(0, 22), tftWidth / 2, tftHeight - 10, 1);
    };

    auto refreshDots = [&]() {
        int dx0 = tftWidth - 6 - pages * 8;
        for (int p = 0; p < pages; p++)
            tft.fillCircle(dx0 + p * 8, 23, 2,
                (p == sel / PER_PAGE) ? mopieConfig.priColor : 0x8C41);
    };

    auto drawChrome = [&]() {
        drawStatusBar();
        // header
        tft.fillRect(0, 16, tftWidth, 15, 0x2004);
        tft.setTextColor(mopieConfig.priColor, 0x2004);
        tft.setTextSize(1);
        tft.setCursor(6, 20);
        tft.print("MOPIE");
        // page dots
        int dx = tftWidth - 6 - pages * 8;
        for (int p = 0; p < pages; p++) {
            int cur = sel / PER_PAGE;
            tft.fillCircle(dx + p * 8, 23, 2,
                (p == cur) ? mopieConfig.priColor : 0x8C41);
        }
    };

    while (true) {
        if (returnToMenu) { returnToMenu = false; return; }

        // keep the long-press-power reboot gesture alive on main menu
        checkReboot();

        int curPage = sel / PER_PAGE;
        if (redrawAll) {
            tft.fillScreen(mopieConfig.bgColor);
            drawChrome();
            int base = curPage * PER_PAGE;
            for (int i = base; i < base + PER_PAGE && i < total; i++)
                drawCell(i, i == sel);
            // blank unused cells
            for (int i = total; i < base + PER_PAGE; i++) {
                int x, y; cellXY(i, x, y);
                tft.fillRect(x, y, CELL_W - 1, CELL_H - 1, mopieConfig.bgColor);
            }
            drawLabel(sel);
            redrawAll = false;
        }

        if (check(NextPress)) {
            // side tap: right one cell; wraps to next row, then next page
            int prevPage = sel / PER_PAGE;
            drawCell(sel, false);
            int col = sel % COLS, row = (sel % PER_PAGE) / COLS;
            if (col < COLS - 1 && sel + 1 < total) sel++;
            else if (row < ROWS - 1 && sel + (COLS - col) < total) sel += COLS - col;  // to next row start
            else sel = 0;                                                              // wrap all
            if (sel / PER_PAGE != prevPage) { redrawAll = true; continue; }
            drawCell(sel, true);
            drawLabel(sel);
            refreshDots();
        }
        if (check(PrevPress)) {
            // side hold: left one cell; wraps to previous row end, then prev page
            int prevPage = sel / PER_PAGE;
            drawCell(sel, false);
            int col = sel % COLS;
            if (col > 0) sel--;
            else if (sel >= COLS) sel--;      // to previous row end (same slot math works: col==0 -> -1)
            else sel = total - 1;             // wrap all
            if (sel / PER_PAGE != prevPage) { redrawAll = true; continue; }
            drawCell(sel, true);
            drawLabel(sel);
            refreshDots();
        }
        if (check(EscPress)) {
            // power tap: page down if multiple pages, else jump to first app
            int prevPage = sel / PER_PAGE;
            drawCell(sel, false);
            if (pages > 1 && prevPage < pages - 1) sel = (prevPage + 1) * PER_PAGE;
            else sel = 0;
            if (sel / PER_PAGE != prevPage) { redrawAll = true; continue; }
            drawCell(sel, true);
            drawLabel(sel);
        }
        if (check(SelPress)) {
            _currentIndex = sel;
            items[sel].item->optionsMenu();
            tft.fillScreen(mopieConfig.bgColor);
            return;   // main loop calls begin() again -> full grid redraw
        }

        static unsigned long lastBar = 0;
        if (millis() - lastBar > 30000) { lastBar = millis(); drawStatusBar(); }

        delay(5);   // tight loop: InputHandler polls faster, no missed/late clicks
    }
}

/*********************************************************************
**  Function: hideAppsMenu
**  Menu to Hide or show menus
**********************************************************************/

void MainMenu::hideAppsMenu() {
    auto items = this->getItems();
    int index = 0;
RESTART: // using gotos to avoid stackoverflow after many choices
    options.clear();
    for (auto item : items) {
        String label = item->getName();
        std::vector<String> l = mopieConfig.disabledMenus;
        bool enabled = find(l.begin(), l.end(), label) == l.end();
        options.push_back(
            {label,
             [this, label, enabled]() {
                 if (enabled) mopieConfig.addDisabledMenu(label);
                 else mopieConfig.removeDisabledMenu(label);
             },
             enabled}
        );
    }
    options.push_back({"Show All", [=]() { mopieConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    index = loopOptions(options, index);
    mopieConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}
