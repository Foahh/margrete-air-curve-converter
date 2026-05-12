#include <algorithm>
#include <array>
#include <atlbase.h>
#include <atlstr.h>
#include <cfloat>
#include <cmath>
#include <commdlg.h>
#include <format>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Dialog.h"

#include "meta.h"
#include "mgxc/Easing.h"

namespace {
constexpr std::array kSnapDivisors = {1920, 960, 480, 384, 192, 128, 96, 64, 48, 32, 24, 16, 12, 8, 4};

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

std::optional<std::string> SelectAffFile(HWND owner) {
    OPENFILENAMEW ofn = {};
    WCHAR pathW[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = pathW;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"ArcCreate Chart (*.aff)\0*.aff\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }

    const CW2AEX<MAX_PATH * 4> utf8Path(pathW, CP_UTF8);
    return std::string(utf8Path);
}
} // namespace

#pragma region UI

void Dialog::UI_Error() {
    const ImGuiViewport *vp = ImGui::GetMainViewport();
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
        mgxc::Chain &chain = m_cctx.chains[m_selChain];
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
        mgxc::Chain &chain = m_cctx.chains[m_selChain];
        mgxc::Joint &note = chain[m_selControl];

        const MpInteger previousTick = note.t;
        if (ImGui::InputInt("t", &note.t)) {
            note.t = (std::max)(note.t, 0);
            if (note.t != previousTick) {
                SelChain_Sort();
            }
        }

        ImGui::SameLine();
        const double snappedTick = std::round(static_cast<double>(note.t) / m_cctx.snap) * m_cctx.snap;
        ImGui::Text("-> %d", static_cast<int>(snappedTick));

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
void Dialog::UI_Component_Editor_Vector(int &selIndex, Container &vec, const Creator &creator, const Labeler &labeler,
                                        const Changed &changed, const Extra &extra) {
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
        } else if (selIndex >= static_cast<int>(vec.size())) {
            selIndex = static_cast<int>(vec.size()) - 1;
        }

        if constexpr (!std::is_same_v<Changed, std::nullptr_t>) {
            changed();
        }
    }
    ImGui::EndDisabled();

    if constexpr (ShowClear) {
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
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        ImGui::PushID(i);
        const std::string label = labeler(vec[i], i);
        if (ImGui::Selectable(label.c_str(), i == selIndex)) {
            selIndex = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
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

    const auto labeler = [](const mgxc::Chain &chain, const int idx) {
        if (chain.empty()) {
            return std::format("![{}] Empty", idx);
        }

        const std::string_view type = GetNoteTypeString(chain.type);
        const char *prefix = chain.size() <= 1 ? "!" : "";
        return std::format("{}[{}] {}:{} {} @ {}", prefix, idx, type, chain.size(), GetKindStr(chain.es.m_kind),
                           chain.front().t);
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
            const std::string text = mgxc::data::Serialize(m_cctx.chains);
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!SelChain_InRange());
        if (ImGui::SmallButton("Copy")) {
            const std::string text = mgxc::data::Serialize(m_cctx.chains[m_selChain]);
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Paste")) {
            const char *text = ImGui::GetClipboardText();
            if (text && *text) {
                try {
                    auto chains = mgxc::data::Parse(text);
                    if (!chains.empty()) {
                        m_cctx.chains.insert(m_cctx.chains.end(), chains.begin(), chains.end());
                    }
                } catch (const std::exception &e) {
                    ShowError(e.what());
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
        mgxc::Chain &chain = m_cctx.chains[m_selChain];
        const int index = SelControl_InRange() ? m_selControl : static_cast<int>(chain.size()) - 1;

        mgxc::Joint joint{};
        if (utils::in_bounds(chain, index)) {
            joint.t = chain[index].t + m_cctx.snap;
            joint.x = chain[index].x;
            joint.y = chain[index].y;
            joint.eX = chain[index].eX;
            joint.eY = chain[index].eY;
        }

        while (std::ranges::any_of(chain, [&joint](const mgxc::Joint &existing) { return existing.t == joint.t; })) {
            joint.t += m_cctx.snap;
        }

        return joint;
    };

    const auto changed = [this] { SelChain_Sort(); };

    const auto extra = [this] {
        ImGui::SameLine();
        ImGui::BeginDisabled(!SelChain_InRange());
        if (ImGui::SmallButton("Paste")) {
            mgxc::Chain &selectedChain = m_cctx.chains[m_selChain];
            const char *text = ImGui::GetClipboardText();
            if (text && *text) {
                try {
                    for (const mgxc::Chain &chain : mgxc::data::Parse(text)) {
                        for (const mgxc::Joint &note : chain) {
                            if (std::ranges::find(selectedChain, note) == selectedChain.end()) {
                                selectedChain.push_back(note);
                            }
                        }
                    }
                    SelChain_Sort();
                } catch (const std::exception &e) {
                    ShowError(e.what());
                }
            }
        }
        ImGui::EndDisabled();
    };

    ImGui::Text("Select Controls @ Note [%d]", m_selChain);
    ImGui::BeginChild("##ControlSelector", {m_childWidth, m_childHeight}, ImGuiChildFlags_Border);

    if (SelChain_InRange()) {
        mgxc::Chain &chain = m_cctx.chains[m_selChain];
        const auto labeler = [&chain](const mgxc::Joint &note, const int idx) {
            const char eX = idx == static_cast<int>(chain.size()) - 1 ? '-' : GetModeChar(note.eX);
            const char eY = idx == static_cast<int>(chain.size()) - 1 ? '-' : GetModeChar(note.eY);
            return std::format("[{}] {}{} ({},{}) @ {}", idx, eX, eY, note.x, note.y, note.t);
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

    if (const auto path = SelectAffFile(m_hWnd)) {
        TryImportAffFile(*path);
    }
}

void Dialog::UI_Component_Combo_Note(mgxc::Chain &chain) {
    constexpr std::array kItems{
        std::pair{MP_NOTETYPE_SLIDE, GetNoteTypeString(MP_NOTETYPE_SLIDE)},
        std::pair{MP_NOTETYPE_AIRSLIDE, GetNoteTypeString(MP_NOTETYPE_AIRSLIDE)},
        std::pair{MP_NOTETYPE_AIRCRUSH, GetNoteTypeString(MP_NOTETYPE_AIRCRUSH)},
    };

    const auto it = std::ranges::find_if(kItems, [&chain](const auto &item) { return item.first == chain.type; });
    ptrdiff_t curr = std::distance(kItems.begin(), it);
    if (curr >= static_cast<ptrdiff_t>(kItems.size())) {
        curr = 0;
        chain.type = kItems[curr].first;
    }

    if (ImGui::BeginCombo("Note Type", kItems[curr].second.data())) {
        for (std::size_t i = 0; i < kItems.size(); ++i) {
            const bool selected = static_cast<ptrdiff_t>(i) == curr;
            if (ImGui::Selectable(kItems[i].second.data(), selected)) {
                curr = static_cast<ptrdiff_t>(i);
                chain.type = kItems[i].first;
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
    static constexpr const char *labels[] = {"Linear", "In", "Out"};

    int curr = utils::enum_to_idx(mode, enumMap);
    if (ImGui::Combo(label.data(), &curr, labels, IM_ARRAYSIZE(labels))) {
        mode = utils::idx_to_enum(curr, enumMap);
    }
}

void Dialog::UI_Component_Combo_Division() {
    int selectedDivisorIndex = -1;
    if (m_cctx.snap > 0 && mgxc::BAR_TICKS % m_cctx.snap == 0) {
        const int divisor = mgxc::BAR_TICKS / m_cctx.snap;
        const auto it = std::ranges::find(kSnapDivisors, divisor);
        if (it != kSnapDivisors.end()) {
            selectedDivisorIndex = static_cast<int>(std::distance(kSnapDivisors.begin(), it));
        }
    }

    std::string preview;
    if (selectedDivisorIndex >= 0) {
        preview = std::to_string(kSnapDivisors[selectedDivisorIndex]);
    }

    if (ImGui::BeginCombo("Division", preview.c_str())) {
        for (int i = 0; i < static_cast<int>(kSnapDivisors.size()); ++i) {
            const bool selected = selectedDivisorIndex == i;
            const std::string label = std::to_string(kSnapDivisors[i]);
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_cctx.snap = mgxc::BAR_TICKS / kSnapDivisors[i];
            }

            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    int snapInput = m_cctx.snap;
    if (ImGui::InputInt("Snap Tick", &snapInput)) {
        m_cctx.snap = std::clamp(snapInput, 1, mgxc::BAR_TICKS);
    }
}

void Dialog::UI_Component_Combo_EasingKind(mgxc::Chain &chain) {
    using enum EasingKind;
    static constexpr std::array enumMap{Sine, Power, Circular};
    static constexpr const char *labels[] = {"Sine", "Power", "Circular"};

    EasingKind kind = chain.es.m_kind;
    double param = chain.es.m_param;
    int idx = utils::enum_to_idx(kind, enumMap);

    if (ImGui::Combo("Easing", &idx, labels, IM_ARRAYSIZE(labels))) {
        kind = utils::idx_to_enum(idx, enumMap);
        switch (kind) {
            case Sine:
                param = 0.0;
                break;
            case Power:
                param = 2.0;
                break;
            case Circular:
                param = 0.2;
                break;
        }
    }

    if (kind == Power) {
        int exponent = static_cast<int>(param);
        const int previousExponent = exponent;

        if (ImGui::InputInt("Exponent [!=0]", &exponent)) {
            if (exponent == 0) {
                param = previousExponent > exponent ? -1.0 : 1.0;
            } else {
                param = exponent;
            }
        }
    } else if (kind == Circular) {
        if (ImGui::InputDouble("Linearity [0,1]", &param, 0.01, 0.1, "%.3f")) {
            param = std::clamp(param, 0.0, 1.0);
        }
    }

    chain.es = {kind, param};
}

void Dialog::UI_Panel_Config_Commit() {
    ImGui::TextUnformatted("Commit Config");
    ImGui::BeginChild("##Config", {m_childWidth, 0}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

    ImGui::PushItemWidth(100.0f);

    if (ImGui::InputInt("x Offset [-15,15]", &m_cctx.xOffset)) {
        m_cctx.xOffset = std::clamp(m_cctx.xOffset, -15, 15);
    }

    if (ImGui::InputInt("y Offset [-360,360]", &m_cctx.yOffset)) {
        m_cctx.yOffset = std::clamp(m_cctx.yOffset, -360, 360);
    }

    ImGui::Checkbox("Clamp (x,y)", &m_cctx.clamp);

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

#pragma endregion UI
