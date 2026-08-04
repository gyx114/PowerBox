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

        // Re-show tab controls and lightweight redraw (no image list rebuild!)
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
        if (pTab) UpdateQuickTab(pTab->GetCurSel());
        CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_QUICK_LAUNCH);
        if (pList && ::IsWindowVisible(pList->m_hWnd))
        {
            pList->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
        }

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

    // Re-show the current quick tab's controls (they may be hidden after hide/show)
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
    if (pTab) UpdateQuickTab(pTab->GetCurSel());

    // Lightweight redraw only — never rebuild image list on restore, that
    // destroys the image handles while ListCtrl still holds indices → blank
    // icons until mouse-hover repaint. Invalidate is enough.
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_QUICK_LAUNCH);
    if (pList && ::IsWindowVisible(pList->m_hWnd))
    {
        pList->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
    }

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

            // Re-show tab controls (may be hidden), then do LIGHTWEIGHT redraw only.
            // Do NOT call RefreshQuickLaunchList here — deleting+recreating the
            // image list while the list ctrl references old indices causes blank
            // icons until the user hovers over items to trigger a repaint.
            CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
            if (pTab) UpdateQuickTab(pTab->GetCurSel());
            CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_QUICK_LAUNCH);
            if (pList && ::IsWindowVisible(pList->m_hWnd))
            {
                pList->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
            }

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
    UpdateTitleBar();
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

    CString showHideText = showHide.IsEmpty()
        ? loc.GetString(_T("Hotkey"), _T("None"))
        : showHide.ToDisplay();
    CString locateText = locate.IsEmpty()
        ? loc.GetString(_T("Hotkey"), _T("None"))
        : locate.ToDisplay();

    // Prepend dynamic hotkey lines to the static shortcut list
    // (static list no longer contains these hotkey descriptions)
    result.Format(_T("%s   - %s\n%s   - %s\n\n%s"),
        (LPCTSTR)showHideText,
        (LPCTSTR)loc.GetString(_T("MainDlg"), _T("MainDlg")),
        (LPCTSTR)locateText,
        (LPCTSTR)loc.GetString(_T("WindowTab"), _T("LocateBtn")),
        (LPCTSTR)fmt);

    m_strShortcutText = result;
}

void CMFCApplication1Dlg::UpdateTitleBar()
{
    auto& loc = CLocalizationManager::GetInstance();
    CString baseTitle = loc.GetString(_T("MainDlg"), _T("MainDlg"));

    // Read current ShowHide hotkey config
    HotkeyInfo showHide = HotkeyInfo::FromConfigString(
        AfxGetApp()->GetProfileString(_T("Hotkeys"), _T("ShowHide"), _T("3|32")));

    if (!showHide.IsEmpty())
    {
        CString hotkeyStr = showHide.ToDisplay();
        CString fmt = loc.GetString(_T("MainDlg"), _T("TitleFormat"));
        // fmt is like "%s (%s to show/hide)" or "%s (%s 唤起此窗口)"
        CString title;
        title.Format(fmt, (LPCTSTR)baseTitle, (LPCTSTR)hotkeyStr);
        SetWindowText(title);
    }
    else
    {
        SetWindowText(baseTitle);
    }
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