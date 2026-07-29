// MFCApplication1Dlg.h: header file
//

#pragma once

#include <vector>
#include <thread>
#include <stop_token>
#include <string>
#include <memory>
#include <utility>
#include "AutoClicker.h"
#include "AutoClickerSpeedDlg.h"
#include "AIApiClient.h"

// Forward declarations for menu-launched dialogs
class CQRCodeGenDlg;
class CScreenshotOCRDlg;
class CBatchRenameDlg;
class CStickyNoteDlg;
class CContextMenuDlg;
class CEnvVarDlg;

// Semantic button aliases
#define IDC_BTN_SHUTDOWN       IDC_BUTTON1   // Shutdown/Restart
#define IDC_BTN_CANCEL_SHUTDOWN IDC_BUTTON2  // Cancel shutdown
#define IDC_BTN_MAKE_COPY      IDC_BUTTON3   // Make copy
#define IDC_BTN_WECHAT         IDC_BUTTON4   // Launch WeChat
#define IDC_BTN_QQ             IDC_BUTTON5   // Launch QQ
#define IDC_BTN_VSCODE         IDC_BUTTON6   // Launch VS Code
#define IDC_BTN_VS             IDC_BUTTON7   // Launch Visual Studio
#define IDC_BTN_BILIBILI       IDC_BUTTON8   // Launch Bilibili
#define IDC_BTN_STUDY          IDC_BUTTON9   // Open study folder
#define IDC_BTN_LOCAL_SERVER   IDC_BUTTON10  // Open local server
#define IDC_BTN_MOOC           IDC_BUTTON11  // Open China University MOOC
#define IDC_BTN_VOLUME_APPLY   IDC_BUTTON12  // Apply volume
#define IDC_BTN_VOLUME_0       IDC_BUTTON13  // Set volume to 0
#define IDC_BTN_VOLUME_10      IDC_BUTTON14  // Set volume to 10
#define IDC_BTN_RUN_CMD        IDC_BUTTON17  // Run command
#define IDC_BTN_CLEAR_CMD      IDC_BUTTON18  // Clear command
#define IDC_BTN_LOCATE         IDC_BUTTON19  // Window locate/topmost
#define IDC_BTN_TASK_MGR       IDC_BUTTON20  // Open Task Manager
#define IDC_BTN_DOWNLOAD       IDC_BUTTON21  // Open downloads folder
#define IDC_BTN_YUANBAO        IDC_BUTTON22  // Launch Yuanbao
#define IDC_BTN_RENAME_FILE    IDC_BUTTON23  // Rename file
#define IDC_BTN_DELETE_FILE    IDC_BUTTON24  // Delete file
#define IDC_BTN_OPEN_FOLDER    IDC_BUTTON25  // Open folder
#define IDC_BTN_COPY_FILE      IDC_BUTTON26  // Copy file
#define IDC_BTN_POWERSHELL     IDC_BUTTON27  // Launch PowerShell
#define IDC_BTN_WSL            IDC_BUTTON28  // Launch WSL
#define IDC_BTN_LEETCODE       IDC_BUTTON29  // Open LeetCode
#define IDC_BTN_GITHUB         IDC_BUTTON30  // Open GitHub
#define IDC_BTN_GIT_BASH       IDC_BUTTON31  // Launch Git Bash
#define IDC_BTN_CLEAR_PATH     IDC_BUTTON32  // Clear drag-drop path
#define IDC_BTN_BILI_NEXT      IDC_BUTTON33  // Bilibili next track

// CMFCApplication1Dlg dialog
class CMFCApplication1Dlg : public CDialogEx
{
// Construction
public:
	CMFCApplication1Dlg(CWnd* pParent = nullptr);	// Standard constructor
	virtual ~CMFCApplication1Dlg();

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	[[nodiscard]] void RefreshProcessList();

// Dialog data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCAPPLICATION1_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Initialization helpers
	void InitTabControl();
	void InitProcessTab();
	void InitStartupTab();
	void InitClipboardTab();
	void InitWindowTab();
	void InitFileTab();
	void InitGitTab();
	void InitQuickTab();
	void UpdateTabVisibility(int nTab);
	void UpdateQuickTab(int nTab);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTcnSelchangeQuickTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillProcess();
	afx_msg void OnKillSameName();
	afx_msg void OnRclickProcessList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLocateProcess();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnCbnSelchangeCombo1();
   // Startup item management
	void RefreshStartupList();
	afx_msg void OnAddStartup();
	afx_msg void OnRemoveStartup();

    // (File management features removed)

	// File management: drag-drop file path and make copy button
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
	afx_msg void OnBnClickedButton3();
	CString m_strDroppedFilePath; // Stored drag-drop file path
public:
	CString m_strInitialFolder;   // Folder path passed via command line (context menu)
protected:



	// Auto-start with Windows checkbox
	afx_msg void OnBnClickedCheck1();

	// System tray
	afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual void OnClose();
	afx_msg void OnDestroy();

	afx_msg void OnTrayShowWindow();
	afx_msg void OnTrayExit();
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);

	NOTIFYICONDATA m_nid{};
	bool m_bTrayVisible{false};
    bool m_bExiting{false};
    bool m_bMinimizeOnClose{true};
	static constexpr UINT WM_TRAYICON = WM_APP + 1;

	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnStnClickedStaticPath();
	afx_msg void OnBnClickedCheck2();
  // volume controls
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedButton12(); // apply edit value
	afx_msg void OnBnClickedButton13(); // set 0
	afx_msg void OnBnClickedButton14(); // set 60
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnBnClickedButton7();
    afx_msg void OnBnClickedButton9();
	afx_msg void OnBnClickedButton8();
	afx_msg void OnBnClickedButton10();

	afx_msg void OnBnClickedButton11();
 afx_msg void OnBnClickedButton20();
	afx_msg void OnBnClickedButton17();
	afx_msg void OnBnClickedButton18();
	afx_msg void OnBnClickedCheck4();
	afx_msg LRESULT OnAutoClickStopped(WPARAM wParam, LPARAM lParam);

	// Clipboard manager: recent copy history (double-click to re-copy)
	afx_msg LRESULT OnClipboardUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnNMDblclkList3(NMHDR* pNMHDR, LRESULT* pResult);

	std::vector<CString> m_clipHistory;
	static constexpr int CLIP_HISTORY_MAX = 10;

    // custom messages for background refresh completion
	static constexpr UINT WM_REFRESH_PROCESSES_DONE = WM_APP + 2;
	static constexpr UINT WM_REFRESH_STARTUPS_DONE = WM_APP + 3;
	// custom message for async volume update
	static constexpr UINT WM_VOLUME_UPDATED = WM_APP + 5;

    struct ProcInfo { CString name; DWORD pid; CString path; SIZE_T memKB; };
	struct StartupInfo { CString name; CString cmd; };

    // Process list: store raw data for sorting and filtering
    std::vector<ProcInfo> m_processes;
    int m_nSortColumn{-1};
    bool m_bSortAscending{true};

    void ApplyProcessFilter();
    void SortProcessList();
    void PopulateProcessList();

	afx_msg LRESULT OnRefreshProcessesDone(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRefreshStartupsDone(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedCheck3();
	afx_msg void OnBnClickedCheck5();

	afx_msg LRESULT OnVolumeUpdated(WPARAM wParam, LPARAM lParam);

    // Window locate / topmost controls
	afx_msg void OnBnClickedButton19(); // Start/cancel locate or cancel current topmost

	// Called by capture overlay when user selects a target window
	void OnTargetSelected(HWND hTarget, POINT pt);

    HWND m_hCaptureWnd{nullptr};
	HWND m_hSelectedWnd{nullptr};
	std::vector<HWND> m_topmostWnds;       // Topmost window list
	std::vector<HWND> m_historyWnds;       // History located window list

	// Window features: transparency, force kill, screenshot
	afx_msg void OnForceKillProcess();
	afx_msg void OnWindowScreenshot();

	// Menu command handlers
	afx_msg void OnViewProcess();
	afx_msg void OnViewStartup();
	afx_msg void OnViewClipboard();
	afx_msg void OnViewWindow();
	afx_msg void OnViewFile();
	afx_msg void OnViewGit();
	afx_msg void OnWindowLocate();
	afx_msg void OnWindowUntopmost();
	afx_msg void OnWindowClose();
	afx_msg void OnUntopmostWindow();   // LIST6 right-click cancel topmost
	afx_msg void OnCopyStartupPath();     // LIST2 right-click copy path
	afx_msg void OnNMDblclkList2(NMHDR* pNMHDR, LRESULT* pResult);  // LIST2 double-click copy
	afx_msg void OnClickList6(NMHDR* pNMHDR, LRESULT* pResult);       // LIST6 click load details
	afx_msg void OnClickList7(NMHDR* pNMHDR, LRESULT* pResult);       // LIST7 click load details
	afx_msg void OnDeleteList6Record();                                // LIST6 right-click delete
	afx_msg void OnDeleteList7Record();                                // LIST7 right-click delete
	afx_msg void OnTopmostFromHistory();                               // LIST7 right-click topmost
	afx_msg void OnUntopmostFromHistory();                             // LIST7 right-click cancel topmost
	afx_msg void OnHelpShortcuts();
	afx_msg void OnHelpGithub();
	afx_msg void OnHelpRegexGuide();

	// UI: track whether file management tab exists (index 4)
	int m_fileTabIndex = 4;

	// Auto-clicker state (C++20: encapsulated in CAutoClicker class)
	CAutoClicker m_autoClicker;
	static constexpr UINT WM_AUTOCLICK_STOPPED = CAutoClicker::WM_STOPPED;
	static constexpr UINT WM_SPEED_DLG_CLOSED = WM_APP + 6;

	// Auto-clicker speed adjustment dialog (topmost)
	std::unique_ptr<CAutoClickerSpeedDlg> m_pSpeedDlg;

	// Speed dialog close callback
	afx_msg LRESULT OnSpeedDlgClosed(WPARAM wParam, LPARAM lParam);

	// Auto-clicker unified start/stop
	void StartAutoClicker();
	void StopAutoClicker();

	// Prevent automatic lock/screen-off checkbox state (IDC_CHECK5)
	bool m_bPreventLockScreen{false};

	// Load window details into LIST5
	void LoadWindowDetailToList5(HWND hWnd);

	// background worker thread for volume retrieval; ensure joined on destroy
	afx_msg void OnBnClickedButton21();
	afx_msg void OnBnClickedButton22();
    afx_msg void OnBnClickedButton23();
    afx_msg void OnBnClickedButton24();
    afx_msg void OnBnClickedButton25();
    afx_msg void OnBnClickedButton26();
    // OnBnClickedButton27 removed
	afx_msg void OnBnClickedCheck6();
	afx_msg void OnBnClickedButton27();
	afx_msg void OnBnClickedButton28();
	afx_msg void OnBnClickedButton29();
  afx_msg void OnBnClickedButton30();
	afx_msg void OnBnClickedButton31();
    afx_msg void OnBnClickedButton32();
    afx_msg void OnBiliNext();
    // Process list sorting and filtering
    afx_msg void OnProcessColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnProcessFilterChange();
    afx_msg void OnProcessRegexHelp();
    // Git list handlers
	afx_msg void OnCopyGitCommand();
	afx_msg void OnNMDblclkList4(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLbnDblclkList4();
    afx_msg void OnNMDblclkList5(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFileSettings();
    afx_msg void OnFileExit();
    afx_msg void OnHelpAbout();
    afx_msg void OnToolsQRCode();
    afx_msg void OnToolsScreenshotOCR();
    afx_msg void OnToolsBatchRename();
    afx_msg void OnToolsStickyNote();
    afx_msg void OnToolsMarkdown();
    afx_msg void OnToolsEncoding();
	afx_msg void OnToolsContextMenu();
	afx_msg void OnToolsEnvVar();
	afx_msg void OnToolsFileLock();

    // Sticky note dialog (modeless, auto-opened on startup)
    CStickyNoteDlg* m_pStickyNoteDlg{nullptr};

    // AI Assistant
    std::vector<std::pair<CString, CString>> m_aiHistory; // (role, content) pairs
    CWnd m_aiBrowser;                     // WebBrowser ActiveX for Markdown rendering
    bool m_aiBrowserReady{false};         // true when WebBrowser document is ready
    CString m_aiPendingHtml;              // buffered HTML before browser is ready
    CString m_aiStreamingContent;         // accumulated streaming content
    CString BuildSystemPrompt();
    void InitAIControls();
    CString BuildAiHtmlPage(const CString& bodyContent);
    bool SetAiBrowserHtml(const CString& html);
    afx_msg void OnBnClickedAiSend();
    afx_msg void OnBnClickedAiClear();
    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamChunk(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamDone(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
};