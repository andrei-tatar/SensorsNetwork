#include <RF24.h>
#include <nRF24L01.h>

RF24 radio(2, 3);
static bool radioConfigured = false;

void sendPacket(const uint8_t *data, uint8_t size);

#define FRAME_CONFIGURE     0x90
#define FRAME_CONFIGURED    0x91
#define FRAME_SENDPACKET    0x92
#define FRAME_RECEIVEPACKET 0x93

void setup() {
    Serial.begin(250000);
}

void loop() {
    updatePacket();
    while (radioConfigured && radio.available()) {
        static uint8_t rxBuffer[33];
        uint8_t length = radio.getDynamicPayloadSize();
        radio.read(&rxBuffer[1], length);
        rxBuffer[0] = FRAME_RECEIVEPACKET;
        sendPacket(rxBuffer, length + 1);
    }
}

void onPacketReceived(uint8_t *data, uint8_t size) {
    switch (*data)
    {
    case FRAME_CONFIGURE:
        if (!radio.begin()) return;
        radio.setChannel(data[6]);
        radio.setAutoAck(false);
        radio.enableDynamicPayloads();
        radio.openReadingPipe(1, &data[1]);
        radio.startListening();
        radioConfigured = true;

        data[0] = FRAME_CONFIGURED;
        sendPacket(data, 1);
        break;

    case FRAME_SENDPACKET:
        radio.openWritingPipe(&data[1]);
        radio.stopListening();
        radio.write(&data[6], size - 6);
        radio.startListening();
        break;

    default:
        sendPacket(data, size);
        break;
    }
}

#define FRAME_HEADER_1  0xDE
#define FRAME_HEADER_2  0x5B
#define CHKSUM_INIT     0x1021

void sendPacket(const uint8_t *data, uint8_t size)
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

#define RX_STATUS_IDLE  0
#define RX_STATUS_HDR   1
#define RX_STATUS_SIZE  2
#define RX_STATUS_DATA  3
#define RX_STATUS_CHK1  4
#define RX_STATUS_CHK2  5

void updatePacket() {
    static uint8_t status = RX_STATUS_IDLE;
    static uint8_t rxBuffer[64];
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
            if (chksum == rxChksum) onPacketReceived(rxBuffer, rxSize);
            status = RX_STATUS_IDLE;
            break;

        default:
            break;

        }
    }
}
