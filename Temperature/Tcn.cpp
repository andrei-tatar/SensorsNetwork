#include "Tcn.h"

#define TCN_ADDR    0x48

void Tcn::begin() {
    Wire.begin();
    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x01); //config register
    Wire.write(0x01); //9 bit resolution / shutdown
    Wire.endTransmission();
    read(); //start a conversion
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
}

uint16_t Tcn::read() {
    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x00); //select the temperature register
    Wire.endTransmission();
    Wire.requestFrom(TCN_ADDR, 2); //read the temp
    uint16_t result = Wire.read() << 8;
    result |= Wire.read();
    
    //do a one shot for the next read
    Wire.beginTransmission(TCN_ADDR);
    Wire.write(0x01); //config
    Wire.write(0x81); //one shot / 9bit / shutdown
    Wire.endTransmission();
    
    return result;
}