#include "SerialCommand.h"
#include <ctype.h> // Include for tolower
#include <string.h> // Include for strncpy, strcmp, strlen
#include <stdio.h> // Include for sscanf

SerialCommand::SerialCommand(WeightData& weightData, WeightSensor& sensor)
    : _weightData(weightData)
    , _sensor(sensor)
    , _inputIndex(0)
{
    memset(_inputBuffer, 0, BUFFER_SIZE);
}

void SerialCommand::process() {
    while (Serial.available()) {
        char inChar = static_cast<char>(Serial.read());

        if (inChar == '\n' || inChar == '\r') {
            _inputBuffer[_inputIndex] = '\0';
            handleCommand(_inputBuffer);
            _inputIndex = 0;
            memset(_inputBuffer, 0, BUFFER_SIZE);
        } else {
            if (_inputIndex < BUFFER_SIZE - 1) {
                _inputBuffer[_inputIndex++] = inChar;
            } else {
                // Buffer overflow, discard character or handle error
                Serial.println(F("Command buffer overflow!"));
                _inputIndex = 0; // Reset buffer
                memset(_inputBuffer, 0, BUFFER_SIZE);
            }
        }
    }
}

void SerialCommand::handleCommand(const char* command) {
    char cmd[BUFFER_SIZE];
    strncpy(cmd, command, BUFFER_SIZE);
    cmd[BUFFER_SIZE - 1] = '\0'; // Ensure null termination

    // Convert command to lowercase
    for (size_t i = 0; cmd[i]; i++) {
        cmd[i] = tolower((unsigned char)cmd[i]);
    }

    char* trimmedCmd = trimWhitespace(cmd);

    if (strcmp(trimmedCmd, "h") == 0 || strcmp(trimmedCmd, "help") == 0) {
        printHelp();
    }
    else if (strcmp(trimmedCmd, "c") == 0) {
        // FIX: Call startCalibration instead of calibrate
        _sensor.startCalibration([](const char* msg) {
            Serial.println(msg);
            // Note: This starts the multi-step calibration.
            // Further steps (captureCalibrationStep1/2) need user interaction
            // usually triggered by another command or button press,
            // which isn't handled here directly via serial.
            // This serial command only initiates the process.
        });
    }
    else if (strcmp(trimmedCmd, "k") == 0) {
        _sensor.tare([](const char* msg) { // Add feedback lambda
             Serial.println(msg);
        });
        // Serial.println(F("Scale tared")); // Tare function provides feedback now
    }
    else if (trimmedCmd[0] == 't') {
        handleTargetWeight(trimmedCmd);
    }
    else if (strcmp(trimmedCmd, "s") == 0) {
        Serial.println(F("Stopping..."));
        _weightData.targetA = 0;
        _weightData.targetB = 0;
        _weightData.goalReachedA = true;
        _weightData.goalReachedB = true;
        // Optionally: Add motor.reset() call here if SerialCommand has access to the motor object
    }
    else if (strlen(trimmedCmd) > 0) {
        Serial.print(F("Unknown command: '"));
        Serial.print(trimmedCmd);
        Serial.println(F("'. Type 'h' for help."));
    }
    // Ignore empty commands (just pressing Enter)
}


void SerialCommand::handleTargetWeight(const char* command) {
    const char* weightStr = command + 1; // Skip the 't'

    float newTargetA = 0.0f;
    float newTargetB = 0.0f;

    // Use sscanf to parse the weights
    // Check if exactly two floats were successfully scanned
    if (sscanf(weightStr, "%f,%f", &newTargetA, &newTargetB) == 2) {
        // Basic validation (optional: add upper limits?)
        if (newTargetA >= 0.0f && newTargetB >= 0.0f && (newTargetA > 0.0f || newTargetB > 0.0f) ) {
             _weightData.targetA = newTargetA;
            _weightData.targetB = newTargetB;
            _weightData.goalReachedA = false;
            _weightData.goalReachedB = false;

            Serial.print(F("Target weights set to: A = "));
            Serial.print(newTargetA, 1);
            Serial.print(F(" g, B = "));
            Serial.print(newTargetB, 1);
            Serial.println(F(" g"));
            // Optionally: Trigger dispensing start here if desired
        } else {
            Serial.println(F("Invalid target weights (must be non-negative, at least one > 0). Format: t<weightA>,<weightB> (e.g., t500.0,300.5)"));
        }
    } else {
        Serial.println(F("Invalid format. Use: t<weightA>,<weightB> (e.g., t500.0,300.5)"));
    }
}

void SerialCommand::printHelp() const {
    Serial.println(F("\nAvailable commands (case-insensitive):"));
    Serial.println(F("  h, help     - Show this help message"));
    Serial.println(F("  c           - Start scale calibration process"));
    Serial.println(F("  k           - Tare (zero) scale"));
    Serial.println(F("  t<wa>,<wb>  - Set target weights in grams (e.g., t500.0,300.5)"));
    Serial.println(F("  s           - Stop current operation (set targets to 0)"));
    Serial.println();
}

// Note: logData and printCurrentState need access to MotorState.
// Consider passing MotorState by const reference if SerialCommand needs it.
void SerialCommand::logData(const WeightData& weightData, const MotorState& motorState) const {
    // Only print if something interesting is happening
    // (Modify condition as needed)
    // bool isDispensing = motorState.isPumpingA || motorState.isPumpingB;
    // bool goalReached = weightData.goalReachedA && weightData.goalReachedB;
    // bool hasTarget = weightData.targetA > 0 || weightData.targetB > 0;

    // Example: Print frequently during dispensing, less otherwise
    // static unsigned long lastLogTime = 0;
    // bool shouldLog = false;
    // if (isDispensing) {
    //     shouldLog = true; // Always log when dispensing
    // } else if (millis() - lastLogTime > 2000) { // Log every 2s when idle
    //      shouldLog = true;
    //      lastLogTime = millis();
    // }

    // Simpler: Log only when pumping or goal just reached
     if (motorState.isPumpingA || motorState.isPumpingB || (weightData.goalReachedA && weightData.goalReachedB /* && !previouslyLoggedGoal */) ) {
         printCurrentState(weightData, motorState);
         // Add flag to prevent logging goal reached continuously
     }
}


void SerialCommand::printCurrentState(const WeightData& weightData, const MotorState& motorState) const {
    Serial.print(millis()); // Timestamp
    Serial.print(F(" | Weight: "));
    Serial.print(weightData.current, 1);
    Serial.print(F(" g"));

    bool goalA = weightData.current >= weightData.targetA;
    bool goalB = (weightData.targetB <= 0) || (weightData.current >= (weightData.targetA + weightData.targetB));

    if (motorState.isPumpingA) {
        Serial.print(F(" | Pumping A -> Target: "));
        Serial.print(weightData.targetA, 1);
        Serial.print(F(" g"));
        if (weightData.targetA > 0) {
            float progressA = constrain((weightData.current / weightData.targetA) * 100.0f, 0.0f, 100.0f);
            Serial.print(F(" ("));
            Serial.print(progressA, 0); // Use 0 decimal places for %
            Serial.print(F("%)"));
        }
    } else if (motorState.isPumpingB) {
         Serial.print(F(" | Pumping B -> Target: "));
        Serial.print(weightData.targetB, 1);
         Serial.print(F(" g (Total: "));
         Serial.print(weightData.targetA + weightData.targetB, 1);
         Serial.print(F(" g)"));
         if (weightData.targetB > 0) {
            float weightDispensedB = max(0.0f, weightData.current - weightData.targetA);
            float progressB = constrain((weightDispensedB / weightData.targetB) * 100.0f, 0.0f, 100.0f);
            Serial.print(F(" (B: "));
            Serial.print(progressB, 0); // Use 0 decimal places for %
            Serial.print(F("%)"));
         }
    } else if (weightData.targetA > 0 || weightData.targetB > 0) {
        // Not pumping, but targets are set
         Serial.print(F(" | Idle | Targets A:"));
         Serial.print(weightData.targetA, 1);
         Serial.print(F(" g ("));
         Serial.print(goalA ? "DONE" : "PENDING");
         Serial.print(F("), B:"));
         Serial.print(weightData.targetB, 1);
         Serial.print(F(" g ("));
         Serial.print(goalB ? "DONE" : "PENDING");
         Serial.print(F(")"));
    }


    if (weightData.goalReachedA && weightData.goalReachedB && (weightData.targetA > 0 || weightData.targetB > 0)) {
        Serial.print(F(" | Goal Reached!"));
    }

    Serial.println();
}


char* SerialCommand::trimWhitespace(char* str) {
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}