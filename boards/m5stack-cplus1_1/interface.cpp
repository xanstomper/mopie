#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include <AXP192.h>
#include <interface.h>

AXP192 axp192;

#define STICK_LED_PIN 10
#define STICK_BUZZ_PIN 2

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    pinMode(SEL_BTN, INPUT);
    pinMode(DW_BTN, INPUT);

    // Red LED (GPIO 10) active-low
    pinMode(STICK_LED_PIN, OUTPUT);
    digitalWrite(STICK_LED_PIN, HIGH);

    // Buzzer (GPIO 2)
    pinMode(STICK_BUZZ_PIN, OUTPUT);
    digitalWrite(STICK_BUZZ_PIN, LOW);

    setSysI2CBus(&Wire1); // AXP192 (BM8563 RTC included) lives on Wire1
#if defined(HAS_RTC)
    _rtc.setWire(getSysI2CBus());
#endif
    axp192.begin(); // Start the energy management of AXP192

    // Clear any startup power-on IRQs
    axp192.GetBtnPress();
    delay(10);
    axp192.GetBtnPress();
}

/***************************************************************************************
** Function name: getBatteryVoltage() & getBattery()
** Description:   Accurate LiPo curve calculation and raw voltage
***************************************************************************************/
float getBatteryVoltage() {
    return axp192.GetBatVoltage();
}

int getBattery() {
    float v = axp192.GetBatVoltage();
    if (v >= 4.18f) return 100;
    if (v <= 3.20f) return 0;

    static const struct { float v; int pct; } lipoCurve[] = {
        {4.18f, 100}, {4.05f, 90}, {3.95f, 80}, {3.87f, 70}, {3.82f, 60},
        {3.79f, 50},  {3.75f, 40}, {3.71f, 30}, {3.65f, 20}, {3.55f, 10},
        {3.30f, 3},   {3.20f, 0}
    };

    for (size_t i = 0; i < 11; i++) {
        if (v >= lipoCurve[i+1].v) {
            float frac = (v - lipoCurve[i+1].v) / (lipoCurve[i].v - lipoCurve[i+1].v);
            return (int)(lipoCurve[i+1].pct + frac * (lipoCurve[i].pct - lipoCurve[i+1].pct));
        }
    }
    return 1;
}

/*********************************************************************
**  Function: setBrightness
**  set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval > 100) brightval = 100;
    axp192.ScreenBreath(brightval);
}

/*********************************************************************
** Function: InputHandler
** Handles ergonomic 3-button navigation:
**  - Front M5 (SEL): Select / Enter
**  - Side B (DW): Short click = Next (DOWN), Long hold (>350ms) = Prev (UP)
**  - Power PEK: Short click = Esc (BACK)
**********************************************************************/
void InputHandler(void) {
    // Ignore input during initial boot stabilization
    if (millis() < 1200) {
        axp192.GetBtnPress();
        return;
    }

    static unsigned long btnBPressStart = 0;
    static bool btnBActive = false;
    static bool btnBLongFired = false;
    static unsigned long lastRepeat = 0;
    static unsigned long lastSelTime = 0;

    // Read physical buttons
    bool selPressed = (digitalRead(SEL_BTN) == LOW);
    bool dwPressed = (digitalRead(DW_BTN) == LOW);
    uint8_t axpBtn = axp192.GetBtnPress(); // 0x01: long press, 0x02: short press

    bool any = selPressed || dwPressed || (axpBtn != 0);
    if (any) {
        AnyKeyPress = true;
        if (wakeUpScreen()) return;
    }

    // Power PEK Short Click -> ESC / Back
    if (axpBtn & 0x02) {
        EscPress = true;
        return;
    }

    // Front Button A (M5) -> SELECT
    if (selPressed) {
        if (millis() - lastSelTime > 250) {
            lastSelTime = millis();
            SelPress = true;
            return;
        }
    }

    // Side Button B -> Smart Dual UP / DOWN
    if (dwPressed) {
        if (!btnBActive) {
            btnBActive = true;
            btnBPressStart = millis();
            btnBLongFired = false;
        } else {
            // Held down
            if (!btnBLongFired && (millis() - btnBPressStart > 350)) {
                btnBLongFired = true;
                PrevPress = true; // UP
                lastRepeat = millis();
            } else if (btnBLongFired && (millis() - lastRepeat > 180)) {
                lastRepeat = millis();
                PrevPress = true; // Repeat UP
            }
        }
    } else {
        if (btnBActive) {
            // Released without long press -> DOWN
            if (!btnBLongFired && (millis() - btnBPressStart < 350) && (millis() - btnBPressStart > 25)) {
                NextPress = true;
            }
            btnBActive = false;
            btnBLongFired = false;
        }
    }
}

/*********************************************************************
** Function: powerOff
** Clean hardware shutdown via AXP192
**********************************************************************/
void powerOff() {
    axp192.PowerOff();
}

/*********************************************************************
** Function: checkReboot
** Note: AXP192 PMIC automatically cuts power when the PEK power button
** is held for 4s in hardware. checkReboot() is called by display.cpp
** on every frame of loopOptions(), so it MUST be non-blocking and NOT
** initiate an immediate shutdown.
**********************************************************************/
void checkReboot() {
    // No-op. AXP192 hardware power-off protects the device.
}

/***************************************************************************************
** Function name: isCharging()
** Description:   Determines if the device is charging or on USB power
***************************************************************************************/
bool isCharging() {
    return (axp192.GetBatCurrent() > 15.0f) || (axp192.GetVBusVoltage() > 4.4f) || (axp192.GetVinVoltage() > 4.4f);
}
