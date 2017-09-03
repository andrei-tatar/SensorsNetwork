#include "SensorLib.h"

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 9, 10);

#define PIN_TOUCH   3
#define PIN_LED     5
#define PIN_TRIAC   6
#define PIN_ZERO    2

#define MAX_BRIGHT  90
#define MIN_BRIGHT  20

#define CMD_SET     1
#define CMD_GET     2

static volatile uint16_t timerDelay = 0xFFFF;
static volatile uint8_t brightness;
static volatile bool next = false;

void updateLed() {
    digitalWrite(PIN_LED, brightness >= MIN_BRIGHT ? HIGH : LOW);
}

void onData(const uint8_t* data, uint8_t length) {
    if (data[0] == CMD_SET && length == 2) {
        if (data[1] > 100) brightness = 100; else brightness = data[1];
        updateLed();
    }
    if (data[0] == CMD_GET && length == 1) {
        uint8_t send = brightness;
        sensor.send(&send, 1);
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
}

ISR(TIMER1_COMPA_vect) {
    if (next) {
        TCNT1 = 0;
        OCR1A = 10000;
        next = false;
    }

    PORTD |= 1 << PIN_TRIAC;
    _delay_us(10);
    PORTD &= ~(1 << PIN_TRIAC);
}

//peak cross
ISR(INT0_vect) {
    TCNT1 = 0;
    OCR1A = timerDelay;
    next = true;
}

static const uint8_t powerToTicks[] PROGMEM = {
    100, 97, 95, 93, 90, 89, 88, 86, 85, 84, 83, 82, 81, 80, 79, 78, 78, 77, 76, 76, 75, 
    73, 73, 72, 71, 71, 70, 69, 69, 69, 68, 67, 67, 66, 65, 65, 64, 63, 63, 63, 62, 61, 61, 
    60, 60, 59, 59, 58, 57, 56, 56, 56, 55, 54, 54, 53, 52, 52, 52, 51, 50, 50, 49, 48, 48, 
    48, 47, 46, 46, 45, 44, 44, 43, 42, 41, 41, 40, 39, 39, 38, 37, 37, 36, 35, 34, 33, 33, 
    31, 31, 29, 29, 27, 26, 24, 23, 21, 19, 16, 13, 0 };

void handleRamp(uint32_t now) {
    static uint32_t nextChange;
    if (now >= nextChange) {
        nextChange = now + 20;

        static uint8_t current = 0;
        if (current != brightness) {
            if (current < brightness) current++;
            else if (current > brightness) current--;

            uint16_t nextDelay;
            if (current < MIN_BRIGHT)
                nextDelay = 0xFFFF;
            else if (current >= MAX_BRIGHT)
                nextDelay = pgm_read_byte(&powerToTicks[MAX_BRIGHT]) * 100;
            else
                nextDelay = pgm_read_byte(&powerToTicks[current]) * 100;

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
        if (touchState) {
            nextManualChange = now + 300;
            levelChanged = false;
            if (!brightness) increaseLevel = true;
        }
        else {
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
            updateLed();
        }
        lastTouchState = touchState;
    }
    else if (touchState) {
        if (now >= nextManualChange) {
            levelChanged = true;
            nextManualChange = now + 30;
            analogWrite(PIN_LED, 128);
            if (increaseLevel) {
                if (brightness < 100) brightness++;
            }
            else {
                if (brightness > MIN_BRIGHT) brightness--;
            }
        }
        return true;
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
            sensor.send(&lastSentBrightness, 1);
        }
    }

    sensor.update();
}
