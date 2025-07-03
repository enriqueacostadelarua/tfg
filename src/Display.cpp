#include "Display.h"
#include <Arduino.h> // Needed for max(), String, snprintf, constrain

// Define some constants for positioning (local to this file)
namespace DisplayDefaults {
    // Using 256x64 display dimensions
    constexpr int DISPLAY_WIDTH = 256;
    constexpr int DISPLAY_HEIGHT = 64;
    constexpr int TEXT_X_POS = 5; // Base X position for text (adjust if needed)
    constexpr int LINE_HEIGHT = 10; // Pixel height for menu lines / text lines (using ncenB08 font)
    constexpr int TITLE_Y_POS = 10; // Y position for menu title (top edge of text)
    constexpr int FIRST_ITEM_Y_POS = TITLE_Y_POS + LINE_HEIGHT + 4; // Y pos for first list item
    constexpr int MSG_PRIMARY_Y = DISPLAY_HEIGHT / 2 - LINE_HEIGHT / 2; // Center primary message roughly
    constexpr int MSG_SECONDARY_Y = MSG_PRIMARY_Y + LINE_HEIGHT + 2;
    constexpr int PROGRESS_Y = MSG_SECONDARY_Y; // Put progress below weight
    constexpr int GOAL_MSG_Y = PROGRESS_Y;
    constexpr int HIGHLIGHT_PADDING_Y = 1; // Padding around highlighted text
    constexpr int LIST_HIGHLIGHT_HEIGHT = LINE_HEIGHT + (2 * HIGHLIGHT_PADDING_Y);
}


Display::Display()
    : _display(U8G2_R0, Config::Pins::OLED_CS,
               Config::Pins::OLED_DC, Config::Pins::OLED_RESET) {}

void Display::begin() {
    _display.begin();
    _display.setFont(u8g2_font_ncenB08_tr); // Set default font
    _display.clearBuffer();
    _display.setCursor(DisplayDefaults::TEXT_X_POS, DisplayDefaults::MSG_PRIMARY_Y);
    _display.print("Display Iniciado");
    _display.sendBuffer();
    delay(1000); // Show message briefly
     _display.clearBuffer(); // Clear after showing initial message
     _display.sendBuffer();
}

// Update screen during dispensing
void Display::update(const WeightData& data, const MotorState& motorState) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_ncenB08_tr);
    _display.setDrawColor(1); // Ensure drawing color is active

    char buffer[45]; // Slightly larger buffer for safety

    // Always show current weight at the top
    snprintf(buffer, sizeof(buffer), "Peso Actual: %.1f g", data.current);
    _display.drawStr(DisplayDefaults::TEXT_X_POS, DisplayDefaults::TITLE_Y_POS, buffer);

    // Show progress or goal message below weight
    int infoYPos = DisplayDefaults::TITLE_Y_POS + DisplayDefaults::LINE_HEIGHT + 2;

    if (data.goalReachedA && data.goalReachedB && (data.targetA > 0 || data.targetB > 0)) {
        drawGoalMessage(DisplayDefaults::MSG_PRIMARY_Y + DisplayDefaults::LINE_HEIGHT); // Position goal message lower
    } else {
        if (motorState.isPumpingA && data.targetA > 0) {
            float percent = constrain((data.current / data.targetA) * 100.0f, 0.0f, 100.0f);
            snprintf(buffer, sizeof(buffer), "-> A: %.1f / %.1f g (%.0f%%)", data.current, data.targetA, percent);
            _display.drawStr(DisplayDefaults::TEXT_X_POS, infoYPos, buffer);
        } else if (motorState.isPumpingB && data.targetB > 0) {
             float weightB = max(0.0f, data.current - data.targetA); // Avoid negative B weight display
             float percent = constrain((weightB / data.targetB) * 100.0f, 0.0f, 100.0f);
             snprintf(buffer, sizeof(buffer), "-> B: %.1f / %.1f g (%.0f%%)", weightB, data.targetB, percent);
             _display.drawStr(DisplayDefaults::TEXT_X_POS, infoYPos, buffer);
        } else if (data.targetA > 0 || data.targetB > 0) {
            // Show targets if not actively pumping (e.g., between A and B, or before start)
             snprintf(buffer, sizeof(buffer), "Metas: A:%.1fg B:%.1fg", data.targetA, data.targetB);
             _display.drawStr(DisplayDefaults::TEXT_X_POS, infoYPos, buffer);
        } else {
             // Idle, no targets set
             _display.drawStr(DisplayDefaults::TEXT_X_POS, infoYPos, "Esperando receta...");
        }
    }

    _display.sendBuffer();
}


void Display::showMessage(const char* message, const char* secondaryMessage) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_ncenB08_tr); // Ensure default font
    _display.setDrawColor(1); // Ensure drawing color is active

    int primary_y = DisplayDefaults::MSG_PRIMARY_Y;
    bool hasSecondary = (secondaryMessage != nullptr && strlen(secondaryMessage) > 0);

    // If there's a secondary message, adjust y positions to fit both
    if (hasSecondary) {
        primary_y = DisplayDefaults::DISPLAY_HEIGHT / 2 - DisplayDefaults::LINE_HEIGHT; // Move primary up
    }

    // Draw primary message (centered)
    if (message != nullptr) { // Check message is not null
        int msgWidth = _display.getStrWidth(message);
        int xPos = max(0, (DisplayDefaults::DISPLAY_WIDTH - msgWidth) / 2);
        _display.drawStr(xPos, primary_y, message);
    }

    // Draw optional secondary message below primary (centered)
    if (hasSecondary) {
        int secondary_y = primary_y + DisplayDefaults::LINE_HEIGHT + DisplayDefaults::HIGHLIGHT_PADDING_Y + 2; // Add slight gap
        int msgWidth = _display.getStrWidth(secondaryMessage);
        int xPos = max(0, (DisplayDefaults::DISPLAY_WIDTH - msgWidth) / 2);
        _display.drawStr(xPos, secondary_y, secondaryMessage);
    }

    _display.sendBuffer();
}

// Overload implementation for Flash Strings
void Display::showMessage(const __FlashStringHelper* message, const __FlashStringHelper* secondaryMessage) {
    // Convert flash strings to temporary String objects to use their c_str() method
    // This copies the string from flash to RAM temporarily.
    String msgStr = (message == nullptr) ? "" : String(message);
    String secMsgStr = (secondaryMessage == nullptr) ? "" : String(secondaryMessage);

    // Call the original function with const char* versions
    // Pass nullptr if the string is empty after conversion.
    showMessage(msgStr.c_str(), secMsgStr.isEmpty() ? nullptr : secMsgStr.c_str());
}


void Display::showList(const std::vector<String>& items, int selectedIndex, int topIndex, const char* title, int visibleLines) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_ncenB08_tr); // Ensure default font
    _display.setDrawColor(1);

    int y = DisplayDefaults::TITLE_Y_POS; // Start with title Y position

    // If there is a title, display it centered and adjust y
    if (title != nullptr && strlen(title) > 0) {
         int titleWidth = _display.getStrWidth(title);
         int titleX = max(0, (DisplayDefaults::DISPLAY_WIDTH - titleWidth) / 2);
        _display.drawStr(titleX, y, title);
        y = DisplayDefaults::FIRST_ITEM_Y_POS; // Move down for the list items
    } else {
        // No title, start list higher
        y = DisplayDefaults::TITLE_Y_POS; // Start list items near the top
    }


    int menuSize = (int)items.size();
    if (menuSize == 0) {
        // Adjust Y position if there was no title
        if (!(title != nullptr && strlen(title) > 0)) {
            y = DisplayDefaults::MSG_PRIMARY_Y; // Center vertically if no title and no items
        }
        _display.drawStr(DisplayDefaults::TEXT_X_POS, y, "Lista Vacia");
        _display.sendBuffer();
        return;
    }

     // Adjust visibleLines based on available height
    int availableHeight = DisplayDefaults::DISPLAY_HEIGHT - y;
    int maxPossibleLines = availableHeight / DisplayDefaults::LINE_HEIGHT;
    visibleLines = min(visibleLines, maxPossibleLines); // Don't try to draw more lines than fit
    visibleLines = max(1, visibleLines); // Ensure at least one line is shown if possible


    // Ensure topIndex is valid, considering the actual visibleLines
     if (menuSize <= visibleLines) {
        topIndex = 0; // If all items fit, start at the top
     } else {
         // Prevent topIndex from showing empty lines at the bottom
        topIndex = max(0, min(topIndex, menuSize - visibleLines));
     }

     // Ensure selectedIndex is valid
     selectedIndex = max(0, min(selectedIndex, menuSize - 1));


    int endIndex = topIndex + visibleLines;
    if (endIndex > menuSize) {
        endIndex = menuSize;
    }


    for (int i = topIndex; i < endIndex; ++i) {
        // Stop drawing if we run out of vertical space
        if (y > DisplayDefaults::DISPLAY_HEIGHT - DisplayDefaults::LINE_HEIGHT) {
             break;
        }

        bool highlight = (i == selectedIndex);

        if (highlight) {
            // Draw highlight box slightly taller than font, full width
             _display.setDrawColor(1); // Ensure box color is active
             _display.drawBox(0, y - DisplayDefaults::LINE_HEIGHT + DisplayDefaults::HIGHLIGHT_PADDING_Y,
                             DisplayDefaults::DISPLAY_WIDTH, DisplayDefaults::LIST_HIGHLIGHT_HEIGHT);
            _display.setDrawColor(0); // Set text color to 'inverted' (0 for black on white box)
        } else {
            _display.setDrawColor(1); // Normal text color (white/active pixel)
        }

        // Draw the text itself
        // Note: drawStr uses top-left corner, print uses baseline. Using drawStr for consistency.
        // Adjust x position slightly for padding inside highlight
        int textX = DisplayDefaults::TEXT_X_POS;
        // _display.setCursor(textX, y); // Set cursor for print if using print
        _display.drawStr(textX, y, items[i].c_str()); // Use drawStr with c_str()

        _display.setDrawColor(1); // Reset draw color back to normal

        y += DisplayDefaults::LINE_HEIGHT; // Move Y down for the next item
    }

    // Optional: Add scroll indicators (up/down arrows)
    _display.setDrawColor(1);
    if (topIndex > 0) {
        _display.drawTriangle(DisplayDefaults::DISPLAY_WIDTH - 8, DisplayDefaults::TITLE_Y_POS - 5,      // Point 1 (Top point)
                               DisplayDefaults::DISPLAY_WIDTH - 12, DisplayDefaults::TITLE_Y_POS,     // Point 2 (Bottom left)
                               DisplayDefaults::DISPLAY_WIDTH - 4, DisplayDefaults::TITLE_Y_POS);     // Point 3 (Bottom right)
    }
    if (endIndex < menuSize) {
         _display.drawTriangle(DisplayDefaults::DISPLAY_WIDTH - 12, DisplayDefaults::DISPLAY_HEIGHT - 5, // Point 1 (Top Left)
                               DisplayDefaults::DISPLAY_WIDTH - 4, DisplayDefaults::DISPLAY_HEIGHT - 5,  // Point 2 (Top Right)
                               DisplayDefaults::DISPLAY_WIDTH - 8, DisplayDefaults::DISPLAY_HEIGHT);    // Point 3 (Bottom point)
    }


    _display.sendBuffer();
}

// Helper to draw the static "Goal Reached" message
void Display::drawGoalMessage(uint8_t y) {
    const char* message = "Receta Completada!";
    int msgWidth = _display.getStrWidth(message);
    int xPos = max(0, (DisplayDefaults::DISPLAY_WIDTH - msgWidth) / 2);
     _display.setDrawColor(1); // Ensure active color
    _display.drawStr(xPos, y, message);
}