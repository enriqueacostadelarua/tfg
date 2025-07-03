#ifndef MOTOR_H
#define MOTOR_H

#include <AccelStepper.h>
#include "Config.h"
#include "Types.h"

class Motor {
public:
    Motor();
    void begin();
    void run(); // Call this frequently to step the motors
    MotorState getState() const;
    void reset(); // Stops both motors immediately

    // Control methods
    void startPumpingA();
    void stopPumpingA();
    void startPumpingB();
    void stopPumpingB();

    void setSpeedA(float speed); // Set target speed for motor A
    void setSpeedB(float speed); // Set target speed for motor B

    void enableMotors();
    void disableMotors();

    // Test mode methods
    void setTestMode(bool enable);
    void setTestSpeed(float speed); // Controls motor A in test mode

private:
    AccelStepper _stepperA;
    AccelStepper _stepperB;
    MotorState _state;

    bool _motorEnabled;
    unsigned long _lastMotorActiveTime;

    bool _testMode;
    float _testSpeed; // Speed for motor A during test
};

#endif // MOTOR_H