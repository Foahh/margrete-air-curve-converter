#pragma once

#include <atlbase.h>

#include <atlapp.h>
#include <atlctrls.h>
#include <atlwin.h>
#include <d3d11.h>

#include "mgxc/Interpolator.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class Dialog final : public CWindowImpl<Dialog> {
public:
    DECLARE_WND_CLASS_EX(L"ImGuiWindowClass", CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    BEGIN_MSG_MAP(Dialog)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
    MESSAGE_RANGE_HANDLER(0, 0xFFFF, OnRange)
    END_MSG_MAP()

    HRESULT ShowDialog();
    void UI_Error();

    Dialog(const Dialog &) = delete;
    Dialog &operator=(const Dialog &) = delete;
    explicit Dialog(Config &cctx, IMargretePluginContext *p_ctx, std::stop_token st);

    bool IsRunning() const noexcept;

private:
    void RenderImGui();

    // State
    bool m_running{false};
    std::stop_token m_st;

    float m_childWidth{235.0f};
    float m_childHeight{150.0f};

    int m_selChain{-1};
    int m_selControl{-1};

    MargreteHandle m_mg;
    Config &m_cctx;

    bool m_showErrorPopup = false;
    std::string m_errorText;

    // Win32
    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &);
    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &);
    LRESULT OnRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) const;
    LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

    // DirectX
    ID3D11Device *m_pd3dDevice = nullptr;
    ID3D11DeviceContext *m_pd3dContext = nullptr;
    IDXGISwapChain *m_pSwapChain = nullptr;
    ID3D11RenderTargetView *m_mainRenderTargetView = nullptr;
    void CreateDeviceD3D();
    void CleanupDeviceD3D();

    // Helper
    template<class F, class... Args>
    bool Catch(F &&f, Args &&...args);
    void Commit(int idx = -1);
    void SelChain_Sort();
    constexpr bool SelChain_InRange() const noexcept;
    constexpr bool SelControl_InRange() const noexcept;

    // UI
    void UI_Main();
    void UI_Main_Column_1();
    void UI_Main_Column_2();
    void UI_Main_Column_3();

    void UI_Panel_Config_Global();
    void UI_Panel_Config_Import();
    void UI_Panel_Config_Commit();

    void UI_Panel_Selector_Chains();
    void UI_Panel_Editor_Chain() const;

    void UI_Panel_Selector_Controls();
    void UI_Panel_Editor_Control();

    void UI_Component_Combo_Division() const;
    static void UI_Component_Combo_EasingKind(mgxc::Chain &chain);
    static void UI_Component_Combo_EasingMode(const std::string_view &label, EasingMode &mode);
    static void UI_Component_Combo_Note(mgxc::Chain &chain);

    void UI_Component_Button_File();

    template<std::size_t Min = 0, bool ShowClear = false, class Container, class Creator, class Labeler,
             class Changed = nullptr_t, class Extra = nullptr_t>
    void UI_Component_Editor_Vector(int &selIndex, Container &vec, Creator creator, Labeler labeler,
                                    Changed changed = nullptr, Extra extra = nullptr);
};
