#include "pch.h"
#include "framework.h"
#include "HotkeyCaptureDlg.h"
#include "LocalizationManager.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// HotkeyInfo helpers
// ============================================================================

CString HotkeyInfo::ToDisplay() const
{
    if (IsEmpty()) return CLocalizationManager::GetInstance().GetString(_T("Hotkey"), _T("None"));

    CString text;
    if (modifier & MOD_CONTROL) text += _T("Ctrl+");
    if (modifier & MOD_ALT)     text += _T("Alt+");
    if (modifier & MOD_SHIFT)   text += _T("Shift+");
    if (modifier & MOD_WIN)     text += _T("Win+");

    switch (vk)
    {
    case VK_SPACE:      text += CLocalizationManager::GetInstance().GetString(_T("Hotkey"), _T("KeySpace")); break;
    case VK_RETURN:     text += CLocalizationManager::GetInstance().GetString(_T("Hotkey"), _T("KeyEnter")); break;
    case VK_TAB:        text += _T("Tab"); break;
    case VK_ESCAPE:     text += _T("Esc"); break;
    case VK_BACK:       text += _T("Backspace"); break;
    case VK_DELETE:     text += _T("Delete"); break;
    case VK_INSERT:     text += _T("Insert"); break;
    case VK_HOME:       text += _T("Home"); break;
    case VK_END:        text += _T("End"); break;
    case VK_PRIOR:      text += _T("PageUp"); break;
    case VK_NEXT:       text += _T("PageDown"); break;
    case VK_UP:         text += _T("Up"); break;
    case VK_DOWN:       text += _T("Down"); break;
    case VK_LEFT:       text += _T("Left"); break;
    case VK_RIGHT:      text += _T("Right"); break;
    case VK_SNAPSHOT:   text += _T("PrintScreen"); break;
    case VK_PAUSE:      text += _T("Pause"); break;
    case VK_APPS:       text += _T("Menu"); break;
    case VK_LWIN: case VK_RWIN: text += _T("Win"); break;
    default:
        if (vk >= 'A' && vk <= 'Z')
            text += (TCHAR)vk;
        else if (vk >= '0' && vk <= '9')
            text += (TCHAR)vk;
        else if (vk >= VK_F1 && vk <= VK_F12)
            text.AppendFormat(_T("F%d"), vk - VK_F1 + 1);
        else
            text.AppendFormat(_T("VK(%d)"), vk);
        break;
    }
    return text;
}

CString HotkeyInfo::ToConfigString() const
{
    CString s;
    s.Format(_T("%u|%u"), modifier, vk);
    return s;
}

HotkeyInfo HotkeyInfo::FromConfigString(const CString& str)
{
    HotkeyInfo info;
    int sep = str.Find(_T('|'));
    if (sep != -1)
    {
        info.modifier = (UINT)_tstol(str.Left(sep));
        info.vk = (UINT)_tstol(str.Mid(sep + 1));
    }
    return info;
}

// ============================================================================
// CHotkeyCaptureDlg implementation
// ============================================================================

IMPLEMENT_DYNAMIC(CHotkeyCaptureDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CHotkeyCaptureDlg, CDialogEx)
    ON_BN_CLICKED(IDC_HOTKEY_CLEAR, &CHotkeyCaptureDlg::OnClear)
END_MESSAGE_MAP()

CHotkeyCaptureDlg::CHotkeyCaptureDlg(CWnd* pParent, const HotkeyInfo& existing)
    : CDialogEx(IDD_HOTKEY_CAPTURE_DLG, pParent), m_current(existing), m_result(existing)
{
}

BOOL CHotkeyCaptureDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(loc.GetString(_T("Hotkey"), _T("CaptureTitle")));
    SetDlgItemText(IDC_HOTKEY_PROMPT, loc.GetString(_T("Hotkey"), _T("CapturePrompt")));
    SetDlgItemText(IDC_HOTKEY_CLEAR, loc.GetString(_T("Hotkey"), _T("BtnClear")));
    SetDlgItemText(IDOK, loc.GetString(_T("Settings"), _T("BtnOK")));
    SetDlgItemText(IDCANCEL, loc.GetString(_T("Settings"), _T("BtnCancel")));
    UpdateDisplay();
    return TRUE;
}

void CHotkeyCaptureDlg::UpdateDisplay()
{
    CString text = m_current.ToDisplay();
    SetDlgItemText(IDC_HOTKEY_DISPLAY, text.IsEmpty() ?
        CLocalizationManager::GetInstance().GetString(_T("Hotkey"), _T("CaptureWaiting")) : text);
}

void CHotkeyCaptureDlg::OnOK()
{
    // Confirm: save current captured hotkey
    m_result = m_current;
    CDialogEx::OnOK();
}

void CHotkeyCaptureDlg::OnCancel()
{
    // Cancel: keep original (m_result was initialized with existing)
    CDialogEx::OnCancel();
}

void CHotkeyCaptureDlg::OnClear()
{
    m_current = HotkeyInfo();
    UpdateDisplay();
}

BOOL CHotkeyCaptureDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
    {
        UINT key = (UINT)pMsg->wParam;

        // Ignore standalone modifier keys
        if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
            key == VK_MENU || key == VK_LMENU || key == VK_RMENU ||
            key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
            key == VK_LWIN || key == VK_RWIN)
        {
            return TRUE;
        }

        // Escape: cancel/close
        if (key == VK_ESCAPE)
        {
            OnCancel();
            return TRUE;
        }

        // Backspace/Delete: clear the hotkey
        if (key == VK_BACK || key == VK_DELETE)
        {
            m_current = HotkeyInfo();
            UpdateDisplay();
            return TRUE;
        }

        // Build modifier flags from current keyboard state (BEFORE Enter/Space check)
        UINT mod = 0;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetAsyncKeyState(VK_MENU) & 0x8000)    mod |= MOD_ALT;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   mod |= MOD_SHIFT;
        if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) mod |= MOD_WIN;

        // Enter: confirm current hotkey and close dialog (even if empty)
        if (key == VK_RETURN)
        {
            m_result = m_current;
            CDialogEx::OnOK();
            return TRUE;
        }

        // Space is a capturable key (with or without modifiers)
        if (key == VK_SPACE)
        {
            m_current.modifier = mod;
            m_current.vk = key;
            UpdateDisplay();
            return TRUE;
        }

        // Require at least one modifier unless it's a function key
        if (mod == 0 && (key < VK_F1 || key > VK_F12))
            return TRUE;

        m_current.modifier = mod;
        m_current.vk = key;
        UpdateDisplay();
        return TRUE;
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}