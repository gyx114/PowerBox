// FileLockDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include <vector>
#include <map>

class CFileLockDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CFileLockDlg)

public:
	CFileLockDlg(CWnd* pParent = nullptr);
	virtual ~CFileLockDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILELOCK_DLG };
#endif

	struct LockEntry
	{
		CString filePath;
		CString processName;
		DWORD   pid;
		CString processPath;
		CString appType;
	};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	CListCtrl m_list;
	std::vector<LockEntry> m_entries;
	CString m_hintText;

	// Layout anchors (read from actual controls at init, DPI-safe)
	int m_hintLeft, m_hintTop, m_hintHeight;
	int m_btnWidth, m_btnHeight, m_btnGap;

	void InitListColumns();
	void QueryFileLocks();
	void AddFile(const CString& filePath);
	void RefreshList();
	void EndProcess(DWORD pid);
	void LocateProcess(int index);
	CString GetProcessPath(DWORD pid);
	CString GetProcessName(DWORD pid);
	size_t CountFilesInList();
	size_t CountLocks();
	void ResizeControls();

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedEnd();
	afx_msg void OnBnClickedEndAll();
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnBnClickedClear();
	afx_msg void OnBnClickedLocate();
	afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
};