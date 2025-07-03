#include "WeightSensor.h"
#include <Arduino.h> // Needed for Serial prints

WeightSensor::WeightSensor()
    : _loadCell(Config::Pins::DOUT, Config::Pins::SCK),
      _currentWeight(0.0f),
      _isReady(false),
      _calibrationFactor(1.0f), // Default scale factor
      _offset(0)             // Default offset
{}

void WeightSensor::begin() {
    EEPROM.begin(Config::EEPROM::TOTAL_SIZE);

    _loadCell.begin(); // Initialize pins
    // Optional: Set gain if different from default (128)
    // _loadCell.setGain(128);

    unsigned long stabilizingtime = 2000;
    // Start continuous conversion. Pass 'true' to perform initial tare.
    _loadCell.start(stabilizingtime, true);

    if (_loadCell.getTareTimeoutFlag()) {
        Serial.println("Sensor timeout during initial tare, check wiring.");
        _isReady = false;
    } else {
        Serial.println("Sensor ready.");
        _isReady = true;
        _offset = _loadCell.getTareOffset(); // Get offset from initial tare
        Serial.print("Initial Offset: "); Serial.println(_offset);
    }

    // Load calibration from EEPROM after starting sensor and initial tare
    if (!loadCalibrationFromEEPROM()) {
        Serial.println("EEPROM: No valid calibration found or error reading. Using defaults.");
        // Apply defaults if loading failed (offset might have been set by initial tare)
        _loadCell.setCalFactor(_calibrationFactor);
        _loadCell.setTareOffset(_offset);
    } else {
         Serial.println("EEPROM: Calibration loaded and applied.");
    }
}

bool WeightSensor::update() {
    if (!_isReady) return false;
    // Update the library's internal data set
    _loadCell.update();
    // Get the calibrated weight using the library's current factor and offset
    _currentWeight = _loadCell.getData();
    return true;
}

float WeightSensor::getWeight() {
    // Apply threshold if needed
    return (_currentWeight >= Config::System::MIN_WEIGHT_THRESHOLD || _currentWeight <= -Config::System::MIN_WEIGHT_THRESHOLD)
           ? _currentWeight
           : 0.0f;
}

bool WeightSensor::isReady() {
    return _isReady && !_loadCell.getTareTimeoutFlag();
}

void WeightSensor::tare(const std::function<void(const char*)>& feedbackCallback) {
    if (!_isReady) {
        if(feedbackCallback) feedbackCallback("Sensor no listo");
        return;
    }
    if(feedbackCallback) feedbackCallback("Poniendo a Cero...");
    _loadCell.tareNoDelay(); // Start non-blocking tare

    unsigned long start = millis();
    while(!_loadCell.getTareStatus()) {
        _loadCell.update(); // Keep updating library state
        if(millis() - start > 3000) {
             if(feedbackCallback) feedbackCallback("Error al poner a Cero!");
             return;
        }
        delay(5);
    }
    // Tare complete, get the new offset
    _offset = _loadCell.getTareOffset();
    // Save the new offset (and the existing calibration factor)
    saveCalibrationToEEPROM();
    if(feedbackCallback) feedbackCallback("Puesto a Cero");
}

// --- Calibration Process ---

void WeightSensor::startCalibration(const std::function<void(const char*)>& feedbackCallback) {
    if (!_isReady) {
        if(feedbackCallback) feedbackCallback("Sensor no listo");
        return;
    }
    _progressCallback = feedbackCallback;
    if(_progressCallback) _progressCallback("Retire todo el peso");
    // Ready for Step 1
}

void WeightSensor::captureCalibrationStep1() {
    // Capture Zero Point
    if (!_isReady) {
         if(_progressCallback) _progressCallback("Sensor no listo");
         return;
    }
    if(_progressCallback) _progressCallback("Calculando Cero...");

    _loadCell.tareNoDelay(); // Start tare
    unsigned long start = millis();
    while(!_loadCell.getTareStatus()) {
        _loadCell.update(); // MUST keep updating
        if(millis() - start > 3000) {
            if(_progressCallback) _progressCallback("Error! Tiempo Expirado Cero");
            return;
        }
        delay(5);
    }
    // Tare finished, store the raw offset value for EEPROM saving later
    _offset = _loadCell.getTareOffset();

    Serial.print("Offset Capturado (Step 1): "); Serial.println(_offset); // Debug

    if(_progressCallback) _progressCallback("Coloque peso conocido");
    // Ready for Step 2
}

void WeightSensor::captureCalibrationStep2(float knownWeight, const std::function<void(const char*)>& feedbackCallback) {
    // Capture Known Weight Point and Calculate Factor using library function
     if (!_isReady) {
        if(feedbackCallback) feedbackCallback("Sensor no listo");
        return;
    }
    if (knownWeight <= 0.0f) {
        if(feedbackCallback) feedbackCallback("Peso invalido!");
        return;
    }

    if(feedbackCallback) feedbackCallback("Calculando Factor...");

    // Before calling getNewCalibration, it's good practice to ensure
    // the dataset reflects the state with the known weight.
    // refreshDataSet is blocking but ensures this.
    if (!_loadCell.refreshDataSet()) {
         if(feedbackCallback) feedbackCallback("Error refrescando dataset!");
         return;
    }
    // Alternative: wait some time after weight placement and assume update() filled buffer
    // delay(1000); // Wait for samples


    Serial.print("Peso Conocido para getNewCalibration: "); Serial.println(knownWeight); // Debug

    // *** USE THE LIBRARY'S BUILT-IN CALIBRATION FUNCTION ***
    _calibrationFactor = _loadCell.getNewCalibration(knownWeight);

    Serial.print("Factor Calculado por Libreria: "); Serial.println(_calibrationFactor); // Debug


    if (_calibrationFactor == 0.0f || isnan(_calibrationFactor) || isinf(_calibrationFactor)) {
         if(feedbackCallback) feedbackCallback("Error en Calibracion!");
         // Maybe reset factor to default if error?
         // _calibrationFactor = 1.0f;
         // _loadCell.setCalFactor(_calibrationFactor);
    } else {
        // getNewCalibration already calls setCalFactor internally according to typical usage.
        // We just need to save the result (and the offset from Step 1)
        saveCalibrationToEEPROM();
        if(feedbackCallback) feedbackCallback("Calibracion Completa");
    }
}


// --- EEPROM --- (Keep the previously corrected versions) ---
void WeightSensor::saveCalibrationToEEPROM() {
    Serial.println("EEPROM: Saving calibration...");
    // Save the offset captured during the last tare/calibration step 1
    EEPROM.put(Config::EEPROM::OFFSET_ADDR, _offset);
    // Save the calibration factor calculated during calibration step 2
    EEPROM.put(Config::EEPROM::SCALE_ADDR, _calibrationFactor);
    // Mark that valid calibration data exists
    EEPROM.write(Config::EEPROM::CALIBRATED_FLAG_ADDR, Config::EEPROM::CALIBRATED_TRUE_VALUE);
    if (!EEPROM.commit()) {
        Serial.println("EEPROM: Error committing calibration!");
    } else {
        Serial.println("EEPROM: Calibration saved.");
        Serial.print("Saved Offset: "); Serial.println(_offset);
        Serial.print("Saved Factor: "); Serial.println(_calibrationFactor);
    }
}

bool WeightSensor::loadCalibrationFromEEPROM() {
    Serial.println("EEPROM: Loading calibration...");
    byte calibratedFlag = EEPROM.read(Config::EEPROM::CALIBRATED_FLAG_ADDR);
    Serial.print("EEPROM: Flag read = 0x"); Serial.println(calibratedFlag, HEX);

    if (calibratedFlag == Config::EEPROM::CALIBRATED_TRUE_VALUE) {
        long storedOffset;
        float storedFactor;
        EEPROM.get(Config::EEPROM::OFFSET_ADDR, storedOffset);
        EEPROM.get(Config::EEPROM::SCALE_ADDR, storedFactor);

        Serial.print("EEPROM: Read Offset="); Serial.print(storedOffset);
        Serial.print(", Read Factor="); Serial.println(storedFactor);

        // Sanity check factor
        if (storedFactor == 0.0f || isnan(storedFactor) || isinf(storedFactor)) {
             Serial.println("EEPROM: Invalid scale factor loaded. Using defaults.");
             _calibrationFactor = 1.0f; // Use default member variable
             _offset = 0;             // Use default member variable
        } else {
             // Use stored values
            _offset = storedOffset;
            _calibrationFactor = storedFactor;
        }
        // Apply the determined offset and factor to the library instance
        _loadCell.setTareOffset(_offset);
        _loadCell.setCalFactor(_calibrationFactor);
        Serial.println("EEPROM: Offset/Factor applied to library.");
        // Return true only if factor was valid, false if defaults were used due to invalid stored factor
        return !(storedFactor == 0.0f || isnan(storedFactor) || isinf(storedFactor));

    } else {
        Serial.println("EEPROM: Calibration flag not set or invalid. Using defaults.");
        // Ensure library uses default member values (_offset=0, _calFactor=1.0)
         _calibrationFactor = 1.0f;
         _offset = 0;
        _loadCell.setTareOffset(_offset);
        _loadCell.setCalFactor(_calibrationFactor);
        return false; // Indicate defaults are in use
    }
}