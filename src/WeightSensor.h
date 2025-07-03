#ifndef WEIGHT_SENSOR_H
#define WEIGHT_SENSOR_H

#include <EEPROM.h>
#include <functional>
#include "Config.h"
#include <HX711_ADC.h> // Include the HX711 library

class WeightSensor {
public:
    WeightSensor();
    void begin();
    bool update(); // Update sensor readings (call this in loop)
    float getWeight();
    void tare(const std::function<void(const char*)>& feedbackCallback = nullptr);
    void startCalibration(const std::function<void(const char*)>& feedbackCallback);
    void captureCalibrationStep1(); // Capture zero point
    void captureCalibrationStep2(float knownWeight, const std::function<void(const char*)>& feedbackCallback); // Capture known weight

    bool isReady(); // Check if sensor is ready

private:
    HX711_ADC _loadCell; // Create an instance of the HX711_ADC class
    float _currentWeight;
    bool _isReady;

    // Calibration values stored here temporarily during the process
    float _calibrationFactor; // scale
    long _offset;             // tare offset

    // EEPROM read/write helpers
    void saveCalibrationToEEPROM();
    bool loadCalibrationFromEEPROM();

    std::function<void(const char*)> _progressCallback; // Store callback
};

#endif // WEIGHT_SENSOR_H