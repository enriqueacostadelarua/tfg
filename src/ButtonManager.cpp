#include "ButtonManager.h"
#include <Arduino.h>

ButtonManager::ButtonManager()
    : _encoderCLKPin(Config::Pins::ENCODER_CLK)
    , _encoderDTPin(Config::Pins::ENCODER_DT)
    , _encoderSWPin(Config::Pins::ENCODER_SW)
    , _lastDebounceTimeSW(0)
    , _lastButtonStateSW(HIGH)
    , _buttonStateSW(HIGH)
{
    _lastEncoderCLK = HIGH;
}

void ButtonManager::begin() {
    pinMode(_encoderCLKPin, INPUT);
    pinMode(_encoderDTPin, INPUT);
    pinMode(_encoderSWPin, INPUT_PULLUP);

    _lastEncoderCLK = digitalRead(_encoderCLKPin);
}

Button ButtonManager::readButton() {
    // Read the encoder switch (push button)
    int readingSW = digitalRead(_encoderSWPin);

    // Debounce the switch
    if (readingSW != _lastButtonStateSW) {
        _lastDebounceTimeSW = millis();
    }

    if ((millis() - _lastDebounceTimeSW) > debounceDelay) {
        if (readingSW != _buttonStateSW) {
            _buttonStateSW = readingSW;

            if (_buttonStateSW == LOW) {
                // Button pressed
                _lastButtonStateSW = readingSW;
                return BUTTON_SELECT;
            }
        }
    }

    _lastButtonStateSW = readingSW;

    // Read the encoder rotation
    int currentCLK = digitalRead(_encoderCLKPin);
    Button result = BUTTON_NONE;

    if (currentCLK != _lastEncoderCLK) {
        // Encoder state changed
        if (currentCLK == HIGH) {
            // CLK went from LOW to HIGH
            int currentDT = digitalRead(_encoderDTPin);
            if (currentDT == LOW) {
                // Rotated clockwise
                result = BUTTON_UP;
            } else {
                // Rotated counter-clockwise
                result = BUTTON_DOWN;
            }
        }
    }

    _lastEncoderCLK = currentCLK;

    return result;
}
