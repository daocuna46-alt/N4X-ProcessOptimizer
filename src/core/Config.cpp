#include "Config.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <shlobj.h>

namespace n4x {
using json = nlohmann::json;

Config& Config::Instance() {
    static Config c;
    return c;
}

std::wstring Config::DefaultPath() {
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p = std::filesystem::path(std::wstring(buf, len)).parent_path() / L"config.json";
    return p.wstring();
}

std::wstring Config::DefaultLogPath() {
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p = std::filesystem::path(std::wstring(buf, len)).parent_path() / L"N4X_ProcessOptimizer.log";
    return p.wstring();
}

bool Config::Load(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    path_ = path;
    try {
        std::ifstream f(std::filesystem::path(path));
        if (!f.is_open()) {
            Logger::Instance().Warn("Config not found, using defaults: " + std::filesystem::path(path).string());
            return false;
        }
        json j;
        f >> j;
        AppConfig c = cfg_;
        if (j.contains("watchdog_interval_ms")) c.watchdogIntervalMs = std::clamp(j["watchdog_interval_ms"].get<int>(), 1000, 10000);
        if (j.contains("max_working_set_mb")) c.maxWorkingSetMb = std::clamp(j["max_working_set_mb"].get<int>(), 512, 8192);
        if (j.contains("reserve_core0")) c.reserveCore0 = j["reserve_core0"].get<bool>();
        if (j.contains("priority_mode")) {
            std::string pm = j["priority_mode"].get<std::string>();
            if (pm == "ABOVE_NORMAL" || pm == "HIGH") c.priorityMode = pm;
            else c.priorityMode = "HIGH";
        }
        if (j.contains("theme_dark")) c.themeDark = j["theme_dark"].get<bool>();
        if (j.contains("auto_start")) c.autoStart = j["auto_start"].get<bool>();
        if (j.contains("filter_show_blacklisted")) c.filterShowBlacklisted = j["filter_show_blacklisted"].get<bool>();
        if (j.contains("selected_process_names") && j["selected_process_names"].is_array()) {
            c.selectedProcessNames.clear();
            for (auto& v : j["selected_process_names"]) {
                if (v.is_string()) c.selectedProcessNames.push_back(v.get<std::string>());
            }
        }
        cfg_ = c;
        Logger::Instance().Info("Config loaded: " + std::filesystem::path(path).string());
        return true;
    } catch (const std::exception& e) {
        Logger::Instance().Error(std::string("Config load failed: ") + e.what());
        return false;
    }
}

bool Config::Save(const std::wstring& path) const {
    AppConfig c;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        c = cfg_;
    }
    try {
        json j;
        j["watchdog_interval_ms"] = std::clamp(c.watchdogIntervalMs, 1000, 10000);
        j["max_working_set_mb"] = std::clamp(c.maxWorkingSetMb, 512, 8192);
        j["reserve_core0"] = c.reserveCore0;
        j["priority_mode"] = (c.priorityMode == "ABOVE_NORMAL") ? "ABOVE_NORMAL" : "HIGH";
        j["theme_dark"] = c.themeDark;
        j["auto_start"] = c.autoStart;
        j["filter_show_blacklisted"] = c.filterShowBlacklisted;
        j["selected_process_names"] = c.selectedProcessNames;
        std::ofstream f(std::filesystem::path(path));
        if (!f.is_open()) {
            Logger::Instance().Error("Config save failed: cannot open file");
            return false;
        }
        f << j.dump(2) << std::endl;
        Logger::Instance().Info("Config saved: " + std::filesystem::path(path).string());
        return true;
    } catch (const std::exception& e) {
        Logger::Instance().Error(std::string("Config save failed: ") + e.what());
        return false;
    }
}

AppConfig Config::Get() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return cfg_;
}

void Config::Set(const AppConfig& cfg) {
    std::lock_guard<std::mutex> lock(mtx_);
    cfg_ = cfg;
    cfg_.watchdogIntervalMs = std::clamp(cfg_.watchdogIntervalMs, 1000, 10000);
    cfg_.maxWorkingSetMb = std::clamp(cfg_.maxWorkingSetMb, 512, 8192);
    if (cfg_.priorityMode != "ABOVE_NORMAL" && cfg_.priorityMode != "HIGH") cfg_.priorityMode = "HIGH";
}

std::wstring Config::Path() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return path_;
}

void Config::SetPath(const std::wstring& p) {
    std::lock_guard<std::mutex> lock(mtx_);
    path_ = p;
}

} // namespace n4x
