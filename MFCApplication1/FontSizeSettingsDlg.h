// FontSizeSettingsDlg.h: AI 助手字号设置对话框（双 tab + 实时预览）。
// 布局完全由 rc 静态决定：对话框本身不动态缩放，控件不在运行时移动。
// 切换 tab 只切换两个预览宿主控件的显示，并对可见的宿主懒创建 WebView2。
#pragma once

#include "WebView2Ctrl.h"
#include "resource.h"

class CFontSizeSettingsDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CFontSizeSettingsDlg)

public:
    CFontSizeSettingsDlg(CWnd* pParent = nullptr);
    virtual ~CFontSizeSettingsDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_FONT_SIZE_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    afx_msg void OnTcnSelchangeFsTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedFsDefault();

    DECLARE_MESSAGE_MAP()

private:
    enum { TabMain = 0, TabStand = 1 };
    enum { IdxBody = 0, IdxCard = 1, IdxResult = 2, IdxCode = 3 };

    static const int kSliderIds[4];   // body/card/result/code trackbar ids
    static const int kValueIds[4];    // body/card/result/code value-label ids

    int  m_fonts[2][4];   // [tab][body/card/result/code] in px
    int  m_orig[4];       // base px used by the sync-trackbar scale
    int  m_curTab = TabMain;
    int  m_syncPct = 100;
    bool m_prevCreated[2] = { false, false };   // per-tab preview WebView2 lazy-created

    CSliderCtrl   m_sliderSync;
    CSliderCtrl   m_slider[4];
    CWebView2Ctrl m_prev[2];   // preview WebView2, one per tab (created on demand)
    CWebView2Ctrl* m_activePreview = nullptr;

    void LoadConfig(int tab);            // read ini "body|card|result|code" into m_fonts[tab]
    void SaveConfig(int tab);            // write m_fonts[tab] back to ini
    void ApplyTab(int tab);              // switch tab: sync sliders/labels + toggle preview + ensure webview
    void MoveSliderToFont(int i);        // font slider i + label <- m_fonts[curTab][i]
    void UpdateSyncLabel();              // sync % label <- m_syncPct
    void UpdatePreview();                // push a fresh preview page to the active WebView2
    static int Clamp(int v) { return v < 8 ? 8 : (v > 32 ? 32 : v); }
    CString BuildPreviewHtml(int tab);   // sample AI page using the tab's font sizes
};