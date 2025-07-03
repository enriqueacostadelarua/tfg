#include <Arduino.h>
#include "Display.h"
#include "WeightSensor.h"
#include "Motor.h"
#include "Types.h"
#include "RecipeManager.h"
#include "ButtonManager.h"
#include "SerialCommand.h"
#include "Menu.h"
#include <SD.h>

// Constants
const unsigned long MESSAGE_DISPLAY_DURATION = 1500; // Shorter duration
const unsigned long SENSOR_UPDATE_INTERVAL = 50;     // Update sensor more frequently
const unsigned long DISPLAY_UPDATE_INTERVAL = 150;   // Update display less frequently than sensor
const unsigned long TEST_DATA_INTERVAL = 200;
const int VISIBLE_LINES = 5;  // Number of visible lines in menus

// Global Objects
Display display;
WeightSensor sensor;
Motor motor;
WeightData weightData {0.0f, 0.0f, 0.0f, false, false};
ButtonManager buttonManager;
RecipeManager recipeManager;
std::vector<Recipe> recipes;
// SerialCommand serialCmd(weightData, sensor); // Serial command needs sensor/motor refs, pass later?

// Global Menu pointers
Menu *mainMenu = nullptr;
Menu *recipeMenu = nullptr;
Menu *editRecipesMenu = nullptr;
Menu *editRecipeWeightsMenu = nullptr;
Menu *confirmMenu = nullptr; // Reusable confirmation menu

MenuState currentMenu = MENU_MAIN;

// Global variable to track the recipe being edited or dispensed
int selectedRecipeIndex = -1; // Use -1 to indicate none selected/active

// Message Display Struct for non-blocking timed messages
struct MessageDisplayInfo {
    String message;            // Use String for easier management
    String secondaryMessage;
    unsigned long startTime;
    unsigned long duration;
    MenuState nextMenu;
    bool active;
    bool stateTransitionDone; // Flag to ensure nextMenu transition happens only once
};
MessageDisplayInfo messageDisplay = {"", "", 0, 0, MENU_MAIN, false, true};

// Variables for testing
float testPumpSpeed = 500.0f;
File testDataFile;
unsigned long testStartTime = 0;
unsigned long lastTestDataTime = 0;

// Variables for dispensing state
unsigned long lastWeightUpdateTime = 0;
unsigned long lastDisplayUpdateTime = 0;

// Main Menu Items (Using String for easier menu updates if needed later)
std::vector<String> MAIN_MENU_ITEMS = {
    "Recetas", "Editar Recetas", "Calibrar", "Poner a Cero", "Pesaje", "Test"
};

// Function Prototypes
void showTimedMessage(const String& msg, const String& secondaryMsg, unsigned long duration, MenuState nextMenu);
void updateRecipeMenu();
void updateEditRecipesMenu();
void updateEditRecipeWeightsMenu(Recipe &recipe);
void returnToMainMenu();
void handleDispensing();
void handleMainMenu(Button btn);
void handleRecipesMenu(Button btn);
void handleCalibration(Button btn); // Combined calibration states
void handleZeroConfirm(Button btn);
void handleEditRecipesMenu(Button btn);
void handleEditRecipeWeights(Button btn);
void handleWeighingMenu(Button btn);
void handleTestSettings(Button btn);
void handleTestRunning(Button btn);
void createConfirmMenu(const char* title, const char* item1 = "Si", const char* item2 = "Volver");
void updateDisplayForCurrentMenu(); // Helper to refresh display based on state

// --- Setup ---
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000); // Wait for serial, max 2s
    Serial.println("\n--- Dispenser Booting ---");

    display.begin();
    display.showMessage("Iniciando...");

    if (!SD.begin(Config::Pins::SD_CS)) {
        Serial.println(F("Error al iniciar SD"));
        display.showMessage(F("Error al iniciar SD"), F("Verifique Tarjeta"));
        while (1) delay(100); // Halt on SD error
    }
    Serial.println("SD Card OK");

    sensor.begin(); // Initializes sensor and loads calibration
    motor.begin();
    buttonManager.begin();
    // serialCmd.begin(); // If SerialCommand needs setup

    if (recipeManager.loadRecipes()) {
        recipes = recipeManager.getRecipes();
        Serial.print("Recetas Cargadas: "); Serial.println(recipes.size());
    } else {
        Serial.println("No se encontraron/cargaron Recetas.");
        recipes.clear(); // Ensure recipes vector is empty
        display.showMessage(F("No hay Recetas"), F("Cree via Editar?"));
        delay(2000); // Show message longer
    }

    // Initialize Menus
    mainMenu = new Menu(MAIN_MENU_ITEMS, "Menu Principal", VISIBLE_LINES);
    recipeMenu = new Menu({}, "Recetas", VISIBLE_LINES); // Start empty, updated later
    editRecipesMenu = new Menu({}, "Editar Recetas", VISIBLE_LINES);
    editRecipeWeightsMenu = new Menu({}, "Editar Receta", 3); // 3 lines: A, B, Save
    confirmMenu = new Menu({}, "", 2); // Reusable confirm menu

    // Initial state
    currentMenu = MENU_MAIN;
    updateDisplayForCurrentMenu(); // Show main menu initially

    Serial.println("Setup Complete.");
}

// --- Main Loop ---
void loop() {
    unsigned long currentMillis = millis();
    // serialCmd.process(); // Process serial commands if needed

    // Always run motor logic (handles stepping and auto-disable)
    motor.run();        

    // Update sensor readings periodically
    static unsigned long lastSensorReadTime = 0;
    if (currentMillis - lastSensorReadTime >= SENSOR_UPDATE_INTERVAL) {
        lastSensorReadTime = currentMillis;
        if(sensor.update()) {
            // Optional: Use new weight reading immediately if needed outside dispensing
            // weightData.current = sensor.getWeight();
        } else {
           // Handle sensor read error? Maybe show temporary error message.
           // display.showMessage("Error Sensor!", nullptr); // Careful: might overwrite important info
        }
    }

    // Handle Non-Blocking Timed Message Display
    if (messageDisplay.active) {
        // Display the message on every loop iteration while active
        display.showMessage(messageDisplay.message.c_str(), messageDisplay.secondaryMessage.isEmpty() ? nullptr : messageDisplay.secondaryMessage.c_str());
        // Check if duration has passed
        if (currentMillis - messageDisplay.startTime >= messageDisplay.duration) {
            messageDisplay.active = false; // Deactivate message
            // Transition to the next state *only if* not already done
            if (!messageDisplay.stateTransitionDone) {
                currentMenu = messageDisplay.nextMenu;
                messageDisplay.stateTransitionDone = true;
                updateDisplayForCurrentMenu(); // Update display for the new state
            }
        }
    } else {
        // Only handle buttons and state logic if no message is active OR
        // if the message has expired and we need to process the state change fallout
        if (currentMenu == MENU_DISPENSING) {
            handleDispensing();
        } else {
            // Read button input ONLY when not dispensing and no active blocking message
            Button btn = buttonManager.readButton();

            // Dispatch button events based on current menu state
            switch (currentMenu) {
                case MENU_MAIN:                 handleMainMenu(btn); break;
                case MENU_RECIPES:              handleRecipesMenu(btn); break;
                case MENU_EDIT_RECIPES:         handleEditRecipesMenu(btn); break;
                case MENU_EDIT_RECIPE_WEIGHTS:  handleEditRecipeWeights(btn); break;
                case MENU_CALIBRATE_CONFIRM:
                case MENU_CALIBRATE_GET_ZERO:
                case MENU_CALIBRATE_GET_WEIGHT: handleCalibration(btn); break;
                case MENU_ZERO_CONFIRM:         handleZeroConfirm(btn); break;
                case MENU_WEIGHING:             handleWeighingMenu(btn); break;
                case MENU_TEST_SETTINGS:        handleTestSettings(btn); break;
                case MENU_TEST_RUNNING:         handleTestRunning(btn); break;
                default:                        currentMenu = MENU_MAIN; updateDisplayForCurrentMenu(); break; // Fallback
            }
        }
    } // end if messageDisplay.active
}

// --- Timed Message ---
void showTimedMessage(const String& msg, const String& secondaryMsg, unsigned long duration, MenuState nextMenu) {
    messageDisplay.message = msg;
    messageDisplay.secondaryMessage = secondaryMsg;
    messageDisplay.startTime = millis();
    messageDisplay.duration = duration;
    messageDisplay.nextMenu = nextMenu;
    messageDisplay.active = true;
    messageDisplay.stateTransitionDone = false; // Reset transition flag
    // The message will be displayed in the main loop
}

// --- Menu Content Updates ---
void updateRecipeMenu() {
    std::vector<String> recipeItems;
    if (recipes.empty()) {
        recipeItems.push_back("No hay recetas");
    } else {
        for (const auto &r : recipes) {
            char buffer[40]; // Increased buffer size
            snprintf(buffer, sizeof(buffer), "%-12s A:%5.1fg B:%5.1fg", r.name.c_str(), r.weightA, r.weightB);
            recipeItems.push_back(String(buffer));
        }
    }
    recipeItems.push_back("Volver");
    recipeMenu->setItems(recipeItems);
    recipeMenu->updateDisplay(display);
}

void updateEditRecipesMenu() {
    std::vector<String> editItems;
     if (recipes.empty()) {
        editItems.push_back("No hay recetas"); // Option to add new?
    } else {
        for (const auto &r : recipes) {
             char buffer[40];
             snprintf(buffer, sizeof(buffer), "%-12s A:%5.1fg B:%5.1fg", r.name.c_str(), r.weightA, r.weightB);
             editItems.push_back(String(buffer));
        }
    }
    // editItems.push_back("Nueva Receta"); // TODO: Implement Add New
    editItems.push_back("Volver");
    editRecipesMenu->setItems(editItems);
    editRecipesMenu->updateDisplay(display);
}

void updateEditRecipeWeightsMenu(Recipe &recipe) {
    char bufferA[32], bufferB[32];
    snprintf(bufferA, sizeof(bufferA), "Peso A: %.1f g", recipe.weightA);
    snprintf(bufferB, sizeof(bufferB), "Peso B: %.1f g", recipe.weightB);
    std::vector<String> editWeightsItems = {String(bufferA), String(bufferB), "Guardar y Volver"};
    editRecipeWeightsMenu->setItems(editWeightsItems);
    // Don't reset selection when updating content during edit
    editRecipeWeightsMenu->updateDisplay(display);
}

// --- Navigation & Display Refresh ---
void returnToMainMenu() {
    currentMenu = MENU_MAIN;
    selectedRecipeIndex = -1; // Clear selection
    updateDisplayForCurrentMenu();
}

void updateDisplayForCurrentMenu() {
    switch (currentMenu) {
        case MENU_MAIN:
            mainMenu->updateDisplay(display);
            break;
        case MENU_RECIPES:
            updateRecipeMenu(); // This calls updateDisplay inside
            break;
        case MENU_EDIT_RECIPES:
            updateEditRecipesMenu(); // This calls updateDisplay inside
            break;
        case MENU_EDIT_RECIPE_WEIGHTS:
             if (selectedRecipeIndex >= 0 && selectedRecipeIndex < recipes.size()) {
                updateEditRecipeWeightsMenu(recipes[selectedRecipeIndex]); // Calls updateDisplay
             } else {
                 returnToMainMenu(); // Invalid state
             }
            break;
        case MENU_CALIBRATE_CONFIRM:
            createConfirmMenu("Iniciar Calibracion?");
            confirmMenu->updateDisplay(display);
            break;
        case MENU_ZERO_CONFIRM:
             createConfirmMenu("Poner a Cero?");
             confirmMenu->updateDisplay(display);
            break;
        // Other states might have specific display updates or use showMessage
        default:
            // Assume states like WEIGHING, TEST_RUNNING update display themselves or via showMessage
            break;
    }
}


// --- State Handlers ---

void handleDispensing() {
    unsigned long currentMillis = millis();

    // Update weight periodically
    if (currentMillis - lastWeightUpdateTime >= SENSOR_UPDATE_INTERVAL) {
        lastWeightUpdateTime = currentMillis;

        if (sensor.update()) { // Get new reading
             weightData.current = sensor.getWeight();
        } else {
             // Sensor error during dispensing? Critical!
             motor.reset(); // Stop everything
             showTimedMessage("Error de Sensor!", "Dispensado Detenido", 3000, MENU_MAIN);
             currentMenu = MENU_MAIN; // Force state change out of dispensing
             return;
        }

        bool motorA_active = motor.getState().isPumpingA;
        bool motorB_active = motor.getState().isPumpingB;

        // --- State Logic ---
        if (motorA_active && !weightData.goalReachedA) {
            // --- Pumping A ---
            if (weightData.current >= weightData.targetA) {
                // Goal A Reached
                motor.stopPumpingA();
                weightData.goalReachedA = true;
                Serial.println("Goal A Reached");

                // Check if B needs to run
                if (weightData.targetB > 0) {
                    motor.startPumpingB(); // Start B immediately
                } else {
                    // Only A was needed, dispensing finished
                    weightData.goalReachedB = true; // Mark B as done too
                    motor.reset(); // Ensure motors stop and disable
                    showTimedMessage("Dispensado Completo", "", MESSAGE_DISPLAY_DURATION, MENU_RECIPES);
                    currentMenu = MENU_RECIPES; // Go back to recipe list
                    updateDisplayForCurrentMenu();
                    return; // Exit handleDispensing
                }
            } else {
                // Still pumping A, adjust speed
                float progressPercent = (weightData.targetA > 0) ? (weightData.current / weightData.targetA) * 100.0f : 100.0f;
                float speed = Config::Motor::MAX_SPEED;
                if (progressPercent >= Config::Motor::SLOW_THRESHOLD_PERCENT) {
                    speed *= Config::Motor::SLOW_SPEED_FACTOR;
                }
                speed = max(speed, Config::Motor::MIN_SPEED); // Ensure minimum speed
                motor.setSpeedA(speed);
            }
        } else if (motorB_active && !weightData.goalReachedB) {
             // --- Pumping B ---
             // Target for B is the total weight
             float totalTarget = weightData.targetA + weightData.targetB;
             if (weightData.current >= totalTarget) {
                 // Goal B Reached (Total goal reached)
                 motor.stopPumpingB();
                 weightData.goalReachedB = true;
                 Serial.println("Goal B Reached");

                 // Dispensing finished
                 motor.reset(); // Ensure motors stop and disable
                 showTimedMessage("Dispensado Completo", "", MESSAGE_DISPLAY_DURATION, MENU_RECIPES);
                 currentMenu = MENU_RECIPES; // Go back to recipe list
                 updateDisplayForCurrentMenu();
                 return; // Exit handleDispensing
             } else {
                 // Still pumping B, adjust speed
                 // Progress calculation needs careful thought: relative to start of B
                 float weightDispensedB = weightData.current - weightData.targetA;
                 float progressPercent = (weightData.targetB > 0) ? (weightDispensedB / weightData.targetB) * 100.0f : 100.0f;
                 float speed = Config::Motor::MAX_SPEED;
                 if (progressPercent >= Config::Motor::SLOW_THRESHOLD_PERCENT) {
                    speed *= Config::Motor::SLOW_SPEED_FACTOR;
                 }
                 speed = max(speed, Config::Motor::MIN_SPEED);
                 motor.setSpeedB(speed);
             }
        }
        // --- End State Logic ---
    } // End weight update check

    // Update display periodically
    if (currentMillis - lastDisplayUpdateTime >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdateTime = currentMillis;
        display.update(weightData, motor.getState()); // Pass current data
    }
}


void handleMainMenu(Button btn) {
    mainMenu->navigate(btn); // Handle navigation UP/DOWN

    if (btn == BUTTON_SELECT) {
        int selected = mainMenu->getSelectedIndex();
        switch (selected) {
            case 0: // Recetas
                if (!recipes.empty()) {
                    currentMenu = MENU_RECIPES;
                    updateRecipeMenu(); // Update content and display
                } else {
                    showTimedMessage("No hay recetas!", "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
                }
                break;
            case 1: // Editar Recetas
                currentMenu = MENU_EDIT_RECIPES;
                updateEditRecipesMenu(); // Update content and display
                break;
            case 2: // Calibrar
                currentMenu = MENU_CALIBRATE_CONFIRM;
                updateDisplayForCurrentMenu(); // Show confirmation
                break;
            case 3: // Poner a Cero
                currentMenu = MENU_ZERO_CONFIRM;
                updateDisplayForCurrentMenu(); // Show confirmation
                break;
            case 4: // Pesaje
                currentMenu = MENU_WEIGHING;
                // Display handled within handleWeighingMenu based on sensor readings
                break;
            case 5: // Test
                currentMenu = MENU_TEST_SETTINGS;
                testPumpSpeed = 500.0f; // Reset test speed
                // Display handled within handleTestSettings
                break;
            default:
                break; // Should not happen
        }
    } else if (btn != BUTTON_NONE) {
        mainMenu->updateDisplay(display); // Update display on UP/DOWN
    }
}

void handleRecipesMenu(Button btn) {
     // Handle "Volver" if recipes list is empty
    if (recipes.empty() && recipeMenu->getItemCount() == 2) { // "No hay recetas" + "Volver"
        if (btn == BUTTON_SELECT || btn == BUTTON_UP || btn == BUTTON_DOWN) { // Any button returns
             returnToMainMenu();
             return;
        }
    }

    recipeMenu->navigate(btn);

    if (btn == BUTTON_SELECT) {
        int idx = recipeMenu->getSelectedIndex();
        // Check if index is within recipe bounds (not "Volver")
        if (idx >= 0 && idx < (int)recipes.size()) {
            selectedRecipeIndex = idx;
            // --- Start Dispensing Sequence ---
            showTimedMessage("Preparando...", "Poniendo a Cero", 1000, MENU_DISPENSING); // Show message while taring

            // Perform Tare before starting
            sensor.tare([](const char* msg){ /* Optional: Show tare progress? */ }); // Tare synchronously for now

            // Wait brief moment after tare before starting motors maybe?
            // delay(100); // Optional short delay

            // Reset weight data
            weightData.current = sensor.getWeight(); // Get weight immediately after tare (should be ~0)
            weightData.targetA = recipes[idx].weightA;
            weightData.targetB = recipes[idx].weightB;
            weightData.goalReachedA = false;
            weightData.goalReachedB = false;
            motor.reset(); // Ensure motors are stopped and state reset

            Serial.print("Starting Recipe: "); Serial.println(recipes[idx].name);
            Serial.print("Target A: "); Serial.print(weightData.targetA);
            Serial.print(" g, Target B: "); Serial.print(weightData.targetB); Serial.println(" g");


            // Start the first pump that has a target weight > 0
            if (weightData.targetA > 0) {
                motor.startPumpingA();
            } else if (weightData.targetB > 0) {
                // If only B is needed, tare should still work, start B directly
                motor.startPumpingB();
                 weightData.goalReachedA = true; // Mark A as done if target is 0
            } else {
                // Both targets are 0 or less? Error or just finish immediately.
                showTimedMessage("Receta Vacia!", "", MESSAGE_DISPLAY_DURATION, MENU_RECIPES);
                return; // Don't enter dispensing state
            }
            // Set state AFTER starting pump (or deciding not to)
            currentMenu = MENU_DISPENSING;
             // Dispensing state handles its own display updates via display.update()

        } else {
            // "Volver" selected or invalid index
            returnToMainMenu();
        }
    } else if (btn != BUTTON_NONE) {
        recipeMenu->updateDisplay(display); // Update display on UP/DOWN
    }
}

// Combined handler for multi-step calibration
void handleCalibration(Button btn) {
    switch (currentMenu) {
        case MENU_CALIBRATE_CONFIRM:
            confirmMenu->navigate(btn);
            if (btn == BUTTON_SELECT) {
                if (confirmMenu->getSelectedIndex() == 0) { // "Si" selected
                    currentMenu = MENU_CALIBRATE_GET_ZERO;
                    sensor.startCalibration([](const char* msg) {
                        display.showMessage(msg, "Presione SELECT Ok"); // Show progress
                    });
                } else { // "Volver" selected
                    returnToMainMenu();
                }
            } else if (btn != BUTTON_NONE) {
                 confirmMenu->updateDisplay(display);
            }
            break;

        case MENU_CALIBRATE_GET_ZERO:
             // Waiting for user to remove weight and press SELECT
            if (btn == BUTTON_SELECT) {
                 sensor.captureCalibrationStep1(); // Captures zero, shows next prompt via callback
                 // Now waiting for user to place weight
                 currentMenu = MENU_CALIBRATE_GET_WEIGHT;
            }
             // Display is handled by the callback in captureCalibrationStep1
            break;

        case MENU_CALIBRATE_GET_WEIGHT:
            // Waiting for user to place known weight and press SELECT
             if (btn == BUTTON_SELECT) {
                 sensor.captureCalibrationStep2(Config::System::DEFAULT_CALIBRATION_WEIGHT, [](const char* msg) {
                     // This callback happens *after* calculation
                     showTimedMessage(msg, "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
                 });
                 // showTimedMessage will handle the transition back to MAIN
                 currentMenu = MENU_MAIN; // Set state conceptually, message handles display/delay
             }
             // Display is handled by the callback in captureCalibrationStep1 or step2
            break;
         default: // Should not happen
              returnToMainMenu();
            break;
    }
}

void handleZeroConfirm(Button btn) {
    confirmMenu->navigate(btn);
    if (btn == BUTTON_SELECT) {
        if (confirmMenu->getSelectedIndex() == 0) { // "Si" selected
             // Use showTimedMessage to display progress and return
             sensor.tare([](const char* msg){
                 // This callback might happen quickly, use timed message for visibility
                 showTimedMessage(msg, "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
             });
             currentMenu = MENU_MAIN; // Set state conceptually, message handles display/delay
        } else { // "Volver" selected
            returnToMainMenu();
        }
    } else if (btn != BUTTON_NONE) {
         confirmMenu->updateDisplay(display);
    }
}


void handleEditRecipesMenu(Button btn) {
    // Handle "Volver" if recipes list is empty
    if (recipes.empty() && editRecipesMenu->getItemCount() == 2) { // "No hay recetas" + "Volver"
        if (btn == BUTTON_SELECT || btn == BUTTON_UP || btn == BUTTON_DOWN) { // Any button returns
             returnToMainMenu();
             return;
        }
    }

    editRecipesMenu->navigate(btn);
    if (btn == BUTTON_SELECT) {
        int idx = editRecipesMenu->getSelectedIndex();
        int recipeCount = recipes.size();

        // Check if index is within recipe bounds (not "Volver" or "Add New")
        if (idx >= 0 && idx < recipeCount) {
            // Edit Existing Recipe
            currentMenu = MENU_EDIT_RECIPE_WEIGHTS;
            selectedRecipeIndex = idx;
            editRecipeWeightsMenu->reset(); // Reset selection within the sub-menu
            updateEditRecipeWeightsMenu(recipes[selectedRecipeIndex]); // Update display
        } else if (idx == recipeCount) { // Assuming "Volver" is the last item
             // "Volver" selected
             returnToMainMenu();
        }
         // TODO: Handle "Add New Recipe" if implemented
    } else if (btn != BUTTON_NONE) {
        editRecipesMenu->updateDisplay(display); // Update display on UP/DOWN
    }
}

void handleEditRecipeWeights(Button btn) {
    if (selectedRecipeIndex < 0 || selectedRecipeIndex >= recipes.size()) {
        // Invalid state, should not happen
        Serial.println("Error: Invalid selectedRecipeIndex in handleEditRecipeWeights");
        returnToMainMenu();
        return;
    }

    Recipe &recipe = recipes[selectedRecipeIndex];
    editRecipeWeightsMenu->navigate(btn); // Use standard navigate for SELECT behavior

    int sel = editRecipeWeightsMenu->getSelectedIndex();

    // Use UP/DOWN for value modification ONLY WHEN A or B is selected
    if (sel == 0) { // Editing Weight A
        if (btn == BUTTON_UP) recipe.weightA += 0.1f;
        else if (btn == BUTTON_DOWN && recipe.weightA >= 0.1f) recipe.weightA -= 0.1f;
        else if (btn == BUTTON_DOWN) recipe.weightA = 0.0f; // Allow setting to zero
    } else if (sel == 1) { // Editing Weight B
        if (btn == BUTTON_UP) recipe.weightB += 0.1f;
        else if (btn == BUTTON_DOWN && recipe.weightB >= 0.1f) recipe.weightB -= 0.1f;
         else if (btn == BUTTON_DOWN) recipe.weightB = 0.0f; // Allow setting to zero
    }

    // Round weights to one decimal place to avoid floating point artifacts
    recipe.weightA = roundf(recipe.weightA * 10.0f) / 10.0f;
    recipe.weightB = roundf(recipe.weightB * 10.0f) / 10.0f;

    // SELECT button action: Move to next item or Save
    if (btn == BUTTON_SELECT) {
        if (sel == 2) { // "Guardar y Volver" selected
            if (recipeManager.saveRecipes()) { // Assumes recipeManager._recipes is updated implicitly
                showTimedMessage("Receta Guardada", "", MESSAGE_DISPLAY_DURATION, MENU_EDIT_RECIPES);
            } else {
                showTimedMessage("Error al Guardar", "", MESSAGE_DISPLAY_DURATION, MENU_EDIT_RECIPES);
            }
            currentMenu = MENU_EDIT_RECIPES; // Set state conceptually
            return; // Let timed message handle the display transition
        }
        // else: SELECT on A or B moves to the next item (handled by navigate)
    }

     // Update display if any button was pressed (or just always update)
    if (btn != BUTTON_NONE) {
        updateEditRecipeWeightsMenu(recipe); // Update display with new values/selection
    }
}

void handleWeighingMenu(Button btn) {
    if (btn == BUTTON_SELECT) {
        returnToMainMenu();
        return; // Exit immediately after returning
    }

    // Continuously update weight display
    // Use sensor.getWeight() as sensor.update() is called in main loop
    float w = sensor.getWeight();
    // Apply thresholding if needed (getWeight already does it based on current implementation)
    // if (abs(w) < Config::System::MIN_WEIGHT_THRESHOLD) w = 0.0f;

    char weightBuffer[32];
    snprintf(weightBuffer, sizeof(weightBuffer), "Peso: %.1f g", w);
    display.showMessage(weightBuffer, "Presione SELECT para volver"); // Continuously overwrite display
}


void handleTestSettings(Button btn) {
    float step = 50.0f; // Increment step

    if (btn == BUTTON_UP) {
        testPumpSpeed += step;
        if (testPumpSpeed > Config::Motor::MAX_SPEED) testPumpSpeed = Config::Motor::MAX_SPEED;
    } else if (btn == BUTTON_DOWN) {
        testPumpSpeed -= step;
        if (testPumpSpeed < Config::Motor::MIN_SPEED) testPumpSpeed = Config::Motor::MIN_SPEED; // Ensure min speed
    } else if (btn == BUTTON_SELECT) {
        // --- Start Test ---
        currentMenu = MENU_TEST_RUNNING;
        display.showMessage("Iniciando Test...", "Presione SELECT para detener"); // Show temp message

        char filename[16];
        int fileIndex = 0;
        do {
            snprintf(filename, sizeof(filename), "/test%03d.csv", fileIndex++);
        } while (SD.exists(filename) && fileIndex < 1000);

        if (fileIndex >= 1000) {
            Serial.println("Demasiados archivos de prueba");
            showTimedMessage("Error: Max Archivos", "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
            return;
        }

        testDataFile = SD.open(filename, FILE_WRITE);
        if (!testDataFile) {
            Serial.println("Error al abrir archivo de prueba");
             showTimedMessage("Error al abrir Archivo", "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
            return;
        }
        Serial.print("Archivo de prueba creado: "); Serial.println(filename);

        testDataFile.println("Time(ms),Weight(g),SetSpeed");
        testDataFile.flush(); // Ensure header is written

        motor.reset();      // Ensure normal pumps are stopped
        sensor.tare();      // Tare before starting test
        motor.setTestMode(true);
        motor.setTestSpeed(testPumpSpeed); // Set the speed for motor A

        testStartTime = millis();
        lastTestDataTime = testStartTime;
        // Display will be updated within handleTestRunning state

        return; // Exit settings handler
    }

    // Update display if UP or DOWN was pressed
    if (btn == BUTTON_UP || btn == BUTTON_DOWN) {
        char speedBuffer[32];
        snprintf(speedBuffer, sizeof(speedBuffer), "Velocidad: %.0f", testPumpSpeed);
        display.showMessage(speedBuffer, "SELECT para iniciar");
    } else if (btn == BUTTON_NONE) {
         // Keep showing the current speed if no button pressed
         char speedBuffer[32];
         snprintf(speedBuffer, sizeof(speedBuffer), "Velocidad: %.0f", testPumpSpeed);
         display.showMessage(speedBuffer, "SELECT para iniciar");
    }
}


void handleTestRunning(Button btn) {
     unsigned long currentTime = millis();

    // Check for STOP button first
    if (btn == BUTTON_SELECT) {
        motor.setTestSpeed(0);    // Stop motor
        motor.setTestMode(false); // Exit test mode
        motor.reset();            // Ensure disable happens if needed
        if (testDataFile) {
            testDataFile.flush();
            testDataFile.close();
            Serial.println("Archivo de prueba cerrado.");
        }
        showTimedMessage("Test Completado", "", MESSAGE_DISPLAY_DURATION, MENU_MAIN);
        // State transition handled by timed message
        return;
    }

    // Log data periodically
    if (currentTime - lastTestDataTime >= TEST_DATA_INTERVAL) {
        lastTestDataTime = currentTime;

        // sensor.update() is called in main loop
        float w = sensor.getWeight();
        // Thresholding applied in getWeight()

        unsigned long elapsedTime = currentTime - testStartTime;

        // Log to SD Card
        if (testDataFile) {
            testDataFile.print(elapsedTime);
            testDataFile.print(",");
            testDataFile.print(w, 2); // Log with 2 decimal places for finer detail
            testDataFile.print(",");
            testDataFile.println(testPumpSpeed);
            // Optionally flush periodically, but can slow things down
            // if (elapsedTime % 1000 < TEST_DATA_INTERVAL) testDataFile.flush();
        }

         // Update display
         char buffer1[32], buffer2[32];
         snprintf(buffer1, sizeof(buffer1),"Test: %.1f g", w);
         snprintf(buffer2, sizeof(buffer2),"T: %lu s | SELECT Detener", elapsedTime / 1000);
         display.showMessage(buffer1, buffer2);
    }

    // motor.run() is called in the main loop, handles stepping
}

// --- Helper Functions ---
void createConfirmMenu(const char* title, const char* item1, const char* item2) {
    std::vector<String> items = {String(item1), String(item2)};
    confirmMenu->setTitle(title); // Assuming Menu class has setTitle or similar
    confirmMenu->setItems(items);
    confirmMenu->reset(); // Reset selection to the first item
}