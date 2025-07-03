#ifndef MENU_H
#define MENU_H

#include <vector>
#include <Arduino.h> // For String
#include "Display.h"
#include "ButtonManager.h" // for the Button enum

class Menu {
public:
    Menu(const std::vector<String>& items, const char* title = nullptr, int visibleLines = 5);

    void navigate(Button btn);
    int getSelectedIndex() const;
    void setSelectedIndex(int idx);
    int getTopIndex() const;
    void reset();
    void updateDisplay(Display &display) const;
    void setItems(const std::vector<String>& items);
    void setTitle(const char* title); // Added
    int getItemCount() const; // Added

private:
    std::vector<String> _items;
    String _title; // Use String for easier updates
    int _selectedIndex;
    int _topIndex;
    int _visibleLines;
};

#endif // MENU_H