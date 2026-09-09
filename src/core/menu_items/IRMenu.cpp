#include "IRMenu.h"
#include "core/mopie_icon_draw.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/ir/TV-B-Gone.h"
#include "modules/ir/custom_ir.h"
#include "modules/ir/ir_jammer.h"
#include "modules/ir/ir_read.h"

void IRMenu::optionsMenu() {
#if defined(ARDUINO_M5STICK_S3)
    bool prevPower = M5.Power.getExtOutput();
    M5.Power.setExtOutput(true); // ENABLE 5V OUTPUT
#endif
    options = {
        {"TV-B-Gone", StartTvBGone              },
        {"Custom IR", otherIRcodes              },
        {"IR Read",   [=]() { IrRead(); }       },
#if !defined(LITE_VERSION)
        {"IR Jammer", startIrJammer             }, // Simple frequency-adjustable jammer
#endif
        {"Config",    [this]() { configMenu(); }},
    };
    addOptionToMainMenu();

    String txt = "Infrared";
    txt += " Tx: " + String(bruceConfigPins.irTx) + " Rx: " + String(bruceConfigPins.irRx) +
           " Rpts: " + String(bruceConfigPins.irTxRepeats);
    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
#if defined(ARDUINO_M5STICK_S3)
    M5.Power.setExtOutput(prevPower);
#endif
}

void IRMenu::configMenu() {
    options = {
        {"Ir TX Pin", lambdaHelper(gsetIrTxPin, true)},
        {"Ir RX Pin", lambdaHelper(gsetIrRxPin, true)},
        {"Ir TX Repeats", setIrTxRepeats},
        {"Back", [this]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "IR Config");
}

void IRMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_IR, iconCenterX, iconCenterY, scale);
}
void IRMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_IR, cx, cy, scale);
}

