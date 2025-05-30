#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <Wire.h>
#include <M5Core2.h>

class RFIDReader {
public:
    RFIDReader(uint8_t address = 0x28);  // Default I2C address for RFID2
    void begin();
    void update();
    bool hasNewTag() const;
    String getLastTag() const;
    bool isReaderActive() const { return _readerActive; }

private:
    uint8_t _address;
    String _lastTag;
    bool _tagAvailable;
    bool _readerActive;
    unsigned long _lastReadTime;
    const unsigned long READ_INTERVAL = 100; // ms between reads
    const unsigned long LED_BLINK_INTERVAL = 500; // ms between LED blinks
    unsigned long _lastLedBlink;

    String readTagFromI2C();
    bool isCardPresent();
    void updateLedStatus();
};

#endif