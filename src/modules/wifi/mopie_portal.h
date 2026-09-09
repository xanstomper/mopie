#ifndef __MOPIE_PORTAL_H__
#define __MOPIE_PORTAL_H__

#include <Arduino.h>

// Mopie's own captive portal — a from-scratch replacement for the inherited
// EvilPortal subsystem. Lenad, intentional, no inherited internals:
//   * host a fake open AP (SSID + channel)
//   * DNS-capture all request domains (captive redirect)
//   * serve a single phishing login page
//   * capture submitted SSID+password to SD/LittleFS + show a live counter
// Distinct, lightweight architecture for the M5StickC Plus 520KB board.

// Run the portal until the user holds/ presses PWR or Esc. Returns captured
// credential count.
int mopiePortal();

#endif