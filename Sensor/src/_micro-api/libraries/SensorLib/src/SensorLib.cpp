#include "SensorLib.h"

const uint8_t iv[16];

#define ADDRESS_GW      0xAD5AB7192FUL
#define RADIO_CHANNEL   0x54
#define RETRY_COUNT     10
#define RETRY_DELAY     50

#define MSG_DATA        0x01
#define MSG_ACK         0x02
#define MSG_NACK        0x03

Sensor::Sensor(const uint8_t* key, uint8_t cepin, uint8_t cspin) :
    _radio(cepin, cspin), _key(key), _handler(NULL) {
    _oldReceiveNonce = random();
    _nextReceiveNonce = random();
}

bool Sensor::begin() {
    if (!_radio.begin())
        return false;

    _radio.setChannel(RADIO_CHANNEL);
    _radio.setAutoAck(false);
    _radio.enableDynamicPayloads();
    _radio.openWritingPipe(ADDRESS_GW);
    _radio.openReadingPipe(1, _key);
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

void Sensor::onPacketReceived(uint8_t *data, uint8_t length) {
    uint32_t nonce;
    length--;
    switch (*data++) {
    case MSG_ACK:
        nonce = *(uint32_t*)data;
        if (length != 8 || nonce != _nextSendNonce) break;

        _nextSendNonce = *(uint32_t*)&data[4];
        _retries = 0;

        _sendOk = true;
        break;

    case MSG_NACK:
        nonce = *(uint32_t*)data;
        if (length != 8 || nonce != _nextSendNonce) break;

        _nextSendNonce = *(uint32_t*)&data[4];
        sendData();
        break;

    case MSG_DATA:
        nonce = *(uint32_t*)data;
        if (nonce == _oldReceiveNonce) {
            sendResponse(nonce, true);
            break;
        }

        if (nonce != _nextReceiveNonce) {
            sendResponse(nonce, false);
            break;
        }

        data += 4;
        length -= 4;

        _oldReceiveNonce = _nextReceiveNonce;
        do { _nextReceiveNonce = random(); } while (_nextReceiveNonce == _oldReceiveNonce);
        sendResponse(nonce, true);

        if (_handler) _handler(data, length);
        break;
    }
}

bool Sensor::send(void * data, uint8_t length)
{
    if (length > 24) return false;

    memcpy(_send, data, length);
    _sendSize = length;
    _retries = RETRY_COUNT;

    sendData();
    return true;
}

bool Sensor::sendAndWait(void* data, uint8_t length) {
    if (!send(data, length)) return false;

    while (_retries) {
        update();
    }

    return _sendOk;
}

void Sensor::sendPacket(uint8_t *data, uint8_t length) {
    if (length > 29) return;

    uint8_t txBuffer[32];
    txBuffer[2] = length;
    memcpy(&txBuffer[3], data, length);
    uint16_t chksum = CHKSUM_INIT;
    for (uint8_t i = 0; i < length + 1; i++)
        updateChecksum(&chksum, txBuffer[2 + i]);
    txBuffer[0] = chksum;
    txBuffer[1] = chksum >> 8;
    
    length = length <= 13 ? 16 : 32;
    aes128_cbc_enc(_key, iv, txBuffer, length);
    _radio.stopListening();
    _radio.write(txBuffer, length);
    _radio.startListening();
}

void Sensor::update() {
    while (_radio.available()) {
        uint8_t buffer[32];
        _radio.read(buffer, sizeof(buffer));
        uint8_t length = _radio.getDynamicPayloadSize();
        if (length % 16 != 0) continue;

        aes128_cbc_dec(_key, iv, buffer, length);
        uint8_t dataLength = buffer[2];
        if (dataLength > 29) continue;

        uint16_t chksum = CHKSUM_INIT;
        for (uint8_t i = 0; i < dataLength + 1; i++) updateChecksum(&chksum, buffer[i+2]);
        uint16_t msgChksum = buffer[0] | buffer[1] << 8;
        if (chksum == msgChksum) {
            onPacketReceived(&buffer[3], dataLength);
        }
    }

    uint32_t now;
    if (_retries && (now = millis()) - _lastSendTime >= RETRY_DELAY) {
        _retries--;
        if (_retries == 0) {
            _sendOk = false;
        }
        else {
            sendData();
        }
    }
}

void Sensor::sendResponse(uint32_t nonce, bool ack) {
    uint8_t data[9];
    data[0] = ack ? MSG_ACK : MSG_NACK;
    *((uint32_t*)&data[1]) = nonce;
    *((uint32_t*)&data[5]) = _nextReceiveNonce;
    sendPacket(data, 9);
}

void Sensor::sendData() {
    uint8_t data[29];
    data[0] = MSG_DATA;
    *((uint32_t*)&data[1]) = _nextSendNonce;
    memcpy(&data[5], _send, _sendSize);
    sendPacket(data, _sendSize + 5);
    _lastSendTime = millis();
}

void Sensor::onMessage(DataReceivedHandler handler) {
    _handler = handler;
}

void Sensor::wake() {
    _seconds = 0;
}

void Sensor::powerDown(uint16_t seconds) {
    _seconds = seconds;
    _radio.stopListening();
    _radio.powerDown();

    if (_seconds == 0) {
        LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    }

    while (_seconds > 8) {
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        _seconds -= 8;
    }

    while (_seconds > 4) {
        LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
        _seconds -= 4;
    }

    while (_seconds > 2) {
        LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
        _seconds -= 2;
    }

    while (_seconds > 0) {
        LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_ON);
        _seconds -= 1;
    }

    _radio.powerUp();
    _radio.openWritingPipe(ADDRESS_GW);
    _radio.startListening();
}

uint16_t Sensor::readVoltage() {
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC));
    return 1126400 / ADC;
}