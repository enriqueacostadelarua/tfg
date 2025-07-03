#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace Config {
    // Pin Configurations
    struct Pins {
        // HX711 Pins
        static constexpr uint8_t DOUT = 35; // Use GPIO35 (input-only pin)
        static constexpr uint8_t SCK = 26;

        // OLED Display SPI Pins
        static constexpr uint8_t OLED_CS = 5;
        static constexpr uint8_t OLED_DC = 16;
        static constexpr uint8_t OLED_RESET = 17;

        // SD Card SPI Pin
        static constexpr uint8_t SD_CS = 4; // Assign SD card CS to GPIO4

        // Stepper Motor Pins for Pump A
        static constexpr uint8_t DIR_A = 12;
        static constexpr uint8_t STEP_A = 13;

        // Stepper Motor Pins for Pump B
        static constexpr uint8_t DIR_B = 32;
        static constexpr uint8_t STEP_B = 33;

        // Shared ENABLE pin for both motors
        static constexpr uint8_t ENABLE = 27;

        // Encoder Pins
        static constexpr uint8_t ENCODER_CLK = 14; // Replace with your actual CLK pin
        static constexpr uint8_t ENCODER_DT = 22;  // Replace with your actual DT pin
        static constexpr uint8_t ENCODER_SW = 25;  // Replace with your actual SW pin

        // For backward compatibility (not strictly needed now)
        // static constexpr uint8_t BUTTON_SELECT = ENCODER_SW;
    };

    // EEPROM Configuration remains unchanged (used for calibration)
    struct EEPROM {
        static constexpr size_t OFFSET_ADDR = 0;
        static constexpr size_t SCALE_ADDR = OFFSET_ADDR + sizeof(long);
        static constexpr size_t CALIBRATED_FLAG_ADDR = SCALE_ADDR + sizeof(float); // Add a flag
        static constexpr size_t TOTAL_SIZE = CALIBRATED_FLAG_ADDR + sizeof(bool);
        static constexpr uint8_t CALIBRATED_TRUE_VALUE = 0x5A; // Arbitrary value to indicate calibration done
    };

    // Motor Configuration
    struct Motor {
        static constexpr float MIN_SPEED = 100.0f;
        static constexpr float MAX_SPEED = 1000.0f; // Steps per second
        static constexpr float ACCELERATION = 1000.0f; // Steps per second per second
        static constexpr float SLOW_THRESHOLD_PERCENT = 75.0f;
        static constexpr float SLOW_SPEED_FACTOR = 0.5f; // Speed multiplier when slow
    };

    // System Configuration
    struct System {
        static constexpr unsigned long UPDATE_INTERVAL_MS = 200; // General UI/logic update interval
        static constexpr float MIN_WEIGHT_THRESHOLD = 0.3f; // Ignore weights below this
        static constexpr unsigned long CALIBRATION_DELAY_MS = 3000; // Shorter delay for user convenience
        static constexpr int CALIBRATION_SAMPLES = 15; // More samples for better stability
        static constexpr float DEFAULT_CALIBRATION_WEIGHT = 11.3f; // Known weight in grams
        static constexpr const char* RECIPE_FILE = "/recipes.json";
        static constexpr unsigned long MOTOR_INACTIVITY_TIMEOUT_MS = 500; // Time before disabling motors
    };
}

#endif // CONFIG_H