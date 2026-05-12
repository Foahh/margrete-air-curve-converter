#include <atlbase.h>

#include <atlstr.h>
#include <atltypes.h>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <stdexcept>
#include <utility>
#include <windows.h>

#include "Dialog.h"

#include "aff/Parser.h"
#include "meta.h"

namespace {
std::string ToUtf8Path(const wchar_t *path) {
    const CW2AEX<MAX_PATH * 4> utf8Path(path, CP_UTF8);
    return std::string(utf8Path);
}
} // namespace

Dialog::Dialog(Config &cctx, IMargretePluginContext *p_ctx, std::stop_token st) :
    m_st(std::move(st)), m_mg(p_ctx), m_cctx(cctx) {}

#pragma region DirectX

void Dialog::CreateDeviceD3D() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    constexpr D3D_FEATURE_LEVEL featureLevelArray[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray,
                                                     _countof(featureLevelArray), D3D11_SDK_VERSION, &sd, &m_pSwapChain,
                                                     &m_pd3dDevice, &featureLevel, &m_pd3dContext);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create D3D11 device and swap chain");
    }

    ID3D11Texture2D *backBuffer = nullptr;
    if (FAILED(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        throw std::runtime_error("Failed to get back buffer from swap chain");
    }

    if (FAILED(m_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &m_mainRenderTargetView))) {
        backBuffer->Release();
        throw std::runtime_error("Failed to create render target view");
    }

    backBuffer->Release();
}

void Dialog::CleanupDeviceD3D() {
    if (m_mainRenderTargetView) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pd3dContext) {
        m_pd3dContext->Release();
        m_pd3dContext = nullptr;
    }
    if (m_pd3dDevice) {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
}

#pragma endregion DirectX

#pragma region Win32

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT Dialog::OnRange(const UINT uMsg, const WPARAM wParam, const LPARAM lParam, BOOL &bHandled) const {
    if (uMsg == WM_NCHITTEST) {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(&pt);

        RECT rc = {};
        GetClientRect(&rc);

        if (pt.y < 30 && pt.x < rc.right - 30) {
            return HTCAPTION;
        }
    }

    if (ImGui_ImplWin32_WndProcHandler(m_hWnd, uMsg, wParam, lParam)) {
        bHandled = TRUE;
    } else {
        bHandled = FALSE;
    }

    return 0;
}

LRESULT Dialog::OnDropFiles(UINT, const WPARAM wParam, LPARAM, BOOL &) {
    const auto hDrop = reinterpret_cast<HDROP>(wParam);

    WCHAR pathW[MAX_PATH]{};
    if (DragQueryFileW(hDrop, 0, pathW, MAX_PATH) != 0) {
        if (std::filesystem::path(pathW).extension() == L".aff") {
            TryImportAffFile(ToUtf8Path(pathW));
        } else {
            MessageBeep(MB_ICONWARNING);
        }
    }

    DragFinish(hDrop);
    return 0;
}

LRESULT Dialog::OnCreate(UINT, WPARAM, LPARAM, BOOL &) {
    CreateDeviceD3D();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dContext);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    SetFocus();

    DragAcceptFiles(TRUE);
    return 0;
}

LRESULT Dialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL &) {
    m_running = false;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    return 0;
}

#pragma endregion Win32

#pragma region Dialog

HRESULT Dialog::ShowDialog() {
    const HWND owner = m_mg.GetHWND();
    if (owner != nullptr && !::IsWindow(owner)) {
        return E_FAIL;
    }

    RECT rect = {0, 0, 740, 370};
    const HWND dialog = Create(owner, rect, W_DIALOG_TITLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    if (dialog == nullptr) {
        const DWORD lastError = GetLastError();
        return lastError == 0 ? E_FAIL : HRESULT_FROM_WIN32(lastError);
    }

    CenterWindow();
    ShowWindow(SW_SHOW);
    UpdateWindow();
    RenderImGui();
    DestroyWindow();
    return S_OK;
}

void Dialog::RenderImGui() {
    m_running = true;

    MSG msg{};
    while (m_running && IsWindow()) {
        if (m_st.stop_requested()) {
            m_running = false;
            break;
        }

        while (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!m_running) {
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_running = false;
            break;
        }

        UI_Error();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        if (ImGui::Begin(DIALOG_TITLE, &m_running,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize)) {
            UI_Main();
        }

        ImGui::End();
        ImGui::Render();

        constexpr float clear[4]{0.45f, 0.55f, 0.60f, 1.0f};
        m_pd3dContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dContext->ClearRenderTargetView(m_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (FAILED(m_pSwapChain->Present(1, 0))) {
            m_running = false;
        }
    }
}

#pragma endregion Dialog

#pragma region Helper

template<class F, class... Args>
bool Dialog::Catch(F &&f, Args &&...args) {
    try {
        std::forward<F>(f)(std::forward<Args>(args)...);
        return true;
    } catch (const std::exception &e) {
        ShowError(e.what());
        return false;
    }
}

void Dialog::ShowError(std::string text) {
    m_errorText = std::move(text);
    m_showErrorPopup = true;
}

bool Dialog::TryImportAffFile(const std::string &filePath) {
    return Catch([this, &filePath] {
        aff::Parser parser(m_cctx);
        parser.ParseFile(filePath);
    });
}

void Dialog::Commit(const int idx) {
    Catch([this, idx] {
        m_cctx.tOffset = m_mg.GetTickOffset();
        Interpolator interpolator(m_cctx);
        interpolator.Convert(idx);

        if (m_mg.CanCommit()) {
            interpolator.Commit(m_mg);
        }
    });
}

bool Dialog::IsRunning() const noexcept { return m_running; }

void Dialog::SelChain_Sort() {
    if (!SelControl_InRange()) {
        return;
    }

    mgxc::Chain &chain = m_cctx.chains[m_selChain];
    const size_t selectedId = chain[m_selControl].GetID();
    chain.sort();

    for (std::size_t i = 0; i < chain.size(); ++i) {
        if (chain[i].GetID() == selectedId) {
            m_selControl = static_cast<int>(i);
            break;
        }
    }
}

bool Dialog::SelChain_InRange() const noexcept { return utils::in_bounds(m_cctx.chains, m_selChain); }

bool Dialog::SelControl_InRange() const noexcept {
    return SelChain_InRange() && utils::in_bounds(m_cctx.chains[m_selChain], m_selControl);
}

#pragma endregion Helper
