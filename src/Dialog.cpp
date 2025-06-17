#define NOMINMAX
#include <atlbase.h>

#include <algorithm>
#include <atlstr.h>
#include <atltypes.h>
#include <atlwin.h>
#include <dwrite.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shobjidl_core.h>
#include <windows.h>

#include "Dialog.h"

#include <array>

#include "aff/Parser.h"
#include "meta.h"
#include "mgxc/EasingSolver.h"
#include "mgxc/Interpolator.h"

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

    auto hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray,
                                            _countof(featureLevelArray), D3D11_SDK_VERSION, &sd, &m_pSwapChain,
                                            &m_pd3dDevice, &featureLevel, &m_pd3dContext);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create D3D11 device and swap chain");
    }

    ID3D11Texture2D *pBackBuffer = nullptr;
    hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get back buffer from swap chain");
    }

    if (pBackBuffer) {
        hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
        if (FAILED(hr)) {
            pBackBuffer->Release();
            throw std::runtime_error("Failed to create render target view");
        }
        pBackBuffer->Release();
    }
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

#pragma region Initialization

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
        if (_wcsicmp(PathFindExtensionW(pathW), L".aff") == 0) {
            const CW2AEX<MAX_PATH * 4> utf8Path(pathW, CP_UTF8);
            const std::string filePath(utf8Path);

            Catch([this, &filePath] {
                aff::Parser parser(m_cctx);
                parser.ParseFile(filePath);
            });
        } else {
            MessageBeep(MB_ICONWARNING);
        }
    }

    DragFinish(hDrop);
    return 0;
}

Dialog::Dialog(Config &cctx, IMargretePluginContext *p_ctx) : m_mg(p_ctx), m_cctx(cctx) {}

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

HRESULT Dialog::ShowDialog() {
    const auto hwnd = m_mg.GetHWND();
    if (hwnd != nullptr && !::IsWindow(hwnd)) {
        return E_FAIL;
    }

    RECT rect = {0, 0, 525, 491};
    Create(hwnd, rect, W_EN_TITLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    CenterWindow();
    ShowWindow(SW_SHOW);
    UpdateWindow();
    ShowImGuiLoop();
    DestroyWindow();
    return S_OK;
}

void Dialog::ShowImGuiLoop() {
    m_running = true;

    MSG msg{};
    while (m_running && IsWindow()) {
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
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        constexpr ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;

        if (ImGui::Begin(EN_TITLE, &m_running, flags)) {
            UI_Main();
            ImGui::End();
            ImGui::Render();

            constexpr float clear[4]{0.45f, 0.55f, 0.60f, 1.0f};
            m_pd3dContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
            m_pd3dContext->ClearRenderTargetView(m_mainRenderTargetView, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            if (FAILED(m_pSwapChain->Present(1, 0))) {
                m_running = false;
            }
        } else {
            ImGui::End();
            break;
        }
    }
}

#pragma endregion Initialization

#pragma region UI

template<class F, class... Args>
bool Dialog::Catch(F &&f, Args &&...args) {
    try {
        std::forward<F>(f)(std::forward<Args>(args)...);
        return true;
    } catch (const std::exception &e) {
        std::wstring w_err;
        w_err.assign(e.what(), e.what() + strlen(e.what()));
        MessageBox(w_err.c_str(), W_EN_TITLE, MB_ICONERROR | MB_OK);
        return false;
    }
}

void Dialog::UI_Main_Column_1() {
    UI_Panel_Config_Import();
    ImGui::Spacing();

    UI_Panel_Config_Convert();
    ImGui::Spacing();

    UI_Panel_Selector_Chains();
    ImGui::Spacing();
}

void Dialog::UI_Main_Column_2() {
    UI_Panel_Selector_Controls();
    ImGui::Spacing();

    UI_Panel_Editor_Chain();
    ImGui::Spacing();

    UI_Panel_Editor_Control();
    ImGui::Spacing();
}

void Dialog::UI_Main() {
    constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##MainTbl", 2, flags)) {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();

        // -------------------- Column 1 --------------------
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginGroup();
        UI_Main_Column_1();
        ImGui::EndGroup();

        // -------------------- Column 2 --------------------
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        UI_Main_Column_2();
        ImGui::EndGroup();
        ImGui::EndTable();
    }
}

void Dialog::UI_Panel_Config_Import() {
    ImGui::TextUnformatted("Import Config");
    ImGui::BeginChild("##Import", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::PushItemWidth(100.0f);
    ImGui::InputInt("Width [1,16]", &m_cctx.width);
    m_cctx.width = std::clamp(m_cctx.width, 1, 16);

    ImGui::InputInt("TIL [0,15]", &m_cctx.til);
    m_cctx.til = std::clamp(m_cctx.til, 0, 15);
    ImGui::PopItemWidth();

    UI_Component_Button_File();
    ImGui::SameLine();
    ImGui::Checkbox("Append", &m_cctx.append);

    ImGui::EndChild();
}

void Dialog::UI_Panel_Editor_Chain() const {
    ImGui::Text("Note [%d]", m_selChain);

    ImGui::BeginChild("##NoteBeginEdit", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    ImGui::PushItemWidth(100.0f);

    if (m_selChain >= 0 && m_selChain < static_cast<int>(m_cctx.chains.size())) {
        auto &note = m_cctx.chains[m_selChain];
        if (!note.empty()) {
            UI_Component_Editor_Chain(note);
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void Dialog::UI_Panel_Editor_Control() const {
    ImGui::Text("Control [%d] @ Note [%d]", m_selControl, m_selChain);
    ImGui::BeginChild("##NoteControlEdit", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    ImGui::PushItemWidth(100.0f);
    if (m_selControl >= 0 && m_selControl < static_cast<int>(m_cctx.chains[m_selChain].size())) {
        auto &note = m_cctx.chains[m_selChain][m_selControl];

        ImGui::InputInt("t", &note.t);
        note.t = std::max(note.t, 0);
        ImGui::SameLine();
        const auto t = std::round(static_cast<double>(note.t) / m_cctx.snap) * m_cctx.snap;
        ImGui::Text("-> %d", static_cast<int>(t));

        ImGui::Separator();
        ImGui::InputInt("x", &note.x);
        UI_Component_Combo_EasingMode("eX", note.eX);

        ImGui::Separator();
        ImGui::InputInt("y [0,360]", &note.y);
        note.y = std::clamp(note.y, 0, 360);
        UI_Component_Combo_EasingMode("eY", note.eY);
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
}

template<class Container, class Creator, class Labeler, class Extra>
void Dialog::UI_Component_Editor_Vector(int &selIndex, Container &vec, Creator creator, Labeler labeler, Extra extra,
                                        const bool showClear, const int minItems) {
    if (ImGui::SmallButton("+")) {
        vec.emplace_back(creator());
        selIndex = static_cast<int>(vec.size()) - 1;
    }
    ImGui::SameLine();

    const bool canRemove =
            selIndex >= 0 && selIndex < static_cast<int>(vec.size()) && vec.size() > static_cast<std::size_t>(minItems);

    ImGui::BeginDisabled(!canRemove);
    if (ImGui::SmallButton("-") && canRemove) {
        vec.erase(vec.begin() + selIndex);
        if (vec.empty()) {
            selIndex = -1;
        } else if (selIndex >= static_cast<int>(vec.size())) {
            selIndex = static_cast<int>(vec.size()) - 1;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (showClear) {
        const bool canClear = vec.size() > static_cast<std::size_t>(minItems);

        ImGui::BeginDisabled(!canClear);
        if (ImGui::SmallButton("Clear") && canClear) {
            vec.clear();
            selIndex = -1;
        }
        ImGui::EndDisabled();
    }

    if constexpr (!std::is_same_v<Extra, std::nullptr_t>) {
        extra();
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        ImGui::PushID(i);
        std::string lbl = labeler(vec[i], i);
        if (ImGui::Selectable(lbl.c_str(), i == selIndex))
            selIndex = i;
        ImGui::PopID();
    }
}

constexpr std::string_view GetNoteTypeString(const MpInteger type) {
    switch (type) {
        case MP_NOTETYPE_AIRSLIDE:
            return "AirSlide";
        case MP_NOTETYPE_AIRCRUSH:
            return "AirCrush";
        case MP_NOTETYPE_SLIDE:
            return "Slide";
        default:
            return "???";
    }
}

void Dialog::UI_Panel_Selector_Chains() {
    const auto creator = [] {
        using enum EasingMode;
        return std::vector{mgxc::Note{MP_NOTETYPE_AIRSLIDE, 0, 0, In, 80, In, 4, 0},
                           mgxc::Note{MP_NOTETYPE_AIRSLIDE, 960, 12, In, 80, In, 4, 0}};
    };

    const auto labeler = [](std::vector<mgxc::Note> const &c, const int idx) {
        if (c.empty()) {
            return std::format("![{}] Empty", idx);
        }
        const auto type = GetNoteTypeString(c.front().type);
        const auto prefix = c.size() <= 1 ? "!" : "";
        return std::format("{}[{}] {}:{} @ {}", prefix, idx, type, c.size(), c.front().t);
    };

    const auto extra = [this] {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.0f, 0.0f, 1.0f));

        ImGui::SameLine();
        ImGui::BeginDisabled(!m_mg.CanCommit() || m_selChain < 0 ||
                             m_selChain >= static_cast<int>(m_cctx.chains.size()));
        if (ImGui::SmallButton("Commit")) {
            Convert(m_selChain);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!m_mg.CanCommit() || m_cctx.chains.empty());
        if (ImGui::SmallButton("Commit All") && !m_cctx.chains.empty()) {
            Convert();
        }
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
    };

    ImGui::TextUnformatted("Notes");
    ImGui::BeginChild("##NoteSelector", {m_childWidth, m_childHeight}, ImGuiChildFlags_Border);
    UI_Component_Editor_Vector(m_selChain, m_cctx.chains, creator, labeler, extra);
    ImGui::EndChild();
}

constexpr std::string_view GetModeString(const EasingMode mode) {
    switch (mode) {
        using enum EasingMode;
        case Linear:
            return "s ";
        case In:
            return "si";
        case Out:
            return "so";
        default:
            return "??";
    }
}

void Dialog::UI_Panel_Selector_Controls() {
    const auto creator = [this] {
        mgxc::Note control{};
        if (m_selChain >= 0 && m_selChain < static_cast<int>(m_cctx.chains.size())) {
            const auto &note = m_cctx.chains[m_selChain];
            if (m_selControl >= 0 && m_selControl < static_cast<int>(note.size())) {
                control = note[m_selControl];
                control.t += m_cctx.snap;
            }
        }
        return control;
    };

    const auto extra = [this] {
        ImGui::SameLine();

        ImGui::BeginDisabled(m_selChain < 0 || m_selChain >= static_cast<int>(m_cctx.chains.size()) ||
                             m_cctx.chains[m_selChain].empty());
        if (ImGui::SmallButton("Sort") && m_selChain >= 0 && m_selChain < static_cast<int>(m_cctx.chains.size())) {
            auto &note = m_cctx.chains[m_selChain];
            std::ranges::sort(note);
        }
        ImGui::EndDisabled();
    };

    ImGui::Text("Controls @ Note [%d]", m_selChain);
    ImGui::BeginChild("##ControlSelector", {m_childWidth, m_childHeight}, ImGuiChildFlags_Border);

    if (m_selChain >= 0 && m_selChain < static_cast<int>(m_cctx.chains.size()) &&
        m_selControl < static_cast<int>(m_cctx.chains[m_selChain].size())) {
        auto &note = m_cctx.chains[m_selChain];
        const auto labeler = [note](mgxc::Note const &n, int idx) {
            std::string eX = idx == note.size() - 1 ? "--" : GetModeString(n.eX).data();
            std::string eY = idx == note.size() - 1 ? "--" : GetModeString(n.eY).data();
            return std::format("[{}] {}{} ({},{}) @ {}", idx, eX, eY, n.x, n.y, n.t);
        };
        const auto title = std::format("Controls @ Note [{}]", m_selChain);
        UI_Component_Editor_Vector(m_selControl, note, creator, labeler, extra, false, 2);
    } else {
        m_selControl = -1;
    }

    ImGui::EndChild();
}

void Dialog::UI_Component_Button_File() {
    if (!ImGui::Button("Import *.aff")) {
        return;
    }

    OPENFILENAMEW ofn = {};
    WCHAR szFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"ArcCreate Chart (*.aff)\0*.aff\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    const CW2AEX<MAX_PATH * 4> utf8Path(szFile, CP_UTF8);
    const std::string filePath(utf8Path);

    Catch([this, filePath] {
        aff::Parser parser(m_cctx);
        parser.ParseFile(filePath);
    });
}

void Dialog::UI_Component_Editor_Chain(std::vector<mgxc::Note> &chain) {
    auto width = chain.front().width;
    if (ImGui::InputInt("Width [1,16]", &width)) {
        for (auto &c: chain) {
            c.width = std::clamp(width, 1, 16);
        }
    }

    auto til = chain.front().til;
    if (ImGui::InputInt("TIL [0,15]", &til)) {
        for (auto &c: chain) {
            c.til = std::clamp(til, 0, 15);
        }
    }

    constexpr std::array kItems{
            std::pair{MP_NOTETYPE_SLIDE, GetNoteTypeString(MP_NOTETYPE_SLIDE)},
            std::pair{MP_NOTETYPE_AIRSLIDE, GetNoteTypeString(MP_NOTETYPE_AIRSLIDE)},
            std::pair{MP_NOTETYPE_AIRCRUSH, GetNoteTypeString(MP_NOTETYPE_AIRCRUSH)},
    };

    const auto it = std::ranges::find_if(kItems, [&chain](const auto &p) { return p.first == chain.front().type; });
    auto current = static_cast<int>(std::distance(kItems.begin(), it));
    if (current >= static_cast<int>(kItems.size())) {
        current = 0;
        for (auto &c: chain) {
            c.type = kItems[current].first;
        }
    }

    if (ImGui::BeginCombo("Note Type", kItems[current].second.data())) {
        for (std::size_t i = 0; i < kItems.size(); ++i) {
            const bool selected = i == static_cast<std::size_t>(current);
            if (ImGui::Selectable(kItems[i].second.data(), selected)) {
                current = static_cast<int>(i);
                for (auto &c: chain) {
                    c.type = kItems[current].first;
                }
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}


void Dialog::UI_Component_Combo_EasingMode(const std::string_view &label, EasingMode &esMode) {
    static const char *esModeNames[] = {"Linear", "In", "Out"};
    auto esModeInt = static_cast<int>(esMode);
    if (ImGui::Combo(label.data(), &esModeInt, esModeNames, IM_ARRAYSIZE(esModeNames))) {
        esMode = static_cast<EasingMode>(esModeInt);
    }
}

void Dialog::UI_Panel_Config_Convert() const {
    ImGui::TextUnformatted("Convert Config");
    ImGui::BeginChild("##Config", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    // Division
    {
        static const std::vector snapDivisors = {1920, 960, 480, 384, 192, 128, 96, 64, 48, 32, 24, 16, 12, 8, 4};
        int selectedDivisorIndex = -1;
        if (m_cctx.snap > 0 && mgxc::BAR_TICKS % m_cctx.snap == 0) {
            const int divisor = mgxc::BAR_TICKS / m_cctx.snap;
            const auto it = std::ranges::find(snapDivisors, divisor);
            if (it != snapDivisors.end()) {
                selectedDivisorIndex = static_cast<int>(std::distance(snapDivisors.begin(), it));
            }
        }

        std::string previewStr;
        if (selectedDivisorIndex >= 0) {
            previewStr = std::to_string(snapDivisors[selectedDivisorIndex]);
        }

        ImGui::PushItemWidth(55.0f);

        if (ImGui::BeginCombo("=", previewStr.c_str())) {
            for (int i = 0; i < snapDivisors.size(); i++) {
                const bool isSelected = selectedDivisorIndex == i;
                if (ImGui::Selectable(std::to_string(snapDivisors[i]).c_str(), isSelected)) {
                    m_cctx.snap = mgxc::BAR_TICKS / snapDivisors[i];
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(85.0f);
        ImGui::SameLine();

        int snapInput = m_cctx.snap;
        if (ImGui::InputInt("Division", &snapInput)) {
            snapInput = std::clamp(snapInput, 1, mgxc::BAR_TICKS);
            m_cctx.snap = snapInput;
        }

        ImGui::PopItemWidth();
    }

    ImGui::Separator();
    ImGui::PushItemWidth(100.0f);

    // Easing
    {
        auto &[m_kind, m_exponent, m_epsilon] = m_cctx.solver;
        static const char *esTypeNames[] = {"Sine", "Power", "Circular"};
        auto esTypeInt = static_cast<int>(m_kind);

        if (ImGui::Combo("Easing", &esTypeInt, esTypeNames, IM_ARRAYSIZE(esTypeNames))) {
            m_kind = static_cast<EasingKind>(esTypeInt);
        }

        if (m_kind == EasingKind::Power) {
            const int prev = m_exponent;
            ImGui::InputInt("Exponent [!=0]", &m_exponent);
            if (m_exponent == 0) {
                if (prev > m_exponent) {
                    m_exponent = -1;
                } else {
                    m_exponent = 1;
                }
            }
        } else if (m_kind == EasingKind::Circular) {
            ImGui::InputDouble("Linearity [0,1]", &m_epsilon, 0.01, 0.1, "%.3f");
            m_epsilon = std::clamp(m_epsilon, 0.0, 1.0);
        }
    }

    ImGui::Separator();

    ImGui::InputInt("x Offset [±15]", &m_cctx.xOffset);
    m_cctx.xOffset = std::clamp(m_cctx.xOffset, -15, 15);

    ImGui::InputInt("y Offset [±360]", &m_cctx.yOffset);
    m_cctx.yOffset = std::clamp(m_cctx.yOffset, -360, 360);

    ImGui::Checkbox("Clamp (x,y)", &m_cctx.clamp);

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void Dialog::Convert(const int idx) {
    Catch([this, idx] {
        m_cctx.tOffset = m_mg.GetTickOffset();
        auto intp = Interpolator(m_cctx);
        intp.Convert(idx);
        intp.CommitChart(m_mg);
    });
}

#pragma endregion UI
