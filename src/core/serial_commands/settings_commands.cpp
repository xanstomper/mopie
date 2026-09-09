#include "settings_commands.h"
#include <globals.h>

uint32_t settingsCallback(cmd *c) {
    Command cmd(c);

    Argument setting_name_arg = cmd.getArgument("setting_name");
    Argument setting_value_arg = cmd.getArgument("setting_value");
    String setting_name = setting_name_arg.getValue();
    String setting_value = setting_value_arg.getValue();
    setting_name.trim();
    setting_value.trim();

    JsonDocument jsonDoc = mopieConfig.toJson();
    JsonObject setting = jsonDoc.as<JsonObject>();

    if (setting_name.length() == 0 && setting_value.length() == 0) {
        // no args, just prints current config
        serializeJsonPretty(jsonDoc, Serial);
        serialDevice->println("");
        return true;
    }

    // Pin/device settings live in mopieConfigPins, which is not part of
    // mopieConfig.toJson(); handle them before the mopieConfig whitelist check
    // below (otherwise they are wrongly rejected as "Invalid field name").
    {
        bool isPinField = true;
        if (setting_name == "bleName") mopieConfigPins.setBleName(setting_value);
        else if (setting_name == "irTx") mopieConfigPins.setIrTxPin(setting_value.toInt());
        else if (setting_name == "irTxRepeats")
            mopieConfigPins.setIrTxRepeats(static_cast<uint8_t>(setting_value.toInt()));
        else if (setting_name == "irRx") mopieConfigPins.setIrRxPin(setting_value.toInt());
        else if (setting_name == "rfTx") mopieConfigPins.setRfTxPin(setting_value.toInt());
        else if (setting_name == "rfRx") mopieConfigPins.setRfRxPin(setting_value.toInt());
        else if (setting_name == "rfModule")
            mopieConfigPins.setRfModule(static_cast<RFModules>(setting_value.toInt()));
        else if (setting_name == "rfFreq") mopieConfigPins.setRfFreq(setting_value.toFloat());
        else if (setting_name == "rfFxdFreq") mopieConfigPins.setRfFxdFreq(setting_value.toInt());
        else if (setting_name == "rfScanRange") mopieConfigPins.setRfScanRange(setting_value.toInt());
        else if (setting_name == "rfidModule")
            mopieConfigPins.setRfidModule(static_cast<RFIDModules>(setting_value.toInt()));
        else isPinField = false;
        if (isPinField) {
            serialDevice->println(setting_name + " = " + setting_value);
            return true;
        }
    }

    if (setting[setting_name].isNull()) {
        serialDevice->println("Invalid field name: " + setting_name);
        return false;
    }

    if (setting_value.length() == 0) {
        serialDevice->print(setting_name + " = ");
        serialDevice->println(setting[setting_name].as<String>());
        return true;
    }

    // TODO: improve this logic and move to MopieConfig
    if (setting_name == "priColor") mopieConfig.setUiColor(setting_value.toInt());
    if (setting_name == "rot") mopieConfigPins.setRotation(setting_value.toInt());
    if (setting_name == "dimmerSet") mopieConfig.setDimmer(setting_value.toInt());
    if (setting_name == "bright") mopieConfig.setBright(setting_value.toInt());
    if (setting_name == "tmz") mopieConfig.setTmz(setting_value.toFloat());
    if (setting_name == "soundEnabled") mopieConfig.setSoundEnabled(setting_value.toInt());
    if (setting_name == "wifiAtStartup") mopieConfig.setWifiAtStartup(setting_value.toInt());
    if (setting_name == "webUI") {
        mopieConfig.setWebUICreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifiAp") {
        mopieConfig.setWifiApCreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifi") {
        mopieConfig.addWifiCredential(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wigleBasicToken") mopieConfig.setWigleBasicToken(setting_value);
    if (setting_name == "wdgwarsApiKey") mopieConfig.setWdgwarsApiKey(setting_value);
    if (setting_name == "devMode") mopieConfig.setDevMode(setting_value.toInt());
    if (setting_name == "disabledMenus") mopieConfig.addDisabledMenu(setting_value);

    return true;
}

uint32_t factoryResetCallback(cmd *c) {
    mopieConfig.factoryReset();
    serialDevice->println("Factory reset done");
    return true;
}

void createSettingsCommands(SimpleCLI *cli) {
    cli->addCommand("factory_reset", factoryResetCallback);

    Command cmd = cli->addCommand("set/tings", settingsCallback);
    cmd.addPosArg("setting_name", "");
    cmd.addPosArg("setting_value", "");
}
