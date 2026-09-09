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

        if (mopieConfig.devMode) {
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
            {String("LED Blink: ") + (mopieConfig.ledBlinkEnabled ? "ON" : "OFF"),
             [this]() {
                 mopieConfig.ledBlinkEnabled = !mopieConfig.ledBlinkEnabled;
                 mopieConfig.saveFile();
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
            {String("Sound: ") + (mopieConfig.soundEnabled ? "ON" : "OFF"),
                                                             [this]() {
                 mopieConfig.soundEnabled = !mopieConfig.soundEnabled;
                 mopieConfig.saveFile();
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
            {String("QuickBoot: ") + (mopieConfig.instantBoot ? "ON" : "OFF"),
             [this]() {
                 mopieConfig.instantBoot = !mopieConfig.instantBoot;
                 mopieConfig.saveFile();
             }                                                                                                           },
            {String("AutoWiFi: ") + (mopieConfig.wifiAtStartup ? "ON" : "OFF"),
             [this]() {
                 mopieConfig.wifiAtStartup = !mopieConfig.wifiAtStartup;
                 mopieConfig.saveFile();
             }                                                                                                           },
            {"Startup App",                                                         [this]() { setStartupApp(); }        },
            {"App Manager",                                                      [this]() { mainMenu.hideAppsMenu(); }},
            {"Clock Setup",                                                       [this]() { setClock(); }             },
            {String("Language: ") + mopieConfig.keyboardLang,              [this]() { setKeyboardLanguage(); }  },
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
                 String name = keyboard(mopieConfigPins.bleName, 30, "Device name");
                 if (name.length() > 0 && name != "\x1B") mopieConfigPins.setBleName(name);
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
                     mopieConfigPins.factoryReset();
                     mopieConfig.factoryReset();
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
            {"Exit Dev Mode",   [this]() { mopieConfig.setDevMode(false); }},
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
            {"CC1101",         [this]() { setSPIPinsMenu(mopieConfigPins.CC1101_bus); }},
            {"NRF24",          [this]() { setSPIPinsMenu(mopieConfigPins.NRF24_bus); } },
#if !defined(LITE_VERSION)
            {"LoRa",           [this]() { setSPIPinsMenu(mopieConfigPins.LoRa_bus); }  },
            {"ST25R3916",      [this]() { setSPIPinsMenu(mopieConfigPins.ST25R_bus); } },
            {"W5500",          [this]() { setSPIPinsMenu(mopieConfigPins.W5500_bus); } },
#endif
            {"SDCard",         [this]() { setSPIPinsMenu(mopieConfigPins.SDCARD_bus); }},
            {"I2C Bus",        [this]() { setI2CPinsMenu(mopieConfigPins.i2c_bus); }   },
            {"UART",           [this]() { setUARTPinsMenu(mopieConfigPins.uart_bus); } },
            {"GPS",            [this]() { setUARTPinsMenu(mopieConfigPins.gps_bus); }  },
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
    if (mopieConfigPins.SDCARD_bus.checkConflict(mopieConfigPins.uart_bus.rx) ||
        mopieConfigPins.SDCARD_bus.checkConflict(mopieConfigPins.uart_bus.tx)) {
        sdcardSPI.end();
    }

    if (mopieConfigPins.CC1101_bus.checkConflict(mopieConfigPins.uart_bus.rx) ||
        mopieConfigPins.CC1101_bus.checkConflict(mopieConfigPins.uart_bus.tx) ||
        mopieConfigPins.NRF24_bus.checkConflict(mopieConfigPins.uart_bus.rx) ||
        mopieConfigPins.NRF24_bus.checkConflict(mopieConfigPins.uart_bus.tx)) {
        AUX_SPI.end();
    }

    pinMode(mopieConfigPins.uart_bus.rx, INPUT);
    pinMode(mopieConfigPins.uart_bus.tx, OUTPUT);
    Serial1.begin(115200, SERIAL_8N1, mopieConfigPins.uart_bus.rx, mopieConfigPins.uart_bus.tx);
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

