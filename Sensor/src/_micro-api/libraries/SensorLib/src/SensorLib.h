#include "RF24.h"
#include "nRF24L01.h"

#ifndef _SensorsLib_h
#define _SensorsLib_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

class Sensor
{
private:
    RF24 _radio;
    const uint8_t *_txAddress, *_ownAddress, *_key, *_iv;
    uint8_t _channel;
    uint16_t _nonce;
    bool _nonceRequestPending;
    bool _nonceRequestSuccess;
    bool _ackPending;
    bool _ackSuccess;

    void sendPacket(uint8_t *data, uint8_t length);
    void onPacketReceived(uint8_t *data, uint8_t length);
    uint16_t getNonce();

public:
    Sensor(const uint8_t *txAddress, const uint8_t *ownAddress, uint8_t channel, const uint8_t* key, const uint8_t* iv);

    bool begin();

    bool send(void* data, uint8_t length);
    void update();
};

#endif

