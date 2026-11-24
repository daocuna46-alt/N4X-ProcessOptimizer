# N4X ProcessOptimizer

**N4X ProcessOptimizer** là ứng dụng Windows desktop (C++17, Win32 + Dear ImGui + DirectX 11) giúp giảm tranh chấp CPU/RAM cho tiến trình bạn chọn (game/app) bằng cách điều chỉnh **Priority** và **CPU Affinity** một cách an toàn. Tool **không** kill/suspend process, **không** dùng REALTIME, **không** đụng registry/driver/pagefile/file hệ thống.

> **Tuyên bố rõ ràng**
> 
> "N4X ProcessOptimizer KHÔNG tăng FPS thần kỳ. Nó chỉ giảm tranh chấp CPU/RAM. Máy yếu vẫn phải nâng cấp phần cứng. Tool cam kết KHÔNG kill/delete bất kỳ process/file nào, KHÔNG dùng REALTIME priority, KHÔNG đụng registry/driver."

## Ảnh mô tả UI (text mock)

```
┌──────────────────────────────────────────────────────────────┐
│ N4X ProcessOptimizer          [Admin OK] [Optimize] [Restore]│
├──────────────────────────────────────────────────────────────┤
│ [ Tiến trình ] [ Cài đặt ] [ Log ]                           │
├──────────────────────────────────────────────────────────────┤
│ Search: [__________]  Filter: [All ▼]                        │
│ ┌──┬───────────────┬──────┬──────┬──────┬───────────┐        │
│ │☑ │ game.exe      │ 1234 │ 45%  │ 2.1G │ HIGH      │        │
│ │☐ │ chrome.exe    │ 5678 │ 12%  │ 800M │ NORMAL    │        │
│ │L │ svchost.exe   │ 888  │  2%  │  50M │ [LOCKED]  │        │
│ └──┴───────────────┴──────┴──────┴──────┴───────────┘        │
│ Status: Dang toi uu 2 tien trinh | Watchdog: 3000ms          │
├──────────────────────────────────────────────────────────────┤
│ [ Luu config ] [ Restore All ] [ Thoat ]  Ctrl+Shift+R       │
└──────────────────────────────────────────────────────────────┘
```

## Tính năng chính

- **Chọn tiến trình ưu tiên**: chỉ process bạn tick mới được set priority cao hơn (ABOVE_NORMAL/HIGH) và affinity tối ưu.
- **Process không chọn**: bị giảm tranh chấp bằng cách đưa về `NORMAL_PRIORITY_CLASS` (nếu tool đã từng chạm vào) — **không kill, không suspend**.
- **Blacklist cứng**: process hệ thống/quan trọng và mọi process `SessionId == 0` bị khóa (`[LOCKED]`), không đổi priority/affinity/workingset.
- **Watchdog an toàn**: vòng lặp mặc định 3000ms (chỉnh 1000–10000ms) tự áp lại cấu hình khi process thay đổi.
- **Restore/Panic**: nút `Restore All`, hotkey `Ctrl+Shift+R`, và CLI `--restore` để khôi phục trạng thái an toàn.
- **GUI Dark theme**: Dear ImGui + DX11, bảng process sortable, search/filter, log realtime, tray icon.
- **Config JSON**: lưu lựa chọn và cài đặt vào `config.json` cạnh exe.

## Ràng buộc an toàn (bắt buộc)

1. **Không bao giờ dùng `REALTIME_PRIORITY_CLASS`**. Chỉ tối đa `HIGH_PRIORITY_CLASS`.
2. **Không suspend/terminate/delete** bất kỳ process hay file nào.
3. **Không gọi `EmptyWorkingSet`** lên process hệ thống (và không dùng `EmptyWorkingSet` nói chung; tool dùng `SetProcessWorkingSetSizeEx` có giới hạn an toàn cho process được chọn).
4. **Không đụng registry, driver, pagefile, boot config**. Auto-start chỉ tạo/xóa `.cmd` trong Startup folder.
5. **Blacklist cứng**: `System, Idle, Registry, Memory Compression, csrss.exe, wininit.exe, winlogon.exe, services.exe, lsass.exe, smss.exe, svchost.exe, explorer.exe, dwm.exe, fontdrvhost.exe, ctfmon.exe, audiodg.exe, MsMpEng.exe, NisSrv.exe, SecurityHealthService.exe, spoolsv.exe, sihost.exe, taskhostw.exe, RuntimeBroker.exe, SearchIndexer.exe, WUDFHost.exe, conhost.exe, LogonUI.exe, userinit.exe, ShellExperienceHost.exe, StartMenuExperienceHost.exe, TextInputHost.exe` và **mọi process SessionId == 0**.
6. **Không bật SeDebugPrivilege** để can thiệp system process.
7. **Working set**: kiểm tra `GlobalMemoryStatusEx`, chỉ set khi RAM trống đủ; giới hạn `<= 30%` RAM trống; không set giá trị âm; không vượt `ullTotalPhys`.
8. **CPU Affinity**: mặc định tránh core 0 (reserve core 0) để giảm giật; có option bật/tắt.
9. **Watchdog**: mặc định 3000ms, có thể chỉnh 1000–10000ms.
10. **Kiểm tra lỗi**: mọi API Win32 đều kiểm tra `GetLastError`/HRESULT, log rõ ràng, không crash.
11. **PANIC / Restore All**: khôi phục `NORMAL_PRIORITY_CLASS` + affinity mặc định cho process từng bị đổi; hotkey `Ctrl+Shift+R`.
12. **CLI `--restore`**: chạy không cần GUI để khôi phục khẩn cấp.
13. **Access Denied**: nếu `OpenProcess` fail → log & bỏ qua, **không retry escalate**.
14. **UAC/Admin**: manifest `requireAdministrator`. Nếu user chạy không có quyền admin (ví dụ chạy bản đã sửa manifest/compat), app vẫn chạy **read-only** (chỉ xem, không đổi).

## Cấu trúc thư mục

```
N4X-ProcessOptimizer/
├── src/
│   ├── main.cpp
│   ├── ui/
│   │   ├── MainWindow.cpp / .h
│   │   ├── ProcessTab.cpp / .h
│   │   ├── SettingsTab.cpp / .h
│   │   └── LogTab.cpp / .h
│   ├── core/
│   │   ├── ProcessManager.cpp / .h
│   │   ├── SafetyGuard.cpp / .h
│   │   ├── Config.cpp / .h
│   │   └── Logger.cpp / .h
│   └── utils/
│       └── WinUtil.cpp / .h
├── third_party/
│   ├── imgui/          (git submodule, docking)
│   ├── imgui_dx11/     (fallback backends nếu cần)
│   └── json/           (nlohmann/json single header)
├── resources/
│   ├── app.rc
│   ├── app.manifest
│   ├── resource.h
│   └── icon.ico
├── .github/workflows/build.yml
├── CMakeLists.txt
├── .gitmodules
├── config.json
└── README.md
```

## Build local (MSVC + CMake)

### Yêu cầu
- Windows 10/11 x64
- Visual Studio 2022 (Desktop development with C++) / Build Tools
- CMake >= 3.20
- Git (để kéo submodule ImGui)

### Các bước

```powershell
git clone --recursive <repo-url>
cd N4X-ProcessOptimizer

# Nếu quên --recursive:
git submodule update --init --recursive

# Tải nlohmann/json single header (nếu chưa có)
mkdir -Force third_party/json/nlohmann
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "third_party/json/nlohmann/json.hpp"

# Configure + build
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Output: `build/Release/N4XProcessOptimizer.exe` (cùng `config.json`, `README.md`).

### Chạy
- Chuột phải → **Run as administrator** (khuyến nghị) để bật tối ưu.
- Không admin: app vẫn mở được ở chế độ read-only.
- Khôi phục khẩn cấp:

```powershell
.\N4XProcessOptimizer.exe --restore
```

## Tải artifact từ GitHub Actions

1. Vào tab **Actions** của repo.
2. Chọn workflow **Build N4X ProcessOptimizer**.
3. Mở run mới nhất (push/main hoặc workflow_dispatch).
4. Tải artifact **N4X-ProcessOptimizer-x64** (zip chứa exe + config + README).
5. Nếu có tag `v*`, xem **Releases** để tải zip đính kèm.

## Giải thích API Win32 và vì sao an toàn

| API | Mục đích | An toàn vì |
|---|---|---|
| `CreateToolhelp32Snapshot` + `Process32First/Next` | Enum process | Chỉ đọc danh sách; không sửa process. |
| `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION \| PROCESS_SET_INFORMATION \| PROCESS_SET_QUOTA)` | Mở handle tối thiểu | Không xin `PROCESS_TERMINATE`/`PROCESS_SUSPEND_RESUME`. Chỉ quyền cần thiết. |
| `QueryFullProcessImageNameW` | Lấy đường dẫn exe | Read-only. |
| `GetProcessTimes` + `GetSystemTimes` | Tính CPU% | Read-only, dùng delta sampling. |
| `GetProcessMemoryInfo` (Psapi) | Đọc RAM working set | Read-only. |
| `GetPriorityClass` / `SetPriorityClass` | Đọc/đặt priority | **Chỉ set** IDLE/BELOW_NORMAL/NORMAL/ABOVE_NORMAL/HIGH. Có `SafetyGuard::ClampPriority` chặn REALTIME. |
| `GetProcessAffinityMask` / `SetProcessAffinityMask` | Đọc/đặt affinity | Chỉ giới hạn trong system mask; mặc định bỏ core 0; không mở rộng ngoài CPU hợp lệ. |
| `SetProcessWorkingSetSizeEx` | Giới hạn working set | Chỉ dùng cho process được chọn; kiểm tra `GlobalMemoryStatusEx`; cap ≤30% RAM trống; không giá trị âm; không vượt tổng RAM. Không gọi `EmptyWorkingSet`. |
| `ProcessIdToSessionId` | Kiểm tra session 0 | Bỏ qua service/system session. |
| `GlobalMemoryStatusEx` | Kiểm tra RAM trống/tổng | Tránh trim khi RAM thấp. |
| `IsUserAnAdmin`/`CheckTokenMembership` | Kiểm tra quyền | Nếu không admin: read-only mode, không cố escalate. |

## Cài đặt an toàn trong app

- **Watchdog interval (1000–10000ms)**: chu kỳ áp lại ưu tiên. 3000ms mặc định cân bằng phản ứng/overhead.
- **Max working set (512–8192 MB)**: giới hạn working set cho process được chọn; tool tự clamp theo RAM trống.
- **Reserve core 0**: tránh gán process đã chọn lên core 0 (thường xử lý interrupt) để giảm giật chuột/phím.
- **Priority mode (ABOVE_NORMAL/HIGH)**: mức tối đa là HIGH. REALTIME bị cấm hoàn toàn.
- **Auto-start**: chỉ tạo file `.cmd` trong Startup folder; không ghi registry.
- **Restore All / Ctrl+Shift+R / `--restore`**: khôi phục nhanh về trạng thái an toàn.

## FAQ

### Tại sao không có REALTIME?
`REALTIME_PRIORITY_CLASS` có thể làm thread hệ thống/input starve CPU, gây đơ chuột/phím hoặc mất ổn định. N4X giới hạn tối đa `HIGH_PRIORITY_CLASS` để an toàn.

### Có hại máy không?
Tool chỉ gọi API Win32 hợp pháp để set priority/affinity/workingset có giới hạn. Không kill/suspend, không ghi registry/driver/pagefile/file hệ thống. Tuy nhiên, mọi tối ưu process đều có rủi ro nhỏ tùy ứng dụng; vì vậy có Restore All và blacklist cứng.

### Khôi phục thế nào?
- Trong app: bấm **Restore All**.
- Hotkey: **Ctrl+Shift+R**.
- CLI khẩn cấp: `N4XProcessOptimizer.exe --restore` (chạy với quyền Admin).
- Khi thoát app: tool tự Restore All cho các process đã bị thay đổi trong phiên.

### Vì sao một số process bị [LOCKED]?
Đó là process hệ thống/quan trọng hoặc Session 0. Can thiệp có thể gây mất ổn định, nên tool bỏ qua hoàn toàn.

### App báo Read-only?
Bạn đang chạy không có quyền Administrator. App vẫn cho xem danh sách process/log nhưng không áp dụng thay đổi. Chạy lại bằng Run as administrator.

## Giới hạn trung thực

- Không tăng FPS thần kỳ; chỉ giảm tranh chấp tài nguyên khi CPU/RAM đang bị cạnh tranh.
- Máy yếu vẫn cần nâng cấp phần cứng (CPU/GPU/RAM/SSD).
- Một số game anti-cheat có thể không thích việc thay đổi priority/affinity từ tiến trình ngoài; hãy tự chịu trách nhiệm kiểm tra.

## License

Đề xuất MIT (bạn có thể thay đổi theo nhu cầu repo).
