#include <Adafruit_BME280.h>
#include "SensorLib.h"
#include "MAX44009.h"

#define PIN_INPUT       2
#define PIN_INTERRUPT   0
#define PIN_LIGHT_POWER 5
#define PIN_LIGHT       A3

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 3, 4);
Adafruit_BME280 bme;
MAX44009 max44009;

void setup() {
    if (!bme.begin(0x76) || !max44009.initialize()) {
        while (1) {
            sensor.powerDown(10000);
        }
    }

    bme.setSampling(
        Adafruit_BME280::sensor_mode::MODE_NORMAL, 
        Adafruit_BME280::sensor_sampling::SAMPLING_X16, 
        Adafruit_BME280::sensor_sampling::SAMPLING_X16, 
        Adafruit_BME280::sensor_sampling::SAMPLING_X16, 
        Adafruit_BME280::sensor_filter::FILTER_X4, 
        Adafruit_BME280::standby_duration::STANDBY_MS_1000);

    sensor.begin();
    pinMode(PIN_INPUT, INPUT);
    attachInterrupt(PIN_INTERRUPT, inputStateChanged, CHANGE);
    pinMode(PIN_LIGHT_POWER, OUTPUT);
    digitalWrite(PIN_LIGHT_POWER, LOW);
}

void inputStateChanged() {
    sensor.wake();
    _delay_ms(100); //debounce
}

void loop() {
    uint8_t msg[16] = { 'B', 0, 'T', 0, 0, 'L', 0, 0, 'S', 0, 'H', 0, 0, 'P', 0, 0 };
    uint16_t voltage = sensor.readVoltage();
    msg[1] = voltage / 10 - 100;

    uint16_t temperature = bme.readTemperature() * 256;
    msg[3] = temperature >> 8;
    msg[4] = temperature;

    uint16_t humidity = bme.readHumidity() * 100;
    msg[11] = humidity >> 8;
    msg[12] = humidity;

    uint16_t pressure = bme.readPressure() / 10;
    msg[14] = pressure >> 8;
    msg[15] = pressure;

    uint16_t light = max44009.getMeasurement();
    msg[6] = light >> 8;
    msg[7] = light;

    uint8_t state = digitalRead(PIN_INPUT);
    msg[9] = state;

    sensor.sendAndWait(msg, sizeof(msg));
    sensor.powerDown(state ? 10 : 300);
}
