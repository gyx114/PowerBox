// FileLockDlg.cpp: implementation file
//

#include "pch.h"
#include "MFCApplication1.h"
#include "FileLockDlg.h"
#include "LocalizationManager.h"
#include "afxdialogex.h"
#include <RestartManager.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <set>
#include <algorithm>

#pragma comment(lib, "Rstrtmgr.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shlwapi.lib")

// Column indices
enum { COL_FILE = 0, COL_PROCESS, COL_PID, COL_TYPE, COL_PATH, COL_COUNT };

IMPLEMENT_DYNAMIC(CFileLockDlg, CDialogEx)

CFileLockDlg::CFileLockDlg(CWnd* pParent)
	: CDialogEx(IDD_FILELOCK_DLG, pParent)
	, m_hintLeft(12), m_hintTop(5), m_hintHeight(10)
	, m_listHeight(0)
	, m_btnWidth(38), m_btnHeight(18), m_btnGap(8)
{
	m_hintText = CLocalizationManager::GetInstance().GetString(_T("FileLock"), _T("Hint"));
}

CFileLockDlg::~CFileLockDlg()
{
}

void CFileLockDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_FILELOCK, m_list);
}

BEGIN_MESSAGE_MAP(CFileLockDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_DROPFILES()
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_BTN_FILELOCK_END,     &CFileLockDlg::OnBnClickedEnd)
	ON_BN_CLICKED(IDC_BTN_FILELOCK_ENDALL,  &CFileLockDlg::OnBnClickedEndAll)
	ON_BN_CLICKED(IDC_BTN_FILELOCK_REFRESH, &CFileLockDlg::OnBnClickedRefresh)
	ON_BN_CLICKED(IDC_BTN_FILELOCK_CLEAR,   &CFileLockDlg::OnBnClickedClear)
	ON_BN_CLICKED(IDC_BTN_FILELOCK_LOCATE,  &CFileLockDlg::OnBnClickedLocate)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_FILELOCK, &CFileLockDlg::OnDblclkList)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILELOCK, &CFileLockDlg::OnNMRClickList)
END_MESSAGE_MAP()

BOOL CFileLockDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("FileLockDlg")));

	// Accept drag-and-drop files
	DragAcceptFiles(TRUE);

	// Init list control
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	InitListColumns();

	// Read actual control positions as layout anchors (DPI-safe)
	CRect rcHint;
	if (CWnd* pHint = GetDlgItem(IDC_STATIC_FILELOCK_HINT))
	{
		pHint->GetWindowRect(&rcHint);
		ScreenToClient(&rcHint);
		m_hintLeft = rcHint.left;
		m_hintTop = rcHint.top;
		m_hintHeight = rcHint.Height();
	}

	CRect rcBtn1, rcBtn2;
	if (CWnd* pBtn1 = GetDlgItem(IDC_BTN_FILELOCK_CLEAR))
	{
		pBtn1->GetWindowRect(&rcBtn1);
		ScreenToClient(&rcBtn1);
		m_btnWidth = rcBtn1.Width();
		m_btnHeight = rcBtn1.Height();
	}
	if (CWnd* pBtn2 = GetDlgItem(IDC_BTN_FILELOCK_REFRESH))
	{
		pBtn2->GetWindowRect(&rcBtn2);
		ScreenToClient(&rcBtn2);
		m_btnGap = rcBtn2.left - rcBtn1.right;
	}

	// Store original list height from RC
	CRect rcList;
	m_list.GetWindowRect(&rcList);
	ScreenToClient(&rcList);
	m_listHeight = rcList.Height();

	// Set hint text
	SetDlgItemText(IDC_STATIC_FILELOCK_HINT, m_hintText);

	return TRUE;
}

BOOL CFileLockDlg::PreTranslateMessage(MSG* pMsg)
{
	// Prevent Enter/Esc from closing the dialog
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)
		{
			// If a button has focus, let it handle the key
			CWnd* pFocus = GetFocus();
			if (pFocus && pFocus->GetSafeHwnd() == GetDlgItem(IDC_LIST_FILELOCK)->GetSafeHwnd())
				return CDialogEx::PreTranslateMessage(pMsg);
			return TRUE;
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CFileLockDlg::PostNcDestroy()
{
	delete this;
}

void CFileLockDlg::InitListColumns()
{
	auto& loc = CLocalizationManager::GetInstance();
	m_list.InsertColumn(COL_FILE,    loc.GetString(_T("FileLock"), _T("ColFilePath")),     LVCFMT_LEFT, 120);
	m_list.InsertColumn(COL_PROCESS, loc.GetString(_T("FileLock"), _T("ColProcessName")),  LVCFMT_LEFT, 100);
	m_list.InsertColumn(COL_PID,     loc.GetString(_T("FileLock"), _T("ColPID")),          LVCFMT_LEFT, 60);
	m_list.InsertColumn(COL_TYPE,    loc.GetString(_T("FileLock"), _T("ColType")),         LVCFMT_LEFT, 70);
	m_list.InsertColumn(COL_PATH,    loc.GetString(_T("FileLock"), _T("ColProcessPath")),  LVCFMT_LEFT, 500);
}

void CFileLockDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType == SIZE_MINIMIZED) return;
	ResizeControls();
}

void CFileLockDlg::ResizeControls()
{
	if (!m_list.GetSafeHwnd()) return;

	CRect rc;
	GetClientRect(&rc);

	const int margin = 7;

	// All controls except the list stay at their original RC positions.
	// Only the list control width adjusts when window is widened horizontally.
	int listTop = m_hintTop + m_hintHeight + 5;
	m_list.SetWindowPos(nullptr, margin, listTop,
		rc.Width() - 2 * margin, m_listHeight, SWP_NOZORDER);
}

void CFileLockDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	// Ensure window can't be shrunk below button row width + list min height
	const int mg = 7;
	int minBtnRowWidth = 5 * m_btnWidth + 4 * m_btnGap + 2 * mg;
	int minHeight = m_hintTop + m_hintHeight + 5 + 60 + mg + m_btnHeight + mg;

	if (lpMMI->ptMinTrackSize.x < minBtnRowWidth)
		lpMMI->ptMinTrackSize.x = minBtnRowWidth;
	if (lpMMI->ptMinTrackSize.y < minHeight)
		lpMMI->ptMinTrackSize.y = minHeight;

	CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CFileLockDlg::OnDropFiles(HDROP hDropInfo)
{
	UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	for (UINT i = 0; i < fileCount; i++)
	{
		TCHAR szPath[MAX_PATH];
		DragQueryFile(hDropInfo, i, szPath, MAX_PATH);
		AddFile(szPath);
	}
	DragFinish(hDropInfo);

	QueryFileLocks();
}

void CFileLockDlg::AddFile(const CString& filePath)
{
	// Check if already in the list
	for (const auto& e : m_entries)
	{
		if (e.filePath.CompareNoCase(filePath) == 0)
			return;
	}

	// Add a placeholder entry
	LockEntry entry;
	entry.filePath = filePath;
	entry.processName = _T("...");
	entry.pid = 0;
	entry.processPath.Empty();
	entry.appType = CLocalizationManager::GetInstance().GetString(_T("FileLock"), _T("AppTypeScanning"));
	m_entries.push_back(entry);
}

void CFileLockDlg::QueryFileLocks()
{
	auto& loc = CLocalizationManager::GetInstance();
	if (m_entries.empty()) return;

	// Collect unique file paths
	std::vector<CStringW> files;
	for (const auto& e : m_entries)
		files.push_back(CStringW(e.filePath));

	// Clear old results
	m_entries.clear();

	// Query each file individually via Restart Manager
	for (size_t fi = 0; fi < files.size(); fi++)
	{
		DWORD dwSession = 0;
		WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
		if (RmStartSession(&dwSession, 0, szSessionKey) != ERROR_SUCCESS)
			continue;

		PCWSTR singleFile = files[fi].GetString();
		RmRegisterResources(dwSession, 1, &singleFile, 0, nullptr, 0, nullptr);

		UINT nNeeded = 0, nGot = 0;
		RM_PROCESS_INFO piBuf[1];
		DWORD reason = 0;
		DWORD ret = RmGetList(dwSession, &nNeeded, &nGot, piBuf, &reason);

		if (ret == ERROR_MORE_DATA && nNeeded > 0)
		{
			std::vector<RM_PROCESS_INFO> piBuf2(nNeeded);
			nGot = nNeeded;
			ret = RmGetList(dwSession, &nNeeded, &nGot, piBuf2.data(), &reason);
			if (ret == ERROR_SUCCESS)
			{
				for (UINT pi = 0; pi < nGot; pi++)
				{
					LockEntry entry;
					entry.filePath = files[fi].GetString();
					entry.pid = piBuf2[pi].Process.dwProcessId;
					entry.processName = GetProcessName(entry.pid);
					entry.processPath = GetProcessPath(entry.pid);

					switch (piBuf2[pi].ApplicationType)
					{
					case RmMainWindow:  entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeMainWindow")); break;
					case RmOtherWindow: entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeOtherWindow")); break;
					case RmService:     entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeService")); break;
					case RmExplorer:    entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeExplorer")); break;
					case RmConsole:     entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeConsole")); break;
					case RmCritical:    entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeCritical")); break;
					default:            entry.appType = loc.GetString(_T("FileLock"), _T("AppTypeUnknown")); break;
					}

					if (piBuf2[pi].strAppName[0])
						entry.appType = piBuf2[pi].strAppName;

					m_entries.push_back(entry);
				}
			}
		}
		else if (ret == ERROR_SUCCESS && nGot > 0)
		{
			for (UINT pi = 0; pi < nGot; pi++)
			{
				LockEntry entry;
				entry.filePath = files[fi].GetString();
				entry.pid = piBuf[pi].Process.dwProcessId;
				entry.processName = GetProcessName(entry.pid);
				entry.processPath = GetProcessPath(entry.pid);
				entry.appType = piBuf[pi].strAppName[0] ? piBuf[pi].strAppName : loc.GetString(_T("FileLock"), _T("AppTypeLocked"));
				m_entries.push_back(entry);
			}
		}
		else
		{
			// No locks on this file, or file not found
			LockEntry entry;
			entry.filePath = files[fi].GetString();
			entry.processName = _T("-");
			entry.pid = 0;
			entry.appType = (ret == ERROR_SUCCESS) ? loc.GetString(_T("FileLock"), _T("AppTypeNotLocked")) : loc.GetString(_T("FileLock"), _T("AppTypeNotFound"));
			m_entries.push_back(entry);
		}

		RmEndSession(dwSession);
	}

	RefreshList();
}

void CFileLockDlg::RefreshList()
{
	auto& loc = CLocalizationManager::GetInstance();
	m_list.DeleteAllItems();

	for (size_t i = 0; i < m_entries.size(); i++)
	{
		const auto& e = m_entries[i];

		// Show only filename (not full path) in first column for readability
		CString fileName = PathFindFileName(e.filePath);

		int idx = m_list.InsertItem((int)i, fileName);
		m_list.SetItemText(idx, COL_PROCESS, e.processName);

		if (e.pid > 0)
		{
			CString pidStr;
			pidStr.Format(_T("%lu"), e.pid);
			m_list.SetItemText(idx, COL_PID, pidStr);
		}
		else
		{
			m_list.SetItemText(idx, COL_PID, _T("-"));
		}
		m_list.SetItemText(idx, COL_TYPE, e.appType);
		m_list.SetItemText(idx, COL_PATH, e.processPath);

		// Store full path as item data for context menu / locate
		m_list.SetItemData(idx, (DWORD_PTR)i);
	}

	// Update hint
	CString hint;
	hint.Format(loc.GetString(_T("FileLock"), _T("HintLoaded")), CountFilesInList(), CountLocks());
	SetDlgItemText(IDC_STATIC_FILELOCK_HINT, hint);
}

size_t CFileLockDlg::CountFilesInList()
{
	// Count unique file paths in entries
	std::set<CString> unique;
	for (const auto& e : m_entries)
		unique.insert(e.filePath);
	return unique.size();
}

size_t CFileLockDlg::CountLocks()
{
	size_t count = 0;
	for (const auto& e : m_entries)
		if (e.pid > 0) count++;
	return count;
}

CString CFileLockDlg::GetProcessName(DWORD pid)
{
	CString name;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProcess)
	{
		TCHAR szPath[MAX_PATH];
		DWORD cbPath = MAX_PATH;
		if (QueryFullProcessImageName(hProcess, 0, szPath, &cbPath))
		{
			name = PathFindFileName(szPath);
		}
		CloseHandle(hProcess);
	}

	if (name.IsEmpty())
	{
		// Fallback: use snapshot
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32 pe = { sizeof(pe) };
			if (Process32First(hSnap, &pe))
			{
				do {
					if (pe.th32ProcessID == pid)
					{
						name = pe.szExeFile;
						break;
					}
				} while (Process32Next(hSnap, &pe));
			}
			CloseHandle(hSnap);
		}
	}

	return name.IsEmpty() ? CLocalizationManager::GetInstance().GetString(_T("FileLock"), _T("AppTypeUnknown")) : name;
}

CString CFileLockDlg::GetProcessPath(DWORD pid)
{
	CString path;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProcess)
	{
		TCHAR szPath[MAX_PATH];
		DWORD cbPath = MAX_PATH;
		if (QueryFullProcessImageName(hProcess, 0, szPath, &cbPath))
		{
			path = szPath;
		}
		CloseHandle(hProcess);
	}
	return path;
}

void CFileLockDlg::EndProcess(DWORD pid)
{
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!hProcess)
	{
		// Try with lower privilege
		hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if (!hProcess) return;
	}
	TerminateProcess(hProcess, 1);
	CloseHandle(hProcess);
}

void CFileLockDlg::OnBnClickedEnd()
{
	auto& loc = CLocalizationManager::GetInstance();
	// Get selected items
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (!pos)
	{
		MessageBox(loc.GetString(_T("Msg"), _T("PleaseSelectProcess")), loc.GetString(_T("FileLock"), _T("BtnEnd")), MB_ICONINFORMATION);
		return;
	}

	// Collect selected PIDs
	std::vector<DWORD> pids;
	std::vector<CString> names;
	while (pos)
	{
		int idx = m_list.GetNextSelectedItem(pos);
		if (idx >= 0 && idx < (int)m_entries.size())
		{
			if (m_entries[idx].pid > 0)
			{
				pids.push_back(m_entries[idx].pid);
				names.push_back(m_entries[idx].processName);
			}
		}
	}

	if (pids.empty()) return;

	// Build warning message
	CString msg;
	msg.Format(loc.GetString(_T("Msg"), _T("ConfirmEndProcesses")), pids.size());
	for (size_t i = 0; i < pids.size() && i < 10; i++)
	{
		CString line;
		line.Format(_T("  %s (PID: %lu)\n"), names[i].GetString(), pids[i]);
		msg += line;
	}
	if (pids.size() > 10)
		msg.AppendFormat(loc.GetString(_T("Msg"), _T("EndMoreHint")), pids.size() - 10);
	msg += loc.GetString(_T("Msg"), _T("UnsavedDataWarning"));

	if (MessageBox(msg, loc.GetString(_T("Msg"), _T("Confirm")), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	for (DWORD pid : pids)
		EndProcess(pid);

	Sleep(500);
	QueryFileLocks();
}

void CFileLockDlg::OnBnClickedEndAll()
{
	auto& loc = CLocalizationManager::GetInstance();
	// Collect all PIDs with locks
	std::vector<DWORD> pids;
	for (const auto& e : m_entries)
	{
		if (e.pid > 0)
			pids.push_back(e.pid);
	}

	if (pids.empty())
	{
		MessageBox(loc.GetString(_T("Msg"), _T("NoLockFound")), loc.GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	// Deduplicate
	std::sort(pids.begin(), pids.end());
	pids.erase(std::unique(pids.begin(), pids.end()), pids.end());

	CString msg;
	msg.Format(loc.GetString(_T("Msg"), _T("ConfirmEndAllLocks")), pids.size());

	if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmEndAllTitle")), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	for (DWORD pid : pids)
		EndProcess(pid);

	Sleep(500);
	QueryFileLocks();
}

void CFileLockDlg::OnBnClickedLocate()
{
	auto& loc = CLocalizationManager::GetInstance();
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (!pos)
	{
		MessageBox(loc.GetString(_T("Msg"), _T("PleaseSelectProcess")), loc.GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	int idx = m_list.GetNextSelectedItem(pos);
	if (idx >= 0 && idx < (int)m_entries.size())
		LocateProcess(idx);
}

void CFileLockDlg::LocateProcess(int index)
{
	if (index < 0 || index >= (int)m_entries.size()) return;

	const auto& e = m_entries[index];

	if (e.pid <= 0) return;

	// Get process path if not already cached
	CString procPath = e.processPath;
	if (procPath.IsEmpty())
		procPath = GetProcessPath(e.pid);

	if (!procPath.IsEmpty())
	{
		// Open Explorer and select the file
		CString cmd;
		cmd.Format(_T("/select,\"%s\""), procPath.GetString());
		ShellExecute(nullptr, _T("open"), _T("explorer.exe"), cmd, nullptr, SW_SHOWNORMAL);
	}
}

void CFileLockDlg::OnBnClickedRefresh()
{
	QueryFileLocks();
}

void CFileLockDlg::OnBnClickedClear()
{
	m_entries.clear();
	m_list.DeleteAllItems();
	SetDlgItemText(IDC_STATIC_FILELOCK_HINT, CLocalizationManager::GetInstance().GetString(_T("FileLock"), _T("Hint")));
}

void CFileLockDlg::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	auto& loc = CLocalizationManager::GetInstance();
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	// If clicked item is not selected, select it first
	if (pNMListView->iItem >= 0)
	{
		if (!(m_list.GetItemState(pNMListView->iItem, LVIS_SELECTED) & LVIS_SELECTED))
		{
			m_list.SetItemState(-1, 0, LVIS_SELECTED);
			m_list.SetItemState(pNMListView->iItem, LVIS_SELECTED, LVIS_SELECTED);
		}
	}

	// Build context menu: Locate / End / Refresh / Clear
	CMenu menu;
	menu.CreatePopupMenu();

	bool hasSelection = (m_list.GetSelectedCount() > 0);
	bool hasLocks = false;
	if (hasSelection)
	{
		POSITION pos = m_list.GetFirstSelectedItemPosition();
		while (pos)
		{
			int idx = m_list.GetNextSelectedItem(pos);
			if (idx >= 0 && idx < (int)m_entries.size() && m_entries[idx].pid > 0)
			{
				hasLocks = true;
				break;
			}
		}
	}

	menu.AppendMenu(hasSelection ? MF_STRING : MF_STRING | MF_GRAYED,
		IDC_BTN_FILELOCK_LOCATE, loc.GetString(_T("FileLock"), _T("BtnLocate")));
	menu.AppendMenu(hasSelection && hasLocks ? MF_STRING : MF_STRING | MF_GRAYED,
		IDC_BTN_FILELOCK_END, loc.GetString(_T("FileLock"), _T("BtnEnd")));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(m_entries.empty() ? MF_STRING | MF_GRAYED : MF_STRING,
		IDC_BTN_FILELOCK_REFRESH, loc.GetString(_T("FileLock"), _T("BtnRefresh")));
	menu.AppendMenu(m_entries.empty() ? MF_STRING | MF_GRAYED : MF_STRING,
		IDC_BTN_FILELOCK_CLEAR, loc.GetString(_T("FileLock"), _T("BtnClear")));

	// Show at cursor position
	CPoint pt;
	GetCursorPos(&pt);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);

	*pResult = 0;
}

void CFileLockDlg::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if (pNMListView->iItem >= 0)
		LocateProcess(pNMListView->iItem);
	*pResult = 0;
}