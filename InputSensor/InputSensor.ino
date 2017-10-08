#include "SensorLib.h"
#include "Tcn.h"

#define PIN_INPUT       2
#define PIN_INTERRUPT   0
#define PIN_LIGHT_POWER 5
#define PIN_LIGHT       A3

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 3, 4);
Tcn temp;

void setup() {
    sensor.begin();
    pinMode(PIN_INPUT, INPUT);
    attachInterrupt(PIN_INTERRUPT, inputStateChanged, CHANGE);
    pinMode(PIN_LIGHT_POWER, OUTPUT);
    digitalWrite(PIN_LIGHT_POWER, LOW);
    temp.begin();
}

void inputStateChanged() {
    sensor.wake();
    _delay_ms(100); //debounce
}

void loop() {
    uint8_t msg[10] = { 'B', 0, 'T', 0, 0, 'L', 0, 0, 'S', digitalRead(PIN_INPUT) };
    uint16_t voltage = sensor.readVoltage();
    msg[1] = voltage / 10 - 100;

    uint16_t temperature = temp.read();
    msg[3] = temperature >> 8;
    msg[4] = temperature;

    digitalWrite(PIN_LIGHT_POWER, HIGH);
    uint16_t light = analogRead(PIN_LIGHT);
    digitalWrite(PIN_LIGHT_POWER, LOW);
    uint16_t resistor = 100UL * 1023 / light - 100; //light resistor = 10kohm*(1023/read_value-1) kohm
    msg[6] = resistor >> 8;
    msg[7] = resistor;

    sensor.sendAndWait(msg, sizeof(msg));
    sensor.powerDown(600);
}
