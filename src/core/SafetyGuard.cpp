#include "SafetyGuard.h"
#include "Logger.h"
#include <algorithm>
#include <TlHelp32.h>

namespace n4x {

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

SafetyGuard& SafetyGuard::Instance() {
    static SafetyGuard s;
    return s;
}

void SafetyGuard::Init() {
    blacklist_.clear();
    const char* names[] = {
        "system", "idle", "registry", "memory compression",
        "csrss.exe", "wininit.exe", "winlogon.exe", "services.exe", "lsass.exe",
        "smss.exe", "svchost.exe", "explorer.exe", "dwm.exe", "fontdrvhost.exe",
        "ctfmon.exe", "audiodg.exe", "msmpeng.exe", "nissrv.exe",
        "securityhealthservice.exe", "spoolsv.exe", "sihost.exe", "taskhostw.exe",
        "runtimebroker.exe", "searchindexer.exe", "wudfhost.exe", "conhost.exe",
        "logonui.exe", "userinit.exe", "shellexperiencehost.exe",
        "startmenuexperiencehost.exe", "textinputhost.exe"
    };
    for (auto n : names) blacklist_.insert(ToLower(n));
    Logger::Instance().Info("SafetyGuard initialized with hard blacklist");
}

bool SafetyGuard::IsBlacklistedName(const std::string& exeNameLower) const {
    return blacklist_.find(ToLower(exeNameLower)) != blacklist_.end();
}

bool SafetyGuard::IsSystemSession(DWORD pid) const {
    DWORD sessionId = 0xFFFFFFFF;
    if (!ProcessIdToSessionId(pid, &sessionId)) {
        // If we can't determine, be conservative: treat as protected when pid is 0-4 or fail.
        DWORD err = GetLastError();
        Logger::Instance().Warn("ProcessIdToSessionId failed for pid " + std::to_string(pid) + " err=" + std::to_string(err));
        return true;
    }
    return sessionId == 0;
}

bool SafetyGuard::IsProtected(DWORD pid, const std::string& exeNameLower) const {
    if (pid <= 4) return true;
    if (IsBlacklistedName(exeNameLower)) return true;
    if (IsSystemSession(pid)) return true;
    return false;
}

bool SafetyGuard::IsAllowedTargetPriority(DWORD priorityClass) {
    // NEVER allow REALTIME.
    if (priorityClass == REALTIME_PRIORITY_CLASS) return false;
    switch (priorityClass) {
        case IDLE_PRIORITY_CLASS:
        case BELOW_NORMAL_PRIORITY_CLASS:
        case NORMAL_PRIORITY_CLASS:
        case ABOVE_NORMAL_PRIORITY_CLASS:
        case HIGH_PRIORITY_CLASS:
            return true;
        default:
            return false;
    }
}

DWORD SafetyGuard::ClampPriority(DWORD desired) {
    if (desired == REALTIME_PRIORITY_CLASS) return HIGH_PRIORITY_CLASS;
    if (IsAllowedTargetPriority(desired)) return desired;
    return HIGH_PRIORITY_CLASS;
}

bool SafetyGuard::IsAllowedPriorityMode(const std::string& mode) {
    return mode == "ABOVE_NORMAL" || mode == "HIGH";
}

} // namespace n4x
