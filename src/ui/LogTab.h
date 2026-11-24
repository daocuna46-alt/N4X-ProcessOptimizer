#pragma once
#include "ProcessTab.h"
#include "../core/Logger.h"

namespace n4x::ui {

class LogTab {
public:
    void Render(UiState& state);
private:
    int levelFilter_ = 0; // 0 all,1 info,2 warn,3 error
    bool autoScroll_ = true;
    char exportPath_[260] = "N4X_ProcessOptimizer_export.log";
};

} // namespace n4x::ui
