#include <Windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include "core/Logger.h"
#include "core/Config.h"
#include "core/SafetyGuard.h"
#include "core/ProcessManager.h"
#include "ui/MainWindow.h"
#include "utils/WinUtil.h"

using namespace n4x;

static std::wstring GetArgValue(int argc, wchar_t** argv, const wchar_t* key) {
    for (int i = 0; i < argc - 1; ++i) {
        if (_wcsicmp(argv[i], key) == 0) return argv[i + 1];
    }
    return L"";
}

static bool HasArg(int argc, wchar_t** argv, const wchar_t* key) {
    for (int i = 0; i < argc; ++i) {
        if (_wcsicmp(argv[i], key) == 0) return true;
    }
    return false;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (HasArg(argc, argv, L"--restore")) {
        int rc = ProcessManager::RunRestoreCLI();
        if (argv) LocalFree(argv);
        return rc;
    }

    // Single instance mutex
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\N4X_ProcessOptimizer_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"N4XProcessOptimizerClass", L"N4X ProcessOptimizer");
        if (existing) {
            ShowWindow(existing, SW_SHOW);
            SetForegroundWindow(existing);
        }
        CloseHandle(hMutex);
        if (argv) LocalFree(argv);
        return 0;
    }

    // Init paths
    std::wstring exeDir = winutil::GetModuleDir();
    std::wstring configPath = exeDir + L"\\config.json";
    std::wstring logPath = exeDir + L"\\N4X_ProcessOptimizer.log";

    Logger::Instance().Init(logPath);
    Logger::Instance().Info("=== N4X ProcessOptimizer starting ===");
    Logger::Instance().Info(std::string("Admin=") + (winutil::IsUserAdmin() ? "true" : "false"));
    Logger::Instance().Info("Config path: " + winutil::WideToUtf8(configPath));

    SafetyGuard::Instance().Init();
    Config::Instance().SetPath(configPath);
    Config::Instance().Load(configPath);
    // Sync selected names into manager
    {
        auto cfg = Config::Instance().Get();
        ProcessManager::Instance().SetSelectedNames(cfg.selectedProcessNames);
    }

    ProcessManager::Instance().Start();

    ui::MainWindow win;
    int rc = 0;
    if (!win.Init(hInstance, nCmdShow)) {
        Logger::Instance().Error("Failed to initialize main window");
        rc = 1;
    } else {
        rc = win.Run();
    }

    // Safety: restore on exit
    Logger::Instance().Info("Shutting down - restoring all modified processes");
    ProcessManager::Instance().RequestRestoreAll();
    // Give optimizer a moment to process restore
    Sleep(300);
    ProcessManager::Instance().Stop();

    auto cfgPath = Config::Instance().Path();
    if (cfgPath.empty()) cfgPath = Config::DefaultPath();
    Config::Instance().Save(cfgPath);

    Logger::Instance().Info("=== N4X ProcessOptimizer exit ===");
    Logger::Instance().Shutdown();

    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    if (argv) LocalFree(argv);
    return rc;
}
