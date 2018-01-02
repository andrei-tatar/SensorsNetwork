#pragma once

typedef struct {
    uint8_t pinTouch;
    uint8_t pinLed;
    uint8_t pinRelaySet;
    uint8_t pinRelayReset;
    uint8_t state;
    uint8_t touchState;
} Channel;
