
#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <Arduino.h>
#include "Types.h"
#include "WeightSensor.h"

class SerialCommand {
public:
    SerialCommand(WeightData& weightData, WeightSensor& sensor);
    void process();
    void printHelp() const;
    void logData(const WeightData& weightData, const MotorState& motorState) const;

private:
    static constexpr size_t BUFFER_SIZE = 64;

    WeightData& _weightData;
    WeightSensor& _sensor;
    char _inputBuffer[BUFFER_SIZE];
    size_t _inputIndex;

    void handleCommand(const char* command);
    void handleTargetWeight(const char* command);
    void printCurrentState(const WeightData& weightData, const MotorState& motorState) const;
    char* trimWhitespace(char* str);
};

#endif // SERIAL_COMMAND_H


