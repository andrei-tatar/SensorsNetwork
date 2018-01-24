#include <RF24.h>
#include <AESLib.h>

#define MAX_SENSORS     10
#define ADDRESS_GW      0xAD5AB7192FUL
#define ADDRESS_NODE    0x164F19B4E6UL
#define RADIO_CHANNEL   0x54
#define RETRY_COUNT     10
#define RETRY_DELAY     50

#define LIGHT_ON        100
#define PIN_RX_LED      6
#define PIN_TX_LED      7

typedef struct {
    uint32_t nextSendNonce;
    uint32_t oldReceiveNonce;
    uint32_t nextReceiveNonce;
    uint8_t send[32];
    uint8_t sendSize;
    uint8_t key[16];
    uint32_t lastSendTime;
    uint8_t retries;
} Sensor;

Sensor sensors[MAX_SENSORS];
uint8_t sensorsCount = 0;
const uint8_t iv[16];

uint32_t turnOffRx, turnOffTx;

RF24 radio(2, 3);
void sendSerialPacket(const uint8_t *data, uint8_t size);
void sendData(Sensor* sensor);
void processReceivedPacket(Sensor *sensor, uint8_t sensorId, const uint8_t *data, uint8_t length);
void sendResponse(Sensor* sensor, uint32_t nonce, bool ack);
void sendPacket(Sensor *sensor, const uint8_t *data, uint8_t length);

#define FRAME_CONFIGURE             0x90
#define FRAME_CONFIGURED            0x91
#define FRAME_SENDPACKET            0x92
#define FRAME_RECEIVEPACKET         0x93
#define FRAME_PACKETSENT            0x94

#define FRAME_ERR_BUSY              0x70
#define FRAME_ERR_INVALID_SIZE      0x71
#define FRAME_ERR_INVALID_ID        0x72
#define FRAME_ERR_TIMEOUT           0x73

#define MSG_DATA                    0x01
#define MSG_ACK                     0x02
#define MSG_NACK                    0x03

void setup() {
    Serial.begin(115200);
    pinMode(PIN_RX_LED, OUTPUT);
    pinMode(PIN_TX_LED, OUTPUT);
}

void ledRx() {
    turnOffRx = millis() + LIGHT_ON;
    digitalWrite(PIN_RX_LED, HIGH);
}

void ledTx() {
    turnOffTx = millis() + LIGHT_ON;
    digitalWrite(PIN_TX_LED, HIGH);
}

void loop() {
    updateSerialPacket();

    uint32_t now = millis();
    while (sensorsCount && radio.available()) {
        static uint8_t rxBuffer[34];
        uint8_t length = radio.getDynamicPayloadSize();
        radio.read(&rxBuffer[2], length);
        if (length % 16 != 0) continue;

        for (uint8_t sensorId = 0; sensorId < sensorsCount; sensorId++) {
            uint8_t decrypted[32];
            memcpy(decrypted, &rxBuffer[2], length);

            Sensor* sensor = &sensors[sensorId];
            aes128_cbc_dec(sensor->key, iv, decrypted, length);

            uint8_t msgLength = decrypted[2];
            if (msgLength > 29) continue;

            uint16_t chksum = getChecksum(&decrypted[2], msgLength + 1);
            uint16_t msgChksum = decrypted[0] | (decrypted[1] << 8);
            if (msgChksum != chksum) continue;
            ledRx();
            processReceivedPacket(sensor, sensorId, &decrypted[3], msgLength);
            break;
        }
    }

    for (uint8_t sensorId = 0; sensorId < sensorsCount; sensorId++) {
        Sensor* sensor = &sensors[sensorId];
        if (sensor->retries && (now - sensor->lastSendTime) >= RETRY_DELAY) {
            sensor->retries--;
            if (sensor->retries == 0) {
                uint8_t aux[2] = { FRAME_ERR_TIMEOUT, sensorId };
                sendSerialPacket(aux, 2);
            }
            else {
                sendData(sensor);
            }
        }
    }

    if (now > turnOffRx) digitalWrite(PIN_RX_LED, LOW);
    if (now > turnOffTx) digitalWrite(PIN_TX_LED, LOW);
}

void processReceivedPacket(Sensor *sensor, uint8_t sensorId, const uint8_t *data, uint8_t length) {
    uint8_t serialPacket[34];
    uint32_t nonce;

    length--;
    switch (*data++) {
    case MSG_ACK:
        nonce = *(uint32_t*)data;
        if (length != 8 || nonce != sensor->nextSendNonce) break;

        sensor->nextSendNonce = *(uint32_t*)&data[4];
        sensor->retries = 0;

        serialPacket[0] = FRAME_PACKETSENT;
        serialPacket[1] = sensorId;
        sendSerialPacket(serialPacket, 2);
        break;

    case MSG_NACK:
        nonce = *(uint32_t*)data;
        if (length != 8 || nonce != sensor->nextSendNonce) break;

        sensor->nextSendNonce = *(uint32_t*)&data[4];
        sendData(sensor);
        break;

    case MSG_DATA:
        nonce = *(uint32_t*)data;
        length -= 4;
        if (nonce == sensor->oldReceiveNonce) {
            sendResponse(sensor, nonce, true);
            break;
        }

        if (nonce != sensor->nextReceiveNonce) {
            sendResponse(sensor, nonce, false);
            break;
        }

        sensor->oldReceiveNonce = sensor->nextReceiveNonce;
        do { sensor->nextReceiveNonce = random(); } while (sensor->nextReceiveNonce == sensor->oldReceiveNonce);
        sendResponse(sensor, nonce, true);

        serialPacket[0] = FRAME_RECEIVEPACKET;
        serialPacket[1] = sensorId;
        memcpy(&serialPacket[2], &data[4], length);
        sendSerialPacket(serialPacket, length + 2);
        break;
    }
}

void sendPacket(Sensor *sensor, const uint8_t *data, uint8_t length) {
    uint8_t msg[32];
    msg[2] = length;
    memcpy(&msg[3], data, length);
    uint16_t chksum = getChecksum(&msg[2], length + 1);
    msg[0] = chksum;
    msg[1] = chksum >> 8;

    length = length + 3 <= 16 ? 16 : 32;
    aes128_cbc_enc(sensor->key, iv, msg, length);

    radio.openWritingPipe(ADDRESS_NODE);
    radio.stopListening();
    radio.write(msg, length);
    radio.startListening();
    ledTx();
}

void sendResponse(Sensor* sensor, uint32_t nonce, bool ack) {
    uint8_t data[9];
    data[0] = ack ? MSG_ACK : MSG_NACK;
    *((uint32_t*)&data[1]) = nonce;
    *((uint32_t*)&data[5]) = sensor->nextReceiveNonce;
    sendPacket(sensor, data, 9);
}

void sendData(Sensor* sensor) {
    uint8_t data[29];
    data[0] = MSG_DATA;
    *((uint32_t*)&data[1]) = sensor->nextSendNonce;
    memcpy(&data[5], sensor->send, sensor->sendSize);
    sendPacket(sensor, data, sensor->sendSize + 5);
    sensor->lastSendTime = millis();
}

void onSerialPacketReceived(uint8_t *data, uint8_t size) {
    switch (*data)
    {
    case FRAME_CONFIGURE:
        if (data[1] > MAX_SENSORS || !radio.begin()) break;
        sensorsCount = data[1];
        if (sensorsCount * 16 + 2 != size) break;

        radio.setPALevel(RF24_PA_MAX);
        radio.setDataRate(RF24_250KBPS);
        radio.setChannel(RADIO_CHANNEL);
        radio.setAutoAck(false);
        radio.enableDynamicPayloads();
        radio.openReadingPipe(1, ADDRESS_GW);
        radio.startListening();

        for (uint8_t sensorId = 0; sensorId < sensorsCount; sensorId++) {
            Sensor *sensor = &sensors[sensorId];
            const uint8_t *key = &data[2 + sensorId * 16];
            memcpy(sensor->key, key, 16);

            sensor->retries = 0;
            sensor->oldReceiveNonce = random();
            sensor->nextReceiveNonce = random();
        }

        data[0] = FRAME_CONFIGURED;
        sendSerialPacket(data, 1);
        break;

    case FRAME_SENDPACKET:
        if (!sensorsCount) break;
        uint8_t sensorId;
        sensorId = data[1];
        size -= 2;
        if (size > 24) {
            data[0] = FRAME_ERR_INVALID_SIZE;
            sendSerialPacket(data, 2);
            break;
        }
        if (sensorId >= sensorsCount) {
            data[0] = FRAME_ERR_INVALID_ID;
            sendSerialPacket(data, 2);
            break;
        }

        Sensor *sensor;
        sensor = &sensors[sensorId];
        if (sensor->retries) {
            data[0] = FRAME_ERR_BUSY;
            sendSerialPacket(data, 2);
            break;
        }

        sensor->sendSize = size;
        memcpy(sensor->send, &data[2], size);
        sensor->retries = RETRY_COUNT;
        sendData(sensor);
        break;

    default:
        sendSerialPacket(data, size);
        break;
    }
}

#define FRAME_HEADER_1  0xDE
#define FRAME_HEADER_2  0x5B
#define CHKSUM_INIT     0x1021

void sendSerialPacket(const uint8_t *data, uint8_t size)
{
    Serial.write(FRAME_HEADER_1);
    Serial.write(FRAME_HEADER_2);
    uint16_t checksum = CHKSUM_INIT;
    Serial.write(size);
    updateChecksum(&checksum, size);
    while (size--) {
        Serial.write(*data);
        updateChecksum(&checksum, *data++);
    }
    Serial.write(checksum >> 8);
    Serial.write(checksum & 0xFF);
}

void updateChecksum(uint16_t *checksum, uint8_t data) {
    bool roll = *checksum & 0x8000 ? true : false;
    *checksum <<= 1;
    if (roll) *checksum |= 1;
    *checksum ^= data;
}

static uint16_t getChecksum(uint8_t *data, uint8_t length) {
    uint16_t checksum = CHKSUM_INIT;
    while (length--) {
        updateChecksum(&checksum, *data++);
    }
    return checksum;
}

#define RX_STATUS_IDLE  0
#define RX_STATUS_HDR   1
#define RX_STATUS_SIZE  2
#define RX_STATUS_DATA  3
#define RX_STATUS_CHK1  4
#define RX_STATUS_CHK2  5

void updateSerialPacket() {
    static uint8_t status = RX_STATUS_IDLE;
    static uint8_t rxBuffer[200];
    static uint8_t rxSize, rxOffset;
    static uint16_t chksum, rxChksum;
    int data;
    while ((data = Serial.read()) != -1) {
        switch (status)
        {
        case RX_STATUS_IDLE:
            if (data == FRAME_HEADER_1) status = RX_STATUS_HDR;
            break;
        case RX_STATUS_HDR:
            status = data == FRAME_HEADER_2 ? RX_STATUS_SIZE : RX_STATUS_IDLE;
            break;
        case RX_STATUS_SIZE:
            rxSize = data;
            status = rxSize > sizeof(rxBuffer) ? RX_STATUS_IDLE : RX_STATUS_DATA;
            chksum = CHKSUM_INIT;
            rxOffset = 0;
            updateChecksum(&chksum, rxSize);
            break;
        case RX_STATUS_DATA:
            updateChecksum(&chksum, data);
            rxBuffer[rxOffset++] = data;
            if (rxOffset == rxSize) status = RX_STATUS_CHK1;
            break;
        case RX_STATUS_CHK1:
            rxChksum = data << 8;
            status = RX_STATUS_CHK2;
            break;
        case RX_STATUS_CHK2:
            rxChksum |= data;
            if (chksum == rxChksum) onSerialPacketReceived(rxBuffer, rxSize);
            status = RX_STATUS_IDLE;
            break;

        default:
            break;

        }
    }
}
