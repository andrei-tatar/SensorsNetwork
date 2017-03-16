#include "SensorLib.h"

uint8_t hostAddr[] = { 0xE0, 0xE8, 0xF0, 0xF0, 0xE0 };
uint8_t myAddr[] = { 0xE8, 0xE8, 0xF0, 0xF0, 0xE1 };
uint8_t key[] = { 0xa9, 0x79, 0x14, 0x3a, 0x44, 0xc3, 0xe9, 0x35, 0x79, 0xe2, 0x48, 0x6d, 0xba, 0xe0, 0xa4, 0x16 };
uint8_t iv[] = { 0x16, 0x55, 0xce, 0xd9, 0x5b, 0x81, 0x0b, 0x79, 0x39, 0x2d, 0x78, 0x6c, 0x3d, 0x25, 0x4c, 0xfe };
uint8_t channel = 75;

Sensor sensor(hostAddr, myAddr, channel, key, iv);

void setup() {
    Serial.begin(57600);
    sensor.begin();
}

void loop() {
    static uint32_t send;

    if (millis() > send) {
        send = millis() + 2000;
        uint32_t start = millis();
        bool success = sensor.send("test", 4);
        uint32_t time = millis() - start;
        Serial.print(success ? "sent - " : "not sent - ");
        Serial.println(time);
    }

    sensor.update();
}
