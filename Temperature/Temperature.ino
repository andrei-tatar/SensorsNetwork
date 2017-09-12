#include "Tcn.h"
#include "SensorLib.h"

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
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
