#pragma once
#include "ProcessTab.h"

namespace n4x::ui {

class SettingsTab {
public:
    void Render(UiState& state, bool isAdmin);
private:
    bool loaded_ = false;
    int watchdogMs_ = 3000;
    int maxWsMb_ = 2048;
    bool reserveCore0_ = true;
    int priorityIdx_ = 1; // 0 ABOVE_NORMAL, 1 HIGH
    bool themeDark_ = true;
    bool autoStart_ = false;
    bool filterShowBlacklisted_ = true;
};

} // namespace n4x::ui
