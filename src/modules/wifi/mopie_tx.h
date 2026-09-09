#ifndef __MOPIE_TX_H__
#define __MOPIE_TX_H__

#include <Arduino.h>

// Mopie's own raw-802.11 transmitter. Built from the 802.11 management-frame
// spec rather than inheriting upstream's frame templates, so Mopie's deauth /
// beacon paths are our own code — protocol-accurate, not a copy of another
// project's frames.

// Send an 802.11 management frame (deauth/beacon) on the given channel.
// Returns the count of frames actually injected (0 on failure).
int mopie_send_raw(const uint8_t *frame, size_t len, uint8_t channel, int retries = 3);

// Build a deauth frame from MACs (kernel handles seq/checksum).
//   bssid   == the AP we are impersonating,  target == station or broadcast.
// strlen: 24-byte management header + 2-byte reason code.
// Returns length (26) or 0 on invalid input.
size_t mopie_build_deauth(uint8_t *buf, const uint8_t *bssid, const uint8_t *target,
                          uint16_t reason = 0x0001);

#endif