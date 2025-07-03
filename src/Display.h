#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include "Config.h"
#include "Types.h"
#include <vector>
#include <Arduino.h> // For String and __FlashStringHelper

class Display {
public:
    Display();
    void begin();

    // Updates a dynamic running screen (e.g., dispensing progress)
    void update(const WeightData& data, const MotorState& motorState);

    /**
     * @brief Displays one or two lines of text centered on the screen.
     * @param message The primary message to display.
     * @param secondaryMessage An optional second line of text. Pass nullptr if no second line is needed.
     */
    void showMessage(const char* message, const char* secondaryMessage = nullptr);

    /**
     * @brief Overload of showMessage to accept Flash Strings (F macro).
     * @param message The primary message (flash string).
     * @param secondaryMessage An optional secondary message (flash string). Pass nullptr if no second line.
     */
    void showMessage(const __FlashStringHelper* message, const __FlashStringHelper* secondaryMessage = nullptr);

    /**
     * @brief Displays a scrollable, selectable list of items with an optional title.
     *
     * @param items A vector of strings representing the menu items.
     * @param selectedIndex Index of the currently selected item. Set to -1 if no item should be highlighted.
     * @param topIndex The index of the topmost visible item (used for scrolling).
     * @param title An optional title displayed at the top. Pass nullptr for no title.
     * @param visibleLines How many items to display at once (default 5).
     */
    void showList(const std::vector<String>& items, int selectedIndex = -1, int topIndex = 0, const char* title = nullptr, int visibleLines = 5);

private:
    U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI _display;

    // Helper function to draw the "Receta Completada!" message or similar static messages.
    void drawGoalMessage(uint8_t y);
};

#endif // DISPLAY_H