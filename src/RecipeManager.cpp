#include "RecipeManager.h"
#include <ArduinoJson.h>
#include <SD.h>
#include "Config.h"

RecipeManager::RecipeManager() {}

bool RecipeManager::loadRecipes() {
    // Check if SD card is available
    if (SD.cardType() == CARD_NONE) {
        Serial.println("Tarjeta SD no inicializada");
        return false;
    }

    if (!SD.exists(Config::System::RECIPE_FILE)) {
        Serial.println("Archivo de recetas no existe en la tarjeta SD");
        _recipes.clear(); // Ensure recipes are empty if file doesn't exist
        return false; // Indicate failure or non-existence
    }

    // Open the recipes.json file from the SD card
    File file = SD.open(Config::System::RECIPE_FILE, FILE_READ);
    if (!file) {
        Serial.println("Error al abrir archivo de recetas en la tarjeta SD");
        return false;
    }

    // Use JsonDocument (adjust size as needed, 2048 might be large)
    JsonDocument doc; // Adapts size dynamically (within limits)


    DeserializationError error = deserializeJson(doc, file);
    file.close(); // Close the file ASAP

    if (error) {
        Serial.print("Error al parsear archivo de recetas: ");
        Serial.println(error.c_str());
        return false;
    }

    // Check if the root is an object and contains the "recipes" array
    if (!doc.is<JsonObject>() || !doc["recipes"].is<JsonArray>()) {
        Serial.println("Formato de archivo de recetas invalido (esperaba objeto con array 'recipes')");
        return false;
    }

    _recipes.clear(); // Clear existing recipes before loading new ones
    JsonArray recipesArray = doc["recipes"].as<JsonArray>();
    _recipes.reserve(recipesArray.size()); // Optimize vector allocation

    for (JsonVariant v : recipesArray) {
         if (v.is<JsonObject>()) {
            JsonObject obj = v.as<JsonObject>();
            // Basic validation: Ensure required fields exist and have correct types
            // FIX: Removed deprecated containsKey checks
            if (obj["name"].is<const char*>() && // Check type directly
                obj["weightA"].is<float>() &&   // Check type directly
                obj["weightB"].is<float>())     // Check type directly
            {
                // Add further checks like non-null name if needed
                if (!obj["name"].isNull()) {
                    Recipe recipe;
                    recipe.name = obj["name"].as<String>();
                    recipe.weightA = obj["weightA"].as<float>();
                    recipe.weightB = obj["weightB"].as<float>();
                    // Optional: Add validation for weights (e.g., non-negative)
                    if(recipe.weightA >= 0.0f && recipe.weightB >= 0.0f) {
                        _recipes.push_back(recipe);
                    } else {
                         Serial.println("Omitiendo entrada de receta: Peso negativo invalido.");
                    }
                } else {
                     Serial.println("Omitiendo entrada de receta: Nombre es null.");
                }
            } else {
                 Serial.println("Omitiendo entrada de receta invalida (faltan campos/tipo incorrecto)");
            }
        } else {
             Serial.println("Omitiendo entrada de receta invalida (no es un objeto)");
        }
    }

    Serial.print("Recetas cargadas desde SD: "); Serial.println(_recipes.size());
    return true;
}

// saveRecipes and getRecipes remain the same as previously corrected
bool RecipeManager::saveRecipes() {
     // Check if SD card is available before trying to write
    if (SD.cardType() == CARD_NONE) {
        Serial.println("Tarjeta SD no disponible para guardar");
        return false;
    }

    // Use JsonDocument
    JsonDocument doc;

    // Use modern API for creating array/objects
    JsonArray recipesArray = doc["recipes"].to<JsonArray>(); // Create or get array associated with "recipes" key
    for (const auto& recipe : _recipes) {
        JsonObject recipeObj = recipesArray.add<JsonObject>(); // Add a new object to the array
        recipeObj["name"] = recipe.name;
        recipeObj["weightA"] = recipe.weightA; // ArduinoJson handles float precision
        recipeObj["weightB"] = recipe.weightB;
    }

    // Open the file for writing (overwrites existing file)
    File file = SD.open(Config::System::RECIPE_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Error al abrir archivo de recetas para escritura en la tarjeta SD");
        return false;
    }

    // Serialize the JSON document to the file
    size_t bytesWritten = serializeJson(doc, file);
    file.close(); // Close the file ASAP

    if (bytesWritten == 0) {
        Serial.println("Error al escribir recetas en el archivo (0 bytes escritos)");
        // Attempt to remove potentially corrupted file?
        // SD.remove(Config::System::RECIPE_FILE);
        return false;
    } else {
         Serial.print("Recetas guardadas en SD (");
         Serial.print(bytesWritten);
         Serial.println(" bytes)");
    }

    return true;
}


const std::vector<Recipe>& RecipeManager::getRecipes() const {
    return _recipes;
}