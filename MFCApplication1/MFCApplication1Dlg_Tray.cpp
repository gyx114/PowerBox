#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "StickyNoteDlg.h"
#include "LocalizationManager.h"
#include <Shellapi.h>

// ========== Tray-related functionality ==========

void CMFCApplication1Dlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    // When window is minimized, hide main window and show tray icon
    if (nType == SIZE_MINIMIZED)
    {
        // Create tray icon
        if (!m_bTrayVisible)
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = m_hWnd;
            m_nid.uID = 1001;
            m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            m_nid.uCallbackMessage = WM_TRAYICON;
            m_nid.hIcon = m_hIcon;
            _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), _T("MFCApplication1"));
            Shell_NotifyIcon(NIM_ADD, &m_nid);
            m_bTrayVisible = true;
        }

        ShowWindow(SW_HIDE);
    }
}

LRESULT CMFCApplication1Dlg::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
    if (wParam != 1001) return 0;

    if (lParam == WM_RBUTTONUP)
    {
        // Show tray menu
        auto& loc = CLocalizationManager::GetInstance();
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 2001, loc.GetString(_T("TrayMenu"), _T("ShowWindow")));
        menu.AppendMenu(MF_STRING, 2002, loc.GetString(_T("TrayMenu"), _T("Exit")));

        POINT pt;
        GetCursorPos(&pt);
        ::SetForegroundWindow(m_hWnd);
        menu.TrackPopupMenu(TPM_RIGHTBUTTON, pt.x, pt.y, this);
        PostMessage(WM_NULL, 0, 0);
    }
    else if (lParam == WM_LBUTTONDBLCLK)
    {
        // Double-click to restore
        ShowWindow(SW_SHOW);
        ShowWindow(SW_RESTORE);
        if (m_bTrayVisible)
        {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_bTrayVisible = false;
        }
    }

    return 0;
}

void CMFCApplication1Dlg::OnTrayShowWindow()
{
    ShowWindow(SW_SHOW);
    ShowWindow(SW_RESTORE);
    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }
}

void CMFCApplication1Dlg::OnTrayExit()
{
    // Delete tray icon and exit program
    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }
    m_bExiting = true;
    EndDialog(IDOK);
}

void CMFCApplication1Dlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
    if (nHotKeyId == 1001)
    {
        // Toggle minimize/restore when Ctrl+Alt+Space pressed.
        if (!m_bTrayVisible && ::IsWindowVisible(m_hWnd))
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = m_hWnd;
            m_nid.uID = 1001;
            m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            m_nid.uCallbackMessage = WM_TRAYICON;
            m_nid.hIcon = m_hIcon;
            _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), _T("MFCApplication1"));
            Shell_NotifyIcon(NIM_ADD, &m_nid);
            m_bTrayVisible = true;
            ShowWindow(SW_HIDE);
        }
        else
        {
            ShowWindow(SW_SHOW);
            ShowWindow(SW_RESTORE);
            if (m_bTrayVisible)
            {
                Shell_NotifyIcon(NIM_DELETE, &m_nid);
                m_bTrayVisible = false;
            }
            ::SetForegroundWindow(m_hWnd);
        }

        // Force-release modifier keys to prevent stuck keys from external SendInput simulation
        // (e.g. desktop pet simulating Ctrl+Alt+Space \xe2\x80\x94 the system may eat key-up events)
        ForceReleaseModifierKeys();

        return;
    }
    else if (nHotKeyId == 1002)
    {
        // Locate window hotkey
        OnWindowLocate();
        return;
    }
    CDialogEx::OnHotKey(nHotKeyId, nKey1, nKey2);
}

void CMFCApplication1Dlg::RegisterHotkeys()
{
    UnregisterHotkeys();

    // Read hotkey configs from config.ini
    HotkeyInfo showHide = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("ShowHide"), _T("3|32")));
    HotkeyInfo locate = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("Locate"), _T("3|68")));

    // Register ShowHide (ID 1001)
    if (!showHide.IsEmpty())
        RegisterHotKey(m_hWnd, 1001, showHide.modifier, showHide.vk);

    // Register Locate (ID 1002)
    if (!locate.IsEmpty())
        RegisterHotKey(m_hWnd, 1002, locate.modifier, locate.vk);

    UpdateShortcutMenuText();
}

void CMFCApplication1Dlg::UnregisterHotkeys()
{
    UnregisterHotKey(m_hWnd, 1001);
    UnregisterHotKey(m_hWnd, 1002);
}

LRESULT CMFCApplication1Dlg::OnHotkeysChanged(WPARAM, LPARAM)
{
    // Re-read config and re-register hotkeys when settings change
    RegisterHotkeys();
    return 0;
}

void CMFCApplication1Dlg::UpdateShortcutMenuText()
{
    // Build dynamic shortcut text from current hotkey config
    HotkeyInfo showHide = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("ShowHide"), _T("3|32")));
    HotkeyInfo locate = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("Locate"), _T("3|68")));

    auto& loc = CLocalizationManager::GetInstance();
    CString fmt = loc.GetString(_T("Shortcut"), _T("ShortcutList"));
    CString result;

    // Replace the first line with dynamic hotkey text
    CString showHideText = showHide.IsEmpty()
        ? loc.GetString(_T("Hotkey"), _T("None"))
        : showHide.ToDisplay();
    CString locateText = locate.IsEmpty()
        ? loc.GetString(_T("Hotkey"), _T("None"))
        : locate.ToDisplay();

    result.Format(_T("%s   - %s\n%s   - %s\n%s"),
        (LPCTSTR)showHideText,
        (LPCTSTR)loc.GetString(_T("MainDlg"), _T("WindowTitleSuffix")),
        (LPCTSTR)locateText,
        (LPCTSTR)loc.GetString(_T("WindowTab"), _T("LocateBtn")),
        (LPCTSTR)(fmt.Mid(fmt.Find(_T('\n')) + 1)));

    // Update the stored shortcut text (OnHelpShortcuts reads from ini, so we update ini dynamically)
    // We'll store the updated text in a temporary way — OnHelpShortcuts shows the ShortcutList
    // which is static. We need to override the shortcut display.
    // Instead, we'll store the modified text in an internal map and use it in OnHelpShortcuts.
    // Actually, the simplest approach: just update the ini content in memory isn't possible.
    // Let's use a different approach: store the formatted text as a member.
    m_strShortcutText = result;
}

void CMFCApplication1Dlg::OnClose()
{
    // If triggered by language-switch restart, exit directly without tray minimize
    if (m_bExiting)
    {
        if (m_pStickyNoteDlg && ::IsWindow(m_pStickyNoteDlg->m_hWnd))
        {
            m_pStickyNoteDlg->SaveIfNeeded();
        }
        if (m_bTrayVisible)
        {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_bTrayVisible = false;
        }
        ::RemoveClipboardFormatListener(m_hWnd);
        if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
        {
            ::DestroyWindow(m_hCaptureWnd);
            m_hCaptureWnd = NULL;
        }
        if (m_bPreventLockScreen)
        {
            SetThreadExecutionState(ES_CONTINUOUS);
            m_bPreventLockScreen = false;
        }
        CDialogEx::OnClose();
        return;
    }

    // Save sticky note before closing (minimize to tray or exit)
    if (m_pStickyNoteDlg && ::IsWindow(m_pStickyNoteDlg->m_hWnd))
    {
        m_pStickyNoteDlg->SaveIfNeeded();
    }

    // Based on m_bMinimizeOnClose, decide whether to minimize to tray or exit directly
    if (m_bMinimizeOnClose)
    {
        // Consistent with Ctrl+Alt+Space hotkey: add tray icon, hide main window
        if (!m_bTrayVisible)
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = m_hWnd;
            m_nid.uID = 1001;
            m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            m_nid.uCallbackMessage = WM_TRAYICON;
            m_nid.hIcon = m_hIcon;
            _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), _T("MFCApplication1"));
            Shell_NotifyIcon(NIM_ADD, &m_nid);
            m_bTrayVisible = true;
        }
        ShowWindow(SW_HIDE);
    }
    else
    {
        // Clean up tray icon and call default close flow
        if (m_bTrayVisible)
        {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_bTrayVisible = false;
        }
        // Cancel clipboard listener
        ::RemoveClipboardFormatListener(m_hWnd);
        // Clean up any existing capture window
        if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
        {
            ::DestroyWindow(m_hCaptureWnd);
            m_hCaptureWnd = NULL;
        }
        // clear prevent-lock state if set
        if (m_bPreventLockScreen)
        {
            SetThreadExecutionState(ES_CONTINUOUS);
            m_bPreventLockScreen = false;
        }
        CDialogEx::OnClose();
    }
}