#include "RFMenu.h"
#include "core/mopie_icon_draw.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/rf/record.h"
#include "modules/rf/rf_bruteforce.h"
#include "modules/rf/rf_jammer.h"
#include "modules/rf/rf_listen.h"
#include "modules/rf/rf_scan.h"
#include "modules/rf/rf_send.h"
#include "modules/rf/rf_utils.h"
#include "modules/rf/rf_spectrum.h"
#include "modules/rf/rf_waterfall.h"

void RFMenu::optionsMenu() {
    options = {
        {"Scan/copy",       [=]() { RFScan(); }       },
#if !defined(LITE_VERSION)
        {"Record RAW",      rf_raw_record             }, // Pablo-Ortiz-Lopez
        {"Custom SubGhz",   sendCustomRF              },
#endif
        {"Spectrum",        rf_spectrum               },
#if !defined(LITE_VERSION)
        {"RSSI Spectrum",   rf_CC1101_rssi            }, // @Pirata
        {"SquareWave Spec", rf_SquareWave             }, // @Pirata
        {"Spectogram",      rf_waterfall              }, // dev_eclipse
#if defined(BUZZ_PIN) or defined(HAS_NS4168_SPKR) and defined(RF_LISTEN_H)
        {"Listen",          rf_listen                 }, // dev_eclipse
#endif
        {"Bruteforce",      rf_bruteforce             }, // dev_eclipse
        {"Jammer",          [=]() { RFJammer(true); } },
#endif
        {"Config",          [this]() { configMenu(); }},
    };
    addOptionToMainMenu();

    delay(200);
    String txt = "Radio Frequency";
    if (mopieConfigPins.rfModule == CC1101_SPI_MODULE) txt += " (CC1101)"; // Indicates if CC1101 is connected
    else txt += " Tx: " + String(mopieConfigPins.rfTx) + " Rx: " + String(mopieConfigPins.rfRx);

    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
}

void RFMenu::configMenu() {
    options = {
        {"RF TX Pin", lambdaHelper(gsetRfTxPin, true)},
        {"RF RX Pin", lambdaHelper(gsetRfRxPin, true)},
        {"RF Module", setRFModuleMenu},
        {"RF Frequency", setRFFreqMenu},
        {"Back", [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "RF Config");
}

void RFMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_RF, iconCenterX, iconCenterY, scale);
}
void RFMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_RF, cx, cy, scale);
}

