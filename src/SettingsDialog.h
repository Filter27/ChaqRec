#pragma once
#define NOMINMAX
#include <windows.h>
#include <functional>
#include "Settings.h"

// Modeless, native Windows trackbars. The callback updates the overlay immediately.
void ShowSettingsDialog(Settings& settings, const std::function<void()>& changed);
