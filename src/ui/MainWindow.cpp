#include "MainWindow.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "../core/Logger.h"
#include "../core/Config.h"
#include "../core/ProcessManager.h"
#include "../utils/WinUtil.h"
#include <shellapi.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace n4x::ui {

static const UINT WM_TRAYICON = WM_APP + 1;
static const UINT ID_TRAY_SHOW = 1001;
static const UINT ID_TRAY_RESTORE = 1002;
static const UINT ID_TRAY_EXIT = 1003;

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() { Shutdown(); }

bool MainWindow::Init(HINSTANCE hInstance, int nCmdShow) {
    hInstance_ = hInstance;
    isAdmin_ = winutil::IsUserAdmin();
    wmTaskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (!CreateAppWindow(hInstance, nCmdShow)) return false;
    if (!CreateDeviceD3D()) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    InitImGui();
    InitTray();
    Logger::Instance().Info("UI initialized");
    return true;
}

bool MainWindow::CreateAppWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"N4XProcessOptimizerClass";
    wc.hIcon = LoadIconW(hInstance, L"IDI_APP_ICON");
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"N4X ProcessOptimizer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 780,
        nullptr, nullptr, hInstance, this);
    if (!hwnd_) return false;

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

bool MainWindow::CreateDeviceD3D() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain_, &device_, &featureLevel, &context_);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain_, &device_, &featureLevel, &context_);
    }
    if (FAILED(hr)) {
        Logger::Instance().Error("D3D11CreateDeviceAndSwapChain failed");
        return false;
    }
    CreateRenderTarget();
    return true;
}

void MainWindow::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (swapChain_ && SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        device_->CreateRenderTargetView(pBackBuffer, nullptr, &rtv_);
        pBackBuffer->Release();
    }
}

void MainWindow::CleanupRenderTarget() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
}

void MainWindow::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (context_) { context_->Release(); context_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void MainWindow::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // don't clutter; we have our own config
    ImGui::StyleColorsDark();
    ApplyTheme(Config::Instance().Get().themeDark);

    // Fonts: Segoe UI
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    if (!font) io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);
}

void MainWindow::ShutdownImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void MainWindow::ApplyTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.WindowPadding = ImVec2(12, 12);

    if (dark) {
        ImGui::StyleColorsDark();
        ImVec4* c = style.Colors;
        ImVec4 bg = ImVec4(0.07f, 0.07f, 0.11f, 1.00f);
        ImVec4 panel = ImVec4(0.11f, 0.11f, 0.16f, 1.00f);
        ImVec4 accent = ImVec4(0.486f, 0.227f, 0.929f, 1.00f); // #7C3AED
        ImVec4 accent2 = ImVec4(0.024f, 0.714f, 0.831f, 1.00f); // #06B6D4
        c[ImGuiCol_WindowBg] = bg;
        c[ImGuiCol_ChildBg] = panel;
        c[ImGuiCol_PopupBg] = ImVec4(0.10f,0.10f,0.14f,1.0f);
        c[ImGuiCol_Border] = ImVec4(0.20f,0.20f,0.28f,1.0f);
        c[ImGuiCol_FrameBg] = ImVec4(0.16f,0.16f,0.22f,1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f,0.22f,0.30f,1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.28f,0.28f,0.36f,1.0f);
        c[ImGuiCol_TitleBg] = ImVec4(0.10f,0.08f,0.18f,1.0f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.20f,0.12f,0.35f,1.0f);
        c[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 0.75f);
        c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 1.0f);
        c[ImGuiCol_ButtonActive] = ImVec4(accent2.x, accent2.y, accent2.z, 1.0f);
        c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.7f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent2.x, accent2.y, accent2.z, 0.8f);
        c[ImGuiCol_Tab] = ImVec4(0.16f,0.16f,0.22f,1.0f);
        c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.8f);
        c[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 1.0f);
        c[ImGuiCol_CheckMark] = accent2;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent2;
        c[ImGuiCol_Separator] = ImVec4(0.25f,0.25f,0.32f,1.0f);
        c[ImGuiCol_TableHeaderBg] = ImVec4(0.14f,0.14f,0.20f,1.0f);
        c[ImGuiCol_TableBorderStrong] = ImVec4(0.25f,0.25f,0.32f,1.0f);
        c[ImGuiCol_TableBorderLight] = ImVec4(0.20f,0.20f,0.28f,1.0f);
    } else {
        ImGui::StyleColorsLight();
    }
}

void MainWindow::InitTray() {
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = LoadIconW(hInstance_, L"IDI_APP_ICON");
    if (!nid_.hIcon) nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"N4X ProcessOptimizer");
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
}

void MainWindow::RemoveTray() {
    if (trayAdded_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        trayAdded_ = false;
    }
}

void MainWindow::ShowTrayMenu() {
    POINT pt; GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show");
    AppendMenuW(menu, MF_STRING, ID_TRAY_RESTORE, L"Restore All");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::ToggleVisibility(bool show) {
    if (show) {
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
        visible_ = true;
        minimizedToTray_ = false;
    } else {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
        minimizedToTray_ = true;
    }
}

void MainWindow::RequestShow() { ToggleVisibility(true); }
void MainWindow::RequestRestoreAll() { state_.requestRestore.store(true); }
void MainWindow::RequestExit() { state_.requestExit.store(true); }

void MainWindow::HandleHotkeys() {
    // Ctrl+Shift+R
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool r = (GetAsyncKeyState('R') & 0x8000) != 0;
    if (ctrl && shift && r) {
        if (!restoreHotkeyPressed_.exchange(true)) {
            Logger::Instance().Warn("Hotkey Ctrl+Shift+R: Restore All requested");
            ProcessManager::Instance().RequestRestoreAll();
        }
    } else {
        restoreHotkeyPressed_.store(false);
    }
}

void MainWindow::RenderFrame() {
    HandleHotkeys();
    if (state_.requestRestore.exchange(false)) {
        ProcessManager::Instance().RequestRestoreAll();
    }
    if (state_.requestSave.exchange(false)) {
        auto p = Config::Instance().Path();
        if (p.empty()) p = Config::DefaultPath();
        Config::Instance().Save(p);
    }
    if (state_.requestExit.exchange(false)) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

    ApplyTheme(Config::Instance().Get().themeDark);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderMain();

    ImGui::Render();
    const float clear[4] = {0.07f, 0.07f, 0.11f, 1.0f};
    context_->OMSetRenderTargets(1, &rtv_, nullptr);
    context_->ClearRenderTargetView(rtv_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain_->Present(1, 0);
}

void MainWindow::RenderMain() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("N4X ProcessOptimizer##main", nullptr, flags);

    // Header
    ImGui::TextColored(ImVec4(0.49f,0.23f,0.93f,1.0f), "N4X");
    ImGui::SameLine();
    ImGui::TextUnformatted("ProcessOptimizer");
    ImGui::SameLine();
    ImGui::Spacing(); ImGui::SameLine();
    if (isAdmin_) ImGui::TextColored(ImVec4(0.3f,0.9f,0.5f,1.0f), "[Admin OK]");
    else ImGui::TextColored(ImVec4(1.0f,0.6f,0.2f,1.0f), "[Read-only]");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isAdmin_ ? "Dang chay voi quyen Administrator." : "Khong co quyen Admin - chi xem, khong toi uu.");

    ImGui::SameLine(ImGui::GetWindowWidth() - 320);
    if (ImGui::Button("Optimize Now", ImVec2(120, 30))) {
        if (isAdmin_) { ProcessManager::Instance().OptimizeOnce(); Logger::Instance().Info("Manual optimize triggered"); }
        else Logger::Instance().Warn("Optimize blocked: not admin");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ap dung toi uu ngay lap tuc cho cac tien trinh da chon.");
    ImGui::SameLine();
    if (ImGui::Button("Restore All", ImVec2(110, 30))) {
        ProcessManager::Instance().RequestRestoreAll();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Khoi phuc NORMAL priority + affinity mac dinh cho tat ca process da bi doi. Phim tat: Ctrl+Shift+R");

    ImGui::Separator();

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Tien trinh")) {
            processTab_.Render(state_, isAdmin_);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cai dat")) {
            settingsTab_.Render(state_, isAdmin_);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Log")) {
            logTab_.Render(state_);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Luu config", ImVec2(120, 28))) state_.requestSave.store(true);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Luu cau hinh hien tai vao config.json.");
    ImGui::SameLine();
    if (ImGui::Button("Restore All##bottom", ImVec2(120, 28))) ProcessManager::Instance().RequestRestoreAll();
    ImGui::SameLine();
    if (ImGui::Button("Thoat", ImVec2(100, 28))) state_.requestExit.store(true);
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+Shift+R = Restore All (panic)");

    ImGui::End();
}

int MainWindow::Run() {
    MSG msg;
    while (state_.running.load()) {
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) state_.running.store(false);
        }
        if (!state_.running.load()) break;
        if (!visible_) { Sleep(50); continue; }
        RenderFrame();
    }
    return 0;
}

void MainWindow::Shutdown() {
    RemoveTray();
    ShutdownImGui();
    CleanupDeviceD3D();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    case WM_SIZE:
        if (self && self->device_ != nullptr && wParam != SIZE_MINIMIZED) {
            self->CleanupRenderTarget();
            self->swapChain_->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            self->CreateRenderTarget();
        }
        if (self && wParam == SIZE_MINIMIZED) {
            self->ToggleVisibility(false);
            return 0;
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        if ((wParam & 0xfff0) == SC_MINIMIZE && self) { self->ToggleVisibility(false); return 0; }
        break;
    case WM_CLOSE:
        if (self) {
            // Restore on exit for safety
            ProcessManager::Instance().RequestRestoreAll();
            Sleep(200);
        }
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_TRAYICON:
        if (self) {
            if (lParam == WM_LBUTTONDBLCLK) self->ToggleVisibility(true);
            else if (lParam == WM_RBUTTONUP) self->ShowTrayMenu();
        }
        return 0;
    case WM_COMMAND:
        if (self) {
            switch (LOWORD(wParam)) {
            case ID_TRAY_SHOW: self->ToggleVisibility(true); break;
            case ID_TRAY_RESTORE: ProcessManager::Instance().RequestRestoreAll(); break;
            case ID_TRAY_EXIT: self->RequestExit(); break;
            }
        }
        return 0;
    }
    if (self && msg == self->wmTaskbarCreated_ && self->wmTaskbarCreated_ != 0) {
        self->trayAdded_ = false;
        self->InitTray();
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace n4x::ui
