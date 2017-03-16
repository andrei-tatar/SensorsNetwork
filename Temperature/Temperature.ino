#include "SensorsLib.h"

#define BRIDGE_ADDRESS  0xe0e8f0f0e0ULL
#define SENSOR_ADDRESS  0xe8e8f0f0e1ULL
#define COMM_CHANNEL    75

Sensor sensor(BRIDGE_ADDRESS, SENSOR_ADDRESS, COMM_CHANNEL);

void setup() {
    Serial.begin(57600);
    Serial.println(F("Init"));
    if (!sensor.begin()) {
        Serial.println(F("Could not init sensor!"));
    }
    else {
        Serial.println(F("Sensor inited"));
    }
}

void loop() {
    sensor.update();
}
