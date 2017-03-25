#ifndef _SensorLib_h
#define _SensorLib_h

#include "RF24.h"
#include "nRF24L01.h"
#include "AESLib.h"
#include "LowPower.h"

typedef void(*DataReceivedHandler)(uint8_t* data, uint8_t length);

class Sensor
{
private:
    RF24 _radio;
    const uint64_t _txAddress, _ownAddress;
    const uint8_t *_key;
    uint8_t _channel;
    uint16_t _nonce;
    bool _nonceRequestPending;
    bool _nonceRequestSuccess;
    bool _ackPending;
    bool _ackSuccess;
    DataReceivedHandler _handler;

    void sendPacket(uint8_t *data, uint8_t length);
    void onPacketReceived(uint8_t *data, uint8_t length);
    uint16_t getNonce();

public:
    Sensor(const uint64_t txAddress, const uint64_t ownAddress, uint8_t channel, const uint8_t* key);

    bool begin();

    bool send(void* data, uint8_t length);
    void update();
    void onMessage(DataReceivedHandler handler);
    void powerDown(uint16_t seconds);
    uint16_t readVoltage();
};

#endif

