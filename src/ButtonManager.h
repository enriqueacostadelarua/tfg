#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "Config.h"

enum Button {
    BUTTON_NONE,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_SELECT
};

class ButtonManager {
public:
    ButtonManager();
    void begin();
    Button readButton();

private:
    uint8_t _encoderCLKPin;
    uint8_t _encoderDTPin;
    uint8_t _encoderSWPin;

    int _lastEncoderCLK;
    unsigned long _lastDebounceTimeSW;
    int _lastButtonStateSW;
    int _buttonStateSW;
    static constexpr unsigned long debounceDelay = 50;
};

#endif // BUTTON_MANAGER_H
