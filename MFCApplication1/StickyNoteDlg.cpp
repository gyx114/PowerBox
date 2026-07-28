// StickyNoteDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "StickyNoteDlg.h"
#include "afxdialogex.h"
#include <fstream>
#include <filesystem>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CStickyNoteDlg, CDialogEx)

CStickyNoteDlg::CStickyNoteDlg(CWnd* pParent)
	: CDialogEx(IDD_STICKY_NOTE_DLG, pParent)
{
}

CStickyNoteDlg::~CStickyNoteDlg()
{
}

void CStickyNoteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_STICKY_NOTE, m_noteText);
}

BEGIN_MESSAGE_MAP(CStickyNoteDlg, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_WM_SYSCOMMAND()
	ON_WM_TIMER()
	ON_WM_NCLBUTTONDBLCLK()
	ON_WM_NCRBUTTONUP()
	ON_BN_CLICKED(IDC_BTN_STICKY_BROWSE, &CStickyNoteDlg::OnBnClickedBrowse)
	ON_EN_CHANGE(IDC_EDIT_STICKY_NOTE, &CStickyNoteDlg::OnEnChangeEdit)
END_MESSAGE_MAP()

CString CStickyNoteDlg::GetSavePath()
{
	CString folder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	if (folder.IsEmpty() || !std::filesystem::exists(std::filesystem::path(folder.GetString())))
	{
		WCHAR exePath[MAX_PATH];
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		folder = exePath;
		int pos = folder.ReverseFind(L'\\');
		if (pos >= 0)
			folder = folder.Left(pos + 1);
	}
	// Ensure trailing backslash
	if (!folder.IsEmpty() && folder[folder.GetLength() - 1] != L'\\')
		folder += L'\\';
	return folder + L"sticky_note.txt";
}

bool CStickyNoteDlg::EnsureSavePath()
{
	CString folder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	if (!folder.IsEmpty() && std::filesystem::exists(std::filesystem::path(folder.GetString())))
		return true;

	CFolderPickerDialog dlg(nullptr, 0, this);
	dlg.m_ofn.lpstrTitle = L"Please select sticky note save directory";
	if (dlg.DoModal() == IDOK)
	{
		AfxGetApp()->WriteProfileString(L"StickyNote", L"SaveFolder", dlg.GetPathName());
		return true;
	}
	return false;
}

void CStickyNoteDlg::LoadNote()
{
	CString filePath = GetSavePath();
	if (std::filesystem::exists(std::filesystem::path(filePath.GetString())))
	{
		std::ifstream ifs(filePath, std::ios::in);
		if (ifs.is_open())
		{
			std::string content((std::istreambuf_iterator<char>(ifs)),
			                    std::istreambuf_iterator<char>());
			ifs.close();
			m_noteText = CString(CA2CT(content.c_str(), CP_UTF8));
			UpdateData(FALSE);
		}
	}
}

void CStickyNoteDlg::OnEnChangeEdit()
{
	UpdateData(TRUE);
}

void CStickyNoteDlg::SaveNote()
{
	// m_noteText is kept in sync via EN_CHANGE, so no need to re-read from control
	// (reading from control during parent OnDestroy may fail)

	CString filePath = GetSavePath();
	// Ensure parent directory exists (create if needed, no dialog)
	std::filesystem::path dir = std::filesystem::path(filePath.GetString()).parent_path();
	if (!dir.empty() && !std::filesystem::exists(dir))
		std::filesystem::create_directories(dir);

	std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
	if (ofs.is_open())
	{
		CT2CA utf8(m_noteText, CP_UTF8);
		ofs.write(utf8, (std::streamsize)strlen(utf8));
		ofs.close();
	}
}

void CStickyNoteDlg::SaveIfNeeded()
{
	SaveNote();
}

void CStickyNoteDlg::CollapseWindow()
{
	if (m_bCollapsed)
		return;

	m_bCollapsed = true;

	// Save current window rect
	GetWindowRect(&m_rcNormal);

	// Calculate title bar height (caption + borders)
	int cyCaption = GetSystemMetrics(SM_CYCAPTION);
	int cyBorder = GetSystemMetrics(SM_CYFRAME);
	int titleBarHeight = cyCaption + cyBorder * 2 + 2;

	// Hide the edit control
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit)
		pEdit->ShowWindow(SW_HIDE);

	// Resize to just title bar, using the original dialog width (not the current maximized width)
	SetWindowPos(nullptr, 0, 0,
		m_rcCollapsed.Width(), titleBarHeight,
		SWP_NOZORDER | SWP_NOMOVE);
}

void CStickyNoteDlg::ExpandWindow()
{
	if (!m_bCollapsed)
		return;

	m_bCollapsed = false;

	// Restore normal size
	SetWindowPos(nullptr, 0, 0,
		m_rcNormal.Width(), m_rcNormal.Height(),
		SWP_NOZORDER | SWP_NOMOVE);

	// Show the edit control
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit)
	{
		pEdit->ShowWindow(SW_SHOW);
		ResizeEdit();
	}
}

BOOL CStickyNoteDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Remove dialog icon from title bar
	SetIcon(NULL, FALSE);
	SetIcon(NULL, TRUE);

	// Set always-on-top
	SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	// Calculate edit control margins for resize
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit)
	{
		CRect rcClient, rcEdit;
		GetClientRect(&rcClient);
		pEdit->GetWindowRect(&rcEdit);
		ScreenToClient(&rcEdit);
		m_editMargins.left = rcEdit.left;
		m_editMargins.top = rcEdit.top;
		m_editMargins.right = rcClient.right - rcEdit.right;
		m_editMargins.bottom = rcClient.bottom - rcEdit.bottom;
	}

	// Save collapsed title bar width (the original dialog size)
	GetWindowRect(&m_rcCollapsed);

	// Load saved note
	LoadNote();

	// Show normally first, then collapse after a delay so the window is fully painted
	SetTimer(1, 300, nullptr);

	return TRUE;
}

void CStickyNoteDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		KillTimer(1);
		CollapseWindow();
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CStickyNoteDlg::OnBnClickedBrowse()
{
	CString currentFolder = AfxGetApp()->GetProfileString(L"StickyNote", L"SaveFolder", L"");
	CFolderPickerDialog dlg(currentFolder.IsEmpty() ? nullptr : (LPCTSTR)currentFolder, 0, this);
	dlg.m_ofn.lpstrTitle = L"Select sticky note save directory";
	if (dlg.DoModal() == IDOK)
	{
		AfxGetApp()->WriteProfileString(L"StickyNote", L"SaveFolder", dlg.GetPathName());
	}
}

void CStickyNoteDlg::OnClose()
{
	SaveNote();
	if (IsZoomed())
	{
		// Maximized: restore to normal size instead of collapsing
		ShowWindow(SW_RESTORE);
	}
	else if (m_bCollapsed)
	{
		DestroyWindow();
	}
	else
	{
		CollapseWindow();
	}
}

void CStickyNoteDlg::OnOK()
{
	SaveNote();
	CollapseWindow();
}

void CStickyNoteDlg::OnCancel()
{
	CollapseWindow();
}

void CStickyNoteDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && !m_bCollapsed)
		ResizeEdit();
}

void CStickyNoteDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_MINIMIZE)
	{
		if (IsZoomed())
		{
			ShowWindow(SW_RESTORE);
		}
		else
		{
			CollapseWindow();
		}
		return;
	}
	if (nID == SC_MAXIMIZE)
	{
		if (m_bCollapsed)
		{
			ExpandWindow();
		}
		else
		{
			CDialogEx::OnSysCommand(nID, lParam);
		}
		return;
	}
	CDialogEx::OnSysCommand(nID, lParam);
}

void CStickyNoteDlg::OnNcLButtonDblClk(UINT nHitTest, CPoint point)
{
	if (m_bCollapsed && nHitTest == HTCAPTION)
	{
		ExpandWindow();
		return;
	}
	CDialogEx::OnNcLButtonDblClk(nHitTest, point);
}

void CStickyNoteDlg::OnNcRButtonUp(UINT nHitTest, CPoint point)
{
	if (nHitTest == HTCAPTION)
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, L"退出便签(&X)");

		// point is already in screen coordinates for NC messages
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
		return;
	}
	CDialogEx::OnNcRButtonUp(nHitTest, point);
}

BOOL CStickyNoteDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == 1)
	{
		SaveNote();
		DestroyWindow();
		return TRUE;
	}
	return CDialogEx::OnCommand(wParam, lParam);
}

void CStickyNoteDlg::ResizeEdit()
{
	CWnd* pEdit = GetDlgItem(IDC_EDIT_STICKY_NOTE);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		CRect rcClient;
		GetClientRect(&rcClient);
		pEdit->SetWindowPos(nullptr,
			m_editMargins.left,
			m_editMargins.top,
			rcClient.Width() - m_editMargins.left - m_editMargins.right,
			rcClient.Height() - m_editMargins.top - m_editMargins.bottom,
			SWP_NOZORDER);
	}
}