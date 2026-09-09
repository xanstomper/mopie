#ifndef MOPIE_BLE_SERVICE_H
#define MOPIE_BLE_SERVICE_H
#if !defined(LITE_VERSION)
#include <NimBLEServer.h>

class MopieBLEService {
protected:
    NimBLEService *pService = nullptr;
    uint16_t mtu = 23; // default MTU size
public:
    virtual ~MopieBLEService() = default;
    virtual void setup(NimBLEServer *pServer) = 0;
    virtual void end() = 0;
    void setMTU(int new_mtu) { mtu = new_mtu; }
};
#endif
#endif // MOPIE_BLE_SERVICE_H
