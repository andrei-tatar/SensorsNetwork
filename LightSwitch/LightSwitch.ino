#include "SensorLib.h"
#include "LightSwitch.h"

uint8_t key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
Sensor sensor(key, 9, 10);

#define RELAY_DELAY         50

#define CMD_SET             1
#define CMD_GET             2
#define CMD_SET_MODE        3

#define RSP_INIT            1
#define RSP_STATE           2

#define MODE_DISABLE_MAN    0x01    //disable manual control

#define CHANNELS            2
static volatile Channel channels[CHANNELS] = {
    { 3, 5, A1, A0, 2 },
    { 4, 6, A3, A2, 2 }
};

static volatile uint8_t mode = 0;

void setup()
{
    sensor.begin();
    sensor.onMessage(onData);
    sensor.powerDown(5);

    for (uint8_t i = 0; i < CHANNELS; i++) {
        auto channel = &channels[i];
        pinMode(channel->pinTouch, INPUT);
        pinMode(channel->pinLed, OUTPUT);
        pinMode(channel->pinRelaySet, OUTPUT);
        pinMode(channel->pinRelayReset, OUTPUT);
        updateRelay(channel, 0);
    }

    uint8_t announce = RSP_INIT;
    sensor.send(&announce, 1);
}

void loop()
{
    bool stateChanged = false;
    for (uint8_t i = 0; i < CHANNELS; i++) {
        auto channel = &channels[i];
        uint32_t now = millis();

        uint8_t touchState = digitalRead(channel->pinTouch);
        if (touchState != channel->touchState) {
            channel->touchState = touchState;
            if (touchState) {
                if (mode & MODE_DISABLE_MAN) continue;
                toggleChannel(channel);
                stateChanged = true;
            }
        }
    }

    if (stateChanged) {
        sendState();
    }

    sensor.update();
}

void onData(const uint8_t* data, uint8_t length) {
    if (data[0] == CMD_SET && length == 2) {
        uint8_t state = data[1];
        for (uint8_t i = 0; i < CHANNELS; i++) {
            auto channel = &channels[i];
            updateRelay(channel, (state & (1 << i)) ? 1 : 0);
        }
    }
    if (data[0] == CMD_GET && length == 1) {
        sendState();
    }
    if (data[0] == CMD_SET_MODE && length == 2) {
        mode = data[1];
    }
}

void toggleChannel(volatile Channel* channel) {
    updateRelay(channel, channel->state ^ 1);
}

void updateRelay(volatile Channel *channel, uint8_t newState) {
    if (newState == channel->state) return;
    channel->state = newState;
    uint8_t pin = channel->state ? channel->pinRelaySet : channel->pinRelayReset;
    digitalWrite(channel->pinLed, channel->state ? HIGH : LOW);
    digitalWrite(pin, HIGH);
    delay(RELAY_DELAY);
    digitalWrite(pin, LOW);
}

void sendState() {
    uint8_t state = 0;
    for (uint8_t i = 0; i < CHANNELS; i++) {
        if (channels[i].state)
            state |= 1 << i;
    }

    uint8_t send[2] = { RSP_STATE, state };
    sensor.send(&send, 2);
}

