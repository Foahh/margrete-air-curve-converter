#define NOMINMAX
#include <atlbase.h>

#include <algorithm>
#include <array>
#include <atlstr.h>
#include <atltypes.h>
#include <atlwin.h>
#include <dwrite.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shobjidl_core.h>
#include <utility>
#include <windows.h>

#include "Dialog.h"

#include "Utils.h"
#include "aff/Parser.h"
#include "meta.h"
#include "mgxc/Easing.h"
#include "mgxc/Interpolator.h"

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
    const auto hwnd = m_mg.GetHWND();
    if (hwnd != nullptr && !::IsWindow(hwnd)) {
        return E_FAIL;
    }

    RECT rect = {0, 0, 740, 370};
    Create(hwnd, rect, W_DIALOG_TITLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
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

#pragma endregion Initialization

#pragma region UI

void Dialog::UI_Error() {
    const auto *vp = ImGui::GetMainViewport();
    const float w = vp->Size.x * 0.5f;
    const auto pos = ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);

    ImGui::SetNextWindowSize(ImVec2(w, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (m_showErrorPopup) {
        ImGui::OpenPopup("Error##Modal");
        m_showErrorPopup = false;
    }

    if (ImGui::BeginPopupModal("Error##Modal", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("%s", m_errorText.c_str());
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(-FLT_MIN, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Dialog::UI_Main_Column_1() {
    UI_Panel_Config_Global();
    ImGui::Spacing();

    UI_Panel_Config_Import();
    ImGui::Spacing();

    UI_Panel_Config_Commit();
    ImGui::Spacing();
}

void Dialog::UI_Main_Column_2() {
    UI_Panel_Selector_Chains();
    ImGui::Spacing();

    UI_Panel_Editor_Chain();
    ImGui::Spacing();
}

void Dialog::UI_Main_Column_3() {
    UI_Panel_Selector_Controls();
    ImGui::Spacing();

    UI_Panel_Editor_Control();
    ImGui::Spacing();
}

void Dialog::UI_Main() {
    constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##MainTbl", 3, flags)) {
        ImGui::TableSetupColumn("##MainTbl@0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##MainTbl@1", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##MainTbl@2", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginGroup();
        UI_Main_Column_1();
        ImGui::EndGroup();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        UI_Main_Column_2();
        ImGui::EndGroup();

        ImGui::TableSetColumnIndex(2);
        ImGui::BeginGroup();
        UI_Main_Column_3();
        ImGui::EndGroup();

        ImGui::EndTable();
    }
}

void Dialog::UI_Panel_Config_Global() {
    ImGui::TextUnformatted("Global Config");
    ImGui::BeginChild("##Global", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::PushItemWidth(100.0f);
    UI_Component_Combo_Division();
    ImGui::PopItemWidth();

    ImGui::EndChild();
}

void Dialog::UI_Panel_Config_Import() {
    ImGui::TextUnformatted("Import Config");
    ImGui::BeginChild("##Import", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::PushItemWidth(100.0f);

    if (ImGui::InputInt("Width [1,16]", &m_cctx.width)) {
        m_cctx.width = std::clamp(m_cctx.width, 1, 16);
    }

    if (ImGui::InputInt("TIL [0,15]", &m_cctx.til)) {
        m_cctx.til = std::clamp(m_cctx.til, 0, 15);
    }

    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.35f, 0.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.93f, 0.46f, 0.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.28f, 0.00f, 1.00f));

    UI_Component_Button_File();

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::Checkbox("Append", &m_cctx.append);

    ImGui::EndChild();
}

void Dialog::UI_Panel_Editor_Chain() const {
    ImGui::Text("Edit Note [%d]", m_selChain);

    ImGui::BeginChild("##NoteBeginEdit", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    ImGui::PushItemWidth(100.0f);

    if (SelChain_InRange()) {
        auto &chain = m_cctx.chains[m_selChain];
        if (!chain.empty()) {
            UI_Component_Combo_Note(chain);
            ImGui::Separator();

            UI_Component_Combo_EasingKind(chain);
            ImGui::Separator();

            if (ImGui::InputInt("Width [1,16]", &chain.width)) {
                chain.width = std::clamp(chain.width, 1, 16);
            }

            if (ImGui::InputInt("TIL [0,15]", &chain.til)) {
                chain.til = std::clamp(chain.til, 0, 15);
            }
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void Dialog::UI_Panel_Editor_Control() {
    ImGui::Text("Edit Control [%d] @ Note [%d]", m_selControl, m_selChain);
    ImGui::BeginChild("##NoteControlEdit", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    ImGui::PushItemWidth(100.0f);

    if (SelControl_InRange()) {
        auto &chain = m_cctx.chains[m_selChain];
        auto &note = chain[m_selControl];

        const auto pT = note.t;
        if (ImGui::InputInt("t", &note.t)) {
            note.t = std::max(note.t, 0);
            if (note.t != pT) {
                SelChain_Sort();
            }
        }

        ImGui::SameLine();
        const auto t = std::round(static_cast<double>(note.t) / m_cctx.snap) * m_cctx.snap;
        ImGui::Text("-> %d", static_cast<int>(t));

        ImGui::Separator();
        ImGui::InputInt("x", &note.x);
        UI_Component_Combo_EasingMode("eX", note.eX);

        ImGui::Separator();
        if (ImGui::InputInt("y [0,360]", &note.y)) {
            note.y = std::clamp(note.y, 0, 360);
        }

        UI_Component_Combo_EasingMode("eY", note.eY);
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();
}


template<std::size_t Min, bool ShowClear, class Container, class Creator, class Labeler, class Changed, class Extra>
void Dialog::UI_Component_Editor_Vector(int &selIndex, Container &vec, Creator creator, Labeler labeler,
                                        Changed changed, Extra extra) {
    if (ImGui::SmallButton("+")) {
        vec.push_back(std::move(creator()));
        selIndex = static_cast<int>(vec.size()) - 1;
        if constexpr (!std::is_same_v<Changed, std::nullptr_t>) {
            changed();
        }
    }

    const bool canRemove = utils::in_bounds(vec, selIndex) && vec.size() > Min;
    ImGui::SameLine();
    ImGui::BeginDisabled(!canRemove);
    if (ImGui::SmallButton("-") && canRemove) {
        vec.erase(vec.begin() + selIndex);

        if (vec.empty()) {
            selIndex = -1;
        } else if (selIndex >= vec.size()) {
            selIndex = static_cast<int>(vec.size()) - 1;
        }

        if constexpr (!std::is_same_v<Changed, std::nullptr_t>) {
            changed();
        }
    }
    ImGui::EndDisabled();

    if (ShowClear) {
        const bool canClear = vec.size() > Min;

        ImGui::SameLine();
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

    ImGui::BeginChild("##SelectorContent");
    for (int i = 0; i < vec.size(); ++i) {
        ImGui::PushID(i);
        std::string lbl = labeler(vec[i], i);
        if (ImGui::Selectable(lbl.c_str(), i == selIndex))
            selIndex = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
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
        return mgxc::Chain{MP_NOTETYPE_AIRSLIDE,
                           4,
                           0,
                           {Easing::Sine},
                           {mgxc::Joint{0, 0, 80, In, In}, mgxc::Joint{960, 12, 80, In, In}}};
    };

    const auto labeler = [](mgxc::Chain const &c, const int idx) {
        if (c.empty()) {
            return std::format("![{}] Empty", idx);
        }
        const auto type = GetNoteTypeString(c.type);
        const auto prefix = c.size() <= 1 ? "!" : "";
        return std::format("{}[{}] {}:{} {} @ {}", prefix, idx, type, c.size(), GetKindStr(c.es.m_kind), c.front().t);
    };

    const auto extra = [this] {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.0f, 0.0f, 1.0f));

        ImGui::SameLine();
        ImGui::BeginDisabled(!SelChain_InRange());
        if (ImGui::SmallButton("Commit")) {
            Commit(m_selChain);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_cctx.chains.empty());
        if (ImGui::SmallButton("Commit All") && !m_cctx.chains.empty()) {
            Commit();
        }
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
        ImGui::BeginDisabled(m_cctx.chains.empty());
        if (ImGui::SmallButton("Copy All")) {
            const auto text = mgxc::data::Serialize(m_cctx.chains);
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!SelChain_InRange());
        if (ImGui::SmallButton("Copy")) {
            const auto text = mgxc::data::Serialize(m_cctx.chains[m_selChain]);
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Paste")) {
            const auto text = ImGui::GetClipboardText();
            if (text && *text) {
                try {
                    const auto chains = mgxc::data::Parse(text);
                    if (!chains.empty()) {
                        m_cctx.chains.insert(m_cctx.chains.end(), chains.begin(), chains.end());
                    }
                } catch (const std::exception &e) {
                    m_errorText = e.what();
                    m_showErrorPopup = true;
                }
            }
        }
    };

    ImGui::TextUnformatted("Select Notes");
    ImGui::BeginChild("##NoteSelector", {m_childWidth, m_childHeight}, ImGuiChildFlags_Border);
    UI_Component_Editor_Vector(m_selChain, m_cctx.chains, creator, labeler, nullptr, extra);
    ImGui::EndChild();
}

void Dialog::UI_Panel_Selector_Controls() {
    const auto creator = [this] {
        if (!SelChain_InRange()) {
            throw std::runtime_error("No chain selected");
        }
        auto &chain = m_cctx.chains[m_selChain];

        const int idx = SelControl_InRange() ? m_selControl : static_cast<int>(chain.size()) - 1;
        mgxc::Joint joint{};
        if (utils::in_bounds(chain, idx)) {
            joint.t = chain[idx].t + m_cctx.snap;
            joint.x = chain[idx].x;
            joint.y = chain[idx].y;
            joint.eX = chain[idx].eX;
            joint.eY = chain[idx].eY;
        }

        while (std::ranges::any_of(chain, [&joint](const mgxc::Joint &n) { return n.t == joint.t; })) {
            joint.t += m_cctx.snap;
        }

        return joint;
    };

    const auto changed = [this] { SelChain_Sort(); };

    const auto extra = [this] {
        ImGui::SameLine();
        ImGui::BeginDisabled(!SelChain_InRange());
        if (ImGui::SmallButton("Paste")) {
            auto &selChain = m_cctx.chains[m_selChain];
            const auto text = ImGui::GetClipboardText();
            if (text && *text) {
                try {
                    for (auto const &chain: mgxc::data::Parse(text)) {
                        for (auto const &note: chain) {
                            if (std::ranges::find(selChain, note) == selChain.end()) {
                                selChain.push_back(note);
                            }
                        }
                    }
                    SelChain_Sort();
                } catch (const std::exception &e) {
                    m_errorText = e.what();
                    m_showErrorPopup = true;
                }
            }
        }
        ImGui::EndDisabled();
    };

    ImGui::Text("Select Controls @ Note [%d]", m_selChain);
    ImGui::BeginChild("##ControlSelector", {m_childWidth, m_childHeight}, ImGuiChildFlags_Border);

    if (SelChain_InRange()) {
        auto &chain = m_cctx.chains[m_selChain];
        const auto labeler = [chain](mgxc::Joint const &n, int idx) {
            char eX = idx == chain.size() - 1 ? '-' : GetModeChar(n.eX);
            char eY = idx == chain.size() - 1 ? '-' : GetModeChar(n.eY);
            return std::format("[{}] {}{} ({},{}) @ {}", idx, eX, eY, n.x, n.y, n.t);
        };
        UI_Component_Editor_Vector<2, false>(m_selControl, chain.joints, creator, labeler, changed, extra);
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

void Dialog::UI_Component_Combo_Note(mgxc::Chain &chain) {
    constexpr std::array kItems{
            std::pair{MP_NOTETYPE_SLIDE, GetNoteTypeString(MP_NOTETYPE_SLIDE)},
            std::pair{MP_NOTETYPE_AIRSLIDE, GetNoteTypeString(MP_NOTETYPE_AIRSLIDE)},
            std::pair{MP_NOTETYPE_AIRCRUSH, GetNoteTypeString(MP_NOTETYPE_AIRCRUSH)},
    };

    const auto it = std::ranges::find_if(kItems, [&chain](const auto &p) { return p.first == chain.type; });
    auto curr = std::distance(kItems.begin(), it);
    if (curr >= kItems.size()) {
        curr = 0;
        chain.type = kItems[curr].first;
    }

    if (ImGui::BeginCombo("Note Type", kItems[curr].second.data())) {
        for (std::size_t i = 0; i < kItems.size(); ++i) {
            const bool selected = i == curr;
            if (ImGui::Selectable(kItems[i].second.data(), selected)) {
                curr = static_cast<int>(i);
                chain.type = kItems[curr].first;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void Dialog::UI_Component_Combo_EasingMode(const std::string_view &label, EasingMode &mode) {
    using enum EasingMode;
    static constexpr std::array enumMap{Linear, In, Out};
    static const char *labels[] = {"Linear", "In", "Out"};

    int curr = utils::enum_to_idx(mode, enumMap);
    if (ImGui::Combo(label.data(), &curr, labels, IM_ARRAYSIZE(labels))) {
        mode = utils::idx_to_enum(curr, enumMap);
    }
}

void Dialog::UI_Component_Combo_Division() const {
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

    if (ImGui::BeginCombo("Division", previewStr.c_str())) {
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

    int snapInput = m_cctx.snap;
    if (ImGui::InputInt("Snap Tick", &snapInput)) {
        snapInput = std::clamp(snapInput, 1, mgxc::BAR_TICKS);
        m_cctx.snap = snapInput;
    }
}

void Dialog::UI_Component_Combo_EasingKind(mgxc::Chain &chain) {
    using enum EasingKind;
    static constexpr std::array enumMap{Sine, Power, Circular};
    static const char *label[] = {"Sine", "Power", "Circular"};

    auto &[m_kind, m_params] = chain.es;
    auto idx = utils::enum_to_idx(m_kind, enumMap);

    if (ImGui::Combo("Easing", &idx, label, IM_ARRAYSIZE(label))) {
        m_kind = utils::idx_to_enum(idx, enumMap);
        switch (m_kind) {
            case Sine:
                m_params = 0.0;
                break;
            case Power:
                m_params = 2.0;
                break;
            case Circular:
                m_params = 0.2;
                break;
        }
    }

    if (m_kind == Power) {
        auto curr = static_cast<int>(m_params);
        const int prev = curr;

        if (ImGui::InputInt("Exponent [!=0]", &curr)) {
            if (curr == 0) {
                if (prev > curr) {
                    m_params = -1;
                } else {
                    m_params = 1;
                }
            } else {
                m_params = curr;
            }
        }
    } else if (m_kind == Circular) {
        if (ImGui::InputDouble("Linearity [0,1]", &m_params, 0.01, 0.1, "%.3f")) {
            m_params = std::clamp(m_params, 0.0, 1.0);
        }
    }
}

void Dialog::UI_Panel_Config_Commit() {
    ImGui::TextUnformatted("Commit Config");
    ImGui::BeginChild("##Config", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::PushItemWidth(100.0f);

    if (ImGui::InputInt("x Offset [±15]", &m_cctx.xOffset)) {
        m_cctx.xOffset = std::clamp(m_cctx.xOffset, -15, 15);
    }

    if (ImGui::InputInt("y Offset [±360]", &m_cctx.yOffset)) {
        m_cctx.yOffset = std::clamp(m_cctx.yOffset, -360, 360);
    }

    ImGui::Checkbox("Clamp (x,y)", &m_cctx.clamp);

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

#pragma endregion UI

#pragma region Helper

template<class F, class... Args>
bool Dialog::Catch(F &&f, Args &&...args) {
    try {
        std::forward<F>(f)(std::forward<Args>(args)...);
        return true;
    } catch (const std::exception &e) {
        m_errorText = e.what();
        m_showErrorPopup = true;
        return false;
    }
}

void Dialog::Commit(const int idx) {
    Catch([this, idx] {
        m_cctx.tOffset = m_mg.GetTickOffset();
        auto intp = Interpolator(m_cctx);
        intp.Convert(idx);

        if (m_mg.CanCommit()) {
            intp.Commit(m_mg);
        }
    });
}

bool Dialog::IsRunning() const noexcept { return m_running; }

void Dialog::SelChain_Sort() {
    if (!SelControl_InRange()) {
        return;
    }
    auto &chain = m_cctx.chains[m_selChain];
    const auto selId = chain[m_selControl].GetID();
    std::ranges::stable_sort(chain, [](const mgxc::Joint &a, const mgxc::Joint &b) { return a.t < b.t; });
    for (std::size_t i = 0; i < chain.size(); ++i) {
        if (chain[i].GetID() == selId) {
            m_selControl = static_cast<int>(i);
            break;
        }
    }
}

constexpr bool Dialog::SelChain_InRange() const noexcept { return utils::in_bounds(m_cctx.chains, m_selChain); }

constexpr bool Dialog::SelControl_InRange() const noexcept {
    return SelChain_InRange() && utils::in_bounds(m_cctx.chains[m_selChain], m_selControl);
}


#pragma endregion Helper
