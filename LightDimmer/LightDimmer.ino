#include "SensorLib.h"

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 9, 10);

#define PIN_TOUCH   3
#define PIN_LED     5
#define PIN_TRIAC   6
#define PIN_ZERO    2

#define CMD_SET         1
#define CMD_GET         2
#define CMD_SET_MODE    3

#define MODE_MAN_DIMMER     0x01    //enable manual dimming
#define MODE_DISABLE_MAN    0x02    //disable manual control

#define RSP_INIT    1
#define RSP_STATE   2

static volatile uint16_t timerDelay = 0xFFFF;
static volatile uint8_t brightness;
static volatile bool next = false;
static volatile uint8_t mode = 0;
static volatile uint8_t modeNoDimmerBrightness = 100;

void updateLed() {
    digitalWrite(PIN_LED, brightness ? HIGH : LOW);
}

void sendState() {
    uint8_t send[2] = { RSP_STATE, brightness };
    sensor.send(&send, 2);
}

void onData(const uint8_t* data, uint8_t length) {
    if (data[0] == CMD_SET && length == 2) {
        if (data[1] > 100) brightness = 100; else brightness = data[1];
        updateLed();
    }
    if (data[0] == CMD_GET && length == 1) {
        sendState();
    }
    if (data[0] == CMD_SET_MODE && length == 3) {
        mode = data[1];
        modeNoDimmerBrightness = data[2];
    }
}

void setup() {
    sensor.begin();
    sensor.onMessage(onData);

    pinMode(PIN_TOUCH, INPUT);
    pinMode(PIN_ZERO, INPUT);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_TRIAC, OUTPUT);

    EICRA = (1 << ISC01) | (1 << ISC00); //rising edge 
    EIMSK = (1 << INT0);

    OCR1A = 0xFFFF; //32.767 msec
    TCNT1 = 0;
    TIMSK1 = 1 << OCIE1A; //enable COMPA interrupt
    TIFR1 = 1 << OCF1A; //clear any flag that may be there
    TCCR1A = 0;
    TCCR1B = 1 << CS11; //(start 8 prescaler)

    sei();

    uint8_t announce = RSP_INIT;
    sensor.send(&announce, 1);
}

ISR(TIMER1_COMPA_vect) {
    if (next) {
        TCNT1 = 0;
        OCR1A = 10000;
        next = false;
    }

    PORTD |= 1 << PIN_TRIAC;
    _delay_us(50);
    PORTD &= ~(1 << PIN_TRIAC);
}

//peak cross
ISR(INT0_vect) {
    TCNT1 = 0;
    OCR1A = timerDelay;
    next = true;
}

static const uint8_t powerToTicks[100] PROGMEM = { 
    75, 72, 71, 70, 69, 68, 68, 67, 66, 66, 65, 65, 64, 64, 63, 63, 62, 62, 62, 61,
    61, 60, 60, 60, 59, 59, 58, 58, 58, 57, 57, 57, 56, 56, 56, 55, 55, 55, 54, 54, 
    54, 53, 53, 53, 52, 52, 52, 51, 51, 51, 50, 50, 50, 49, 49, 49, 48, 48, 48, 47, 
    47, 47, 46, 46, 46, 45, 45, 45, 44, 44, 44, 43, 43, 43, 42, 42, 41, 41, 41, 40, 
    40, 39, 39, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 33, 32, 31, 30, 29 
};

void handleRamp(uint32_t now) {
    static uint32_t nextChange;
    if (now >= nextChange) {
        nextChange = now + 15;

        static uint8_t current = 0;
        if (current != brightness) {
            if (current < brightness) current++;
            else if (current > brightness) current--;

            uint16_t nextDelay = current 
                ? pgm_read_byte(&powerToTicks[current - 1]) * 100 : 0xFFFF;

            if (nextDelay != timerDelay) {
                cli();
                timerDelay = nextDelay;
                sei();
            }
        }
    }
}

bool handleTouchEvents(uint32_t now) {
    static uint8_t lastTouchState = LOW;
    uint8_t touchState = digitalRead(PIN_TOUCH);
    static uint32_t lastChange = 0;
    static bool increaseLevel = true;
    static uint32_t nextManualChange;
    static bool levelChanged;
    static uint8_t lastBrightness = 100;
    if (touchState != lastTouchState) {
        lastTouchState = touchState;
        if (mode & MODE_DISABLE_MAN) {
            if (touchState)
                analogWrite(PIN_LED, 128);
            else
                updateLed();
            return false;
        }

        if (touchState) {
            if (mode & MODE_MAN_DIMMER) {
                nextManualChange = now + 300;
                levelChanged = false;
                if (!brightness) increaseLevel = true;
            }
            else {
                if (brightness != modeNoDimmerBrightness) {
                    brightness = modeNoDimmerBrightness;
                }
                else {
                    brightness = 0;
                }
                updateLed();
            }
        }
        else {
            if (mode & MODE_MAN_DIMMER) {
                if (!levelChanged) {
                    if (brightness) {
                        lastBrightness = brightness;
                        brightness = 0;
                        increaseLevel = true;
                    }
                    else {
                        brightness = lastBrightness;
                        increaseLevel = false;
                    }
                }
                else {
                    increaseLevel = !increaseLevel;
                }
            }
            updateLed();
        }
    }
    else if (touchState) {
        if (mode & MODE_MAN_DIMMER) {
            if (now >= nextManualChange) {
                levelChanged = true;
                nextManualChange = now + 30;
                analogWrite(PIN_LED, 128);
                if (increaseLevel) {
                    if (brightness < 100) brightness++;
                }
                else {
                    if (brightness > 1) brightness--;
                }
            }
            return true;
        }
    }
    return false;
}

void loop() {
    auto now = millis();
    
    handleRamp(now);
    auto touchPresent = handleTouchEvents(now);

    if (!touchPresent) {
        static uint8_t lastSentBrightness;
        if (brightness != lastSentBrightness) {
            lastSentBrightness = brightness;
            sendState();
        }
    }

    sensor.update();
}
