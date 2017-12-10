#define PIN_IR_IN       2
#define PIN_IR_OUT      3
#define PIN_BUTTON      12
#define PIN_LED_RED     9
#define PIN_LED_BLUE    10 

#define PACKET_CODE             0x01
#define PACKET_BUTT_CLICK       0x02
#define PACKET_BUTT_DBL_CLICK   0x03
#define PACKET_BUTT_LONG_CLICK  0x04

void updateSerialPacket();

void setup()
{
    Serial.begin(115200);
    
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_IR_OUT, OUTPUT);
    digitalWrite(PIN_IR_OUT, LOW);

    pinMode(PIN_IR_IN, INPUT);
    pinMode(PIN_BUTTON, INPUT);
    
    EICRA = 1 << ISC00; //any edge 
    EIMSK = 1 << INT0;

    sei();
}

#define MAX_LENGTH  300
#define RESOLUTION  50
#define MAX_DELTA   10000

volatile uint32_t lastIrChange;
volatile uint16_t rxPos = 0, procPos = 0;
volatile uint8_t rxBuf[MAX_LENGTH];
volatile uint8_t procBuf[MAX_LENGTH] = { PACKET_CODE };
volatile uint8_t lastState = 1;

bool transferToProc() {
    if (procPos) return false;
    if (rxPos > 6) {
        memcpy((void*)&procBuf[1], (void*)rxBuf, rxPos);
        procPos = rxPos;
    }
    rxPos = 0;
    return true;
}

ISR(INT0_vect) {
    uint32_t now = micros();
    uint32_t delta = now - lastIrChange;

    if (delta > MAX_DELTA) {
        if (transferToProc()) {
            rxPos = 0;
        }
        else {
            return;
        }
    }

    lastIrChange = now;
    lastState = PIND & (1 << PIN_IR_IN);

    uint8_t ticks = delta / RESOLUTION;
    if (rxPos && rxPos < MAX_LENGTH) {
        rxBuf[rxPos - 1] = ticks;
    }
    rxPos++;
}

void sendCode(const uint8_t *code, uint16_t length) {
    EIMSK = 0;
    uint32_t end = micros();
    for (uint8_t i=0; i<length; i++)
    {
        end += (uint16_t)code[i] * RESOLUTION;
        if (i % 2 == 0)
        {
            // @ 38 kHz
            uint32_t now;
            do
            {
                PORTD |= 1 << PIN_IR_OUT;
                delayMicroseconds(13);
                PORTD &= ~(1 << PIN_IR_OUT);
                delayMicroseconds(13);
            } while (micros() < end);
        }
        else
        {
            //just wait for pulse length
            while (micros() < end) {
                _NOP();
            }
        }
    }
    delayMicroseconds(200);
    EIMSK = 1 << INT0;
}

void loop()
{
    cli();
    if (rxPos && lastState && (micros() - lastIrChange) > MAX_DELTA) {
        transferToProc();
    }
    sei();

    if (procPos) {
        sendSerialPacket((uint8_t*)procBuf, procPos);
        procPos = 0;
    }

    updateSerialPacket();
}


void onSerialPacketReceived(uint8_t *data, uint8_t size) {
    switch (*data++) {
    case PACKET_CODE:
        sendCode(data, size - 1);
        break;
    }
}

#define FRAME_HEADER_1  0xDE
#define FRAME_HEADER_2  0x5B
#define CHKSUM_INIT     0x1021

void sendSerialPacket(const uint8_t *data, uint8_t size)
{
    Serial.write(FRAME_HEADER_1);
    Serial.write(FRAME_HEADER_2);
    uint16_t checksum = CHKSUM_INIT;
    Serial.write(size);
    updateChecksum(&checksum, size);
    while (size--) {
        Serial.write(*data);
        updateChecksum(&checksum, *data++);
    }
    Serial.write(checksum >> 8);
    Serial.write(checksum & 0xFF);
}

void updateChecksum(uint16_t *checksum, uint8_t data) {
    bool roll = *checksum & 0x8000 ? true : false;
    *checksum <<= 1;
    if (roll) *checksum |= 1;
    *checksum ^= data;
}

static uint16_t getChecksum(uint8_t *data, uint8_t length) {
    uint16_t checksum = CHKSUM_INIT;
    while (length--) {
        updateChecksum(&checksum, *data++);
    }
    return checksum;
}

#define RX_STATUS_IDLE  0
#define RX_STATUS_HDR   1
#define RX_STATUS_SIZE  2
#define RX_STATUS_DATA  3
#define RX_STATUS_CHK1  4
#define RX_STATUS_CHK2  5

void updateSerialPacket() {
    static uint8_t status = RX_STATUS_IDLE;
    static uint8_t rxBuffer[MAX_LENGTH];
    static uint8_t rxSize, rxOffset;
    static uint16_t chksum, rxChksum;
    int data;
    while ((data = Serial.read()) != -1) {
        switch (status)
        {
        case RX_STATUS_IDLE:
            if (data == FRAME_HEADER_1) status = RX_STATUS_HDR;
            break;
        case RX_STATUS_HDR:
            status = data == FRAME_HEADER_2 ? RX_STATUS_SIZE : RX_STATUS_IDLE;
            break;
        case RX_STATUS_SIZE:
            rxSize = data;
            status = rxSize > sizeof(rxBuffer) ? RX_STATUS_IDLE : RX_STATUS_DATA;
            chksum = CHKSUM_INIT;
            rxOffset = 0;
            updateChecksum(&chksum, rxSize);
            break;
        case RX_STATUS_DATA:
            updateChecksum(&chksum, data);
            rxBuffer[rxOffset++] = data;
            if (rxOffset == rxSize) status = RX_STATUS_CHK1;
            break;
        case RX_STATUS_CHK1:
            rxChksum = data << 8;
            status = RX_STATUS_CHK2;
            break;
        case RX_STATUS_CHK2:
            rxChksum |= data;
            if (chksum == rxChksum) onSerialPacketReceived(rxBuffer, rxSize);
            status = RX_STATUS_IDLE;
            break;

        default:
            break;

        }
    }
}
