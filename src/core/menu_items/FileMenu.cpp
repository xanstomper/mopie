#include "FileMenu.h"
#include "core/mopie_icon_draw.h"
#include "core/display.h"
#include "core/massStorage.h"
#include "core/sd_functions.h"
#include "core/utils.h"
#include "core/wifi/webInterface.h"
#include "core/connect/file_sharing.h"
#include "core/connect/serial_commands.h"

void FileMenu::optionsMenu() {
    options.clear();
    if (setupSdCard()) options.push_back({"SD Card", [=]() { loopSD(SD); }});
    options.push_back({"LittleFS", [=]() { loopSD(LittleFS); }});
    options.push_back({"WebUI", loopOptionsWebUi});

#if !defined(LITE_VERSION)
    options.push_back({"Connect", [=]() {
        std::vector<Option> connectOpts = {
            {"Send File", [=]() { FileSharing().sendFile(); }        },
            {"Recv File", [=]() { FileSharing().receiveFile(); }     },
            {"Send Cmds", [=]() { EspSerialCmd().sendCommands(); }   },
            {"Recv Cmds", [=]() { EspSerialCmd().receiveCommands(); }},
            {"Back",      [=]() { optionsMenu(); }                   }
        };
        loopOptions(connectOpts, MENU_TYPE_SUBMENU, "Connect");
    }});
#endif

#if defined(SOC_USB_OTG_SUPPORTED)
    options.push_back({"Mass Storage", [=]() { MassStorage(); }});
#endif
    addOptionToMainMenu();

    loopOptions(options, MENU_TYPE_SUBMENU, "Files");
}
void FileMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_FILES, iconCenterX, iconCenterY, scale);
}
void FileMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_FILES, cx, cy, scale);
}

