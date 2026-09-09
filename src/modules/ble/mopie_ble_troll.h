#ifndef __MOPIE_BLE_TROLL_H__
#define __MOPIE_BLE_TROLL_H__

#include <Arduino.h>

// Mopie's own BLE "disconnect flood" — actively connect/disconnect to nearby
// BLE peripherals to disrupt them. Distinct from the inherited advertising-spam
// suite (that only advertises; this does real connect/disconnect churn).

// Run the disconnect flood for `durationMs` (default ~10s) unless stopped by
// Esc. Scans, connects to a found peripheral, disconnects, repeats. Returns the
// number of connect/disconnect cycles attempted.
int mopieBLEFlood(uint32_t durationMs = 10000);

#endif