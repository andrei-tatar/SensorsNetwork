#include "SensorLib.h"
#include <AESLib.h>

Sensor::Sensor(const uint8_t *txAddress, const uint8_t *ownAddress, uint8_t channel, const uint8_t* key, const uint8_t* iv) :
    _radio(2, 3), _txAddress(txAddress), _ownAddress(ownAddress), _channel(channel), _key(key), _iv(iv) {

    //TODO: set a random seed
}

bool Sensor::begin() {
    if (!_radio.begin())
        return false;

    _radio.setChannel(_channel);
    _radio.setAutoAck(false);
    _radio.enableDynamicPayloads();
    _radio.openWritingPipe(_txAddress);
    _radio.openReadingPipe(1, _ownAddress);
    _radio.startListening();
    return true;
}

void Sensor::send(void * data, uint8_t length)
{
    //TODO...
}

#define CHKSUM_INIT     0x1021

void updateChecksum(uint16_t *checksum, uint8_t data) {
    bool roll = *checksum & 0x8000 ? true : false;
    *checksum <<= 1;
    if (roll) *checksum |= 1;
    *checksum ^= data;
}

typedef enum {
    RequestNonce = 0x34,
    Nonce,
    Data,
    Ack
} MessageType;

void Sensor::onPacketReceived(uint8_t *msg, uint8_t length) {
    uint16_t nonce;
    switch (msg[0]) {
    case MessageType::RequestNonce:
        msg[0] = MessageType::Nonce;
        msg[3] = msg[1];
        msg[4] = msg[2];
        nonce = getNonce();
        msg[1] = nonce >> 8;
        msg[2] = nonce;
        sendPacket(msg, 5);
        break;
    case MessageType::Data:
        /*Serial.print("received data: ");
        for (uint8_t i=0; i<length; i++) {
        Serial.print(msg[i+5], HEX);
        Serial.print(':');
        }
        Serial.println();*/
        msg[0] = MessageType::Ack;
        sendPacket(msg, 3);
        break;
    }

}

void Sensor::sendPacket(uint8_t *data, uint8_t length) {
    if (length > 29) {
        Serial.println("send_packet: packet size too large");
        return;
    }

    static uint8_t txBuffer[32];
    txBuffer[2] = length;
    memcpy(&txBuffer[3], data, length);
    length = length <= 13 ? 16 : 32;
    uint16_t chksum = CHKSUM_INIT;
    for (uint8_t i = 2; i < length; i++)
        updateChecksum(&chksum, txBuffer[i]);
    txBuffer[0] = chksum >> 8;
    txBuffer[1] = chksum;
    aes128_cbc_enc(_key, _iv, txBuffer, length);
    _radio.stopListening();
    _radio.write(txBuffer, length);
    _radio.startListening();
}

uint16_t Sensor::getNonce() {
    return rand() << 8 | rand();
}

void Sensor::update() {
    while (_radio.available()) {
        static uint8_t buffer[32];
        _radio.read(buffer, sizeof(buffer));
        uint8_t length = _radio.getDynamicPayloadSize();
        if (length % 16 != 0) return;

        aes128_cbc_dec(_key, _iv, buffer, length);

        uint16_t chksum = CHKSUM_INIT;
        for (uint8_t i = 2; i < length; i++) updateChecksum(&chksum, buffer[i]);
        uint16_t msgChksum = buffer[0] << 8 | buffer[1];
        if (chksum == msgChksum) {
            onPacketReceived(&buffer[3], buffer[2]);
        }
    }
}
