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

// Static member initialization
CHotkeyCaptureDlg* CHotkeyCaptureDlg::s_pCaptureDlg = nullptr;
UINT CHotkeyCaptureDlg::WM_CAPTURE_HOTKEY_KEY = ::RegisterWindowMessage(_T("WM_CAPTURE_HOTKEY_KEY"));

BEGIN_MESSAGE_MAP(CHotkeyCaptureDlg, CDialogEx)
    ON_BN_CLICKED(IDC_HOTKEY_CLEAR, &CHotkeyCaptureDlg::OnClear)
    ON_REGISTERED_MESSAGE(WM_CAPTURE_HOTKEY_KEY, &CHotkeyCaptureDlg::OnCapturedKey)
    ON_WM_DESTROY()
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

    // Reset modifier key tracking state
    m_bCtrlDown = false;
    m_bAltDown = false;
    m_bShiftDown = false;
    m_bWinDown = false;

    // Install a low-level keyboard hook to intercept keys before they reach
    // any target window. This prevents the target application from receiving
    // the hotkey being captured (e.g. when configuring a wake hotkey for an
    // already-running program).
    s_pCaptureDlg = this;
    m_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
        GetModuleHandle(nullptr), 0);

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

void CHotkeyCaptureDlg::OnDestroy()
{
    // Uninstall the low-level keyboard hook
    if (m_hHook)
    {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = nullptr;
    }
    s_pCaptureDlg = nullptr;
    CDialogEx::OnDestroy();
}

// ============================================================================
// Low-level keyboard hook: intercepts keys before they reach any window
// ============================================================================
LRESULT CALLBACK CHotkeyCaptureDlg::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Only intercept when processing and the dialog is still alive
    if (nCode == HC_ACTION && s_pCaptureDlg && s_pCaptureDlg->m_hHook)
    {
        KBDLLHOOKSTRUCT* pInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        UINT vk = pInfo->vkCode;

        // ---- Track modifier state manually ----
        // The hook callback context may not have accurate async key state,
        // so we track modifier keys ourselves based on key-down/key-up.

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // Update modifier state on key-down
            if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)  { s_pCaptureDlg->m_bCtrlDown = true;  return 1; }
            if (vk == VK_MENU   || vk == VK_LMENU   || vk == VK_RMENU)       { s_pCaptureDlg->m_bAltDown = true;   return 1; }
            if (vk == VK_SHIFT  || vk == VK_LSHIFT  || vk == VK_RSHIFT)      { s_pCaptureDlg->m_bShiftDown = true; return 1; }
            if (vk == VK_LWIN   || vk == VK_RWIN)                            { s_pCaptureDlg->m_bWinDown = true;   return 1; }

            // Non-modifier key: build modifier flags from tracked state
            UINT mod = 0;
            if (s_pCaptureDlg->m_bCtrlDown)  mod |= MOD_CONTROL;
            if (s_pCaptureDlg->m_bAltDown)   mod |= MOD_ALT;
            if (s_pCaptureDlg->m_bShiftDown) mod |= MOD_SHIFT;
            if (s_pCaptureDlg->m_bWinDown)   mod |= MOD_WIN;

            // Process the key directly in the hook callback. This avoids
            // message-queue timing issues (PostMessage) and re-entrancy
            // problems (SendMessage) associated with the modal dialog.
            s_pCaptureDlg->OnCapturedKey(0, MAKELPARAM(mod, vk));
            return 1; // block — prevent the target from ever seeing this key
        }

        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            // Update modifier state on key-up
            if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)  { s_pCaptureDlg->m_bCtrlDown = false; return 1; }
            if (vk == VK_MENU   || vk == VK_LMENU   || vk == VK_RMENU)       { s_pCaptureDlg->m_bAltDown = false;  return 1; }
            if (vk == VK_SHIFT  || vk == VK_LSHIFT  || vk == VK_RSHIFT)      { s_pCaptureDlg->m_bShiftDown = false; return 1; }
            if (vk == VK_LWIN   || vk == VK_RWIN)                            { s_pCaptureDlg->m_bWinDown = false;  return 1; }

            // Block non-modifier key-up too so the target never sees an
            // unbalanced sequence for the key-down we blocked above.
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Process a key captured by the low-level hook
LRESULT CHotkeyCaptureDlg::OnCapturedKey(WPARAM wParam, LPARAM lParam)
{
    UINT mod = LOWORD(lParam);
    UINT key = HIWORD(lParam);

    // Escape: cancel/close the dialog
    if (key == VK_ESCAPE)
    {
        OnCancel();
        return 0;
    }

    // Backspace/Delete: clear the hotkey
    if (key == VK_BACK || key == VK_DELETE)
    {
        m_current = HotkeyInfo();
        UpdateDisplay();
        return 0;
    }

    // Enter: confirm current hotkey and close
    if (key == VK_RETURN)
    {
        m_result = m_current;
        CDialogEx::OnOK();
        return 0;
    }

    // Space is capturable (with or without modifiers)
    if (key == VK_SPACE)
    {
        m_current.modifier = mod;
        m_current.vk = key;
        UpdateDisplay();
        return 0;
    }

    // Require at least one modifier unless it's a function key
    if (mod == 0 && (key < VK_F1 || key > VK_F12))
        return 0;

    m_current.modifier = mod;
    m_current.vk = key;
    UpdateDisplay();
    return 0;
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