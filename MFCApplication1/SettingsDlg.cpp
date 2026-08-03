#include "pch.h"
#include "framework.h"
#include "SettingsDlg.h"
#include "LocalizationManager.h"
#include "AIApiClient.h"
#include "resource.h"
#include "MFCApplication1Dlg.h"
#include <Shellapi.h>

CSettingsDlg::CSettingsDlg(CWnd* pParent /*= nullptr*/) : CDialogEx(IDD_SETTINGS_DIALOG, pParent) {}

void CSettingsDlg::OnBrowseScreenshot() { BrowseFolder(IDC_EDIT_SCREENSHOT_DIR, CLocalizationManager::GetInstance().GetString(_T("Settings"), _T("DlgTitleScreenshot"))); }
void CSettingsDlg::OnBrowseStickyDir() { BrowseFolder(IDC_EDIT_STICKY_DIR, CLocalizationManager::GetInstance().GetString(_T("Settings"), _T("DlgTitleSticky"))); }

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BROWSE_SCREENSHOT, &CSettingsDlg::OnBrowseScreenshot)
    ON_BN_CLICKED(IDC_BROWSE_STICKY_DIR, &CSettingsDlg::OnBrowseStickyDir)
    ON_BN_CLICKED(IDC_BUTTON_AI_KEY_SHOW, &CSettingsDlg::OnBnClickedAiKeyShow)
    ON_CBN_SELCHANGE(IDC_COMBO_AI_VENDOR_CFG, &CSettingsDlg::OnCbnSelchangeAiVendor)
    ON_CBN_SELCHANGE(IDC_COMBO_LANGUAGE, &CSettingsDlg::OnCbnSelchangeLanguage)
    ON_BN_CLICKED(IDC_BTN_HOTKEY_SHOWHIDE, &CSettingsDlg::OnBnClickedHotkeyShowHide)
    ON_BN_CLICKED(IDC_BTN_HOTKEY_LOCATE, &CSettingsDlg::OnBnClickedHotkeyLocate)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

// Helper: find a child window by its current text and set new text
static void SetChildTextByCurrentText(CWnd* pParent, LPCTSTR oldText, LPCTSTR newText)
{
    if (!pParent || !oldText || !newText) return;
    for (CWnd* pChild = pParent->GetWindow(GW_CHILD); pChild; pChild = pChild->GetNextWindow())
    {
        CString text;
        pChild->GetWindowText(text);
        if (text == oldText)
        {
            pChild->SetWindowText(newText);
            return;
        }
    }
}

BOOL CSettingsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Translate UI
    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(loc.GetString(_T("DlgCaption"), _T("SettingsDlg")));

    // Group boxes (all use IDC_STATIC, so find by current text)
    SetChildTextByCurrentText(this, _T("文件命名"), loc.GetString(_T("Settings"), _T("GroupFileNaming")));
    SetChildTextByCurrentText(this, _T("路径设置"), loc.GetString(_T("Settings"), _T("GroupPathSettings")));
    SetChildTextByCurrentText(this, _T("连点器"), loc.GetString(_T("Settings"), _T("GroupAutoClicker")));
    SetChildTextByCurrentText(this, _T("网址"), loc.GetString(_T("Settings"), _T("GroupSites")));
    SetChildTextByCurrentText(this, _T("AI助手"), loc.GetString(_T("Settings"), _T("GroupAI")));
    SetChildTextByCurrentText(this, _T("语言"), loc.GetString(_T("Settings"), _T("GroupLanguage")));

    // Static labels (find by current text)
    SetChildTextByCurrentText(this, _T("默认文件名:"), loc.GetString(_T("Settings"), _T("LabelDefaultName")));
    SetChildTextByCurrentText(this, _T("截图保存目录:"), loc.GetString(_T("Settings"), _T("LabelScreenshot")));
    SetChildTextByCurrentText(this, _T("便签保存目录:"), loc.GetString(_T("Settings"), _T("LabelStickyDir")));
    SetChildTextByCurrentText(this, _T("连点器间隔(ms):"), loc.GetString(_T("Settings"), _T("LabelClickInterval")));
    SetChildTextByCurrentText(this, _T("开始键:"), loc.GetString(_T("Settings"), _T("LabelStartKey")));
    SetChildTextByCurrentText(this, _T("停止键:"), loc.GetString(_T("Settings"), _T("LabelStopKey")));
    SetChildTextByCurrentText(this, _T("切换语言后应用将自动重启。"), loc.GetString(_T("Settings"), _T("LanguageRestartHint")));

    // Browse buttons (unique IDs)
    SetDlgItemText(IDC_BROWSE_SCREENSHOT, loc.GetString(_T("Settings"), _T("BtnBrowse")));
    SetDlgItemText(IDC_BROWSE_STICKY_DIR, loc.GetString(_T("Settings"), _T("BtnBrowse")));

    // AI labels (unique IDs)
    SetDlgItemText(IDC_STATIC_AI_VENDOR_CFG, loc.GetString(_T("Settings"), _T("LabelVendor")));
    SetDlgItemText(IDC_STATIC_AI_KEY_CFG, loc.GetString(_T("Settings"), _T("LabelApiKey")));
    SetDlgItemText(IDC_BUTTON_AI_KEY_SHOW, loc.GetString(_T("Settings"), _T("BtnShow")));

    // Language label (unique ID)
    SetDlgItemText(IDC_STATIC_LANGUAGE, loc.GetString(_T("Settings"), _T("LabelLanguage")));

    // OK/Cancel buttons
    SetDlgItemText(IDOK, loc.GetString(_T("Settings"), _T("BtnOK")));
    SetDlgItemText(IDCANCEL, loc.GetString(_T("Settings"), _T("BtnCancel")));

    SetDlgItemText(IDC_EDIT_DEFAULT_NAME, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));
    SetDlgItemText(IDC_EDIT_MOOC_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("MoocUrl"), _T("")));
    SetDlgItemText(IDC_EDIT_SDUCS_URL, AfxGetApp()->GetProfileString(_T("Sites"), _T("Sducs"), _T("")));

    // Screenshot save directory, default desktop
    CString strDefaultScreenshot;
    TCHAR szDesktop[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, szDesktop)))
        strDefaultScreenshot = szDesktop;
    SetDlgItemText(IDC_EDIT_SCREENSHOT_DIR,
        AfxGetApp()->GetProfileString(_T("Paths"), _T("ScreenshotDir"), strDefaultScreenshot));

    // Sticky note save directory
    SetDlgItemText(IDC_EDIT_STICKY_DIR,
        AfxGetApp()->GetProfileString(_T("StickyNote"), _T("SaveFolder"), _T("")));

    // Auto-clicker interval, default 100ms
    SetDlgItemInt(IDC_EDIT_CLICK_INTERVAL,
        AfxGetApp()->GetProfileInt(_T("AutoClicker"), _T("IntervalMs"), 100));

    // Auto-clicker trigger keys, default A and B, auto-create if config missing
    TCHAR szKey[2] = {};
    CString strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStart"), _T(""));
    if (strKey.IsEmpty())
    {
        strKey = _T("A");
        AfxGetApp()->WriteProfileString(_T("AutoClicker"), _T("KeyStart"), strKey);
    }
    SetDlgItemText(IDC_EDIT_CLICK_KEY_START, strKey);

    strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStop"), _T(""));
    if (strKey.IsEmpty())
    {
        strKey = _T("B");
        AfxGetApp()->WriteProfileString(_T("AutoClicker"), _T("KeyStop"), strKey);
    }
    SetDlgItemText(IDC_EDIT_CLICK_KEY_STOP, strKey);

    // AI config
    InitAIVendorCombo();
    m_strCurrentVendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    LoadVendorKey(m_strCurrentVendor);

    // Language selection
    m_strCurrentLang = AfxGetApp()->GetProfileString(_T("Settings"), _T("Language"), _T("zh-CN"));
    CComboBox* pLangCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_LANGUAGE));
    if (pLangCombo)
    {
        auto langs = CLocalizationManager::GetInstance().GetAvailableLanguages();
        for (const auto& [id, name] : langs)
        {
            int idx = pLangCombo->AddString(name);
            pLangCombo->SetItemData(idx, reinterpret_cast<DWORD_PTR>(_tcsdup(id)));
        }
        // Select current language
        int count = pLangCombo->GetCount();
        for (int i = 0; i < count; i++)
        {
            CString langId = reinterpret_cast<LPCTSTR>(pLangCombo->GetItemData(i));
            if (langId == m_strCurrentLang)
            {
                pLangCombo->SetCurSel(i);
                break;
            }
        }
    }

    // Hotkey group translation
    SetChildTextByCurrentText(this, _T("快捷键"), loc.GetString(_T("Settings"), _T("GroupHotkey")));
    SetChildTextByCurrentText(this, _T("显示/隐藏主窗口:"), loc.GetString(_T("Settings"), _T("LabelHotkeyShowHide")));
    SetChildTextByCurrentText(this, _T("窗口定位:"), loc.GetString(_T("Settings"), _T("LabelHotkeyLocate")));
    SetDlgItemText(IDC_BTN_HOTKEY_SHOWHIDE, loc.GetString(_T("Settings"), _T("BtnHotkeyCapture")));
    SetDlgItemText(IDC_BTN_HOTKEY_LOCATE, loc.GetString(_T("Settings"), _T("BtnHotkeyCapture")));

    // Load hotkey configs
    m_hotkeyShowHide = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("ShowHide"), _T("3|32"))); // Ctrl+Alt+Space
    m_hotkeyLocate = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("Locate"), _T("3|68")));   // Ctrl+Alt+D

    SetDlgItemText(IDC_EDIT_HOTKEY_SHOWHIDE, m_hotkeyShowHide.ToDisplay());
    SetDlgItemText(IDC_EDIT_HOTKEY_LOCATE, m_hotkeyLocate.ToDisplay());

    return TRUE;
}

BOOL CSettingsDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = CWnd::FromHandle(::GetFocus());
        if (pFocus)
        {
            TCHAR className[64] = {0};
            ::GetClassName(pFocus->GetSafeHwnd(), className, 64);
            if (_tcsstr(className, _T("Edit")) || _tcsstr(className, _T("edit")))
            {
                return TRUE;
            }
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CSettingsDlg::OnOK()
{
    CString v;
    GetDlgItemText(IDC_EDIT_DEFAULT_NAME, v); AfxGetApp()->WriteProfileString(_T("Template"), _T("DefaultReportName"), v);
    GetDlgItemText(IDC_EDIT_SCREENSHOT_DIR, v); AfxGetApp()->WriteProfileString(_T("Paths"), _T("ScreenshotDir"), v);
    GetDlgItemText(IDC_EDIT_STICKY_DIR, v); AfxGetApp()->WriteProfileString(_T("StickyNote"), _T("SaveFolder"), v);
    GetDlgItemText(IDC_EDIT_MOOC_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("MoocUrl"), v);
    GetDlgItemText(IDC_EDIT_SDUCS_URL, v); AfxGetApp()->WriteProfileString(_T("Sites"), _T("Sducs"), v);
    AfxGetApp()->WriteProfileInt(_T("AutoClicker"), _T("IntervalMs"), GetDlgItemInt(IDC_EDIT_CLICK_INTERVAL));

    // Auto-clicker trigger key validation: empty defaults to A/B, cannot be same
    CString strStart, strStop;
    GetDlgItemText(IDC_EDIT_CLICK_KEY_START, strStart);
    GetDlgItemText(IDC_EDIT_CLICK_KEY_STOP, strStop);
    if (strStart.IsEmpty()) strStart = _T("A");
    if (strStop.IsEmpty()) strStop = _T("B");
    if (strStart == strStop)
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Settings"), _T("MsgKeySame")), loc.GetString(_T("Settings"), _T("GroupAutoClicker")), MB_OK | MB_ICONWARNING);
        return;
    }
    AfxGetApp()->WriteProfileString(_T("AutoClicker"), _T("KeyStart"), strStart);
    AfxGetApp()->WriteProfileString(_T("AutoClicker"), _T("KeyStop"), strStop);

    // Save AI config
    SaveVendorKey(m_strCurrentVendor);
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_AI_VENDOR_CFG));
    if (pCombo)
    {
        CString aiVendor;
        pCombo->GetWindowText(aiVendor);
        AfxGetApp()->WriteProfileString(_T("AI"), _T("Vendor"), aiVendor);
    }

    // Save language
    CString strOldLang = AfxGetApp()->GetProfileString(_T("Settings"), _T("Language"), _T("zh-CN"));
    CComboBox* pLangCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_LANGUAGE));
    if (pLangCombo)
    {
        int sel = pLangCombo->GetCurSel();
        if (sel != CB_ERR)
            m_strCurrentLang = reinterpret_cast<LPCTSTR>(pLangCombo->GetItemData(sel));
        AfxGetApp()->WriteProfileString(_T("Settings"), _T("Language"), m_strCurrentLang);
    }

    // Check for duplicate hotkeys (non-empty only)
    if (!m_hotkeyShowHide.IsEmpty() && !m_hotkeyLocate.IsEmpty() && m_hotkeyShowHide == m_hotkeyLocate)
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Settings"), _T("MsgHotkeyDuplicate")),
            loc.GetString(_T("Msg"), _T("Warning")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Save hotkey configs
    AfxGetApp()->WriteProfileString(_T("Hotkeys"), _T("ShowHide"), m_hotkeyShowHide.ToConfigString());
    AfxGetApp()->WriteProfileString(_T("Hotkeys"), _T("Locate"), m_hotkeyLocate.ToConfigString());

    bool bRestart = (strOldLang != m_strCurrentLang);

    DestroyWindow();

    if (bRestart)
    {
        TCHAR szExePath[MAX_PATH] = {0};
        GetModuleFileName(nullptr, szExePath, MAX_PATH);
        ShellExecute(nullptr, _T("open"), szExePath, nullptr, nullptr, SW_SHOWNORMAL);
        // Set exiting flag before sending close, so main window won't be minimized to tray
        CMFCApplication1Dlg* pMain = static_cast<CMFCApplication1Dlg*>(AfxGetMainWnd());
        if (pMain)
        {
            pMain->m_bExiting = true;
            pMain->PostMessage(WM_CLOSE);
        }
    }
}

void CSettingsDlg::OnCancel()
{
    PostMessage(WM_CLOSE);
}

void CSettingsDlg::OnClose()
{
    DestroyWindow();
}

void CSettingsDlg::PostNcDestroy()
{
    // Free language combo item data strings
    CComboBox* pLangCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_LANGUAGE));
    if (pLangCombo)
    {
        for (int i = 0; i < pLangCombo->GetCount(); i++)
        {
            LPCTSTR pStr = reinterpret_cast<LPCTSTR>(pLangCombo->GetItemData(i));
            if (pStr) free(const_cast<LPTSTR>(pStr));
        }
    }
    delete this;
}

void CSettingsDlg::OnCbnSelchangeLanguage()
{
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_LANGUAGE));
    if (!pCombo) return;
    int sel = pCombo->GetCurSel();
    if (sel == CB_ERR) return;
    m_strCurrentLang = reinterpret_cast<LPCTSTR>(pCombo->GetItemData(sel));
}

void CSettingsDlg::BrowseFile(UINT id, LPCTSTR title)
{
    CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, NULL, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}

void CSettingsDlg::BrowseFolder(UINT id, LPCTSTR title)
{
    CFolderPickerDialog dlg(NULL, 0, this);
    dlg.m_ofn.lpstrTitle = title;
    if (dlg.DoModal() == IDOK) SetDlgItemText(id, dlg.GetPathName());
}

void CSettingsDlg::InitAIVendorCombo()
{
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_AI_VENDOR_CFG));
    if (!pCombo) return;

    pCombo->ResetContent();
    for (const auto& v : CAIApiClient::GetVendors())
        pCombo->AddString(v.name);

    CString savedVendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    int idx = pCombo->FindStringExact(-1, savedVendor);
    if (idx != CB_ERR)
        pCombo->SetCurSel(idx);
    else
        pCombo->SetCurSel(0);
}

void CSettingsDlg::OnBnClickedAiKeyShow()
{
    CEdit* pEdit = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_KEY_CFG));
    if (!pEdit) return;

    static bool bShowing = false;
    bShowing = !bShowing;

    pEdit->SetPasswordChar(bShowing ? 0 : _T('*'));
    auto& loc = CLocalizationManager::GetInstance();
    SetDlgItemText(IDC_BUTTON_AI_KEY_SHOW, bShowing ? loc.GetString(_T("Settings"), _T("BtnHide")) : loc.GetString(_T("Settings"), _T("BtnShow")));
    pEdit->Invalidate();
}

void CSettingsDlg::OnCbnSelchangeAiVendor()
{
    // Save current vendor's key before switching
    SaveVendorKey(m_strCurrentVendor);

    // Get new vendor
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_AI_VENDOR_CFG));
    if (!pCombo) return;
    pCombo->GetWindowText(m_strCurrentVendor);

    // Load new vendor's key
    LoadVendorKey(m_strCurrentVendor);
}

void CSettingsDlg::LoadVendorKey(const CString& vendor)
{
    CString keyName = _T("ApiKey_") + vendor;
    CString key = AfxGetApp()->GetProfileString(_T("AI"), keyName, _T(""));
    SetDlgItemText(IDC_EDIT_AI_KEY_CFG, key);
}

void CSettingsDlg::SaveVendorKey(const CString& vendor)
{
    if (vendor.IsEmpty()) return;
    CString keyName = _T("ApiKey_") + vendor;
    CString key;
    GetDlgItemText(IDC_EDIT_AI_KEY_CFG, key);
    AfxGetApp()->WriteProfileString(_T("AI"), keyName, key);
}

void CSettingsDlg::OnBnClickedHotkeyShowHide()
{
    CHotkeyCaptureDlg dlg(this, m_hotkeyShowHide);
    if (dlg.DoModal() == IDOK)
    {
        m_hotkeyShowHide = dlg.m_result;
        SetDlgItemText(IDC_EDIT_HOTKEY_SHOWHIDE, m_hotkeyShowHide.ToDisplay());
    }
}

void CSettingsDlg::OnBnClickedHotkeyLocate()
{
    CHotkeyCaptureDlg dlg(this, m_hotkeyLocate);
    if (dlg.DoModal() == IDOK)
    {
        m_hotkeyLocate = dlg.m_result;
        SetDlgItemText(IDC_EDIT_HOTKEY_LOCATE, m_hotkeyLocate.ToDisplay());
    }
}