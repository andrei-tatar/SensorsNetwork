#include "SensorLib.h"

#define PIN_INPUT       2
#define PIN_INTERRUPT   0

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 3, 4);

void setup() {
    sensor.begin();
    pinMode(PIN_INPUT, INPUT);
    attachInterrupt(PIN_INTERRUPT, inputStateChanged, CHANGE);
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
