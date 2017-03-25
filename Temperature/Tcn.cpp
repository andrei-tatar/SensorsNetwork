// 
// 
// 

#include "Tcn.h"

#define TCN_ADDR    0x48

void Tcn::begin() {
    Wire.begin();
    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x01); //config register
    Wire.write(0x61); //12 bit resolution / shutdown
    Wire.endTransmission();
}

uint16_t Tcn::read() {
    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x01); //config
    Wire.write(0xE1); //one shot / shutdown
    Wire.endTransmission();

    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x00); //select the temperature register
    Wire.endTransmission();
    Wire.requestFrom(TCN_ADDR, 2); //read the temp
    uint16_t result = Wire.read() << 8;
    result |= Wire.read();
    return result >> 4;
}