// EnvVarDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "EnvVarDlg.h"
#include "LocalizationManager.h"
#include "afxdialogex.h"
#include <algorithm>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Local input dialog for add/edit environment variable name and value
class CEnvInputDlg : public CDialogEx
{
public:
	CString m_strName;
	CString m_strValue;
	bool m_bEditMode;
	CString m_strTitle;

	CEnvInputDlg(bool bEdit, const CString& title, CWnd* pParent)
		: CDialogEx(IDD_ENVVAR_INPUT_DLG, pParent), m_bEditMode(bEdit), m_strTitle(title) {}

protected:
	virtual BOOL OnInitDialog()
	{
		CDialogEx::OnInitDialog();
		SetWindowText(m_strTitle);
		SetDlgItemText(IDC_ENV_INPUT_NAME, m_strName);
		SetDlgItemText(IDC_ENV_INPUT_VALUE, m_strValue);
		if (m_bEditMode)
			GetDlgItem(IDC_ENV_INPUT_NAME)->EnableWindow(FALSE);
		return TRUE;
	}
	virtual void OnOK()
	{
		GetDlgItemText(IDC_ENV_INPUT_NAME, m_strName);
		GetDlgItemText(IDC_ENV_INPUT_VALUE, m_strValue);
		m_strName.Trim();
		m_strValue.Trim();
		if (m_strName.IsEmpty())
		{
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrNameEmpty")),
				CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Warning")), MB_ICONWARNING);
			return;
		}
		CDialogEx::OnOK();
	}
};

// Local PATH editor dialog for multi-value variables like PATH
class CEnvPathDlg : public CDialogEx
{
public:
	CString m_strValue;
	std::vector<CString> m_entries;
	CString m_strVarName;

	CEnvPathDlg(const CString& varName, CWnd* pParent)
		: CDialogEx(IDD_ENVVAR_PATH_DLG, pParent), m_strVarName(varName) {}

protected:
	virtual BOOL OnInitDialog()
	{
		CDialogEx::OnInitDialog();

		CString title;
		title.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("PathEditTitle")), m_strVarName);
		SetWindowText(title);

		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (pList)
		{
			pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
			pList->InsertColumn(0, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("PathListCol")), LVCFMT_LEFT, 800);
		}

		// Parse semicolon-separated value into entries
		int start = 0;
		while (start < m_strValue.GetLength())
		{
			int end = m_strValue.Find(_T(';'), start);
			if (end == -1) end = m_strValue.GetLength();
			CString entry = m_strValue.Mid(start, end - start);
			entry.Trim();
			if (!entry.IsEmpty())
				m_entries.push_back(entry);
			start = end + 1;
		}

		RefreshList();
		return TRUE;
	}

	virtual void OnOK()
	{
		// Join entries back into semicolon-separated string
		m_strValue.Empty();
		for (size_t i = 0; i < m_entries.size(); ++i)
		{
			if (i > 0) m_strValue += _T(";");
			m_strValue += m_entries[i];
		}
		CDialogEx::OnOK();
	}

	void RefreshList()
	{
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (!pList) return;
		pList->SetRedraw(FALSE);
		pList->DeleteAllItems();
		for (size_t i = 0; i < m_entries.size(); ++i)
			pList->InsertItem((int)i, m_entries[i]);
		pList->SetRedraw(TRUE);
	}

	int GetSelectedIndex()
	{
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (!pList) return -1;
		POSITION pos = pList->GetFirstSelectedItemPosition();
		if (!pos) return -1;
		return pList->GetNextSelectedItem(pos);
	}

	void OnAddEntry()
	{
		CString entry;
		CFolderPickerDialog dlg(nullptr, 0, this);
		dlg.m_ofn.lpstrTitle = CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("PathSelectFolder"));
		if (dlg.DoModal() == IDOK)
			entry = dlg.GetPathName();

		if (entry.IsEmpty()) return;

		int idx = GetSelectedIndex();
		if (idx >= 0 && idx < (int)m_entries.size())
			m_entries.insert(m_entries.begin() + idx + 1, entry);
		else
			m_entries.push_back(entry);

		RefreshList();
	}

	void OnEditEntry()
	{
		int idx = GetSelectedIndex();
		if (idx < 0 || idx >= (int)m_entries.size())
		{
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrSelectEdit")),
				CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
			return;
		}

		CFolderPickerDialog dlg(nullptr, 0, this);
		dlg.m_ofn.lpstrTitle = CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("PathSelectNewFolder"));
		if (dlg.DoModal() == IDOK)
		{
			m_entries[idx] = dlg.GetPathName();
			RefreshList();
		}
	}

	void OnDeleteEntry()
	{
		int idx = GetSelectedIndex();
		if (idx < 0 || idx >= (int)m_entries.size())
		{
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrSelectDelete")),
				CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
			return;
		}

		m_entries.erase(m_entries.begin() + idx);
		RefreshList();
	}

	void OnMoveUp()
	{
		int idx = GetSelectedIndex();
		if (idx <= 0 || idx >= (int)m_entries.size()) return;
		std::swap(m_entries[idx], m_entries[idx - 1]);
		RefreshList();
		// Reselect the moved item
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (pList)
		{
			pList->SetItemState(idx - 1, LVIS_SELECTED | LVIS_FOCUSED,
				LVIS_SELECTED | LVIS_FOCUSED);
		}
	}

	void OnMoveDown()
	{
		int idx = GetSelectedIndex();
		if (idx < 0 || idx >= (int)m_entries.size() - 1) return;
		std::swap(m_entries[idx], m_entries[idx + 1]);
		RefreshList();
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (pList)
		{
			pList->SetItemState(idx + 1, LVIS_SELECTED | LVIS_FOCUSED,
				LVIS_SELECTED | LVIS_FOCUSED);
		}
	}

	void OnCopyEntry()
	{
		int idx = GetSelectedIndex();
		if (idx < 0 || idx >= (int)m_entries.size()) return;

		if (OpenClipboard())
		{
			EmptyClipboard();
			int nLen = (m_entries[idx].GetLength() + 1) * sizeof(TCHAR);
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
			if (hMem)
			{
				memcpy(GlobalLock(hMem), m_entries[idx].GetString(), nLen);
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
			CloseClipboard();
		}
	}

	void OnRclickPathList(NMHDR* pNMHDR, LRESULT* pResult)
	{
		*pResult = 0;
		NMITEMACTIVATE* pNMItem = (NMITEMACTIVATE*)pNMHDR;
		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_PATH_ENTRIES);
		if (!pList) return;

		// Select the right-clicked item
		int nItem = pNMItem->iItem;
		if (nItem >= 0)
		{
			if (pList->GetItemState(nItem, LVIS_SELECTED) != LVIS_SELECTED)
			{
				for (int i = pList->GetItemCount() - 1; i >= 0; --i)
					pList->SetItemState(i, 0, LVIS_SELECTED);
				pList->SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
			}
		}

		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickEdit")));
		menu.AppendMenu(MF_STRING, 2, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickCopy")));
		menu.AppendMenu(MF_STRING, 3, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickDelete")));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, 4, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickMoveUp")));
		menu.AppendMenu(MF_STRING, 5, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickMoveDown")));

		CPoint pt;
		GetCursorPos(&pt);
		int nCmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
		menu.DestroyMenu();

		switch (nCmd)
		{
		case 1: OnEditEntry(); break;
		case 2: OnCopyEntry(); break;
		case 3: OnDeleteEntry(); break;
		case 4: OnMoveUp(); break;
		case 5: OnMoveDown(); break;
		}
	}

	afx_msg void OnCustomDrawPathList(NMHDR* pNMHDR, LRESULT* pResult)
	{
		NMLVCUSTOMDRAW* pLVCD = (NMLVCUSTOMDRAW*)pNMHDR;
		*pResult = CDRF_DODEFAULT;

		if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT)
		{
			*pResult = CDRF_NOTIFYITEMDRAW;
		}
		else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		{
			// Draw horizontal grid lines with a more visible color
			CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
			CRect rc = pLVCD->nmcd.rc;

			CPen pen(PS_SOLID, 1, RGB(160, 160, 160));
			CPen* pOldPen = pDC->SelectObject(&pen);

			pDC->MoveTo(rc.left, rc.bottom - 1);
			pDC->LineTo(rc.right, rc.bottom - 1);

			pDC->SelectObject(pOldPen);

			*pResult = CDRF_DODEFAULT;
		}
	}

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CEnvPathDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_PATH_ADD, &CEnvPathDlg::OnAddEntry)
	ON_BN_CLICKED(IDC_BTN_PATH_EDIT, &CEnvPathDlg::OnEditEntry)
	ON_BN_CLICKED(IDC_BTN_PATH_DELETE, &CEnvPathDlg::OnDeleteEntry)
	ON_BN_CLICKED(IDC_BTN_PATH_UP, &CEnvPathDlg::OnMoveUp)
	ON_BN_CLICKED(IDC_BTN_PATH_DOWN, &CEnvPathDlg::OnMoveDown)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_PATH_ENTRIES, &CEnvPathDlg::OnRclickPathList)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_PATH_ENTRIES, &CEnvPathDlg::OnCustomDrawPathList)
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CEnvVarDlg, CDialogEx)

CEnvVarDlg::CEnvVarDlg(CWnd* pParent)
	: CDialogEx(IDD_ENVVAR_DLG, pParent)
	, m_listSysLeft(0), m_listSysTop(0), m_listSysHeight(0)
	, m_listUserLeft(0), m_listUserTop(0), m_listUserHeight(0)
{
}

CEnvVarDlg::~CEnvVarDlg()
{
}

void CEnvVarDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEnvVarDlg, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BTN_ENV_ADD, &CEnvVarDlg::OnBnClickedEnvAdd)
	ON_BN_CLICKED(IDC_BTN_ENV_EDIT, &CEnvVarDlg::OnBnClickedEnvEdit)
	ON_BN_CLICKED(IDC_BTN_ENV_DELETE, &CEnvVarDlg::OnBnClickedEnvDelete)
	ON_BN_CLICKED(IDC_BTN_ENV_REFRESH, &CEnvVarDlg::OnBnClickedEnvRefresh)
	ON_BN_CLICKED(IDC_BTN_ENV_EXPORT, &CEnvVarDlg::OnBnClickedEnvExport)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_ENV_SYSTEM, &CEnvVarDlg::OnNMDblclkListEnv)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_ENV_USER, &CEnvVarDlg::OnNMDblclkListEnv)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_ENV_SYSTEM, &CEnvVarDlg::OnNMRclickListEnv)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_ENV_USER, &CEnvVarDlg::OnNMRclickListEnv)
	ON_EN_CHANGE(IDC_EDIT_ENV_SEARCH, &CEnvVarDlg::OnEnChangeEnvSearch)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_ENV_SYSTEM, &CEnvVarDlg::OnCustomDrawListEnv)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_ENV_USER, &CEnvVarDlg::OnCustomDrawListEnv)
END_MESSAGE_MAP()

BOOL CEnvVarDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
	{
		RefreshAll();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

// ========== Registry helpers ==========

bool CEnvVarDlg::ReadEnvVars(bool bSystem, std::vector<EnvVarEntry>& outVars)
{
	outVars.clear();

	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return false;

	DWORD dwIndex = 0;
	TCHAR szName[MAX_PATH];
	DWORD cbName = MAX_PATH;
	TCHAR szValue[65536];
	DWORD cbValue = sizeof(szValue);
	DWORD dwType = 0;

	while (RegEnumValue(hKey, dwIndex, szName, &cbName,
		nullptr, &dwType, (LPBYTE)szValue, &cbValue) == ERROR_SUCCESS)
	{
		if (dwType == REG_SZ || dwType == REG_EXPAND_SZ)
		{
			EnvVarEntry entry;
			entry.name = szName;
			entry.value = szValue;
			entry.bSystem = bSystem;
			outVars.push_back(entry);
		}

		dwIndex++;
		cbName = MAX_PATH;
		cbValue = sizeof(szValue);
	}

	RegCloseKey(hKey);

	std::sort(outVars.begin(), outVars.end(),
		[](const EnvVarEntry& a, const EnvVarEntry& b) {
			return _tcsicmp(a.name, b.name) < 0;
		});

	return true;
}

bool CEnvVarDlg::WriteEnvVar(const CString& name, const CString& value, bool bSystem)
{
	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return false;

	LONG lResult = RegSetValueEx(hKey, name, 0, REG_EXPAND_SZ,
		(const BYTE*)value.GetString(),
		(value.GetLength() + 1) * sizeof(TCHAR));

	RegCloseKey(hKey);

	if (lResult == ERROR_SUCCESS)
		BroadcastEnvChange();

	return lResult == ERROR_SUCCESS;
}

bool CEnvVarDlg::DeleteEnvVar(const CString& name, bool bSystem)
{
	HKEY hRoot = bSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	const TCHAR* szSubKey = bSystem
		? _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
		: _T("Environment");

	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, szSubKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return false;

	LONG lResult = RegDeleteValue(hKey, name);
	RegCloseKey(hKey);

	if (lResult == ERROR_SUCCESS)
		BroadcastEnvChange();

	return lResult == ERROR_SUCCESS;
}

void CEnvVarDlg::BroadcastEnvChange()
{
	SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
		(LPARAM)_T("Environment"), SMTO_ABORTIFHUNG, 5000, nullptr);
}

CString CEnvVarDlg::BackupEnvVars(const std::vector<EnvVarEntry>& sysVars,
	const std::vector<EnvVarEntry>& userVars)
{
	// Get temp directory
	TCHAR szTempPath[MAX_PATH];
	GetTempPath(MAX_PATH, szTempPath);

	// Generate timestamped filename
	SYSTEMTIME st;
	GetLocalTime(&st);
	CString filename;
	filename.Format(_T("%senv_backup_%04d%02d%02d_%02d%02d%02d.txt"),
		szTempPath, st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);

	std::ofstream ofs(filename.GetString(), std::ios::out | std::ios::trunc);
	if (!ofs.is_open()) return _T("");

	ofs << "# System Environment Variables\n";
	for (const auto& v : sysVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs << "\n# User Environment Variables\n";
	for (const auto& v : userVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs.close();
	return filename;
}

// ========== Dialog initialization ==========

BOOL CEnvVarDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("EnvVarDlg")));

	auto ReadRect = [&](int id) -> CRect {
		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(id);
		if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
		return rc;
	};

	CRect rcSys = ReadRect(IDC_LIST_ENV_SYSTEM);
	m_listSysLeft = rcSys.left; m_listSysTop = rcSys.top;
	m_listSysHeight = rcSys.Height();

	CRect rcUser = ReadRect(IDC_LIST_ENV_USER);
	m_listUserLeft = rcUser.left; m_listUserTop = rcUser.top;
	m_listUserHeight = rcUser.Height();

	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	if (pListSys)
	{
		pListSys->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
		pListSys->InsertColumn(0, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ColName")), LVCFMT_LEFT, 180);
		pListSys->InsertColumn(1, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ColValue")), LVCFMT_LEFT, 332);
	}

	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
	if (pListUser)
	{
		pListUser->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
		pListUser->InsertColumn(0, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ColName")), LVCFMT_LEFT, 180);
		pListUser->InsertColumn(1, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ColValue")), LVCFMT_LEFT, 332);
	}

	RefreshAll();

	return TRUE;
}

void CEnvVarDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

// ========== Layout ==========

void CEnvVarDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && IsWindow(m_hWnd))
	{
		// Both lists are stacked vertically, same x and width.
		// Only list width adjusts when window is widened horizontally.
		int nListWidth = cx - m_listSysLeft - 10;

		CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
		if (pListSys)
		{
			pListSys->SetWindowPos(nullptr, m_listSysLeft, m_listSysTop,
				nListWidth, m_listSysHeight, SWP_NOZORDER);
			AdjustColumnWidths(IDC_LIST_ENV_SYSTEM);
		}

		CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
		if (pListUser)
		{
			pListUser->SetWindowPos(nullptr, m_listUserLeft, m_listUserTop,
				nListWidth, m_listUserHeight, SWP_NOZORDER);
			AdjustColumnWidths(IDC_LIST_ENV_USER);
		}
	}
}

void CEnvVarDlg::AdjustColumnWidths(int listId)
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(listId);
	if (!pList || !pList->GetSafeHwnd()) return;

	CRect rcList;
	pList->GetClientRect(&rcList);
	int nWidth = rcList.Width();
	if (nWidth <= 0) return;

	pList->SetColumnWidth(0, nWidth * 35 / 100);
	pList->SetColumnWidth(1, nWidth * 65 / 100);
}

// ========== Data refresh ==========

void CEnvVarDlg::RefreshAll()
{
	ReadEnvVars(true, m_systemVars);
	ReadEnvVars(false, m_userVars);

	DoSearchFilter();
}

void CEnvVarDlg::PopulateList(int listId, const std::vector<EnvVarEntry>& vars)
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(listId);
	if (!pList) return;

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	for (size_t i = 0; i < vars.size(); ++i)
	{
		int idx = pList->InsertItem((int)i, vars[i].name);
		pList->SetItemText(idx, 1, vars[i].value);
	}

	pList->SetRedraw(TRUE);
}

void CEnvVarDlg::UpdateStatus(const CString& text)
{
	SetDlgItemText(IDC_STATIC_ENV_STATUS, text);
}

void CEnvVarDlg::OnEnChangeEnvSearch()
{
	GetDlgItemText(IDC_EDIT_ENV_SEARCH, m_strSearchFilter);
	DoSearchFilter();
}

void CEnvVarDlg::DoSearchFilter()
{
	FilterList(IDC_LIST_ENV_SYSTEM, m_systemVars);
	FilterList(IDC_LIST_ENV_USER, m_userVars);

	int nSysVisible = 0, nUserVisible = 0;
	CListCtrl* pSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
	if (pSys) nSysVisible = pSys->GetItemCount();
	if (pUser) nUserVisible = pUser->GetItemCount();

	if (m_strSearchFilter.IsEmpty())
	{
		CString status;
		status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusTotal")),
			(int)m_systemVars.size(), (int)m_userVars.size());
		UpdateStatus(status);
	}
	else
	{
		CString status;
		status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusSearch")),
			m_strSearchFilter, nSysVisible, (int)m_systemVars.size(),
			nUserVisible, (int)m_userVars.size());
		UpdateStatus(status);
	}
}

void CEnvVarDlg::FilterList(int listId, const std::vector<EnvVarEntry>& vars)
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(listId);
	if (!pList) return;

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	if (m_strSearchFilter.IsEmpty())
	{
		// No filter: show all
		for (size_t i = 0; i < vars.size(); ++i)
		{
			int idx = pList->InsertItem((int)i, vars[i].name);
			pList->SetItemText(idx, 1, vars[i].value);
		}
	}
	else
	{
		CString filter = m_strSearchFilter;
		filter.MakeLower();
		int nIdx = 0;
		for (size_t i = 0; i < vars.size(); ++i)
		{
			CString nameLower = vars[i].name;
			nameLower.MakeLower();
			CString valueLower = vars[i].value;
			valueLower.MakeLower();

			if (nameLower.Find(filter) >= 0 || valueLower.Find(filter) >= 0)
			{
				int idx = pList->InsertItem(nIdx++, vars[i].name);
				pList->SetItemText(idx, 1, vars[i].value);
			}
		}
	}

	pList->SetRedraw(TRUE);
}

void CEnvVarDlg::OnCustomDrawListEnv(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = (NMLVCUSTOMDRAW*)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
	}
	else if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		*pResult = CDRF_NOTIFYSUBITEMDRAW;
	}
	else if (pLVCD->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
	{
		// Draw cell borders with a more visible color
		CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
		CRect rc = pLVCD->nmcd.rc;

		// Darker grid lines: use dark gray instead of default light gray
		CPen pen(PS_SOLID, 1, RGB(160, 160, 160));
		CPen* pOldPen = pDC->SelectObject(&pen);

		// Draw right border
		pDC->MoveTo(rc.right - 1, rc.top);
		pDC->LineTo(rc.right - 1, rc.bottom);

		// Draw bottom border
		pDC->MoveTo(rc.left, rc.bottom - 1);
		pDC->LineTo(rc.right, rc.bottom - 1);

		pDC->SelectObject(pOldPen);

		*pResult = CDRF_DODEFAULT;
	}
}

// ========== Button handlers ==========

void CEnvVarDlg::OnBnClickedEnvAdd()     { OnAdd(); }
void CEnvVarDlg::OnBnClickedEnvEdit()    { OnEdit(); }
void CEnvVarDlg::OnBnClickedEnvDelete()  { OnDelete(); }
void CEnvVarDlg::OnBnClickedEnvRefresh() { RefreshAll(); UpdateStatus(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusRefreshed"))); }
void CEnvVarDlg::OnBnClickedEnvExport()  { OnExport(); }
void CEnvVarDlg::OnNMDblclkListEnv(NMHDR*, LRESULT* pResult) { *pResult = 0; OnEdit(); }

void CEnvVarDlg::OnNMRclickListEnv(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	NMITEMACTIVATE* pNMItem = (NMITEMACTIVATE*)pNMHDR;
	CListCtrl* pList = (CListCtrl*)GetDlgItem((int)pNMHDR->idFrom);
	if (!pList) return;

	// Select the right-clicked item if not already selected
	int nItem = pNMItem->iItem;
	if (nItem >= 0)
	{
		// If the clicked item is not selected, select only it
		if (pList->GetItemState(nItem, LVIS_SELECTED) != LVIS_SELECTED)
		{
			for (int i = pList->GetItemCount() - 1; i >= 0; --i)
				pList->SetItemState(i, 0, LVIS_SELECTED);
			pList->SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
		}
	}

	// Build popup menu
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickEdit")));
	menu.AppendMenu(MF_STRING, 2, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickDelete")));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, 3, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickCopyName")));
	menu.AppendMenu(MF_STRING, 4, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("RClickCopyValue")));

	CPoint pt;
	GetCursorPos(&pt);
	int nCmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
	menu.DestroyMenu();

	switch (nCmd)
	{
	case 1: OnEdit(); break;
	case 2: OnDelete(); break;
	case 3: OnCopyName(); break;
	case 4: OnCopyValue(); break;
	}
}

// ========== Helper: get selected item from either list ==========

static bool GetSelectedEnvVar(CWnd* pParent, int idSys, int idUser,
	CListCtrl*& pOutList, std::vector<CEnvVarDlg::EnvVarEntry>*& pOutVars, bool& bOutSystem, int& nOutIdx)
{
	pOutList = nullptr;
	pOutVars = nullptr;

	// Try system list first
	CListCtrl* pListSys = (CListCtrl*)pParent->GetDlgItem(idSys);
	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pOutList = pListSys;
		nOutIdx = pListSys->GetNextSelectedItem(pos);
		return true;  // bOutSystem stays false (caller must set it)
	}

	// Try user list
	CListCtrl* pListUser = (CListCtrl*)pParent->GetDlgItem(idUser);
	pos = pListUser->GetFirstSelectedItemPosition();
	if (pos)
	{
		pOutList = pListUser;
		nOutIdx = pListUser->GetNextSelectedItem(pos);
		return true;
	}

	return false;
}

// Find the actual data vector index by matching text from the (possibly filtered) list
static bool FindSelectedInData(CWnd* pParent, int listId,
	const std::vector<CEnvVarDlg::EnvVarEntry>& vars, int& outIdx)
{
	CListCtrl* pList = (CListCtrl*)pParent->GetDlgItem(listId);
	if (!pList) return false;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos) return false;

	int selIdx = pList->GetNextSelectedItem(pos);
	CString selName = pList->GetItemText(selIdx, 0);
	CString selValue = pList->GetItemText(selIdx, 1);

	for (int i = 0; i < (int)vars.size(); ++i)
	{
		if (vars[i].name == selName && vars[i].value == selValue)
		{
			outIdx = i;
			return true;
		}
	}
	return false;
}

// ========== Actions ==========

void CEnvVarDlg::OnAdd()
{
	auto& loc = CLocalizationManager::GetInstance();
	CString msg = loc.GetString(_T("EnvVar"), _T("ConfirmAddType"));
	int nRet = MessageBox(msg, loc.GetString(_T("EnvVar"), _T("DlgAddTitle")), MB_YESNOCANCEL | MB_ICONQUESTION);
	if (nRet == IDCANCEL) return;
	bool bSystem = (nRet == IDYES);

	CEnvInputDlg dlg(false, loc.GetString(_T("EnvVar"), _T("DlgAddTitle")), this);
	if (dlg.DoModal() != IDOK) return;

	// Backup before modification
	CString backupPath = BackupEnvVars(m_systemVars, m_userVars);

	if (!WriteEnvVar(dlg.m_strName, dlg.m_strValue, bSystem))
	{
		CString errMsg;
		errMsg.Format(loc.GetString(_T("EnvVar"), _T("ErrWriteFail")),
			dlg.m_strName);
		MessageBox(errMsg, loc.GetString(_T("EnvVar"), _T("DlgWriteFail")), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(loc.GetString(_T("EnvVar"), _T("StatusAdded")),
			bSystem ? loc.GetString(_T("EnvVar"), _T("SystemLabel")) : loc.GetString(_T("EnvVar"), _T("UserLabel")),
			dlg.m_strName, backupPath);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnEdit()
{
	CListCtrl* pList = nullptr;
	std::vector<EnvVarEntry>* pVars = nullptr;
	bool bSystem = false;
	int listId = 0;

	// Check system list
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pList = pListSys;
		pVars = &m_systemVars;
		bSystem = true;
		listId = IDC_LIST_ENV_SYSTEM;
	}
	else
	{
		CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos)
		{
			pList = pListUser;
			pVars = &m_userVars;
			bSystem = false;
			listId = IDC_LIST_ENV_USER;
		}
	}

	if (!pList || !pVars)
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrSelectVar")),
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	int idx = -1;
	if (!FindSelectedInData(this, listId, *pVars, idx)) return;

	const auto& entry = (*pVars)[idx];

	// Check if this is a PATH-like variable (semicolon-separated)
	bool bIsPathVar = (entry.name.CompareNoCase(_T("Path")) == 0 ||
		entry.name.CompareNoCase(_T("PATH")) == 0);

	if (bIsPathVar)
	{
		// Use specialized PATH editor
		CEnvPathDlg dlg(entry.name, this);
		dlg.m_strValue = entry.value;
		if (dlg.DoModal() != IDOK) return;

		// Backup before modification
		CString backupPath = BackupEnvVars(m_systemVars, m_userVars);

		if (!WriteEnvVar(entry.name, dlg.m_strValue, bSystem))
		{
			CString errMsg;
			errMsg.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrWriteFail")),
				entry.name);
			MessageBox(errMsg, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgWriteFail")), MB_ICONERROR);
		}
		else
		{
			RefreshAll();
			CString status;
			status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusUpdated")),
				bSystem ? CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("SystemLabel")) : CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("UserLabel")),
				entry.name, backupPath);
			UpdateStatus(status);
		}
		return;
	}

	// Generic input dialog for regular variables
	CEnvInputDlg dlg(true, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgEditTitle")), this);
	dlg.m_strName = entry.name;
	dlg.m_strValue = entry.value;
	if (dlg.DoModal() != IDOK) return;

	// Backup before modification
	CString backupPath = BackupEnvVars(m_systemVars, m_userVars);

	if (!WriteEnvVar(dlg.m_strName, dlg.m_strValue, bSystem))
	{
		CString errMsg;
		errMsg.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrWriteFail")),
			dlg.m_strName);
		MessageBox(errMsg, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgWriteFail")), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusUpdated")),
			bSystem ? CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("SystemLabel")) : CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("UserLabel")),
			dlg.m_strName, backupPath);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnDelete()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	std::vector<EnvVarEntry>* pVars = nullptr;
	bool bSystem = false;
	int listId = 0;

	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos)
	{
		pVars = &m_systemVars;
		bSystem = true;
		listId = IDC_LIST_ENV_SYSTEM;
	}
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos)
		{
			pVars = &m_userVars;
			bSystem = false;
			listId = IDC_LIST_ENV_USER;
		}
	}

	if (!pVars)
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrSelectVarDel")),
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	int idx = -1;
	if (!FindSelectedInData(this, listId, *pVars, idx)) return;

	const auto& entry = (*pVars)[idx];

	CString msg;
	msg.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ConfirmDeleteVar")),
		bSystem ? CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("SystemLabel")) : CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("UserLabel")),
		entry.name);
	if (MessageBox(msg, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgConfirmDelete")), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	// Backup before modification
	CString backupPath = BackupEnvVars(m_systemVars, m_userVars);

	if (!DeleteEnvVar(entry.name, bSystem))
	{
		CString errMsg;
		errMsg.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrDeleteFail")),
			entry.name);
		MessageBox(errMsg, CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgDeleteFail")), MB_ICONERROR);
	}
	else
	{
		RefreshAll();
		CString status;
		status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusDeleted")),
			bSystem ? CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("SystemLabel")) : CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("UserLabel")),
			entry.name, backupPath);
		UpdateStatus(status);
	}
}

void CEnvVarDlg::OnCopyName()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	std::vector<EnvVarEntry>* pVars = nullptr;
	int listId = 0;
	int idx = -1;

	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos) { pVars = &m_systemVars; listId = IDC_LIST_ENV_SYSTEM; }
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos) { pVars = &m_userVars; listId = IDC_LIST_ENV_USER; }
	}

	if (!pVars || !FindSelectedInData(this, listId, *pVars, idx)) return;

	const auto& entry = (*pVars)[idx];

	if (OpenClipboard())
	{
		EmptyClipboard();
		int nLen = (entry.name.GetLength() + 1) * sizeof(TCHAR);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), entry.name.GetString(), nLen);
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
	CString st;
	st.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusCopiedName")), entry.name);
	UpdateStatus(st);
}

void CEnvVarDlg::OnCopyValue()
{
	CListCtrl* pListSys = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_SYSTEM);
	CListCtrl* pListUser = (CListCtrl*)GetDlgItem(IDC_LIST_ENV_USER);

	std::vector<EnvVarEntry>* pVars = nullptr;
	int listId = 0;
	int idx = -1;

	POSITION pos = pListSys->GetFirstSelectedItemPosition();
	if (pos) { pVars = &m_systemVars; listId = IDC_LIST_ENV_SYSTEM; }
	else
	{
		pos = pListUser->GetFirstSelectedItemPosition();
		if (pos) { pVars = &m_userVars; listId = IDC_LIST_ENV_USER; }
	}

	if (!pVars || !FindSelectedInData(this, listId, *pVars, idx)) return;

	const auto& entry = (*pVars)[idx];

	if (OpenClipboard())
	{
		EmptyClipboard();
		int nLen = (entry.value.GetLength() + 1) * sizeof(TCHAR);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
		if (hMem)
		{
			memcpy(GlobalLock(hMem), entry.value.GetString(), nLen);
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
	}
	CString st;
	st.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusCopiedValue")), entry.value);
	UpdateStatus(st);
}

void CEnvVarDlg::OnExport()
{
	CFileDialog dlg(FALSE, _T(".txt"), _T("env_vars.txt"),
		OFN_OVERWRITEPROMPT,
		CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ExportFilter")),
		this);

	if (dlg.DoModal() != IDOK) return;

	CString filePath = dlg.GetPathName();

	std::ofstream ofs(filePath.GetString(), std::ios::out | std::ios::trunc);
	if (!ofs.is_open())
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("ErrCreateFile")),
			CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("DlgExportFail")), MB_ICONERROR);
		return;
	}

	ofs << "# System Environment Variables\n";
	for (const auto& v : m_systemVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs << "\n# User Environment Variables\n";
	for (const auto& v : m_userVars)
	{
		CStringA nameA(v.name);
		CStringA valueA(v.value);
		ofs << nameA.GetString() << "=" << valueA.GetString() << "\n";
	}

	ofs.close();

	CString status;
	status.Format(CLocalizationManager::GetInstance().GetString(_T("EnvVar"), _T("StatusExported")), filePath);
	UpdateStatus(status);
}