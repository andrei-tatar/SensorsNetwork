#include "SensorLib.h"

#define GW_ADDR   0xE0E8F0F0E0ULL
#define ADDR      0xE8E8F0F0E1ULL
#define CHANNEL   75
uint8_t key[] = { 0xa9, 0x79, 0x14, 0x3a, 0x44, 0xc3, 0xe9, 0x35, 0x79, 0xe2, 0x48, 0x6d, 0xba, 0xe0, 0xa4, 0x16 };

Sensor sensor(GW_ADDR, ADDR, CHANNEL, key);

void setup() {
    Serial.begin(57600);
    sensor.begin();
    sensor.onMessage(messageReceived);
}

void loop() {
    static uint32_t send;

    if (millis() > send) {
        send = millis() + 1000;
        static uint32_t value = 0;
        uint32_t start = millis();
        bool success = sensor.send(&value, 4);
        if (success) value++;
        uint32_t time = millis() - start;
        Serial.print(success ? "sent - " : "not sent - ");
        Serial.println(time);
    }

    sensor.update();
}

void messageReceived(uint8_t *data, uint8_t length) {
    Serial.print("Received: ");
    while (length--) {
        Serial.print(*data++, HEX);
        Serial.print(':');
    }
    Serial.println();
}

