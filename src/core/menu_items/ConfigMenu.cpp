#include "ConfigMenu.h"
#include "core/mopie_icon_draw.h"
#include "../mykeyboard.h"
#include "core/display.h"
#include "core/i2c_finder.h"
#include "core/main_menu.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#ifdef HAS_RGB_LED
#include "core/led_control.h"
#endif

/*********************************************************************
**  Function: optionsMenu
**  Mopie SysCtl - System Control Panel
**********************************************************************/
void ConfigMenu::optionsMenu() {
    returnToMenu = false;
    while (true) {
        if (returnToMenu) {
            returnToMenu = false;
            return;
        }

        std::vector<Option> localOptions = {
            {"Display",       [this]() { displayUIMenu(); }},
#ifdef HAS_RGB_LED
            {"LED Matrix",    [this]() { ledMenu(); }      },
#endif
#if !defined(LITE_VERSION) && (defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR))
            {"Audio",         [this]() { audioMenu(); }    },
#endif
            {"System",        [this]() { systemMenu(); }   },
            {"Power Mgmt",    [this]() { powerMenu(); }    },
        };

#if !defined(LITE_VERSION)
        if (!appStoreInstalled()) {
            localOptions.push_back({"Get More Apps", []() { installAppStoreJS(); }});
        }
#endif

        if (bruceConfig.devMode) {
            localOptions.push_back({"Dev Console", [this]() { devMenu(); }});
        }

        localOptions.push_back({"Device Info", showDeviceInfo});
        localOptions.push_back({"Main Menu", []() {}});

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "SysCtl");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: displayUIMenu
**  Display configuration - Mopie style
**********************************************************************/
void ConfigMenu::displayUIMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Brightness",  [this]() { setBrightnessMenu(); }               },
            {"Dim Timeout", [this]() { setDimmerTimeMenu(); }               },
            {"Rotation",    [this]() { lambdaHelper(gsetRotation, true)(); }},
            {"Accent Color",[this]() { setUIColor(); }                      },
            {"Theme Pack",  [this]() { setTheme(); }                        },
            {"Back",        []() {}                                         },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Display");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: ledMenu
**  LED configuration - Mopie style
**********************************************************************/
#ifdef HAS_RGB_LED
void ConfigMenu::ledMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"LED Color",
             [this]() {
                 beginLed();
                 setLedColorConfig();
             }                                                                            },
            {"LED Effect",
             [this]() {
                 beginLed();
                 setLedEffectConfig();
             }                                                                            },
            {"LED Intensity",
             [this]() {
                 beginLed();
                 setLedBrightnessConfig();
             }                                                                            },
            {String("LED Blink: ") + (bruceConfig.ledBlinkEnabled ? "ON" : "OFF"),
             [this]() {
                 bruceConfig.ledBlinkEnabled = !bruceConfig.ledBlinkEnabled;
                 bruceConfig.saveFile();
             }                                                                            },
            {"Back",                                                               []() {}},
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "LED Matrix");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}
#endif

/*********************************************************************
**  Function: audioMenu
**  Audio configuration - Mopie style
**********************************************************************/
void ConfigMenu::audioMenu() {
    while (true) {
        std::vector<Option> localOptions = {
#if !defined(LITE_VERSION)
#if defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR)
            {String("Sound: ") + (bruceConfig.soundEnabled ? "ON" : "OFF"),
                                                             [this]() {
                 bruceConfig.soundEnabled = !bruceConfig.soundEnabled;
                 bruceConfig.saveFile();
             }                                                                                                                                            },
#if defined(HAS_NS4168_SPKR)
            {"Volume",                                                    [this]() { setSoundVolume(); }},
#endif
#endif
#endif
            {"Back",                                                        []() {}                       },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Audio");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: systemMenu
**  System configuration - Mopie style
**********************************************************************/
void ConfigMenu::systemMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {String("QuickBoot: ") + (bruceConfig.instantBoot ? "ON" : "OFF"),
             [this]() {
                 bruceConfig.instantBoot = !bruceConfig.instantBoot;
                 bruceConfig.saveFile();
             }                                                                                                           },
            {String("AutoWiFi: ") + (bruceConfig.wifiAtStartup ? "ON" : "OFF"),
             [this]() {
                 bruceConfig.wifiAtStartup = !bruceConfig.wifiAtStartup;
                 bruceConfig.saveFile();
             }                                                                                                           },
            {"Startup App",                                                         [this]() { setStartupApp(); }        },
            {"App Manager",                                                      [this]() { mainMenu.hideAppsMenu(); }},
            {"Clock Setup",                                                       [this]() { setClock(); }             },
            {String("Language: ") + bruceConfig.keyboardLang,              [this]() { setKeyboardLanguage(); }  },
            {"Advanced",                                                            [this]() { advancedMenu(); }         },
            {"Back",                                                                []() {}                              },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "System");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: advancedMenu
**  Advanced settings - Mopie style
**********************************************************************/
void ConfigMenu::advancedMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"GPIO Pins", [this]() { pinsMenu(); }           },
#if !defined(LITE_VERSION)
            {"BLE API",   [this]() { enableBLEAPI(); }       },
            {"HID Mode",  [this]() { setBadUSBBLEMenu(); }   },
#endif
            {"Device Name",
             [this]() {
                 String name = keyboard(bruceConfigPins.bleName, 30, "Device name");
                 if (name.length() > 0 && name != "\x1B") bruceConfigPins.setBleName(name);
             }                                                     },
            {"Network Creds",   [this]() { setNetworkCredsMenu(); }},
            {"Factory Reset",
             []() {
                 drawMainBorder(true);
                 int8_t choice = displayMessage(
                     "Factory Reset?\nAll data will be lost!",
                     "No",
                     nullptr,
                     "Yes",
                     TFT_RED
                 );
                 if (choice == 1) {
                     bruceConfigPins.factoryReset();
                     bruceConfig.factoryReset();
                 }
             }                                                     },
            {"Back",            []() {}                            },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Advanced");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: powerMenu
**  Power management - Mopie style
**********************************************************************/
void ConfigMenu::powerMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Deep Sleep", goToDeepSleep          },
            {"Sleep Mode", setSleepMode           },
            {"Reboot",     []() { ESP.restart(); }},
            {"Power Off",
             []() {
                 drawMainBorder(true);
                 int8_t choice = displayMessage("Power Off?", "No", nullptr, "Yes", TFT_RED);
                 if (choice == 1) { powerOff(); }
             }                                    },
            {"Back",       []() {}                },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Power Mgmt");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: devMenu
**  Developer console - Mopie style
**********************************************************************/
void ConfigMenu::devMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Serial -> USB",   [this]() { switchToUSBSerial(); }          },
            {"Serial -> UART",  [this]() { switchToUARTSerial(); }         },
            {"Exit Dev Mode",   [this]() { bruceConfig.setDevMode(false); }},
            {"Back",            []() {}                                    },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Dev Console");

        if (selected == localOptions.size() - 2) {
            returnToMenu = true;
            return;
        }

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: pinsMenu
**  GPIO pin configuration - Mopie style
**********************************************************************/
void ConfigMenu::pinsMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"I2C Scanner",    [this]() { find_i2c_addresses(); }                      },
            {"CC1101",         [this]() { setSPIPinsMenu(bruceConfigPins.CC1101_bus); }},
            {"NRF24",          [this]() { setSPIPinsMenu(bruceConfigPins.NRF24_bus); } },
#if !defined(LITE_VERSION)
            {"LoRa",           [this]() { setSPIPinsMenu(bruceConfigPins.LoRa_bus); }  },
            {"ST25R3916",      [this]() { setSPIPinsMenu(bruceConfigPins.ST25R_bus); } },
            {"W5500",          [this]() { setSPIPinsMenu(bruceConfigPins.W5500_bus); } },
#endif
            {"SDCard",         [this]() { setSPIPinsMenu(bruceConfigPins.SDCARD_bus); }},
            {"I2C Bus",        [this]() { setI2CPinsMenu(bruceConfigPins.i2c_bus); }   },
            {"UART",           [this]() { setUARTPinsMenu(bruceConfigPins.uart_bus); } },
            {"GPS",            [this]() { setUARTPinsMenu(bruceConfigPins.gps_bus); }  },
            {"Back",           []() {}                                                 },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "GPIO Pins");

        if (selected == -1 || selected == localOptions.size() - 1) { return; }
    }
}

/*********************************************************************
**  Function: switchToUSBSerial
**********************************************************************/
void ConfigMenu::switchToUSBSerial() {
    USBserial.setSerialOutput(&Serial);
    Serial1.end();
}

/*********************************************************************
**  Function: switchToUARTSerial
**********************************************************************/
void ConfigMenu::switchToUARTSerial() {
    if (bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.uart_bus.tx)) {
        sdcardSPI.end();
    }

    if (bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.uart_bus.tx) ||
        bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.uart_bus.rx) ||
        bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.uart_bus.tx)) {
        AUX_SPI.end();
    }

    pinMode(bruceConfigPins.uart_bus.rx, INPUT);
    pinMode(bruceConfigPins.uart_bus.tx, OUTPUT);
    Serial1.begin(115200, SERIAL_8N1, bruceConfigPins.uart_bus.rx, bruceConfigPins.uart_bus.tx);
    USBserial.setSerialOutput(&Serial1);
}

/*********************************************************************
**  Function: drawIcon
**  Mopie SysCtl icon - hexagonal gear
**********************************************************************/
void ConfigMenu::drawIcon(float scale) {
    clearIconArea();
    drawMopieIcon(ICON_CONFIG, iconCenterX, iconCenterY, scale);
}
void ConfigMenu::drawIconAt(int cx, int cy, float scale) {
    drawMopieIcon(ICON_CONFIG, cx, cy, scale);
}

