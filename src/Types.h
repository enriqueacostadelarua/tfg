#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <Arduino.h>

struct WeightData {
    float current;
    float targetA;
    float targetB;
    bool goalReachedA;
    bool goalReachedB;
};

struct MotorState {
    float currentSpeedA;
    float currentSpeedB;
    bool isPumpingA;
    bool isPumpingB;
};

struct Recipe {
    String name;
    float weightA;
    float weightB;
};

enum MenuState {
    MENU_MAIN,
    MENU_RECIPES,
    MENU_EDIT_RECIPES,
    MENU_EDIT_RECIPE_WEIGHTS,
    MENU_CALIBRATE_CONFIRM,
    MENU_CALIBRATE_GET_ZERO,
    MENU_CALIBRATE_GET_WEIGHT,
    MENU_ZERO_CONFIRM,
    MENU_WEIGHING,
    MENU_DISPENSING, // Added explicit dispensing state
    MENU_TEST_SETTINGS,
    MENU_TEST_RUNNING
};

#endif // TYPES_H