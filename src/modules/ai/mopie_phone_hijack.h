#ifndef __MOPIE_PHONE_HIJACK_H__
#define __MOPIE_PHONE_HIJACK_H__
#include <Arduino.h>

// Phone Hijack — BLE HID "bad keyboard" tool.
// Advertises as a Bluetooth keyboard; once a phone pairs+connects, it auto-runs
// an Android/Samsung chaos payload (open tabs, search, tweak the device).
// Uses the firmware's existing BleKeyboard + DuckyScript engine (real HID injection).

void mopie_phone_hijack();   // entry point (called from Bad BLE section)

#endif