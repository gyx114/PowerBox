// ContextMenuDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include <vector>
#include <set>
#include <map>
#include <string>

class CContextMenuDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CContextMenuDlg)

public:
	CContextMenuDlg(CWnd* pParent = nullptr);
	virtual ~CContextMenuDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONTEXT_MENU_DLG };
#endif

	struct MenuEntry
	{
		CString location;      // scene name (e.g. "文件 (*)")
		CString keyName;       // registry subkey name (verb or handler name / CLSID)
		CString displayName;   // resolved display text (MUIVerb/MUI/CLSID resolved)
		CString command;       // command line or DLL path
		CString regPath;       // HKCR-relative parent path (e.g. "*\\shell", "*\\shellex\\ContextMenuHandlers")
		bool   bIsShellEx;     // true: COM ShellEx handler, false: static verb
		bool   bExtended;      // true: Shift+right-click only (Extended subkey exists)
		bool   bDisabled;      // true: LegacyDisable / ProgrammaticAccessOnly / in -ContextMenuHandlers
		bool   bEnabled;       // true: currently enabled (not disabled by user)
	};

	// GuidInfo dictionary entry
	struct DictEntry { CString resText; CString zhText; CString text; };
	static std::map<CString, DictEntry> s_guidDict;
	static bool s_bDictLoaded;
	static CString s_dictPath;          // user-configured external dictionary path

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	// Scene definition matching ContextMenuManager's MENUPATH_* constants.
	// Each scene has a registry base path (e.g. "*", "Folder", "Directory\\Background").
	// ScanEntries scans both <basePath>\\shell (static verbs) and
	// <basePath>\\shellex (COM handlers, both ContextMenuHandlers and -ContextMenuHandlers).
	struct Scene
	{
		CString name;
		CString basePath;   // HKCR-relative base path (e.g. "*", "Folder", "Directory\\Background")
	};

	std::vector<MenuEntry> m_entries;
	std::vector<Scene> m_scenes;

	int m_listLeft, m_listTop;
	int m_statusTop;

	void InitLocations();
	void ScanEntries(const CString& filter);
	void ScanScene(const CString& basePath, const CString& sceneName, std::set<CString>& seen);
	static void ScanShellVerbs(const CString& shellPath, const CString& sceneName, std::vector<MenuEntry>& entries, std::set<CString>& seen);
	static void ScanShellExHandlers(const CString& shellexBase, const CString& sceneName, std::vector<MenuEntry>& entries, std::set<CString>& seen, const CString& scenePath = CString());
	static CString ResolveShellExKeyName(HKEY hHandlerKey, const CString& clsid, const CString& scenePath = CString());
	static bool IsRunningAsAdmin();
	static CString ResolveClsidName(const CString& clsid, const CString& scenePath = CString());
	static CString ResolveMUIString(const CString& raw);
	static void LoadGuidDictionary();
	static CString LookupGuidDict(const CString& clsid, const CString& scenePath = CString());
	void RefreshList();
	void UpdateStatus(const CString& text);
	void AdjustColumnWidths();
	bool DeleteRegistryKeyRecursive(HKEY hParent, const CString& subKey);
	void OpenRegEditToPath(HKEY hRoot, const CString& path);
	void LoadSelfContextMenuState();
	void SaveSelfContextMenuState(bool bEnable);
	void LoadWin11ClassicState();
	void SaveWin11ClassicState(bool bEnable);
	void ToggleEntry(int index, bool bRefresh = true);
	void RestartExplorer();
	void RebuildDictionary();            // COM-based: query real context menu names
	static CString GetCachePath();       // %AppData%\MFCApplication1\GuidInfosDic.cache.ini
	static CString GetUserOverridePath(); // user_override.ini path
	static CString GetConfigPath();      // <exe_dir>\config.ini
	bool MigrateDictionaryFiles(const CString& oldPath, const CString& newPath);
	bool InputCustomName(const CString& clsid, const CString& scenePath,
		const CString& currentName, CString& outName);
	bool SaveUserOverride(const CString& clsid, const CString& scenePath,
		const CString& customName);
	void OnBnClickedDictOpen();           // 打开字典目录

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedRefresh();
	afx_msg void OnCbnSelchangeLocation();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedLocate();
	afx_msg void OnBnClickedCheckFolder();
	afx_msg void OnBnClickedCheckWin11Classic();
	afx_msg void OnBnClickedRebuild();    // 重建字典
	afx_msg void OnBnClickedDictPath();   // 字典路径
	afx_msg void OnBnClickedExtension();  // 扩展名查询
	afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMenuDelete();
	afx_msg void OnMenuToggle();
	afx_msg void OnMenuLocate();
	afx_msg void OnMenuCustomParse();
};