#pragma once

#include <windows.h>
#include <string>

// Pops up a small window with an edit box, OK/Cancel.
// Returns user text if OK was pressed, or empty string on Cancel/close.
std::string ShowInputDialog(
    HINSTANCE hInst,
    const std::wstring& title = L"Enter Text",
    int width = 300,
    int height = 120
);
