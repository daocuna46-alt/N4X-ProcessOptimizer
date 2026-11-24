#include "ProcessManager.h"
#include "Logger.h"
#include "SafetyGuard.h"
#include "../utils/WinUtil.h"
#include <TlHelp32.h>
#include <Psapi.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "Psapi.lib")

namespace n4x {

static ULONGLONG FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

ProcessManager& ProcessManager::Instance() {
    static ProcessManager pm;
    return pm;
}

ProcessManager::~ProcessManager() { Stop(); }

bool ProcessManager::IsAdmin() const { return isAdmin_; }

void ProcessManager::Start() {
    if (running_.exchange(true)) return;
    isAdmin_ = winutil::IsUserAdmin();
    Logger::Instance().Info(std::string("ProcessManager started. Admin=") + (isAdmin_ ? "true" : "false"));
    scannerThread_ = std::thread(&ProcessManager::ScannerLoop, this);
    optimizerThread_ = std::thread(&ProcessManager::OptimizerLoop, this);
}

void ProcessManager::Stop() {
    if (!running_.exchange(false)) return;
    Logger::Instance().Info("ProcessManager stopping...");
    if (scannerThread_.joinable()) scannerThread_.join();
    if (optimizerThread_.joinable()) optimizerThread_.join();
    Logger::Instance().Info("ProcessManager stopped");
}

std::vector<ProcessInfo> ProcessManager::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(snapshotMtx_);
    return snapshot_;
}

OptimizeStats ProcessManager::GetLastStats() const {
    std::lock_guard<std::mutex> lock(statsMtx_);
    return lastStats_;
}

void ProcessManager::SetSelectedNames(const std::vector<std::string>& names) {
    std::lock_guard<std::mutex> lock(selectionMtx_);
    selectedNames_.clear();
    for (const auto& n : names) selectedNames_.insert(winutil::ToLower(n));
}

std::vector<std::string> ProcessManager::GetSelectedNames() const {
    std::lock_guard<std::mutex> lock(selectionMtx_);
    return std::vector<std::string>(selectedNames_.begin(), selectedNames_.end());
}

void ProcessManager::ToggleSelection(const std::string& name, bool selected) {
    std::lock_guard<std::mutex> lock(selectionMtx_);
    std::string k = winutil::ToLower(name);
    if (selected) selectedNames_.insert(k);
    else selectedNames_.erase(k);
}

void ProcessManager::RequestRestoreAll() { restoreRequested_.store(true); }
bool ProcessManager::IsRestoreRequested() const { return restoreRequested_.load(); }

std::string ProcessManager::PriorityToString(DWORD p) {
    switch (p) {
        case IDLE_PRIORITY_CLASS: return "IDLE";
        case BELOW_NORMAL_PRIORITY_CLASS: return "BELOW_NORMAL";
        case NORMAL_PRIORITY_CLASS: return "NORMAL";
        case ABOVE_NORMAL_PRIORITY_CLASS: return "ABOVE_NORMAL";
        case HIGH_PRIORITY_CLASS: return "HIGH";
        case REALTIME_PRIORITY_CLASS: return "REALTIME"; // should never be set by us
        default: return "UNKNOWN";
    }
}

DWORD ProcessManager::PriorityFromMode(const std::string& mode) {
    if (mode == "ABOVE_NORMAL") return ABOVE_NORMAL_PRIORITY_CLASS;
    return HIGH_PRIORITY_CLASS; // default safe max
}

DWORD_PTR ProcessManager::ComputeAffinityMask(const AppConfig& cfg) const {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    DWORD_PTR sysMask = (DWORD_PTR)si.dwActiveProcessorMask;
    if (sysMask == 0) {
        // fallback: all processors reported by dwNumberOfProcessors
        if (si.dwNumberOfProcessors >= sizeof(DWORD_PTR) * 8) sysMask = ~(DWORD_PTR)0;
        else sysMask = ((DWORD_PTR)1 << si.dwNumberOfProcessors) - 1;
    }
    if (cfg.reserveCore0 && si.dwNumberOfProcessors > 1) {
        DWORD_PTR withoutCore0 = sysMask & ~((DWORD_PTR)1);
        if (withoutCore0 != 0) return withoutCore0;
    }
    return sysMask;
}

void ProcessManager::ScannerLoop() {
    while (running_.load()) {
        auto snap = ScanProcesses();
        {
            std::lock_guard<std::mutex> lock(snapshotMtx_);
            snapshot_ = std::move(snap);
        }
        for (int i = 0; i < 20 && running_.load(); ++i) Sleep(100); // ~2s
    }
}

std::vector<ProcessInfo> ProcessManager::ScanProcesses() {
    std::vector<ProcessInfo> out;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        Logger::Instance().Error("CreateToolhelp32Snapshot failed: " + winutil::LastErrorString(GetLastError()));
        return out;
    }

    // System times for CPU%
    FILETIME idleFT{}, kernelFT{}, userFT{};
    double sysDelta = 0;
    ULONGLONG sysIdle = 0, sysKernel = 0, sysUser = 0;
    if (GetSystemTimes(&idleFT, &kernelFT, &userFT)) {
        sysIdle = FileTimeToULL(idleFT);
        sysKernel = FileTimeToULL(kernelFT);
        sysUser = FileTimeToULL(userFT);
    }

    std::unordered_set<std::string> selected;
    {
        std::lock_guard<std::mutex> lock(selectionMtx_);
        selected = selectedNames_;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(hSnap, &pe)) {
        Logger::Instance().Error("Process32FirstW failed: " + winutil::LastErrorString(GetLastError()));
        CloseHandle(hSnap);
        return out;
    }

    // Gather current cpu samples
    struct ProcCpu { ULONGLONG k=0,u=0; bool ok=false; };
    std::unordered_map<DWORD, ProcCpu> cpuNow;

    do {
        ProcessInfo pi;
        pi.pid = pe.th32ProcessID;
        pi.name = winutil::WideToUtf8(pe.szExeFile);
        std::string lower = winutil::ToLower(pi.name);
        pi.sessionId = 0xFFFFFFFF;
        ProcessIdToSessionId(pi.pid, &pi.sessionId);
        pi.isProtected = SafetyGuard::Instance().IsProtected(pi.pid, lower);
        pi.isSelected = selected.find(lower) != selected.end();

        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (h) {
            wchar_t pathBuf[MAX_PATH * 2] = {};
            DWORD sz = MAX_PATH * 2;
            if (QueryFullProcessImageNameW(h, 0, pathBuf, &sz)) {
                pi.path = winutil::WideToUtf8(std::wstring(pathBuf, sz));
            }
            DWORD prio = GetPriorityClass(h);
            if (prio != 0) {
                pi.priorityClass = prio;
                pi.priorityName = PriorityToString(prio);
            } else {
                DWORD err = GetLastError();
                if (err != ERROR_ACCESS_DENIED) {
                    // log sparingly
                }
                pi.priorityClass = NORMAL_PRIORITY_CLASS;
                pi.priorityName = "N/A";
            }
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                pi.ramBytes = pmc.WorkingSetSize;
            }
            FILETIME cft{}, eft{}, kft{}, uft{};
            if (GetProcessTimes(h, &cft, &eft, &kft, &uft)) {
                cpuNow[pi.pid] = {FileTimeToULL(kft), FileTimeToULL(uft), true};
            }
            CloseHandle(h);
        } else {
            // OpenProcess failed - try to still get memory/priority via limited means; skip if access denied
            DWORD err = GetLastError();
            // log only occasionally would be nice; keep simple
            (void)err;
        }

        out.push_back(std::move(pi));
    } while (Process32NextW(hSnap, &pe));
    CloseHandle(hSnap);

    // Compute CPU%
    {
        std::lock_guard<std::mutex> lock(cpuMtx_);
        ULONGLONG sysDeltaTotal = 0;
        if (prevSys_.valid) {
            ULONGLONG dK = (sysKernel >= prevSys_.sysKernel) ? (sysKernel - prevSys_.sysKernel) : 0;
            ULONGLONG dU = (sysUser >= prevSys_.sysUser) ? (sysUser - prevSys_.sysUser) : 0;
            // kernel includes idle on Windows; total = kernel + user
            sysDeltaTotal = dK + dU;
        }
        for (auto& pi : out) {
            auto itNow = cpuNow.find(pi.pid);
            if (itNow == cpuNow.end() || !itNow->second.ok) { pi.cpuPercent = 0.0; continue; }
            auto itPrev = prevCpu_.find(pi.pid);
            if (itPrev == prevCpu_.end() || !itPrev->second.valid || !prevSys_.valid || sysDeltaTotal == 0) {
                pi.cpuPercent = 0.0;
                continue;
            }
            ULONGLONG dK = (itNow->second.k >= itPrev->second.procKernel) ? (itNow->second.k - itPrev->second.procKernel) : 0;
            ULONGLONG dU = (itNow->second.u >= itPrev->second.procUser) ? (itNow->second.u - itPrev->second.procUser) : 0;
            ULONGLONG procDelta = dK + dU;
            double pct = (double)procDelta * 100.0 / (double)sysDeltaTotal;
            if (pct < 0) pct = 0;
            if (pct > 100.0) pct = 100.0;
            pi.cpuPercent = pct;
        }
        // update prev
        std::unordered_map<DWORD, CpuSample> next;
        for (auto& kv : cpuNow) {
            CpuSample s;
            s.procKernel = kv.second.k;
            s.procUser = kv.second.u;
            s.valid = true;
            next[kv.first] = s;
        }
        prevCpu_.swap(next);
        prevSys_.sysKernel = sysKernel;
        prevSys_.sysUser = sysUser;
        prevSys_.sysIdle = sysIdle;
        prevSys_.valid = true;
    }

    return out;
}

void ProcessManager::RememberOriginal(DWORD pid, HANDLE h) {
    std::lock_guard<std::mutex> lock(originalMtx_);
    if (originals_.find(pid) != originals_.end()) return;
    OriginalProcessState st;
    DWORD prio = GetPriorityClass(h);
    st.priorityClass = (prio != 0) ? prio : NORMAL_PRIORITY_CLASS;
    DWORD_PTR procMask = 0, sysMask = 0;
    if (GetProcessAffinityMask(h, &procMask, &sysMask)) {
        st.affinityMask = procMask;
        st.hadAffinity = true;
    }
    originals_[pid] = st;
}

void ProcessManager::RestoreProcess(DWORD pid) {
    OriginalProcessState st;
    {
        std::lock_guard<std::mutex> lock(originalMtx_);
        auto it = originals_.find(pid);
        if (it == originals_.end()) return;
        st = it->second;
    }
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        DWORD err = GetLastError();
        if (err != ERROR_INVALID_PARAMETER && err != ERROR_ACCESS_DENIED) {
            Logger::Instance().Warn("Restore OpenProcess failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(err));
        }
        return;
    }
    DWORD safePrio = SafetyGuard::ClampPriority(st.priorityClass);
    if (safePrio == 0) safePrio = NORMAL_PRIORITY_CLASS;
    if (!SetPriorityClass(h, safePrio)) {
        Logger::Instance().Warn("Restore SetPriorityClass failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(GetLastError()));
    }
    if (st.hadAffinity && st.affinityMask != 0) {
        if (!SetProcessAffinityMask(h, st.affinityMask)) {
            Logger::Instance().Warn("Restore SetProcessAffinityMask failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(GetLastError()));
        }
    }
    CloseHandle(h);
}

void ProcessManager::RestoreAll() {
    Logger::Instance().Info("RestoreAll requested");
    std::vector<DWORD> pids;
    {
        std::lock_guard<std::mutex> lock(originalMtx_);
        for (auto& kv : originals_) pids.push_back(kv.first);
    }
    for (DWORD pid : pids) RestoreProcess(pid);
    {
        std::lock_guard<std::mutex> lock(originalMtx_);
        originals_.clear();
    }
    Logger::Instance().Info("RestoreAll completed for " + std::to_string(pids.size()) + " processes");
}

bool ProcessManager::RestoreAllAndWait(int timeoutMs) {
    RestoreAll();
    (void)timeoutMs;
    return true;
}

void ProcessManager::OptimizerLoop() {
    while (running_.load()) {
        if (restoreRequested_.exchange(false)) {
            RestoreAll();
        }
        AppConfig cfg = Config::Instance().Get();
        auto snap = GetSnapshot();
        if (isAdmin_) {
            ApplyOptimization(snap, cfg);
        }
        int interval = std::clamp(cfg.watchdogIntervalMs, 1000, 10000);
        int steps = interval / 100;
        for (int i = 0; i < steps && running_.load(); ++i) {
            if (restoreRequested_.load()) break;
            Sleep(100);
        }
    }
}

bool ProcessManager::SetProcessPrioritySafe(HANDLE h, DWORD target, DWORD pid, const std::string& name) {
    DWORD safeTarget = SafetyGuard::ClampPriority(target);
    if (safeTarget == REALTIME_PRIORITY_CLASS) {
        Logger::Instance().Error("Blocked REALTIME attempt for pid=" + std::to_string(pid));
        return false;
    }
    DWORD cur = GetPriorityClass(h);
    if (cur == 0) {
        Logger::Instance().Warn("GetPriorityClass failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(GetLastError()));
        return false;
    }
    if (cur == safeTarget) return true;
    if (!SetPriorityClass(h, safeTarget)) {
        Logger::Instance().Warn("SetPriorityClass failed pid=" + std::to_string(pid) + " (" + name + ") err=" + winutil::LastErrorString(GetLastError()));
        return false;
    }
    return true;
}

bool ProcessManager::SetProcessAffinitySafe(HANDLE h, DWORD pid, const std::string& name, const AppConfig& cfg) {
    DWORD_PTR desired = ComputeAffinityMask(cfg);
    DWORD_PTR procMask = 0, sysMask = 0;
    if (!GetProcessAffinityMask(h, &procMask, &sysMask)) {
        Logger::Instance().Warn("GetProcessAffinityMask failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(GetLastError()));
        return false;
    }
    // Only restrict, never expand beyond system mask
    desired &= sysMask;
    if (desired == 0) desired = sysMask;
    if (procMask == desired) return true;
    if (!SetProcessAffinityMask(h, desired)) {
        Logger::Instance().Warn("SetProcessAffinityMask failed pid=" + std::to_string(pid) + " (" + name + ") err=" + winutil::LastErrorString(GetLastError()));
        return false;
    }
    return true;
}

bool ProcessManager::TrimWorkingSetSafe(HANDLE h, DWORD pid, const std::string& name, const AppConfig& cfg) {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return false;
    // Only trim if there's enough physical RAM headroom. Never aggressive.
    uint64_t avail = ms.ullAvailPhys;
    uint64_t total = ms.ullTotalPhys;
    uint64_t limitBytes = (uint64_t)std::clamp(cfg.maxWorkingSetMb, 512, 8192) * 1024ULL * 1024ULL;
    // cap limit to 30% of available RAM
    uint64_t availCap = (uint64_t)(avail * 0.30);
    if (limitBytes > availCap) limitBytes = availCap;
    if (limitBytes < 64ULL * 1024ULL * 1024ULL) return false; // too little avail, skip
    if (limitBytes > total) limitBytes = total;

    // Safe trim: set min=0, max=limitBytes (not -1, not negative)
    SIZE_T minWs = 0;
    SIZE_T maxWs = (SIZE_T)limitBytes;
    if (!SetProcessWorkingSetSizeEx(h, minWs, maxWs, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_ENABLE)) {
        DWORD err = GetLastError();
        // Not critical, just log
        Logger::Instance().Warn("SetProcessWorkingSetSizeEx failed pid=" + std::to_string(pid) + " (" + name + ") err=" + winutil::LastErrorString(err));
        return false;
    }
    return true;
}

void ProcessManager::ApplyToProcess(DWORD pid, const std::string& name, const AppConfig& cfg, bool selected, bool& didChange) {
    didChange = false;
    DWORD desiredAccess = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_INFORMATION;
    // For working set trim we need PROCESS_SET_QUOTA
    desiredAccess |= PROCESS_SET_QUOTA;
    HANDLE h = OpenProcess(desiredAccess, FALSE, pid);
    if (!h) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            Logger::Instance().Warn("OpenProcess denied pid=" + std::to_string(pid) + " (" + name + "), skipping (no escalation)");
        } else if (err != ERROR_INVALID_PARAMETER) {
            Logger::Instance().Warn("OpenProcess failed pid=" + std::to_string(pid) + " err=" + winutil::LastErrorString(err));
        }
        return;
    }

    RememberOriginal(pid, h);

    DWORD targetPrio = selected ? PriorityFromMode(cfg.priorityMode) : NORMAL_PRIORITY_CLASS;
    targetPrio = SafetyGuard::ClampPriority(targetPrio);

    if (SetProcessPrioritySafe(h, targetPrio, pid, name)) didChange = true;
    if (selected) {
        if (SetProcessAffinitySafe(h, pid, name, cfg)) didChange = true;
        TrimWorkingSetSafe(h, pid, name, cfg);
    } else {
        // For non-selected, restore default affinity? We only touch processes we previously touched (remembered).
        // Avoid changing affinity of random processes; restore only if in originals.
        std::lock_guard<std::mutex> lock(originalMtx_);
        auto it = originals_.find(pid);
        if (it != originals_.end() && it->second.hadAffinity && it->second.affinityMask != 0) {
            DWORD_PTR procMask=0, sysMask=0;
            if (GetProcessAffinityMask(h, &procMask, &sysMask) && procMask != it->second.affinityMask) {
                SetProcessAffinityMask(h, it->second.affinityMask);
            }
        }
    }

    CloseHandle(h);
}

void ProcessManager::ApplyOptimization(const std::vector<ProcessInfo>& snapshot, const AppConfig& cfg) {
    std::unordered_set<std::string> selected;
    {
        std::lock_guard<std::mutex> lock(selectionMtx_);
        selected = selectedNames_;
    }
    OptimizeStats stats;
    for (const auto& pi : snapshot) {
        std::string lower = winutil::ToLower(pi.name);
        bool isSel = selected.find(lower) != selected.end();
        if (pi.isProtected) { stats.skippedCount++; continue; }
        if (SafetyGuard::Instance().IsProtected(pi.pid, lower)) { stats.skippedCount++; continue; }
        // Only touch selected processes, OR processes we previously modified (to restore them to normal)
        bool wasTouched = false;
        {
            std::lock_guard<std::mutex> lock(originalMtx_);
            wasTouched = originals_.find(pi.pid) != originals_.end();
        }
        if (!isSel && !wasTouched) { continue; }

        bool didChange = false;
        ApplyToProcess(pi.pid, pi.name, cfg, isSel, didChange);
        if (didChange) stats.optimizedCount++;
    }
    {
        std::lock_guard<std::mutex> lock(statsMtx_);
        lastStats_ = stats;
    }
}

void ProcessManager::OptimizeOnce() {
    if (!isAdmin_) return;
    ApplyOptimization(GetSnapshot(), Config::Instance().Get());
}

int ProcessManager::RunRestoreCLI() {
    Logger::Instance().Init(Config::DefaultLogPath());
    Logger::Instance().Info("CLI --restore invoked");
    if (!winutil::IsUserAdmin()) {
        Logger::Instance().Error("CLI restore requires Administrator");
        Logger::Instance().Shutdown();
        return 2;
    }
    // Best-effort: we cannot know previous originals across runs; restore means set NORMAL for non-protected user processes
    // that are not in blacklist and not session 0. This is a safe emergency reset.
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        Logger::Instance().Shutdown();
        return 1;
    }
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            DWORD pid = pe.th32ProcessID;
            std::string name = winutil::WideToUtf8(pe.szExeFile);
            std::string lower = winutil::ToLower(name);
            if (SafetyGuard::Instance().IsProtected(pid, lower)) continue;
            HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!h) continue;
            DWORD cur = GetPriorityClass(h);
            if (cur != 0 && cur != NORMAL_PRIORITY_CLASS && cur != REALTIME_PRIORITY_CLASS) {
                SetPriorityClass(h, NORMAL_PRIORITY_CLASS);
            }
            // restore affinity to system default
            DWORD_PTR procMask=0, sysMask=0;
            if (GetProcessAffinityMask(h, &procMask, &sysMask) && sysMask != 0 && procMask != sysMask) {
                SetProcessAffinityMask(h, sysMask);
            }
            CloseHandle(h);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    Logger::Instance().Info("CLI --restore completed");
    Logger::Instance().Shutdown();
    return 0;
}

} // namespace n4x
