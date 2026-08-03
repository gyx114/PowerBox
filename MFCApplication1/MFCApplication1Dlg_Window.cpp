#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "LocalizationManager.h"

// Overlay window procedure used for capture
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CMFCApplication1Dlg* pDlg = (CMFCApplication1Dlg*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        {
            POINTS pts = MAKEPOINTS(lParam);
            POINT pt = { pts.x, pts.y };
            ::ClientToScreen(hwnd, &pt);
            // hide our overlay first so WindowFromPoint returns the underlying window
            ::ShowWindow(hwnd, SW_HIDE);
            HWND hTarget = ::WindowFromPoint(pt);
            if (pDlg) pDlg->OnTargetSelected(hTarget, pt);
            if (::GetCapture() == hwnd) ::ReleaseCapture();
            ::DestroyWindow(hwnd);
            return 0;
        }
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CMFCApplication1Dlg::OnBnClickedButton19()
{
    // If overlay exists, cancel it
    if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
    {
        ::DestroyWindow(m_hCaptureWnd);
        m_hCaptureWnd = NULL;
        // continue: recreate overlay so user can locate again
    }

    // Start capture overlay: create a simple full-screen window class on the fly
    WNDCLASS wc = {0};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.lpszClassName = _T("MyCaptureOverlayClass");
    wc.hCursor = ::LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    // Register the class only if it isn't already registered to avoid leaking
    // repeated registrations.
    WNDCLASS existing = {0};
    if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &existing))
    {
        RegisterClass(&wc);
    }

    HWND hOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, _T(""), WS_POPUP,
                                    0,0,::GetSystemMetrics(SM_CXSCREEN),::GetSystemMetrics(SM_CYSCREEN),
                                    m_hWnd, NULL, AfxGetInstanceHandle(), NULL);
    if (!hOverlay)
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Msg"), _T("EnterLocateModeFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }
    ::SetWindowLongPtr(hOverlay, GWLP_USERDATA, (LONG_PTR)this);
    // Ensure our wndproc is the overlay proc
    ::SetWindowLongPtr(hOverlay, GWLP_WNDPROC, (LONG_PTR)OverlayWndProc);
    ::ShowWindow(hOverlay, SW_SHOW);
    ::SetCapture(hOverlay);
    m_hCaptureWnd = hOverlay;
}

void CMFCApplication1Dlg::OnTargetSelected(HWND hTarget, POINT pt)
{
    // Promote child control to its top-level parent, avoid recording buttons/edit boxes
    hTarget = ::GetAncestor(hTarget, GA_ROOT);

    m_hSelectedWnd = hTarget;
    if (!IsWindow(hTarget))
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Msg"), _T("NoWindowSelected")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    // When a target window is selected via the overlay, treat it as "located":
    // add to history immediately so LIST7 gets populated on tab switch
    {
        auto it = std::find(m_historyWnds.begin(), m_historyWnds.end(), hTarget);
        if (it == m_historyWnds.end())
            m_historyWnds.push_back(hTarget);
    }

    // switch to the "Window handling" tab and refresh the list to show this window's info.
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    if (pTab)
    {
        pTab->SetCurSel(3);
        LRESULT res = 0;
        OnTcnSelchangeTab1(NULL, &res);
    }

    // Show operation menu
    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();
    // Provide only topmost and close options
    menu.AppendMenu(MF_STRING, 1, loc.GetString(_T("WindowTab"), _T("RClickTopmost")));
    menu.AppendMenu(MF_STRING, 3, loc.GetString(_T("WindowTab"), _T("RClickCloseWindow")));
    menu.AppendMenu(MF_STRING, 0, loc.GetString(_T("WindowTab"), _T("RClickCancel")));

    ::SetForegroundWindow(m_hWnd);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this);
    if (cmd == 1)
    {
        if (!::SetWindowPos(hTarget, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE))
            MessageBox(loc.GetString(_T("Msg"), _T("TopmostFail")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONERROR);
        else
        {
            // Avoid duplicate additions
            auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hTarget);
            if (it == m_topmostWnds.end())
                m_topmostWnds.push_back(hTarget);

            // If topmost is the toolbox itself, sync checkbox
            if (hTarget == m_hWnd)
            {
                CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
                if (pCheck) pCheck->SetCheck(BST_CHECKED);
            }

            // Refresh topmost list display
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab) UpdateTabVisibility(pTab->GetCurSel());
        }
    }
    else if (cmd == 3)
    {
        ::PostMessage(hTarget, WM_CLOSE, 0, 0);
    }
}

void CMFCApplication1Dlg::OnForceKillProcess()
{
	auto& loc = CLocalizationManager::GetInstance();
	if (!m_hSelectedWnd || !IsValidWindow(m_hSelectedWnd))
	{
		MessageBox(loc.GetString(_T("Msg"), _T("InvalidWindow")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
		return;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(m_hSelectedWnd, &pid);
	if (pid == 0)
	{
		MessageBox(loc.GetString(_T("Msg"), _T("CannotGetPID")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
		return;
	}

	CString msg;
	msg.Format(loc.GetString(_T("Msg"), _T("ConfirmForceKill")), pid);
	if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmForceKillTitle")), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!hProc)
	{
		DWORD err = GetLastError();
		msg.Format(loc.GetString(_T("Msg"), _T("CannotOpenProcErr")), err);
		MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
		return;
	}

	if (TerminateProcess(hProc, 1))
	{
		MessageBox(loc.GetString(_T("Msg"), _T("ProcessForceKilled")), loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
		// Remove terminated windows from topmost list
		m_topmostWnds.erase(
			std::remove_if(m_topmostWnds.begin(), m_topmostWnds.end(),
				[](HWND h) { return !IsValidWindow(h); }),
			m_topmostWnds.end());
		// Refresh display
		CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
		if (pTab) UpdateTabVisibility(pTab->GetCurSel());
	}
	else
	{
		MessageBox(loc.GetString(_T("Msg"), _T("ProcessForceKillFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
	}
	CloseHandle(hProc);
}

void CMFCApplication1Dlg::OnWindowScreenshot()
{
	auto& loc = CLocalizationManager::GetInstance();
	if (!m_hSelectedWnd || !IsValidWindow(m_hSelectedWnd))
	{
		MessageBox(loc.GetString(_T("Msg"), _T("InvalidWindow")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
		return;
	}

	// Get window dimensions
	RECT rc;
	::GetWindowRect(m_hSelectedWnd, &rc);
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	if (width <= 0 || height <= 0)
	{
		MessageBox(loc.GetString(_T("Msg"), _T("InvalidWindowSize")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
		return;
	}

	// Create device context
	HDC hdcScreen = ::GetDC(NULL);
	HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, width, height);
	HBITMAP hOldBmp = static_cast<HBITMAP>(::SelectObject(hdcMem, hBitmap));

	// Use PrintWindow to capture window (multi-level fallback)
	// PW_RENDERFULLCONTENT(0x2): best for DWM composited windows, supports GPU rendered content
	BOOL bPrintOK = ::PrintWindow(m_hSelectedWnd, hdcMem, 0x2);
	if (!bPrintOK)
	{
		// Fallback 1: PrintWindow without flags
		bPrintOK = ::PrintWindow(m_hSelectedWnd, hdcMem, 0);
	}
	if (!bPrintOK)
	{
		// Fallback 2: BitBlt from screen directly
		::SetForegroundWindow(m_hSelectedWnd);
		::Sleep(100);
		::BitBlt(hdcMem, 0, 0, width, height, hdcScreen, rc.left, rc.top, SRCCOPY);
	}

	// Copy to clipboard
	if (::OpenClipboard(m_hWnd))
	{
		::EmptyClipboard();
		::SetClipboardData(CF_BITMAP, hBitmap);
		::CloseClipboard();
	}
	else
	{
		MessageBox(loc.GetString(_T("Msg"), _T("ClipboardFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
		::DeleteObject(hBitmap);
	}

	// Save to file
	{
		CString sDir = AfxGetApp()->GetProfileString(_T("Paths"), _T("ScreenshotDir"), _T(""));
		if (sDir.IsEmpty())
		{
			TCHAR szDesktop[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, szDesktop)))
				sDir = szDesktop;
		}

		// Generate timestamped filename
		SYSTEMTIME st;
		GetLocalTime(&st);
		CString sFilename;
		sFilename.Format(_T("\\screenshot_%04d%02d%02d_%02d%02d%02d.png"),
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		CString sFullPath = sDir + sFilename;

		// Ensure directory exists
		::SHCreateDirectoryEx(NULL, sDir, NULL);

		// Save as PNG (using ATL CImage)
		CImage img;
		img.Attach(hBitmap);
		img.Save(sFullPath);
		img.Detach();  // Prevent hBitmap from being freed by CImage destructor

		CString sMsg;
		sMsg.Format(loc.GetString(_T("Msg"), _T("ScreenshotSavedTo")), sFullPath);
		MessageBox(sMsg, loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
	}

	::SelectObject(hdcMem, hOldBmp);
	::DeleteDC(hdcMem);
	::ReleaseDC(NULL, hdcScreen);
}

void CMFCApplication1Dlg::OnWindowLocate()
{
	// Switch to window handling tab and trigger locate
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) { pTab->SetCurSel(3); UpdateTabVisibility(3); }
	OnBnClickedButton19();
}

void CMFCApplication1Dlg::OnWindowUntopmost()
{
	for (HWND hWnd : m_topmostWnds)
	{
		if (IsValidWindow(hWnd))
			::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
	}
	m_topmostWnds.clear();

	// Sync uncheck toolbox topmost checkbox
	CButton* pCheck3 = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
	if (pCheck3) pCheck3->SetCheck(BST_UNCHECKED);

	// Refresh display
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnWindowClose()
{
	auto& loc = CLocalizationManager::GetInstance();
	if (m_hSelectedWnd && IsValidWindow(m_hSelectedWnd))
	{
		CString title;
		::GetWindowText(m_hSelectedWnd, title.GetBuffer(256), 256);
		title.ReleaseBuffer();

		CString msg;
		msg.Format(loc.GetString(_T("Msg"), _T("ConfirmCloseWindow")), title);
		if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmCloseTitle")), MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			::SendMessage(m_hSelectedWnd, WM_CLOSE, 0, 0);
			// Remove from topmost list
			m_topmostWnds.erase(
				std::remove_if(m_topmostWnds.begin(), m_topmostWnds.end(),
					[](HWND h) { return !IsValidWindow(h); }),
				m_topmostWnds.end());
			m_hSelectedWnd = nullptr;
			CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
			if (pTab) UpdateTabVisibility(pTab->GetCurSel());
		}
	}
	else
	{
		MessageBox(loc.GetString(_T("Msg"), _T("InvalidWindow")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
	}
}

void CMFCApplication1Dlg::OnUntopmostWindow()
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) return;

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[idx];
	if (IsValidWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	m_topmostWnds.erase(m_topmostWnds.begin() + idx);

	// If it is the toolbox itself, sync uncheck checkbox
	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}

	// Refresh list display
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnCopyStartupPath()
{
	CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
	if (!pList2) return;

	int nSel = pList2->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	CString cmd = pList2->GetItemText(nSel, 1);
	if (!cmd.IsEmpty())
		CopyToClipboard(m_hWnd, cmd);
}

void CMFCApplication1Dlg::OnNMDblclkList2(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Double-click startup list to copy path directly
	OnCopyStartupPath();
	*pResult = 0;
}

void CMFCApplication1Dlg::OnHelpShortcuts()
{
    auto& loc = CLocalizationManager::GetInstance();
    CString text = m_strShortcutText.IsEmpty()
        ? loc.GetString(_T("Shortcut"), _T("ShortcutList"))
        : m_strShortcutText;
    MessageBox(text, loc.GetString(_T("Shortcut"), _T("ShortcutListTitle")), MB_OK | MB_ICONINFORMATION);
}

void CMFCApplication1Dlg::OnHelpGithub()
{
	ShellExecute(m_hWnd, _T("open"), _T("https://github.com"), NULL, NULL, SW_SHOWNORMAL);
}

void CMFCApplication1Dlg::LoadWindowDetailToList5(HWND hWnd)
{
	auto& loc = CLocalizationManager::GetInstance();
	CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));
	if (!pList5) return;

	pList5->DeleteAllItems();

	if (!hWnd || !IsValidWindow(hWnd)) return;

	DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);

	CString procName = _T("");
	CString procPath = _T("");
	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProc)
	{
		TCHAR buf[MAX_PATH] = {0};
		DWORD size = _countof(buf);
		if (QueryFullProcessImageName(hProc, 0, buf, &size)) procPath = buf;
		int psep = procPath.ReverseFind(_T('\\'));
		if (psep != -1) procName = procPath.Mid(psep + 1);
		CloseHandle(hProc);
	}

	CString title;
	::GetWindowText(hWnd, title.GetBuffer(512), 512);
	title.ReleaseBuffer();

	int row = 0;
	CString val;

	val.Format(_T("0x%08X"), reinterpret_cast<UINT_PTR>(hWnd));
	pList5->InsertItem(row, loc.GetString(_T("WindowTab"), _T("Handle"))); pList5->SetItemText(row++, 1, val);
	pList5->InsertItem(row, loc.GetString(_T("WindowTab"), _T("ProcName"))); pList5->SetItemText(row++, 1, procName);
	val.Format(_T("%u"), pid);
	pList5->InsertItem(row, loc.GetString(_T("WindowTab"), _T("PID"))); pList5->SetItemText(row++, 1, val);
	pList5->InsertItem(row, loc.GetString(_T("WindowTab"), _T("Path"))); pList5->SetItemText(row++, 1, procPath);
	pList5->InsertItem(row, loc.GetString(_T("WindowTab"), _T("Title"))); pList5->SetItemText(row++, 1, title);
}

void CMFCApplication1Dlg::OnClickList6(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) { *pResult = 0; return; }

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) { *pResult = 0; return; }

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_topmostWnds[idx];
	if (hWnd && IsValidWindow(hWnd))
	{
		m_hSelectedWnd = hWnd;
		LoadWindowDetailToList5(hWnd);
	}

	*pResult = 0;
}

void CMFCApplication1Dlg::OnClickList7(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) { *pResult = 0; return; }

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) { *pResult = 0; return; }

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) { *pResult = 0; return; }

	HWND hWnd = m_historyWnds[idx];
	if (hWnd && IsValidWindow(hWnd))
	{
		m_hSelectedWnd = hWnd;
		LoadWindowDetailToList5(hWnd);
	}

	*pResult = 0;
}

void CMFCApplication1Dlg::OnDeleteList6Record()
{
	CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
	if (!pList6) return;

	int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList6->GetItemData(nSel));
	if (idx >= m_topmostWnds.size()) return;

	HWND hWnd = m_topmostWnds[idx];
	if (IsValidWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}
	m_topmostWnds.erase(m_topmostWnds.begin() + idx);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnDeleteList7Record()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	m_historyWnds.erase(m_historyWnds.begin() + idx);

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnTopmostFromHistory()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[idx];
	if (!IsValidWindow(hWnd)) return;

	if (!::SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE))
	{
		auto& loc = CLocalizationManager::GetInstance();
		MessageBox(loc.GetString(_T("Msg"), _T("TopmostFail")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONERROR);
		return;
	}

	auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd);
	if (it == m_topmostWnds.end())
		m_topmostWnds.push_back(hWnd);

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_CHECKED);
	}

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnUntopmostFromHistory()
{
	CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
	if (!pList7) return;

	int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
	if (idx >= m_historyWnds.size()) return;

	HWND hWnd = m_historyWnds[idx];
	if (IsValidWindow(hWnd))
		::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);

	m_topmostWnds.erase(
		std::remove(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd),
		m_topmostWnds.end());

	if (hWnd == m_hWnd)
	{
		CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
		if (pCheck) pCheck->SetCheck(BST_UNCHECKED);
	}

	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) UpdateTabVisibility(pTab->GetCurSel());
}

void CMFCApplication1Dlg::OnBnClickedCheck3()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK3);
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        // Topmost
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        // Avoid duplicate additions
        auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd);
        if (it == m_topmostWnds.end())
            m_topmostWnds.push_back(m_hWnd);
    }
    else
    {
        // Cancel topmost
        SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        m_topmostWnds.erase(
            std::remove(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd),
            m_topmostWnds.end());
    }

    // Refresh topmost window list
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab && pTab->GetCurSel() == 3)
        UpdateTabVisibility(3);
}