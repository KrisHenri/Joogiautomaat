#include "RFID.h"

RFIDReader::RFIDReader(uint8_t address)
    : _address(address), _lastTag(""), _tagAvailable(false), _readerActive(false), _lastReadTime(0), _lastLedBlink(0) {}

void RFIDReader::begin() {
    Wire.begin();
    delay(100);  // Give the reader time to initialize
    
    // Initialize the RFID reader for 13.56 MHz
    Wire.beginTransmission(_address);
    Wire.write(0x00);  // Command register
    Wire.write(0x02);  // Initialize for 13.56 MHz
    if (Wire.endTransmission() != 0) {
        Serial.println("Failed to initialize RFID reader");
        _readerActive = false;
        return;
    }
    delay(100);
    
    // Configure for ISO/IEC 14443A (MIFARE) protocol
    Wire.beginTransmission(_address);
    Wire.write(0x03);  // Protocol register
    Wire.write(0x01);  // ISO/IEC 14443A
    if (Wire.endTransmission() != 0) {
        Serial.println("Failed to set protocol");
        _readerActive = false;
        return;
    }
    delay(50);
    
    // Test if reader is responding
    Wire.beginTransmission(_address);
    Wire.write(0x01);  // Status register
    if (Wire.endTransmission() != 0) {
        Serial.println("Failed to access status register");
        _readerActive = false;
        return;
    }
    
    Wire.requestFrom(_address, (uint8_t)1);
    _readerActive = (Wire.available() > 0);
    
    if (_readerActive) {
        Serial.println("RFID Reader initialized successfully for 13.56 MHz");
        // Read and print initial status
        uint8_t status = Wire.read();
        Serial.print("Initial status register value: 0x");
        Serial.println(status, HEX);
        
        // Print status register bit meanings
        Serial.println("Status register bits:");
        Serial.print("Bit 0 (Card Present): ");
        Serial.println((status & 0x01) ? "Yes" : "No");
        Serial.print("Bit 1 (Read Error): ");
        Serial.println((status & 0x02) ? "Yes" : "No");
        Serial.print("Bit 2 (Write Error): ");
        Serial.println((status & 0x04) ? "Yes" : "No");
        Serial.print("Bit 3 (Antenna Error): ");
        Serial.println((status & 0x08) ? "Yes" : "No");
        Serial.print("Bit 4 (Buffer Full): ");
        Serial.println((status & 0x10) ? "Yes" : "No");
        Serial.print("Bit 5 (Reader Active): ");
        Serial.println((status & 0x20) ? "Yes" : "No");
        Serial.print("Bit 6 (Protocol Error): ");
        Serial.println((status & 0x40) ? "Yes" : "No");
        Serial.print("Bit 7 (Collision): ");
        Serial.println((status & 0x80) ? "Yes" : "No");
    } else {
        Serial.println("RFID Reader not responding!");
    }
}

void RFIDReader::update() {
    if (millis() - _lastReadTime < READ_INTERVAL) {
        return;
    }
    _lastReadTime = millis();

    updateLedStatus();

    // Add debug output for reader status
    if (!_readerActive) {
        Serial.println("Reader not active");
        return;
    }

    if (isCardPresent()) {
        Serial.println("Card detected!");
        String tag = readTagFromI2C();
        if (!tag.isEmpty()) {
            Serial.print("Tag read: ");
            Serial.println(tag);
            if (tag != _lastTag) {
                _lastTag = tag;
                _tagAvailable = true;
                M5.Lcd.fillScreen(GREEN);  // Flash screen green when card detected
                delay(100);
                M5.Lcd.fillScreen(BLACK);
            }
        } else {
            Serial.println("Failed to read tag data");
        }
    } else {
        _tagAvailable = false;
    }
}

void RFIDReader::updateLedStatus() {
    if (!_readerActive) {
        M5.Lcd.fillScreen(RED);  // Red screen if reader not active
        delay(100);
        M5.Lcd.fillScreen(BLACK);
        return;
    }

    if (millis() - _lastLedBlink >= LED_BLINK_INTERVAL) {
        _lastLedBlink = millis();
        if (_tagAvailable) {
            M5.Lcd.fillScreen(GREEN);  // Green when card detected
            Serial.println("LED Status: Green (Card detected)");
        } else {
            M5.Lcd.fillScreen(BLUE);   // Blue when reader active but no card
            Serial.println("LED Status: Blue (Reader active, no card)");
        }
        delay(50);
        M5.Lcd.fillScreen(BLACK);
    }
}

bool RFIDReader::hasNewTag() const {
    return _tagAvailable;
}

String RFIDReader::getLastTag() const {
    return _lastTag;
}

bool RFIDReader::isCardPresent() {
    if (!_readerActive) {
        Serial.println("Reader not active in isCardPresent");
        return false;
    }

    // Try reading status register multiple times for reliability
    for (int attempt = 0; attempt < 3; attempt++) {
        Wire.beginTransmission(_address);
        Wire.write(0x01);  // Status register
        if (Wire.endTransmission() != 0) {
            Serial.println("I2C transmission failed");
            delay(10);
            continue;
        }
        
        Wire.requestFrom(_address, (uint8_t)1);
        if (Wire.available()) {
            uint8_t status = Wire.read();
            Serial.print("Status register value: 0x");
            Serial.println(status, HEX);
            
            // Check for various error conditions
            if (status & 0x08) {
                Serial.println("WARNING: Antenna error detected!");
            }
            if (status & 0x40) {
                Serial.println("WARNING: Protocol error detected!");
            }
            if (status & 0x80) {
                Serial.println("WARNING: Card collision detected!");
            }
            
            bool cardPresent = (status & 0x01) != 0;
            if (cardPresent) {
                Serial.println("Card presence detected in status register");
            }
            return cardPresent;
        }
        Serial.println("No data available from status register");
        delay(10);
    }
    Serial.println("Failed to read status register after 3 attempts");
    return false;
}

String RFIDReader::readTagFromI2C() {
    if (!_readerActive) {
        Serial.println("Reader not active in readTagFromI2C");
        return "";
    }

    // Try reading data register multiple times for reliability
    for (int attempt = 0; attempt < 3; attempt++) {
        Wire.beginTransmission(_address);
        Wire.write(0x02);  // Data register
        if (Wire.endTransmission() != 0) {
            Serial.println("I2C transmission failed in readTagFromI2C");
            delay(10);
            continue;
        }
        
        Wire.requestFrom(_address, (uint8_t)8);
        if (Wire.available() >= 8) {
            String tag = "";
            for (int i = 0; i < 8; ++i) {
                uint8_t data = Wire.read();
                if (data < 0x10) tag += "0";
                tag += String(data, HEX);
            }
            tag.toUpperCase();
            Serial.print("Successfully read tag: ");
            Serial.println(tag);
            return tag;
        }
        Serial.print("Insufficient bytes received: ");
        Serial.println(Wire.available());
        delay(10);
    }
    Serial.println("Failed to read tag data after 3 attempts");
    return "";
}