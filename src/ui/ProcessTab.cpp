#include "ProcessTab.h"
#include "imgui.h"
#include "../core/Logger.h"
#include "../core/SafetyGuard.h"
#include "../utils/WinUtil.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace n4x::ui {

static bool IContains(const std::string& hay, const std::string& needleLower) {
    if (needleLower.empty()) return true;
    std::string h = winutil::ToLower(hay);
    return h.find(needleLower) != std::string::npos;
}

void ProcessTab::Render(UiState& state, bool isAdmin) {
    auto& pm = ProcessManager::Instance();
    cache_ = pm.GetSnapshot();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::TextUnformatted("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##search", "process name or pid...", searchBuf_, sizeof(searchBuf_));
    ImGui::SameLine();
    ImGui::TextUnformatted("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    const char* filters[] = {"All", "Selected", "Unselected", "Protected"};
    ImGui::Combo("##filter", &filterMode_, filters, IM_ARRAYSIZE(filters));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) { /* snapshot auto-refreshes; button just gives feedback */ }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Process list auto-refreshes every ~2s from scanner thread.");

    std::string needle = winutil::ToLower(std::string(searchBuf_));
    std::vector<ProcessInfo> rows;
    rows.reserve(cache_.size());
    for (const auto& p : cache_) {
        bool matchSearch = needle.empty() || IContains(p.name, needle) || std::to_string(p.pid).find(needle) != std::string::npos;
        if (!matchSearch) continue;
        if (filterMode_ == 1 && !p.isSelected) continue;
        if (filterMode_ == 2 && p.isSelected) continue;
        if (filterMode_ == 3 && !p.isProtected) continue;
        rows.push_back(p);
    }

    auto cmp = [&](const ProcessInfo& a, const ProcessInfo& b) {
        int dir = sortAsc_ ? 1 : -1;
        switch (sortColumn_) {
            case 0: if (a.isSelected != b.isSelected) return sortAsc_ ? (a.isSelected > b.isSelected) : (a.isSelected < b.isSelected); break;
            case 1: { int c = _stricmp(a.name.c_str(), b.name.c_str()); if (c != 0) return (dir * c) < 0; } break;
            case 2: if (a.pid != b.pid) return dir < 0 ? a.pid > b.pid : a.pid < b.pid; break;
            case 3: if (a.cpuPercent != b.cpuPercent) return dir < 0 ? a.cpuPercent > b.cpuPercent : a.cpuPercent < b.cpuPercent; break;
            case 4: if (a.ramBytes != b.ramBytes) return dir < 0 ? a.ramBytes > b.ramBytes : a.ramBytes < b.ramBytes; break;
            case 5: { int c = _stricmp(a.priorityName.c_str(), b.priorityName.c_str()); if (c != 0) return (dir * c) < 0; } break;
        }
        return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
    };
    std::sort(rows.begin(), rows.end(), cmp);

    ImGui::Separator();
    ImGui::BeginChild("##proc_table_child", ImVec2(0, -70), true);
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable;
    if (ImGui::BeginTable("proc_table", 6, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Sel", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSortDescending, 40.0f, 0);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 220.0f, 1);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f, 2);
        ImGui::TableSetupColumn("CPU%", ImGuiTableColumnFlags_WidthFixed, 90.0f, 3);
        ImGui::TableSetupColumn("RAM", ImGuiTableColumnFlags_WidthFixed, 90.0f, 4);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 110.0f, 5);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsCount > 0) {
                sortColumn_ = sortSpecs->Specs[0].ColumnUserID;
                sortAsc_ = sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)rows.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& p = rows[i];
                ImGui::TableNextRow();
                ImGui::PushID((int)p.pid);

                bool isProtected = p.isProtected;
                bool selected = p.isSelected;
                if (selected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(124, 58, 237, 45));
                }

                // Sel
                ImGui::TableSetColumnIndex(0);
                if (isProtected) {
                    ImGui::TextDisabled("L");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("He thong - bao ve. Khong the chon/toi uu.");
                } else {
                    bool chk = selected;
                    if (!isAdmin) ImGui::BeginDisabled();
                    if (ImGui::Checkbox("##sel", &chk)) {
                        pm.ToggleSelection(p.name, chk);
                        auto names = pm.GetSelectedNames();
                        auto cfg = Config::Instance().Get();
                        cfg.selectedProcessNames = names;
                        Config::Instance().Set(cfg);
                        state.dirtyConfig.store(true);
                        Logger::Instance().Info(std::string(chk ? "Selected " : "Unselected ") + p.name);
                    }
                    if (!isAdmin) { ImGui::EndDisabled(); if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Can quyen Administrator de toi uu."); }
                    else if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chon tien trinh nay de uu tien CPU/RAM.");
                }

                // Name
                ImGui::TableSetColumnIndex(1);
                if (isProtected) ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "%s", p.name.c_str());
                else ImGui::TextUnformatted(p.name.c_str());
                if (ImGui::IsItemHovered() && !p.path.empty()) ImGui::SetTooltip("%s", p.path.c_str());

                // PID
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", p.pid);

                // CPU
                ImGui::TableSetColumnIndex(3);
                {
                    float cpu = (float)std::clamp(p.cpuPercent, 0.0, 100.0);
                    char buf[32]; snprintf(buf, sizeof(buf), "%.1f%%", cpu);
                    ImGui::ProgressBar(cpu / 100.0f, ImVec2(-1, 0), buf);
                }

                // RAM
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(winutil::FormatBytes(p.ramBytes).c_str());

                // Priority
                ImGui::TableSetColumnIndex(5);
                if (isProtected) ImGui::TextColored(ImVec4(0.9f,0.6f,0.2f,1), "[LOCKED]");
                else if (selected) ImGui::TextColored(ImVec4(0.5f,0.9f,0.6f,1), "%s", p.priorityName.c_str());
                else ImGui::TextUnformatted(p.priorityName.c_str());

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    auto stats = pm.GetLastStats();
    auto cfg = Config::Instance().Get();
    int selectedCount = 0;
    for (auto& p : cache_) if (p.isSelected) selectedCount++;
    ImGui::Spacing();
    ImGui::Text("Status: %s %d tien trinh | Watchdog: %dms | Optimized: %d | Skipped: %d",
        isAdmin ? "Dang toi uu" : "Read-only (khong co quyen Admin) -",
        selectedCount, cfg.watchdogIntervalMs, stats.optimizedCount, stats.skippedCount);
    if (!isAdmin) {
        ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "Chay lai app voi quyen Administrator de bat che do toi uu.");
    }
    ImGui::PopStyleVar();
}

} // namespace n4x::ui
