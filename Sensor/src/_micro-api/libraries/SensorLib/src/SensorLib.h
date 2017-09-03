#ifndef _SensorLib_h
#define _SensorLib_h

#include "RF24.h"
#include "nRF24L01.h"
#include "AESLib.h"
#include "LowPower.h"

typedef void(*DataReceivedHandler)(const uint8_t* data, uint8_t length);

class Sensor
{
private:
    RF24 _radio;
    const uint8_t *_key;
    uint32_t _nextSendNonce;
    uint32_t _oldReceiveNonce;
    uint32_t _nextReceiveNonce;
    uint8_t _send[32];
    uint8_t _sendSize;
    uint32_t _lastSendTime;
    uint8_t _retries;
    bool _sendOk;

    DataReceivedHandler _handler;

    void sendData();
    void sendResponse(uint32_t nonce, bool ack);
    void sendPacket(uint8_t *data, uint8_t length);
    void onPacketReceived(uint8_t *data, uint8_t length);

public:
    Sensor(const uint8_t* key, uint8_t cepin = 2, uint8_t cspin = 3);

    bool begin();

    bool send(void* data, uint8_t length);
    bool sendAndWait(void* data, uint8_t length);
    void update();
    void onMessage(DataReceivedHandler handler);
    void powerDown(uint16_t seconds);
    uint16_t readVoltage();
};

#endif

