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

    Dialog(const Dialog &) = delete;
    Dialog &operator=(const Dialog &) = delete;
    explicit Dialog(Config &cctx, IMargretePluginContext *p_ctx);

private:
    // Margrete
    Margrete m_mg;
    Config &m_cctx;

    // Win32
    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &);
    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL &);
    LRESULT OnRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) const;
    LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
    void ShowImGuiLoop();

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
    void UI_Panel_Import();

    // State
    float m_childWidth{250.0f};
    float m_childHeight{165.0f};
    bool m_running{false};
    int m_selChain{-1};
    int m_selNote{-1};

    // UI
    void UI_Main();
    void UI_Panel_Editor_Note() const;
    void UI_Panel_Selector_Chain();
    void UI_Button_File();
    static void UI_Panel_Note_Edit(std::vector<mgxc::Note> &chain);
    static void UI_Combo_EasingMode(const std::string_view &label, EasingMode &esMode);
    void UI_Panel_Snap() const;
    void UI_Panel_Config() const;
    void UI_Combo_EasingType() const;
    void UI_Panel_Selector_Note();

    template<class Container, class Creator, class Labeler, class Extra = std::nullptr_t>
    void UI_Panel_Editor_Vector(const char *title, const char *childId, Container &vec, int &selIndex, Creator creator,
                                Labeler labeler, Extra extra = nullptr);

    void ConvertAction(int idx = -1);
};
