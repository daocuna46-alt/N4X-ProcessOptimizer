#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <atomic>
#include "ProcessTab.h"
#include "SettingsTab.h"
#include "LogTab.h"

namespace n4x::ui {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Init(HINSTANCE hInstance, int nCmdShow);
    int Run();
    void Shutdown();

    void RequestShow();
    void RequestRestoreAll();
    void RequestExit();

    HWND Handle() const { return hwnd_; }

private:
    bool CreateAppWindow(HINSTANCE hInstance, int nCmdShow);
    bool CreateDeviceD3D();
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void InitImGui();
    void ShutdownImGui();
    void ApplyTheme(bool dark);

    void RenderFrame();
    void RenderMain();
    void HandleHotkeys();

    void InitTray();
    void RemoveTray();
    void ShowTrayMenu();
    void ToggleVisibility(bool show);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;

    UiState state_;
    ProcessTab processTab_;
    SettingsTab settingsTab_;
    LogTab logTab_;

    bool visible_ = true;
    bool minimizedToTray_ = false;
    bool isAdmin_ = false;
    std::atomic<bool> restoreHotkeyPressed_{false};

    NOTIFYICONDATAW nid_{};
    bool trayAdded_ = false;

    UINT wmTaskbarCreated_ = 0;
};

} // namespace n4x::ui
