#include "SettingsTab.h"
#include "imgui.h"
#include "../core/Config.h"
#include "../core/Logger.h"
#include "../utils/WinUtil.h"
#include <filesystem>
#include <shlobj.h>
#include <fstream>

namespace n4x::ui {

static bool SetAutoStart(bool enable) {
    // Only Startup folder shortcut via .lnk? Simpler and safe: create/remove a .cmd in Startup folder. No registry.
    wchar_t startupPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) return false;
    std::filesystem::path link = std::filesystem::path(startupPath) / L"N4X-ProcessOptimizer.cmd";
    std::error_code ec;
    if (enable) {
        std::wstring exe = winutil::GetExePath();
        std::ofstream f(link, std::ios::trunc);
        if (!f) return false;
        f << "@echo off\r\n";
        f << "start \"\" \"" << winutil::WideToUtf8(exe) << "\"\r\n";
        f.close();
        Logger::Instance().Info("Auto-start enabled (Startup folder .cmd)");
        return true;
    } else {
        std::filesystem::remove(link, ec);
        Logger::Instance().Info("Auto-start disabled (Startup folder)");
        return !ec;
    }
}

static bool GetAutoStartEnabled() {
    wchar_t startupPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) return false;
    std::filesystem::path link = std::filesystem::path(startupPath) / L"N4X-ProcessOptimizer.cmd";
    return std::filesystem::exists(link);
}

void SettingsTab::Render(UiState& state, bool isAdmin) {
    auto& cfgStore = Config::Instance();
    if (!loaded_) {
        auto c = cfgStore.Get();
        watchdogMs_ = c.watchdogIntervalMs;
        maxWsMb_ = c.maxWorkingSetMb;
        reserveCore0_ = c.reserveCore0;
        priorityIdx_ = (c.priorityMode == "ABOVE_NORMAL") ? 0 : 1;
        themeDark_ = c.themeDark;
        autoStart_ = GetAutoStartEnabled();
        filterShowBlacklisted_ = c.filterShowBlacklisted;
        loaded_ = true;
    }

    ImGui::BeginChild("##settings_child", ImVec2(0, 0), false);
    ImGui::TextColored(ImVec4(0.55f,0.35f,0.95f,1), "Toi uu an toan - khong REALTIME, khong kill/suspend");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::SliderInt("Watchdog interval (ms)", &watchdogMs_, 1000, 10000)) {
        if (watchdogMs_ < 1000) watchdogMs_ = 1000;
        if (watchdogMs_ > 10000) watchdogMs_ = 10000;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chu ky watchdog ap lai priority/affinity. Mac dinh 3000ms. Cao hon = it can thiep hon.");

    if (ImGui::SliderInt("Max working set (MB)", &maxWsMb_, 512, 8192)) {
        if (maxWsMb_ < 512) maxWsMb_ = 512;
        if (maxWsMb_ > 8192) maxWsMb_ = 8192;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gioi han working set cho process duoc chon. Tool chi set khi RAM trong du (<=30%% avail). Khong bao gio set gia tri am.");

    if (ImGui::Checkbox("Reserve core 0 (khuyen nghi)", &reserveCore0_)) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tranh gan process duoc chon len core 0 - core thuong xu ly interrupt he thong, giup giam giat chuot/phim.");

    ImGui::TextUnformatted("Priority muc cao nhat cho process chon:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    const char* prioItems[] = {"ABOVE_NORMAL", "HIGH"};
    ImGui::Combo("##prio_mode", &priorityIdx_, prioItems, IM_ARRAYSIZE(prioItems));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("KHONG BAO GIO dung REALTIME_PRIORITY_CLASS. HIGH la muc toi da an toan.");

    if (ImGui::Checkbox("Dark theme", &themeDark_)) {
        auto c = cfgStore.Get(); c.themeDark = themeDark_; cfgStore.Set(c);
        state.dirtyConfig.store(true);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chuyen Dark/Light theme.");

    if (ImGui::Checkbox("Auto-start with Windows (Startup folder)", &autoStart_)) {
        if (!SetAutoStart(autoStart_)) {
            Logger::Instance().Error("Failed to update auto-start setting");
            autoStart_ = GetAutoStartEnabled();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chi tao/xoa file .cmd trong Startup folder. KHONG ghi registry.");

    if (ImGui::Checkbox("Show blacklisted/protected processes in list", &filterShowBlacklisted_)) {}

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!isAdmin) {
        ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "Read-only mode: can quyen Admin de ap dung toi uu. Ban van co the luu cai dat.");
    }

    if (ImGui::Button("Apply & Save Settings", ImVec2(200, 32))) {
        auto c = cfgStore.Get();
        c.watchdogIntervalMs = watchdogMs_;
        c.maxWorkingSetMb = maxWsMb_;
        c.reserveCore0 = reserveCore0_;
        c.priorityMode = (priorityIdx_ == 0) ? "ABOVE_NORMAL" : "HIGH";
        c.themeDark = themeDark_;
        c.autoStart = autoStart_;
        c.filterShowBlacklisted = filterShowBlacklisted_;
        cfgStore.Set(c);
        std::wstring path = cfgStore.Path().empty() ? Config::DefaultPath() : cfgStore.Path();
        cfgStore.Save(path);
        state.dirtyConfig.store(false);
        Logger::Instance().Info("Settings applied and saved");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset defaults", ImVec2(140, 32))) {
        AppConfig d;
        watchdogMs_ = d.watchdogIntervalMs;
        maxWsMb_ = d.maxWorkingSetMb;
        reserveCore0_ = d.reserveCore0;
        priorityIdx_ = 1;
        themeDark_ = d.themeDark;
        filterShowBlacklisted_ = d.filterShowBlacklisted;
        Logger::Instance().Info("Settings reset to defaults (not saved yet)");
    }

    ImGui::Spacing();
    ImGui::TextWrapped("Luu y an toan: Tool khong dung REALTIME, khong suspend/kill, khong EmptyWorkingSet len process he thong, khong dong registry/driver/pagefile. Neu OpenProcess bi Access Denied, tool se log va bo qua.");

    ImGui::EndChild();
}

} // namespace n4x::ui
