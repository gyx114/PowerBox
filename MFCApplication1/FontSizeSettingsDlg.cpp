// FontSizeSettingsDlg.cpp : AI 助手字号设置对话框（双 tab + 实时预览）。
// 布局完全由 rc 静态决定：不在运行时移动控件或缩放对话框。切换 tab 只切换
// 两个预览宿主控件的显示，并对当前可见宿主懒创建 WebView2。
#include "pch.h"
#include "framework.h"
#include "FontSizeSettingsDlg.h"
#include "LocalizationManager.h"
#include "AIApiClient.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// body/card/result/code trackbar and value-label ids
const int CFontSizeSettingsDlg::kSliderIds[4] = {
    IDC_FS_SLIDER_BODY, IDC_FS_SLIDER_CARD, IDC_FS_SLIDER_RESULT, IDC_FS_SLIDER_CODE
};
const int CFontSizeSettingsDlg::kValueIds[4] = {
    IDC_FS_VALUE_BODY, IDC_FS_VALUE_CARD, IDC_FS_VALUE_RESULT, IDC_FS_VALUE_CODE
};

IMPLEMENT_DYNAMIC(CFontSizeSettingsDlg, CDialogEx)

CFontSizeSettingsDlg::CFontSizeSettingsDlg(CWnd* pParent)
    : CDialogEx(IDD_FONT_SIZE_DLG, pParent)
{
    const int mainDefault[4]  = { 12, 12, 12, 12 };   // 主窗口 AI
    const int standDefault[4] = { 14, 14, 14, 14 };   // 独立 AI 窗口
    for (int i = 0; i < 4; ++i)
    {
        m_fonts[TabMain][i]  = mainDefault[i];
        m_fonts[TabStand][i] = standDefault[i];
        m_orig[i] = mainDefault[i];
    }
}

CFontSizeSettingsDlg::~CFontSizeSettingsDlg() {}

void CFontSizeSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FS_SLIDER_SYNC, m_sliderSync);
    DDX_Control(pDX, IDC_FS_SLIDER_BODY,   m_slider[IdxBody]);
    DDX_Control(pDX, IDC_FS_SLIDER_CARD,   m_slider[IdxCard]);
    DDX_Control(pDX, IDC_FS_SLIDER_RESULT, m_slider[IdxResult]);
    DDX_Control(pDX, IDC_FS_SLIDER_CODE,   m_slider[IdxCode]);
}

BEGIN_MESSAGE_MAP(CFontSizeSettingsDlg, CDialogEx)
    ON_NOTIFY(TCN_SELCHANGE, IDC_FS_TAB, &CFontSizeSettingsDlg::OnTcnSelchangeFsTab)
    ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_FS_BTN_DEFAULT, &CFontSizeSettingsDlg::OnBnClickedFsDefault)
END_MESSAGE_MAP()

BOOL CFontSizeSettingsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    auto& loc = CLocalizationManager::GetInstance();

    // WS_CLIPCHILDREN stops the dialog background painting over the WebView2 host,
    // preventing ghosting/ghost trails.
    ModifyStyle(0, WS_CLIPCHILDREN);

    SetWindowText(loc.GetString(_T("FontSizeDlg"), _T("Caption"), _T("AI 助手字号设置")));

    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_FS_TAB));
    if (pTab)
    {
        pTab->InsertItem(0, loc.GetString(_T("FontSizeDlg"), _T("TabMain"), _T("主窗口 AI")));
        pTab->InsertItem(1, loc.GetString(_T("FontSizeDlg"), _T("TabStand"), _T("独立 AI 窗口")));
        pTab->SetCurSel(m_curTab);
    }

    // Static labels / buttons (positions come from the RC template, untouched).
    SetDlgItemText(IDC_FS_LABEL_SYNC,   loc.GetString(_T("FontSizeDlg"), _T("GrpSync"), _T("同步缩放")));
    SetDlgItemText(IDC_FS_LABEL_BODY,   loc.GetString(_T("FontSizeDlg"), _T("GrpBody"), _T("正文")));
    SetDlgItemText(IDC_FS_LABEL_CARD,   loc.GetString(_T("FontSizeDlg"), _T("GrpCard"), _T("命令卡片")));
    SetDlgItemText(IDC_FS_LABEL_RESULT, loc.GetString(_T("FontSizeDlg"), _T("GrpResult"), _T("执行结果")));
    SetDlgItemText(IDC_FS_LABEL_CODE,   loc.GetString(_T("FontSizeDlg"), _T("GrpCode"), _T("代码块")));
    SetDlgItemText(IDC_FS_BTN_DEFAULT,  loc.GetString(_T("FontSizeDlg"), _T("BtnDefault"), _T("恢复默认")));
    SetDlgItemText(IDOK,      loc.GetString(_T("FontSizeDlg"), _T("OK"), _T("确定")));
    SetDlgItemText(IDCANCEL,  loc.GetString(_T("FontSizeDlg"), _T("Cancel"), _T("取消")));

    // Trackbar ranges: fonts 8..32 px, sync scale 60..150 %
    m_sliderSync.SetRange(60, 150, TRUE);
    for (int i = 0; i < 4; ++i)
        m_slider[i].SetRange(8, 32, TRUE);

    LoadConfig(TabMain);
    LoadConfig(TabStand);

    ApplyTab(m_curTab);
    return TRUE;
}

void CFontSizeSettingsDlg::OnOK()
{
    SaveConfig(TabMain);
    SaveConfig(TabStand);
    // Tell the main-window AI and any open standalone AI window to re-render.
    ::PostMessage(HWND_BROADCAST, WM_AI_FONT_CHANGED, 0, 0);
    // Also refresh the main window synchronously so it applies immediately
    // (no wait for a restart / for the broadcast to be pumped).
    if (CWnd* pMain = AfxGetMainWnd())
        pMain->SendMessage(WM_AI_FONT_CHANGED, 0, 0);
    CDialogEx::OnOK();
}

void CFontSizeSettingsDlg::LoadConfig(int tab)
{
    // Config stores 4 absolute px values as "body|card|result|code".
    static const TCHAR* kKey[2] = { _T("FontSizeMain"), _T("FontSizeStand") };
    CString cfg = AfxGetApp()->GetProfileString(_T("AI"), kKey[tab], _T(""));
    int v[4];
    for (int i = 0; i < 4; ++i) v[i] = m_fonts[tab][i];
    if (!cfg.IsEmpty())
    {
        int start = 0, i = 0;
        while (i < 4)
        {
            int bar = cfg.Find(_T('|'), start);
            if (bar < 0) break;
            v[i++] = _ttoi(cfg.Mid(start, bar - start));
            start = bar + 1;
        }
        if (i < 4 && start < cfg.GetLength())
            v[i] = _ttoi(cfg.Mid(start));
    }
    for (int i = 0; i < 4; ++i)
        m_fonts[tab][i] = Clamp(v[i]);
}

void CFontSizeSettingsDlg::SaveConfig(int tab)
{
    static const TCHAR* kKey[2] = { _T("FontSizeMain"), _T("FontSizeStand") };
    CString val;
    val.Format(_T("%d|%d|%d|%d"), m_fonts[tab][IdxBody], m_fonts[tab][IdxCard],
        m_fonts[tab][IdxResult], m_fonts[tab][IdxCode]);
    AfxGetApp()->WriteProfileString(_T("AI"), kKey[tab], val);
}

void CFontSizeSettingsDlg::ApplyTab(int tab)
{
    m_curTab = tab;

    // Sync the slider values to this tab's stored sizes (positions of sliders
    // are static in rc; only their knob/values change).
    m_syncPct = 100;
    m_sliderSync.SetPos(100);
    for (int i = 0; i < 4; ++i)
    {
        m_orig[i] = m_fonts[tab][i];
        m_slider[i].SetPos(m_fonts[tab][i]);
        MoveSliderToFont(i);
    }
    UpdateSyncLabel();

    // Toggle which preview host is visible (no control is ever moved).
    static const int hostIds[2] = { IDC_FS_PREV_MAIN, IDC_FS_PREV_STAND };
    CWnd* cur = GetDlgItem(hostIds[tab]);
    CWnd* hid = GetDlgItem(hostIds[1 - tab]);
    if (hid) hid->ShowWindow(SW_HIDE);
    if (cur) cur->ShowWindow(SW_SHOW);

    m_activePreview = &m_prev[tab];
    if (!m_prevCreated[tab])
    {
        m_prevCreated[tab] = true;
        if (cur)
        {
            cur->ModifyStyle(0, WS_CLIPCHILDREN);
            m_prev[tab].OnReady = [this, tab]() {
                // Size to the visible host's client rect (its static rc position).
                CWnd* h = GetDlgItem(tab == TabMain ? IDC_FS_PREV_MAIN : IDC_FS_PREV_STAND);
                if (h)
                {
                    CRect r;
                    h->GetClientRect(&r);
                    m_prev[tab].Resize(0, 0, r.Width(), r.Height());
                }
                if (m_curTab == tab) UpdatePreview();
            };
            m_prev[tab].Create(cur->GetSafeHwnd());
        }
    }
    UpdatePreview();
}

void CFontSizeSettingsDlg::MoveSliderToFont(int i)
{
    m_slider[i].SetPos(m_fonts[m_curTab][i]);
    CString s;
    s.Format(_T("%dpx"), m_fonts[m_curTab][i]);
    SetDlgItemText(kValueIds[i], s);
}

void CFontSizeSettingsDlg::UpdateSyncLabel()
{
    CString s;
    s.Format(_T("%d%%"), m_syncPct);
    SetDlgItemText(IDC_FS_VALUE_SYNC, s);
}

void CFontSizeSettingsDlg::UpdatePreview()
{
    if (!m_activePreview || !m_activePreview->IsReady()) return;
    CString html = BuildPreviewHtml(m_curTab);
    m_activePreview->NavigateToString(std::wstring((LPCWSTR)html));
}

void CFontSizeSettingsDlg::OnTcnSelchangeFsTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    int sel = -1;
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_FS_TAB));
    if (pTab) sel = pTab->GetCurSel();
    if (sel == TabMain || sel == TabStand)
        ApplyTab(sel);
    *pResult = 0;
}

void CFontSizeSettingsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    HWND h = pScrollBar ? pScrollBar->GetSafeHwnd() : nullptr;
    if (h)
    {
        int id = (int)::GetDlgCtrlID(h);
        if (id == IDC_FS_SLIDER_SYNC)
        {
            m_syncPct = m_sliderSync.GetPos();
            for (int i = 0; i < 4; ++i)
            {
                m_fonts[m_curTab][i] = Clamp(
                    (int)((m_orig[i] * (__int64)m_syncPct) / 100));
                MoveSliderToFont(i);
            }
            UpdateSyncLabel();
            UpdatePreview();
        }
        else
        {
            for (int i = 0; i < 4; ++i)
            {
                if (id == kSliderIds[i])
                {
                    m_fonts[m_curTab][i] = Clamp(m_slider[i].GetPos());
                    m_orig[i] = m_fonts[m_curTab][i];
                    MoveSliderToFont(i);
                    UpdatePreview();
                    break;
                }
            }
        }
    }
    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CFontSizeSettingsDlg::OnBnClickedFsDefault()
{
    // Reset current tab's four sizes to its built-in default.
    const int def[2][4] = {
        { 12, 12, 12, 12 },   // TabMain
        { 14, 14, 14, 14 }    // TabStand
    };
    for (int i = 0; i < 4; ++i)
    {
        m_fonts[m_curTab][i] = def[m_curTab][i];
        m_orig[i] = m_fonts[m_curTab][i];
        MoveSliderToFont(i);
    }
    m_syncPct = 100;
    m_sliderSync.SetPos(100);
    UpdateSyncLabel();
    UpdatePreview();
}

CString CFontSizeSettingsDlg::BuildPreviewHtml(int tab)
{
    const int body = m_fonts[tab][IdxBody];
    const int card = m_fonts[tab][IdxCard];
    const int result = m_fonts[tab][IdxResult];
    const int code = m_fonts[tab][IdxCode];

    CString html;
    html.Format(
        _T("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>")
        _T("body{margin:0;padding:8px;background:#1e1e1e;color:#d4d4d4;")
        _T("font-family:Consolas,'Microsoft YaHei',sans-serif;font-size:%dpx;line-height:1.5;}")
        _T("code{background:#2d2d2d;padding:1px 3px;border-radius:3px;font-size:%dpx;}")
        _T("pre{background:#2d2d2d;padding:6px;border-radius:4px;font-size:%dpx;}")
        _T(".card{border:2px solid #2da44e;border-radius:8px;padding:8px;margin:8px 0;")
        _T("background:#2a2a2a;font-size:%dpx;}")
        _T(".result{border-left:3px solid #569cd6;padding-left:8px;color:#569cd6;font-size:%dpx;}")
        _T("</style></head><body>")
        _T("<p>正文 sample text, 含 <code>inline code</code>。</p>")
        _T("<div class=\"card\"><b>命令卡片</b> · 用途 purpose / 风险 risk</div>")
        _T("<div class=\"result\"><b>执行结果</b> · 命令返回 output 示例</div>")
        _T("<pre><code>printf(\"hello powerbox\");</code></pre>")
        _T("</body></html>"),
        body, card, code, card, result);
    return html;
}