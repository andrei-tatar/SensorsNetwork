#include "Tcn.h"
#include "SensorLib.h"

uint8_t key[] = { 0xa9, 0x79, 0x14, 0x3a, 0x44, 0xc3, 0xe9, 0x35, 0x79, 0xe2, 0x48, 0x6d, 0xba, 0xe0, 0xa4, 0x16 };
Sensor sensor(key);
Tcn temp;

void setup() {
    temp.begin();
    sensor.begin();
}

void loop() {
    static uint8_t msg[6] = { 'T', 0, 0, 'V', 0, 0 };
    static uint8_t readVoltage = 0;

    uint16_t temperature = temp.read();

    msg[1] = temperature >> 8;
    msg[2] = temperature;

    if (readVoltage-- == 0) {
        readVoltage = 11;
        uint16_t voltage = sensor.readVoltage();
        msg[4] = voltage >> 8;
        msg[5] = voltage;
    }
    sensor.sendAndWait(msg, sizeof(msg));
    sensor.powerDown(300); //5 min
}
