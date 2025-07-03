#include "Menu.h"

Menu::Menu(const std::vector<String>& items, const char* title, int visibleLines)
    : _items(items),
      _title(title ? title : ""), // Initialize String, handle nullptr
      _visibleLines(visibleLines),
      _selectedIndex(0),
      _topIndex(0)
{}

void Menu::navigate(Button btn) {
    int itemCount = _items.size();
    if (itemCount == 0) return; // No items to navigate

    if (btn == BUTTON_UP) {
        if (_selectedIndex > 0) {
            _selectedIndex--;
            // Scroll up if necessary
            if (_selectedIndex < _topIndex) {
                _topIndex = _selectedIndex;
            }
        } else {
            // Optionally wrap around to the bottom
            // _selectedIndex = itemCount - 1;
            // _topIndex = max(0, itemCount - _visibleLines);
        }
    } else if (btn == BUTTON_DOWN) {
        if (_selectedIndex < itemCount - 1) {
            _selectedIndex++;
            // Scroll down if necessary
            if (_selectedIndex >= _topIndex + _visibleLines) {
                _topIndex = _selectedIndex - _visibleLines + 1;
            }
        } else {
             // Optionally wrap around to the top
             // _selectedIndex = 0;
             // _topIndex = 0;
        }
    }
    // BUTTON_SELECT is handled by the state machine, not directly here
}


int Menu::getSelectedIndex() const {
    return _selectedIndex;
}

void Menu::setSelectedIndex(int idx) {
    if (idx >= 0 && idx < (int)_items.size()) {
        _selectedIndex = idx;
        // Adjust topIndex if selectedIndex is out of view
        if (_selectedIndex < _topIndex) {
            _topIndex = _selectedIndex;
        } else if (_selectedIndex >= _topIndex + _visibleLines) {
             _topIndex = _selectedIndex - _visibleLines + 1;
        }
    }
}

int Menu::getTopIndex() const {
    return _topIndex;
}

void Menu::reset() {
    _selectedIndex = 0;
    _topIndex = 0;
}

void Menu::updateDisplay(Display &display) const {
    // Pass C-string version of title if not empty, otherwise nullptr
    display.showList(_items, _selectedIndex, _topIndex, _title.isEmpty() ? nullptr : _title.c_str(), _visibleLines);
}

void Menu::setItems(const std::vector<String>& items) {
    _items = items;
    // Adjust selection if it's now out of bounds
    if (_selectedIndex >= (int)_items.size()) {
         _selectedIndex = max(0, (int)_items.size() - 1);
    }
     _topIndex = 0; // Reset scroll position when items change usually makes sense
     // Consider if _selectedIndex should also be reset, depends on use case
     // reset(); // uncomment if selection should always reset too
}

void Menu::setTitle(const char* title) {
    _title = title ? title : "";
}

int Menu::getItemCount() const {
    return _items.size();
}