// Tcn.h

#ifndef _TCN_h
#define _TCN_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <Wire.h>
#include "LowPower.h"

class Tcn {
public:
    void begin();
    uint16_t read();
};

#endif

