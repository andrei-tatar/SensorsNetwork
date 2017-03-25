#include "Tcn.h"
#include "SensorLib.h"

#define GW_ADDR   0xE0E8F0F0E0ULL
#define ADDR      0xE8E8F0F0E1ULL
#define CHANNEL   75
uint8_t key[] = { 0xa9, 0x79, 0x14, 0x3a, 0x44, 0xc3, 0xe9, 0x35, 0x79, 0xe2, 0x48, 0x6d, 0xba, 0xe0, 0xa4, 0x16 };

Sensor sensor(GW_ADDR, ADDR, CHANNEL, key);
Tcn temp;

void setup() {
    sensor.begin();
    temp.begin();
}

void loop() {
    static uint8_t msg[6];
    static uint8_t sendVoltage = 0;

    uint16_t temperature = temp.read();
    uint8_t size = 3;

    msg[0] = 'T';
    msg[1] = temperature >> 8;
    msg[2] = temperature;

    if (sendVoltage-- == 0) {
        sendVoltage = 12;
        uint16_t voltage = sensor.readVoltage();
        msg[3] = 'V';
        msg[4] = voltage >> 8;
        msg[5] = voltage;
        size += 3;
    }

    sensor.send(msg, size);
    sensor.powerDown(300); //5 min
}
