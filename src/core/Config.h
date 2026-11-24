#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <Windows.h>

namespace n4x {

struct AppConfig {
    int watchdogIntervalMs = 3000;
    int maxWorkingSetMb = 2048;
    bool reserveCore0 = true;
    std::string priorityMode = "HIGH"; // ABOVE_NORMAL or HIGH
    bool themeDark = true;
    bool autoStart = false;
    std::vector<std::string> selectedProcessNames;
    bool filterShowBlacklisted = true;
};

class Config {
public:
    static Config& Instance();

    bool Load(const std::wstring& path);
    bool Save(const std::wstring& path) const;

    AppConfig Get() const;
    void Set(const AppConfig& cfg);

    std::wstring Path() const;
    void SetPath(const std::wstring& p);

    static std::wstring DefaultPath();
    static std::wstring DefaultLogPath();

private:
    Config() = default;
    mutable std::mutex mtx_;
    AppConfig cfg_;
    std::wstring path_;
};

} // namespace n4x
