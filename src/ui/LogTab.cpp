#include "LogTab.h"
#include "imgui.h"
#include "../utils/WinUtil.h"
#include <fstream>
#include <filesystem>

namespace n4x::ui {

static ImVec4 LevelColor(LogLevel l) {
    switch (l) {
        case LogLevel::Info: return ImVec4(0.7f, 0.9f, 0.7f, 1.0f);
        case LogLevel::Warn: return ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        case LogLevel::Error: return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    }
    return ImVec4(1,1,1,1);
}

void LogTab::Render(UiState& /*state*/) {
    ImGui::TextUnformatted("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    const char* levels[] = {"ALL", "INFO", "WARN", "ERROR"};
    ImGui::Combo("##loglvl", &levelFilter_, levels, IM_ARRAYSIZE(levels));
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear view")) {
        // We don't clear logger buffer; just informational. Provide export instead.
        Logger::Instance().Info("Log view refresh requested");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260);
    ImGui::InputText("##exportpath", exportPath_, sizeof(exportPath_));
    ImGui::SameLine();
    if (ImGui::Button("Export log")) {
        auto entries = Logger::Instance().Snapshot(5000);
        std::ofstream f(exportPath_, std::ios::trunc);
        if (f) {
            for (auto& e : entries) {
                auto t = std::chrono::system_clock::to_time_t(e.ts);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(e.ts.time_since_epoch()) % 1000;
                std::tm tm{}; localtime_s(&tm, &t);
                char ts[64];
                std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
                const char* lvl = (e.level == LogLevel::Info) ? "INFO" : (e.level == LogLevel::Warn ? "WARN" : "ERROR");
                f << ts << "." << ms.count() << " [" << lvl << "] " << e.text << "\n";
            }
            Logger::Instance().Info(std::string("Exported log to ") + exportPath_);
        } else {
            Logger::Instance().Error(std::string("Failed to export log to ") + exportPath_);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Xuat log hien tai ra file text.");

    ImGui::Separator();
    ImGui::BeginChild("##logscroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    auto entries = Logger::Instance().Snapshot(1000);
    for (const auto& e : entries) {
        if (levelFilter_ == 1 && e.level != LogLevel::Info) continue;
        if (levelFilter_ == 2 && e.level != LogLevel::Warn) continue;
        if (levelFilter_ == 3 && e.level != LogLevel::Error) continue;
        auto t = std::chrono::system_clock::to_time_t(e.ts);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(e.ts.time_since_epoch()) % 1000;
        std::tm tm{}; localtime_s(&tm, &t);
        char ts[64];
        std::strftime(ts, sizeof(ts), "%H:%M:%S", &tm);
        const char* lvl = (e.level == LogLevel::Info) ? "INFO" : (e.level == LogLevel::Warn ? "WARN" : "ERROR");
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.level));
        ImGui::Text("[%s.%03lld] [%s] %s", ts, (long long)ms.count(), lvl, e.text.c_str());
        ImGui::PopStyleColor();
    }
    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

} // namespace n4x::ui
