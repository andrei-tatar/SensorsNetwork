#include "SensorLib.h"

const uint8_t iv[16];

Sensor::Sensor(const uint64_t txAddress, const uint64_t ownAddress, uint8_t channel, const uint8_t* key) :
    _radio(2, 3), _txAddress(txAddress), _ownAddress(ownAddress), _channel(channel), _key(key), _handler(NULL) {
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

typedef struct {
    uint8_t type;
    uint16_t rcvdNonce;
    uint16_t sentNonce;
    uint32_t time;
} Awaiter;

#define SEND_INTERVAL   50
#define RETRIES         20
#define TIMEOUT         (RETRIES * SEND_INTERVAL)
#define AWAITERS        5

void Sensor::onPacketReceived(uint8_t *msg, uint8_t length) {
    uint16_t nonce;
    static Awaiter awaiters[AWAITERS];
    uint32_t now = millis();

    uint16_t receivedNonce = msg[1] << 8 | msg[2];

    if (msg[0] == MessageType::Nonce) {
        uint16_t sentNonce = msg[3] << 8 | msg[4];
        if (_nonceRequestPending && _nonce == sentNonce) {
            _nonceRequestSuccess = true;
            _nonce = receivedNonce;
        }
    }
    else if (msg[0] == MessageType::Ack) {
        if (_nonce == receivedNonce) {
            _ackSuccess = true;
        }
    }
    else if (msg[0] == MessageType::RequestNonce) {
        Awaiter *free = NULL, *found = NULL;
        for (uint8_t i = 0; i < AWAITERS; i++) {
            if (now - awaiters[i].time > TIMEOUT) {
                free = &awaiters[i];
                continue;
            }
            if (awaiters[i].rcvdNonce == receivedNonce && awaiters[i].type == MessageType::RequestNonce) {
                found = &awaiters[i];
                break;
            }
        }

        if (!free) return;

        uint16_t nonceToSend;
        if (found) {
            nonceToSend = found->sentNonce;
        }
        else {
            nonceToSend = getNonce();
            free->rcvdNonce = receivedNonce;
            free->sentNonce = nonceToSend;
            free->time = now;
            free->type = MessageType::RequestNonce;
        }

        msg[0] = MessageType::Nonce;
        msg[3] = msg[1];
        msg[4] = msg[2];
        msg[1] = nonceToSend >> 8;
        msg[2] = nonceToSend;
        sendPacket(msg, 5);
    }
    else if (msg[0] == MessageType::Data) {
        uint16_t sentNonce = msg[3] << 8 | msg[4];
        for (uint8_t i = 0; i < AWAITERS; i++) {
            if (now - awaiters[i].time < TIMEOUT &&
                awaiters[i].sentNonce == sentNonce &&
                awaiters[i].type == MessageType::RequestNonce) {
                if (_handler != NULL) {
                    _handler(&msg[5], length - 5);
                }
                msg[0] = MessageType::Ack;
                sendPacket(msg, 3);
                return;
            }
        }
    }
}

bool Sensor::send(void * data, uint8_t length)
{
    if (length > 24) return;
    uint8_t msg[32];

    _nonce = getNonce();
    msg[0] = MessageType::RequestNonce;
    msg[1] = _nonce >> 8;
    msg[2] = _nonce;
    _nonceRequestPending = true;
    _nonceRequestSuccess = false;

    uint8_t retries = RETRIES;
    uint32_t send = millis();
    while (retries) {
        uint32_t now = millis();
        if (now >= send) {
            sendPacket(msg, 3);
            send = now + SEND_INTERVAL;
            retries--;
        }
        update();
        if (_nonceRequestSuccess) break;
    }
    _nonceRequestPending = false;
    if (!_nonceRequestSuccess) return false;

    msg[0] = MessageType::Data;
    msg[3] = _nonce >> 8;
    msg[4] = _nonce;
    _nonce = getNonce();
    msg[1] = _nonce >> 8;
    msg[2] = _nonce;
    memcpy(&msg[5], data, length);

    _ackPending = true;
    _ackSuccess = false;
    retries = RETRIES;
    send = millis();
    while (retries) {
        uint32_t now = millis();
        if (now >= send) {
            sendPacket(msg, length + 5);
            send = now + SEND_INTERVAL;
            retries--;
        }
        update();
        if (_ackSuccess) break;
    }
    _ackPending = false;
    return _ackSuccess;
}

void Sensor::sendPacket(uint8_t *data, uint8_t length) {
    if (length > 29) return;

    uint8_t txBuffer[32];
    txBuffer[2] = length;
    memcpy(&txBuffer[3], data, length);
    length = length <= 13 ? 16 : 32;
    uint16_t chksum = CHKSUM_INIT;
    for (uint8_t i = 2; i < length; i++)
        updateChecksum(&chksum, txBuffer[i]);
    txBuffer[0] = chksum >> 8;
    txBuffer[1] = chksum;
    aes128_cbc_enc(_key, iv, txBuffer, length);
    _radio.stopListening();
    _radio.write(txBuffer, length);
    _radio.startListening();
}

uint16_t Sensor::getNonce() {
    return rand() << 8 | rand();
}

void Sensor::update() {
    while (_radio.available()) {
        uint8_t buffer[32];
        _radio.read(buffer, sizeof(buffer));
        uint8_t length = _radio.getDynamicPayloadSize();
        if (length % 16 != 0) continue;

        aes128_cbc_dec(_key, iv, buffer, length);

        uint16_t chksum = CHKSUM_INIT;
        for (uint8_t i = 2; i < length; i++) updateChecksum(&chksum, buffer[i]);
        uint16_t msgChksum = buffer[0] << 8 | buffer[1];
        if (chksum == msgChksum) {
            onPacketReceived(&buffer[3], buffer[2]);
        }
    }
}

void Sensor::onMessage(DataReceivedHandler handler) {
    _handler = handler;
}
