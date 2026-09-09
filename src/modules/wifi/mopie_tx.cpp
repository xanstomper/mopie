#include "mopie_tx.h"
#include <esp_wifi.h>
#include "core/wifi/wifi_common.h"   // wifiRawTx helper

// From-scratch raw 802.11 frame injection for Mopie.
// We build the 802.11 management frames ourselves (deauth etc.) per the spec,
// then hand them to Mopie's wifi_common -> esp_wifi_80211_tx transport to put
// them on the air (kernel sets seq + FCS). This is Mopie's own frame code, not
// a copied upstream template.

int mopie_send_raw(const uint8_t *frame, size_t len, uint8_t channel, int retries) {
    if (!frame || len < 10) return 0;
    // set + lock the channel so the injection lands where we expect
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(5));

    int sent = 0;
    for (int i = 0; i < retries; i++) {
        esp_err_t err = wifiRawTx(WIFI_IF_AP, frame, (int)len);
        if (err == ESP_OK) sent++;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    esp_wifi_set_promiscuous(false);
    return sent;
}

// Build a deauth: management frame control=0x00C0, duration 0, 6-byte addrs,
// seq (filled by the kernel), then reason code (2 bytes).
//   buf   : 26 bytes written
//   bssid : AP we impersonate (addr2 + addr3)
//   target: station to deauth (addr1); pass broadcast to kick the whole network
size_t mopie_build_deauth(uint8_t *buf, const uint8_t *bssid, const uint8_t *target,
                          uint16_t reason) {
    if (!buf || !bssid || !target) return 0;

    buf[0] = 0xC0;   // deauth subtype
    buf[1] = 0x00;
    buf[2] = 0x00;   // duration
    buf[3] = 0x00;
    memcpy(&buf[4], target, 6);   // addr1 = destination (station being kicked)
    memcpy(&buf[10], bssid, 6);   // addr2 = source (AP we impersonate)
    memcpy(&buf[16], bssid, 6);   // addr3 = BSSID
    buf[22] = 0x00;               // seq (kernel fills with en_sys_seq)
    buf[23] = 0x00;
    buf[24] = (uint8_t)(reason & 0xFF);
    buf[25] = (uint8_t)((reason >> 8) & 0xFF);
    return 26;
}

// Build a beacon from the 802.11 spec:
//   24-byte mgmt header (type 0x8 = beacon) + 12-byte fixed params
//   (timestamp 8, beacon interval 2, capability 2)
//   then tagged params: SSID, supported rates, DS parameter set (channel auto).
size_t mopie_build_beacon(uint8_t *buf, const uint8_t *bssid, const char *ssid) {
    if (!buf || !bssid || !ssid) return 0;
    size_t ssidLen = strlen(ssid);
    if (ssidLen == 0 || ssidLen > 32) return 0;

    size_t i = 0;
    // management header
    buf[i++] = 0x80;   // beacon subtype
    buf[i++] = 0x00;
    buf[i++] = 0x00;   // duration
    buf[i++] = 0x00;
    memset(&buf[i], 0xFF, 6); i += 6;   // addr1 = broadcast
    memcpy(&buf[i], bssid, 6); i += 6;  // addr2 = source (us)
    memcpy(&buf[i], bssid, 6); i += 6;  // addr3 = BSSID
    buf[i++] = 0x00; buf[i++] = 0x00;   // seq (kernel fills)
    // fixed params
    memset(&buf[i], 0, 8); i += 8;       // timestamp 0
    buf[i++] = 0x64; buf[i++] = 0x00;    // beacon interval 100 TU (~1s)
    buf[i++] = 0x04; buf[i++] = 0x00;    // capability: ESS + privacy clear (open)
    // tagged params
    buf[i++] = 0x00;                     // SSID tag
    buf[i++] = (uint8_t)ssidLen;
    memcpy(&buf[i], ssid, ssidLen); i += ssidLen;
    buf[i++] = 0x01;                     // supported rates
    buf[i++] = 0x04;
    buf[i++] = 0x82; buf[i++] = 0x84;    // 1, 2
    buf[i++] = 0x8B; buf[i++] = 0x96;    // 5.5, 11
    return i;
}