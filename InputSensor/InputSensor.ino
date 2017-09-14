#include "SensorLib.h"

#define PIN_INPUT   2

uint8_t key[] = { 0xae, 0xbb, 0xf1, 0xa9, 0xbe, 0x6d, 0x9c, 0x24, 0xbc, 0xdc, 0x59, 0x3a, 0x55, 0x29, 0xa3, 0x8c };
//uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 3, 4);

void setup() {
    sensor.begin();
    pinMode(PIN_INPUT, INPUT_PULLUP);
    attachInterrupt(0, inputStateChanged, CHANGE);
}

void inputStateChanged() {
    _delay_ms(100); //debounce
}

void loop() {
    uint8_t msg[3] = { digitalRead(PIN_INPUT) };
    uint16_t voltage = sensor.readVoltage();
    msg[1] = voltage >> 8;
    msg[2] = voltage;
    sensor.sendAndWait(&msg, 3);
    sensor.powerDown(0);
}
