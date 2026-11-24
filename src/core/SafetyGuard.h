#pragma once
#include <string>
#include <unordered_set>
#include <Windows.h>

namespace n4x {

class SafetyGuard {
public:
    static SafetyGuard& Instance();

    void Init();

    bool IsBlacklistedName(const std::string& exeNameLower) const;
    bool IsSystemSession(DWORD pid) const;
    bool IsProtected(DWORD pid, const std::string& exeNameLower) const;

    // Returns true if this priority is allowed to be SET by us (never REALTIME).
    static bool IsAllowedTargetPriority(DWORD priorityClass);

    // Clamp priority to safe max (HIGH). Never returns REALTIME.
    static DWORD ClampPriority(DWORD desired);

    static bool IsAllowedPriorityMode(const std::string& mode);

private:
    SafetyGuard() = default;
    std::unordered_set<std::string> blacklist_;
};

} // namespace n4x
