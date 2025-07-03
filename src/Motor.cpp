#include "Motor.h"
#include <Arduino.h> // for millis()

Motor::Motor()
    : _stepperA(AccelStepper::DRIVER, Config::Pins::STEP_A, Config::Pins::DIR_A),
      _stepperB(AccelStepper::DRIVER, Config::Pins::STEP_B, Config::Pins::DIR_B),
      _motorEnabled(false),
      _lastMotorActiveTime(0),
      _testMode(false),
      _testSpeed(0.0f)
{
    // Initialize state
    _state.currentSpeedA = 0;
    _state.isPumpingA = false;
    _state.currentSpeedB = 0;
    _state.isPumpingB = false;
}

void Motor::begin() {
    pinMode(Config::Pins::ENABLE, OUTPUT);
    digitalWrite(Config::Pins::ENABLE, HIGH); // Start disabled (active LOW)

    // Configure Stepper A
    _stepperA.setMaxSpeed(Config::Motor::MAX_SPEED * 1.5); // Set higher max speed for AccelStepper
    _stepperA.setAcceleration(Config::Motor::ACCELERATION);
    _stepperA.setEnablePin(Config::Pins::ENABLE); // AccelStepper can manage enable pin if needed (optional)
    _stepperA.setPinsInverted(false, false, true); // Invert enable pin signal (HIGH = disabled)
    _stepperA.disableOutputs(); // Start disabled

    // Configure Stepper B
    _stepperB.setMaxSpeed(Config::Motor::MAX_SPEED * 1.5);
    _stepperB.setAcceleration(Config::Motor::ACCELERATION);
    // Stepper B uses the same enable pin logic but doesn't control it directly
    // We manage the enable pin globally based on activity

    _motorEnabled = false;
}

void Motor::enableMotors() {
    if (!_motorEnabled) {
        //_stepperA.enableOutputs(); // Let AccelStepper handle its enable if configured
        digitalWrite(Config::Pins::ENABLE, LOW); // Manual control for shared enable
        _motorEnabled = true;
        _lastMotorActiveTime = millis(); // Reset timer when enabling
        //Serial.println("Motors Enabled");
    }
}

void Motor::disableMotors() {
    if (_motorEnabled) {
       // _stepperA.disableOutputs(); // Let AccelStepper handle its enable if configured
        digitalWrite(Config::Pins::ENABLE, HIGH); // Manual control
        _motorEnabled = false;
       // Serial.println("Motors Disabled");
    }
}

void Motor::setSpeedA(float speed) {
    if (_testMode) return; // Ignore in test mode

    // Ensure speed is within reasonable bounds (0 to MAX_SPEED)
    speed = constrain(speed, 0, Config::Motor::MAX_SPEED);

    if (speed > 0 && !_motorEnabled) {
        enableMotors();
    }
    _stepperA.setSpeed(speed);
    _state.currentSpeedA = _stepperA.speed(); // Use actual speed from library

    if (speed > 0) {
        _lastMotorActiveTime = millis();
    }
}

void Motor::setSpeedB(float speed) {
     if (_testMode) return; // Ignore in test mode

    speed = constrain(speed, 0, Config::Motor::MAX_SPEED);

    if (speed > 0 && !_motorEnabled) {
        enableMotors();
    }
    _stepperB.setSpeed(speed);
    _state.currentSpeedB = _stepperB.speed();

     if (speed > 0) {
        _lastMotorActiveTime = millis();
    }
}


void Motor::startPumpingA() {
    if (_testMode) return;
    if (!_state.isPumpingA) {
        _state.isPumpingA = true;
        setSpeedA(Config::Motor::MAX_SPEED); // Start at max speed
        //Serial.println("Start Pump A");
    }
}

void Motor::stopPumpingA() {
    if (_state.isPumpingA) {
        _state.isPumpingA = false;
        setSpeedA(0); // Stop motor
        _lastMotorActiveTime = millis(); // Update inactivity timer
       // Serial.println("Stop Pump A");
    }
}

void Motor::startPumpingB() {
    if (_testMode) return;
     if (!_state.isPumpingB) {
        _state.isPumpingB = true;
        setSpeedB(Config::Motor::MAX_SPEED); // Start at max speed
       // Serial.println("Start Pump B");
    }
}

void Motor::stopPumpingB() {
     if (_state.isPumpingB) {
        _state.isPumpingB = false;
        setSpeedB(0); // Stop motor
        _lastMotorActiveTime = millis(); // Update inactivity timer
       // Serial.println("Stop Pump B");
    }
}

void Motor::run() {
    bool motorA_needs_run = _state.isPumpingA || (_testMode && _testSpeed != 0);
    bool motorB_needs_run = _state.isPumpingB;

    // Enable motors if any motor needs to run and they are currently disabled
    if ((motorA_needs_run || motorB_needs_run) && !_motorEnabled) {
        enableMotors();
    }

    // Run the steppers if needed
    if (_testMode) {
        // Only run Stepper A in test mode based on _testSpeed
        if (_testSpeed != 0) {
            _stepperA.runSpeed();
             _lastMotorActiveTime = millis();
        }
    } else {
        // Normal operation
        if (_state.isPumpingA) {
            _stepperA.runSpeed();
             _lastMotorActiveTime = millis(); // Keep updating timer while active
        }
        if (_state.isPumpingB) {
            _stepperB.runSpeed();
             _lastMotorActiveTime = millis(); // Keep updating timer while active
        }
    }

    // Auto-disable logic
    if (_motorEnabled && !motorA_needs_run && !motorB_needs_run) {
        if (millis() - _lastMotorActiveTime >= Config::System::MOTOR_INACTIVITY_TIMEOUT_MS) {
            disableMotors();
        }
    }
}

MotorState Motor::getState() const {
    return _state;
}

void Motor::reset() {
    // Stop both pumps immediately and reset state
    _testMode = false;
    _testSpeed = 0;
    _stepperA.stop(); // Use AccelStepper stop for immediate halt with deceleration if needed (or setSpeed(0))
    _stepperB.stop();
    _stepperA.setSpeed(0); // Ensure speed is set to 0
    _stepperB.setSpeed(0);
    _state.isPumpingA = false;
    _state.isPumpingB = false;
    _state.currentSpeedA = 0;
    _state.currentSpeedB = 0;
    _lastMotorActiveTime = millis(); // Reset inactivity timer start point
    // Motors will auto-disable via run() if they remain inactive
    Serial.println("Motor Reset");
}


// --- Test Mode ---

void Motor::setTestMode(bool enable) {
    _testMode = enable;
    if (!enable) {
        // When disabling test mode, ensure motor A stops
        _testSpeed = 0;
        _stepperA.setSpeed(0);
        _state.isPumpingA = false; // Ensure state reflects test mode end
        _lastMotorActiveTime = millis(); // Update inactivity timer
    } else {
        // When enabling test mode, ensure normal pumps are stopped
        stopPumpingA();
        stopPumpingB();
    }
}

void Motor::setTestSpeed(float speed) {
    if (!_testMode) return; // Only effective in test mode

    _testSpeed = constrain(speed, 0, Config::Motor::MAX_SPEED);
    _stepperA.setSpeed(_testSpeed); // Set speed for motor A

    if (_testSpeed > 0) {
         if (!_motorEnabled) enableMotors(); // Enable if starting motor
         _lastMotorActiveTime = millis(); // Update activity timer
    } else {
         _lastMotorActiveTime = millis(); // Update timer even when stopping
    }
}