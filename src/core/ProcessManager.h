#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>
#include <Windows.h>
#include "Config.h"

namespace n4x {

struct ProcessInfo {
    DWORD pid = 0;
    std::string name;
    std::string path;
    double cpuPercent = 0.0;
    uint64_t ramBytes = 0;
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    std::string priorityName = "NORMAL";
    DWORD sessionId = 0;
    bool isProtected = false;
    bool isSelected = false;
};

struct OriginalProcessState {
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    DWORD_PTR affinityMask = 0;
    bool hadAffinity = false;
};

struct OptimizeStats {
    int optimizedCount = 0;
    int skippedCount = 0;
    int errorCount = 0;
};

class ProcessManager {
public:
    static ProcessManager& Instance();

    void Start();
    void Stop();

    std::vector<ProcessInfo> GetSnapshot() const;
    OptimizeStats GetLastStats() const;
    bool IsAdmin() const;

    void SetSelectedNames(const std::vector<std::string>& names);
    std::vector<std::string> GetSelectedNames() const;
    void ToggleSelection(const std::string& name, bool selected);

    void OptimizeOnce();
    void RestoreAll();
    bool RestoreAllAndWait(int timeoutMs = 5000);

    // CLI restore without GUI
    static int RunRestoreCLI();

    void RequestRestoreAll();
    bool IsRestoreRequested() const;

private:
    ProcessManager() = default;
    ~ProcessManager();
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    void ScannerLoop();
    void OptimizerLoop();

    std::vector<ProcessInfo> ScanProcesses();
    void ApplyOptimization(const std::vector<ProcessInfo>& snapshot, const AppConfig& cfg);
    void ApplyToProcess(DWORD pid, const std::string& name, const AppConfig& cfg, bool selected, bool& didChange);

    bool SetProcessPrioritySafe(HANDLE h, DWORD target, DWORD pid, const std::string& name);
    bool SetProcessAffinitySafe(HANDLE h, DWORD pid, const std::string& name, const AppConfig& cfg);
    bool TrimWorkingSetSafe(HANDLE h, DWORD pid, const std::string& name, const AppConfig& cfg);

    DWORD_PTR ComputeAffinityMask(const AppConfig& cfg) const;
    static std::string PriorityToString(DWORD p);
    static DWORD PriorityFromMode(const std::string& mode);

    void RememberOriginal(DWORD pid, HANDLE h);
    void RestoreProcess(DWORD pid);

    mutable std::mutex snapshotMtx_;
    std::vector<ProcessInfo> snapshot_;

    mutable std::mutex selectionMtx_;
    std::unordered_set<std::string> selectedNames_;

    mutable std::mutex originalMtx_;
    std::unordered_map<DWORD, OriginalProcessState> originals_;

    mutable std::mutex statsMtx_;
    OptimizeStats lastStats_;

    std::atomic<bool> running_{false};
    std::atomic<bool> restoreRequested_{false};
    std::thread scannerThread_;
    std::thread optimizerThread_;

    // CPU calculation state
    struct CpuSample {
        ULONGLONG procKernel = 0;
        ULONGLONG procUser = 0;
        ULONGLONG sysKernel = 0;
        ULONGLONG sysUser = 0;
        ULONGLONG sysIdle = 0;
        bool valid = false;
    };
    std::mutex cpuMtx_;
    std::unordered_map<DWORD, CpuSample> prevCpu_;
    CpuSample prevSys_;

    bool isAdmin_ = false;
};
} // namespace n4x
