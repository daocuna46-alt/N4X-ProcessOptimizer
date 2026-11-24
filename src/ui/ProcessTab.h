#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <Windows.h>
#include "../core/ProcessManager.h"

namespace n4x::ui {

struct UiState {
    std::atomic<bool> running{true};
    std::atomic<bool> requestRestore{false};
    std::atomic<bool> requestSave{false};
    std::atomic<bool> requestExit{false};
    std::atomic<bool> dirtyConfig{false};
};

class ProcessTab {
public:
    void Render(UiState& state, bool isAdmin);
private:
    char searchBuf_[128] = {};
    int filterMode_ = 0; // 0 All, 1 Selected, 2 Unselected, 3 Protected
    int sortColumn_ = 1; // 0 sel,1 name,2 pid,3 cpu,4 ram,5 prio
    bool sortAsc_ = true;
    std::vector<ProcessInfo> cache_;
    double lastRefresh_ = 0.0;
};

} // namespace n4x::ui
