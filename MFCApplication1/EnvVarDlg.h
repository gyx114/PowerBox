// EnvVarDlg.h: Environment variable management dialog
#pragma once
#include "afxdialogex.h"
#include <vector>

class CEnvVarDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CEnvVarDlg)

public:
	CEnvVarDlg(CWnd* pParent = nullptr);
	virtual ~CEnvVarDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ENVVAR_DLG };
#endif

	struct EnvVarEntry
	{
		CString name;
		CString value;
		bool bSystem;  // true: system variable, false: user variable
	};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()

private:
	std::vector<EnvVarEntry> m_systemVars;
	std::vector<EnvVarEntry> m_userVars;
	CString m_strSearchFilter;

	// Layout anchors for resize (only list width adjusts on horizontal resize)
	int m_listSysLeft, m_listSysTop, m_listSysHeight;
	int m_listUserLeft, m_listUserTop, m_listUserHeight;

	void RefreshAll();
	void RefreshList(int listId, const std::vector<EnvVarEntry>& vars);
	void PopulateList(int listId, const std::vector<EnvVarEntry>& vars);
	void AdjustColumnWidths(int listId);
	void UpdateStatus(const CString& text);
	void DoSearchFilter();
	void FilterList(int listId, const std::vector<EnvVarEntry>& vars);

	// Registry helpers
	static bool ReadEnvVars(bool bSystem, std::vector<EnvVarEntry>& outVars);
	static bool WriteEnvVar(const CString& name, const CString& value, bool bSystem);
	static bool DeleteEnvVar(const CString& name, bool bSystem);
	static void BroadcastEnvChange();
	static CString BackupEnvVars(const std::vector<EnvVarEntry>& sysVars,
		const std::vector<EnvVarEntry>& userVars);

	// Actions
	void OnAdd();
	void OnEdit();
	void OnDelete();
	void OnCopyName();
	void OnCopyValue();
	void OnExport();

	afx_msg void OnBnClickedEnvAdd();
	afx_msg void OnBnClickedEnvEdit();
	afx_msg void OnBnClickedEnvDelete();
	afx_msg void OnBnClickedEnvRefresh();
	afx_msg void OnBnClickedEnvExport();
	afx_msg void OnEnChangeEnvSearch();
	afx_msg void OnNMDblclkListEnv(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRclickListEnv(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDrawListEnv(NMHDR* pNMHDR, LRESULT* pResult);
};