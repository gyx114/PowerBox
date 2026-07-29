// MFCApplication1Dlg.cpp: Implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "afxdialogex.h"
#include "Utils.h"
#include "AutoClicker.h"
#include "VolumeManager.h"
#include "ProcessManager.h"
#include "AboutDlg.h"
#include "AutoClickerSpeedDlg.h"
#include "SettingsDlg.h"
#include "MarkdownDlg.h"
#include "QRCodeGenDlg.h"
#include "ScreenshotOCRDlg.h"
#include "BatchRenameDlg.h"
#include "RegexGuideDlg.h"
#include "StickyNoteDlg.h"
#include "EncodingConverterDlg.h"
#include "ContextMenuDlg.h"
#include "EnvVarDlg.h"
#include "FileLockDlg.h"
#include <TlHelp32.h>
#include <Shellapi.h>
#include <Psapi.h>
#include <afxdlgs.h>
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <stdio.h>
#include <string>
#include <afxole.h>
#pragma comment(lib, "Ole32.lib")

#include <windows.h>
#include <processthreadsapi.h>
#include <userenv.h>
#pragma comment(lib, "Userenv.lib")

// capture overlay is implemented via a window class registered at runtime

// Bilibili-specific window-finding helpers removed per user request.





// File management (drag/drop) and DropHelper removed

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Define message constants declared in header (now constexpr in header, so no need to redefine here)
// Autoclicker message is defined in the dialog header as WM_AUTOCLICK_STOPPED

// Autoclicker functionality moved to CAutoClicker class (AutoClicker.h/cpp)

// Master volume helpers using Core Audio EndpointVolume (0-100)
// Async volume retrieval: run in background and post WM_VOLUME_UPDATED with percent in WPARAM


// Next-track feature removed



// CMFCApplication1Dlg dialog



CMFCApplication1Dlg::CMFCApplication1Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCAPPLICATION1_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    m_bTrayVisible = false;
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_bExiting = false;
    m_bMinimizeOnClose = true; // Checked by default

    m_hCaptureWnd = NULL;
    m_hSelectedWnd = nullptr;
    m_fileTabIndex = 4;
    m_bPreventLockScreen = false;
}

CMFCApplication1Dlg::~CMFCApplication1Dlg()
{
	// Last-resort save: called when dialog object is destroyed
	if (m_pStickyNoteDlg && ::IsWindow(m_pStickyNoteDlg->m_hWnd))
	{
		m_pStickyNoteDlg->SaveIfNeeded();
	}
}

void CMFCApplication1Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMFCApplication1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, &CMFCApplication1Dlg::OnTcnSelchangeTab1)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_QUICK, &CMFCApplication1Dlg::OnTcnSelchangeQuickTab)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(32771, &CMFCApplication1Dlg::OnKillProcess)
    ON_COMMAND(IDM_KILL_SAME_NAME, &CMFCApplication1Dlg::OnKillSameName)
    ON_NOTIFY(NM_RCLICK, IDC_LIST1, &CMFCApplication1Dlg::OnRclickProcessList)
    ON_COMMAND(32772, &CMFCApplication1Dlg::OnAddStartup)
    ON_COMMAND(32773, &CMFCApplication1Dlg::OnRemoveStartup)
    ON_COMMAND(32774, &CMFCApplication1Dlg::OnLocateProcess)
    ON_COMMAND(32805, &CMFCApplication1Dlg::OnUntopmostWindow)
    ON_COMMAND(32806, &CMFCApplication1Dlg::OnCopyStartupPath)
    ON_COMMAND(32807, &CMFCApplication1Dlg::OnDeleteList6Record)
    ON_COMMAND(32808, &CMFCApplication1Dlg::OnDeleteList7Record)
    ON_COMMAND(32809, &CMFCApplication1Dlg::OnTopmostFromHistory)
    ON_COMMAND(32810, &CMFCApplication1Dlg::OnUntopmostFromHistory)
    ON_BN_CLICKED(IDC_BUTTON1, &CMFCApplication1Dlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CMFCApplication1Dlg::OnBnClickedButton2)
    ON_CBN_SELCHANGE(IDC_COMBO1, &CMFCApplication1Dlg::OnCbnSelchangeCombo1)
    ON_WM_DROPFILES()
    ON_WM_COPYDATA()
    ON_BN_CLICKED(IDC_BUTTON3, &CMFCApplication1Dlg::OnBnClickedButton3)
    ON_BN_CLICKED(IDC_BUTTON4, &CMFCApplication1Dlg::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_CHECK1, &CMFCApplication1Dlg::OnBnClickedCheck1)
    ON_MESSAGE(WM_TRAYICON, &CMFCApplication1Dlg::OnTrayNotification)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_COMMAND(2001, &CMFCApplication1Dlg::OnTrayShowWindow)
    ON_COMMAND(2002, &CMFCApplication1Dlg::OnTrayExit)
    ON_STN_CLICKED(IDC_STATIC_PATH, &CMFCApplication1Dlg::OnStnClickedStaticPath)
    ON_BN_CLICKED(IDC_CHECK2, &CMFCApplication1Dlg::OnBnClickedCheck2)
    ON_BN_CLICKED(IDC_BUTTON5, &CMFCApplication1Dlg::OnBnClickedButton5)
    ON_BN_CLICKED(IDC_BUTTON6, &CMFCApplication1Dlg::OnBnClickedButton6)
    ON_BN_CLICKED(IDC_BUTTON7, &CMFCApplication1Dlg::OnBnClickedButton7)
    ON_BN_CLICKED(IDC_BUTTON8, &CMFCApplication1Dlg::OnBnClickedButton8)
    ON_BN_CLICKED(IDC_BUTTON9, &CMFCApplication1Dlg::OnBnClickedButton9)
    ON_BN_CLICKED(IDC_BUTTON10, &CMFCApplication1Dlg::OnBnClickedButton10)
    ON_BN_CLICKED(IDC_BUTTON11, &CMFCApplication1Dlg::OnBnClickedButton11)
    ON_BN_CLICKED(IDC_BUTTON20, &CMFCApplication1Dlg::OnBnClickedButton20)
    ON_BN_CLICKED(IDC_BUTTON12, &CMFCApplication1Dlg::OnBnClickedButton12)
    ON_BN_CLICKED(IDC_BUTTON13, &CMFCApplication1Dlg::OnBnClickedButton13)
    ON_BN_CLICKED(IDC_BUTTON14, &CMFCApplication1Dlg::OnBnClickedButton14)
    ON_WM_HSCROLL()
    ON_MESSAGE(WM_REFRESH_PROCESSES_DONE, &CMFCApplication1Dlg::OnRefreshProcessesDone)
    ON_MESSAGE(WM_REFRESH_STARTUPS_DONE, &CMFCApplication1Dlg::OnRefreshStartupsDone)
    ON_WM_HOTKEY()
    ON_BN_CLICKED(IDC_CHECK3, &CMFCApplication1Dlg::OnBnClickedCheck3)
    ON_BN_CLICKED(IDC_CHECK4, &CMFCApplication1Dlg::OnBnClickedCheck4)
    ON_BN_CLICKED(IDC_CHECK5, &CMFCApplication1Dlg::OnBnClickedCheck5)
    ON_BN_CLICKED(IDC_BUTTON17, &CMFCApplication1Dlg::OnBnClickedButton17)
    ON_BN_CLICKED(IDC_BUTTON18, &CMFCApplication1Dlg::OnBnClickedButton18)
    ON_MESSAGE(WM_CLIPBOARDUPDATE, &CMFCApplication1Dlg::OnClipboardUpdate)
    ON_MESSAGE(CMFCApplication1Dlg::WM_AUTOCLICK_STOPPED, &CMFCApplication1Dlg::OnAutoClickStopped)
    ON_MESSAGE(CMFCApplication1Dlg::WM_SPEED_DLG_CLOSED, &CMFCApplication1Dlg::OnSpeedDlgClosed)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST3, &CMFCApplication1Dlg::OnNMDblclkList3)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST2, &CMFCApplication1Dlg::OnNMDblclkList2)
    ON_NOTIFY(NM_CLICK,  IDC_LIST6, &CMFCApplication1Dlg::OnClickList6)
    ON_NOTIFY(NM_CLICK,  IDC_LIST7, &CMFCApplication1Dlg::OnClickList7)
    ON_BN_CLICKED(IDC_BUTTON19, &CMFCApplication1Dlg::OnBnClickedButton19)
    // Window handling controls
    ON_BN_CLICKED(IDC_BUTTON15, &CMFCApplication1Dlg::OnForceKillProcess)
    ON_BN_CLICKED(IDC_BUTTON16, &CMFCApplication1Dlg::OnWindowScreenshot)
    // Menu bar extensions
    ON_COMMAND(ID_VIEW_PROCESS,     &CMFCApplication1Dlg::OnViewProcess)
    ON_COMMAND(ID_VIEW_STARTUP,     &CMFCApplication1Dlg::OnViewStartup)
    ON_COMMAND(ID_VIEW_CLIPBOARD,   &CMFCApplication1Dlg::OnViewClipboard)
    ON_COMMAND(ID_VIEW_WINDOW,      &CMFCApplication1Dlg::OnViewWindow)
    ON_COMMAND(ID_VIEW_FILE,        &CMFCApplication1Dlg::OnViewFile)
    ON_COMMAND(ID_VIEW_GIT,         &CMFCApplication1Dlg::OnViewGit)
    ON_COMMAND(ID_TOOLS_WECHAT,     &CMFCApplication1Dlg::OnBnClickedButton4)
    ON_COMMAND(ID_TOOLS_QQ,         &CMFCApplication1Dlg::OnBnClickedButton5)
    ON_COMMAND(ID_TOOLS_VSCODE,     &CMFCApplication1Dlg::OnBnClickedButton6)
    ON_COMMAND(ID_TOOLS_VS,         &CMFCApplication1Dlg::OnBnClickedButton7)
    ON_COMMAND(ID_TOOLS_BILIBILI,   &CMFCApplication1Dlg::OnBnClickedButton8)
    ON_COMMAND(ID_TOOLS_STUDY,      &CMFCApplication1Dlg::OnBnClickedButton9)
    ON_COMMAND(ID_TOOLS_DOWNLOAD,   &CMFCApplication1Dlg::OnBnClickedButton21)
    ON_COMMAND(ID_TOOLS_POWERSHELL, &CMFCApplication1Dlg::OnBnClickedButton27)
    ON_COMMAND(ID_TOOLS_WSL,        &CMFCApplication1Dlg::OnBnClickedButton28)
    ON_COMMAND(ID_TOOLS_GITBASH,    &CMFCApplication1Dlg::OnBnClickedButton31)
    ON_COMMAND(ID_TOOLS_QRCODE,         &CMFCApplication1Dlg::OnToolsQRCode)
    ON_COMMAND(ID_TOOLS_SCREENSHOT_OCR, &CMFCApplication1Dlg::OnToolsScreenshotOCR)
    ON_COMMAND(ID_TOOLS_BATCH_RENAME,   &CMFCApplication1Dlg::OnToolsBatchRename)
    ON_COMMAND(ID_TOOLS_STICKY_NOTE,   &CMFCApplication1Dlg::OnToolsStickyNote)
    ON_COMMAND(ID_TOOLS_MARKDOWN,     &CMFCApplication1Dlg::OnToolsMarkdown)
    ON_COMMAND(ID_TOOLS_ENCODING,     &CMFCApplication1Dlg::OnToolsEncoding)
	ON_COMMAND(ID_TOOLS_CONTEXT_MENU, &CMFCApplication1Dlg::OnToolsContextMenu)
	ON_COMMAND(ID_TOOLS_ENVVAR, &CMFCApplication1Dlg::OnToolsEnvVar)
	ON_COMMAND(ID_TOOLS_FILELOCK, &CMFCApplication1Dlg::OnToolsFileLock)
    ON_COMMAND(ID_WINDOW_LOCATE,    &CMFCApplication1Dlg::OnWindowLocate)
    ON_COMMAND(ID_WINDOW_UNTOPMOST, &CMFCApplication1Dlg::OnWindowUntopmost)
    ON_COMMAND(ID_WINDOW_CLOSE,     &CMFCApplication1Dlg::OnWindowClose)
    ON_COMMAND(ID_HELP_SHORTCUTS,   &CMFCApplication1Dlg::OnHelpShortcuts)
    ON_COMMAND(ID_HELP_GITHUB,      &CMFCApplication1Dlg::OnHelpGithub)
    ON_COMMAND(ID_HELP_REGEX_GUIDE, &CMFCApplication1Dlg::OnHelpRegexGuide)
    ON_BN_CLICKED(IDC_BUTTON21, &CMFCApplication1Dlg::OnBnClickedButton21)
    ON_BN_CLICKED(IDC_BUTTON22, &CMFCApplication1Dlg::OnBnClickedButton22)
    ON_BN_CLICKED(IDC_BUTTON23, &CMFCApplication1Dlg::OnBnClickedButton23)
    ON_BN_CLICKED(IDC_BUTTON24, &CMFCApplication1Dlg::OnBnClickedButton24)
    ON_BN_CLICKED(IDC_BUTTON25, &CMFCApplication1Dlg::OnBnClickedButton25)
    ON_BN_CLICKED(IDC_BUTTON26, &CMFCApplication1Dlg::OnBnClickedButton26)
    ON_MESSAGE(CMFCApplication1Dlg::WM_VOLUME_UPDATED, &CMFCApplication1Dlg::OnVolumeUpdated)
    ON_BN_CLICKED(IDC_CHECK6, &CMFCApplication1Dlg::OnBnClickedCheck6)
    ON_BN_CLICKED(IDC_BUTTON27, &CMFCApplication1Dlg::OnBnClickedButton27)
    ON_BN_CLICKED(IDC_BUTTON28, &CMFCApplication1Dlg::OnBnClickedButton28)
    ON_BN_CLICKED(IDC_BUTTON29, &CMFCApplication1Dlg::OnBnClickedButton29)
    ON_BN_CLICKED(IDC_BUTTON30, &CMFCApplication1Dlg::OnBnClickedButton30)
    ON_BN_CLICKED(IDC_BUTTON31, &CMFCApplication1Dlg::OnBnClickedButton31)
    ON_BN_CLICKED(IDC_BUTTON32, &CMFCApplication1Dlg::OnBnClickedButton32)
    // Bind IDC_BUTTON33 to the bilibili "next" handler if the control exists
#ifdef IDC_BUTTON33
    ON_BN_CLICKED(IDC_BUTTON33, &CMFCApplication1Dlg::OnBiliNext)
#endif
    ON_COMMAND(41001, &CMFCApplication1Dlg::OnBiliNext)
    ON_COMMAND(40001, &CMFCApplication1Dlg::OnCopyGitCommand)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST4, &CMFCApplication1Dlg::OnNMDblclkList4)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST5, &CMFCApplication1Dlg::OnNMDblclkList5)
    ON_LBN_DBLCLK(IDC_LIST4, &CMFCApplication1Dlg::OnLbnDblclkList4)
    ON_COMMAND(ID_FILE_SETTINGS, &CMFCApplication1Dlg::OnFileSettings)
    ON_COMMAND(ID_FILE_EXIT, &CMFCApplication1Dlg::OnFileExit)
    ON_COMMAND(ID_HELP_ABOUT, &CMFCApplication1Dlg::OnHelpAbout)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST1, &CMFCApplication1Dlg::OnProcessColumnClick)
    ON_EN_CHANGE(IDC_EDIT_PROCESS_FILTER, &CMFCApplication1Dlg::OnProcessFilterChange)
    ON_BN_CLICKED(IDC_CHECK_PROCESS_REGEX, &CMFCApplication1Dlg::OnProcessFilterChange)
    ON_BN_CLICKED(IDC_BTN_PROCESS_REGEX_HELP, &CMFCApplication1Dlg::OnProcessRegexHelp)
    // AI Assistant
    ON_BN_CLICKED(IDC_BUTTON_AI_SEND, &CMFCApplication1Dlg::OnBnClickedAiSend)
    ON_BN_CLICKED(IDC_BUTTON_AI_CLEAR, &CMFCApplication1Dlg::OnBnClickedAiClear)
    ON_MESSAGE(WM_AI_RESPONSE, &CMFCApplication1Dlg::OnAiResponse)
END_MESSAGE_MAP()


// CMFCApplication1Dlg message handlers


BOOL CMFCApplication1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// Menu bar
	CMenu menu;
	menu.LoadMenu(IDR_MAIN_MENU);
	SetMenu(&menu);
	menu.Detach();

	m_bMinimizeOnClose = true;
	m_strDroppedFilePath.Empty();
	SetDlgItemText(IDC_EDIT4, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));

	// Initialize tab control and tab controls
	InitTabControl();
	InitProcessTab();
	InitStartupTab();
	InitClipboardTab();
	InitWindowTab();
	InitFileTab();
	InitGitTab();
	InitQuickTab();
	InitAIControls();

	// Update control visibility based on current selected tab
	int nCur = 0;
	CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
	if (pTab) nCur = pTab->GetCurSel();
	UpdateTabVisibility(nCur);
	UpdateQuickTab(0);

	// Load initial data
	if (nCur == 0)
		RefreshProcessList();
	else
		RefreshStartupList();

	// If launched from folder context menu, open batch rename dialog
	if (!m_strInitialFolder.IsEmpty())
	{
		auto* pDlg = new CBatchRenameDlg(nullptr, m_strInitialFolder);
		pDlg->Create(IDD_BATCH_RENAME_DLG, nullptr);
		pDlg->ShowWindow(SW_SHOW);
		m_strInitialFolder.Empty();
	}

	// Shutdown/restart combo initialization
	CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO1));
	if (pCombo)
	{
		pCombo->ResetContent();
		pCombo->AddString(_T("1分钟后重启"));
		pCombo->AddString(_T("默认3分钟关机"));
		pCombo->AddString(_T("设定时间关机"));
		int idx = pCombo->FindStringExact(-1, _T("默认3分钟关机"));
		if (idx != CB_ERR) pCombo->SetCurSel(idx);
		else pCombo->SetCurSel(0);
		OnCbnSelchangeCombo1();
	}

	// Auto-start checkbox
	CButton* pCheck1 = static_cast<CButton*>(GetDlgItem(IDC_CHECK1));
	if (pCheck1)
	{
		TCHAR exePath[MAX_PATH] = {0};
		if (GetModuleFileName(NULL, exePath, MAX_PATH) > 0)
		{
			CString csExePath = exePath;
			int pos = csExePath.ReverseFind(_T('\\'));
			CString keyName = (pos != -1) ? csExePath.Mid(pos + 1) : csExePath;

			HKEY hKey = NULL;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
			{
				DWORD type = 0;
				TCHAR buf[MAX_PATH] = {0};
				DWORD bufSize = sizeof(buf);
				LONG ret = RegQueryValueEx(hKey, keyName, NULL, &type, reinterpret_cast<LPBYTE>(buf), &bufSize);
				if (ret == ERROR_SUCCESS && type == REG_SZ)
					pCheck1->SetCheck(BST_CHECKED);
				else
					pCheck1->SetCheck(BST_UNCHECKED);
				RegCloseKey(hKey);
			}
			else
			{
				pCheck1->SetCheck(BST_UNCHECKED);
			}
		}
	}

	// Drag-and-drop acceptance
	CWnd* pCtrl = nullptr;
	pCtrl = GetDlgItem(IDC_STATIC_PATH); if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_EDIT4);       if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_BUTTON3);     if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);
	pCtrl = GetDlgItem(IDC_STATIC7);     if (pCtrl) ::DragAcceptFiles(pCtrl->GetSafeHwnd(), TRUE);

	// Minimize to tray checkbox
	CButton* pCheckMin = static_cast<CButton*>(GetDlgItem(IDC_CHECK2));
	if (pCheckMin) pCheckMin->SetCheck(m_bMinimizeOnClose ? BST_CHECKED : BST_UNCHECKED);

	// Clipboard listener
	::AddClipboardFormatListener(m_hWnd);

	// File drag-and-drop
	::DragAcceptFiles(m_hWnd, TRUE);
	AllowUIPIMessage(m_hWnd, WM_DROPFILES, TRUE);
	AllowUIPIMessage(m_hWnd, WM_COPYDATA, TRUE);
	AllowUIPIMessage(m_hWnd, 0x0049, TRUE);

	// Elevate to admin privileges
	if (!IsProcessElevated())
	{
		auto RelaunchElevatedNoPrompt = []() -> bool {
			TCHAR path[MAX_PATH];
			if (GetModuleFileName(NULL, path, MAX_PATH) > 0)
			{
				SHELLEXECUTEINFO sei = { sizeof(sei) };
				sei.fMask = SEE_MASK_FLAG_NO_UI;
				sei.lpVerb = _T("runas");
				sei.lpFile = path;
				// Pass through the original command line (e.g. folder path from context menu)
				// so the elevated instance can still open folder processing.
				CString strCmd = AfxGetApp()->m_lpCmdLine;
				strCmd.Trim();
				if (!strCmd.IsEmpty())
					sei.lpParameters = strCmd;
				sei.nShow = SW_SHOWNORMAL;
				return ShellExecuteEx(&sei) != FALSE;
			}
			return false;
		};

		if (RelaunchElevatedNoPrompt())
		{
			EndDialog(IDOK);
			return FALSE;
		}
		else
		{
			MessageBox(_T("无法以管理员权限重新启动。请手动以管理员身份运行程序。"), _T("提示"), MB_OK | MB_ICONWARNING);
		}
	}

	// Global hotkey
	CString strTitle;
	GetWindowText(strTitle);
	strTitle += _T("(ctrl+alt+空格唤起此窗口)");
	SetWindowText(strTitle);
	RegisterHotKey(m_hWnd, 1001, MOD_CONTROL | MOD_ALT, VK_SPACE);

	// Volume slider
	CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER1));
	CEdit* pEditVol = static_cast<CEdit*>(GetDlgItem(IDC_EDIT5));
	if (pSlider)
	{
		pSlider->SetRange(0, 100);
		pSlider->SetPos(100);
		if (pEditVol) pEditVol->SetWindowText(_T("100"));
		CVolumeManager::FetchVolumeAsync(m_hWnd);
	}

	// Auto-create sticky note in collapsed state at startup
	if (!m_pStickyNoteDlg || !::IsWindow(m_pStickyNoteDlg->m_hWnd))
	{
		m_pStickyNoteDlg = new CStickyNoteDlg(nullptr);
		m_pStickyNoteDlg->Create(IDD_STICKY_NOTE_DLG, nullptr);
		CRect rcDlg;
		m_pStickyNoteDlg->GetWindowRect(&rcDlg);
		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int x = screenW * 3 / 5;
		int y = 10;
		m_pStickyNoteDlg->SetWindowPos(nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		m_pStickyNoteDlg->ShowWindow(SW_SHOW);
		// The dialog will auto-collapse after a short delay via its own timer
	}

	// Global hotkey
	return TRUE;
}

// ========== Right-side quick-action tab ==========

void CMFCApplication1Dlg::InitQuickTab()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB_QUICK));
    if (!pTab) return;

    pTab->InsertItem(0, _T("常用"));
    pTab->InsertItem(1, _T("系统"));
    pTab->InsertItem(2, _T("工具"));

    pTab->SetCurSel(0);
}

void CMFCApplication1Dlg::UpdateQuickTab(int nTab)
{
    static const int kCommonIds[] = {
        IDC_BUTTON4, IDC_BUTTON5, IDC_BUTTON8, IDC_BUTTON22, IDC_BUTTON7, IDC_BUTTON6,
        IDC_BUTTON9, IDC_BUTTON11, IDC_BUTTON21, IDC_BUTTON10, IDC_BUTTON29
    };
    static const int kSystemIds[] = {
        IDC_STATIC_QUICK_SHUTDOWN, IDC_COMBO1, IDC_BUTTON1, IDC_BUTTON2,
        IDC_STATIC_QUICK_HOUR, IDC_EDIT1,
        IDC_STATIC_QUICK_MIN, IDC_EDIT2,
        IDC_STATIC_QUICK_SEC, IDC_EDIT3,
        IDC_STATIC_QUICK_SEP1,
        IDC_STATIC_QUICK_VOLUME, IDC_SLIDER1, IDC_EDIT5, IDC_BUTTON12, IDC_BUTTON13,
        IDC_STATIC_QUICK_SEP2,
        IDC_STATIC_QUICK_SYSMGMT, IDC_BUTTON20
    };
    static const int kToolIds[] = {
        IDC_STATIC_QUICK_CMDLINE, IDC_BUTTON27, IDC_BUTTON28,
        IDC_STATIC_QUICK_SEP3,
        IDC_STATIC_QUICK_RUNCMD, IDC_EDIT6, IDC_BUTTON17, IDC_BUTTON18
    };

    auto showGroup = [&](const int* ids, int count, bool show) {
        for (int i = 0; i < count; i++)
        {
            CWnd* pWnd = GetDlgItem(ids[i]);
            if (pWnd) pWnd->ShowWindow(show ? SW_SHOW : SW_HIDE);
        }
    };

    showGroup(kCommonIds, _countof(kCommonIds), nTab == 0);
    showGroup(kSystemIds, _countof(kSystemIds), nTab == 1);
    showGroup(kToolIds, _countof(kToolIds), nTab == 2);
}

void CMFCApplication1Dlg::OnTcnSelchangeQuickTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB_QUICK));
    if (pTab)
        UpdateQuickTab(pTab->GetCurSel());
    *pResult = 0;
}

// ========== Window handling new features ==========


void CMFCApplication1Dlg::OnDestroy()
{
    // Save sticky note before destroying
    if (m_pStickyNoteDlg && ::IsWindow(m_pStickyNoteDlg->m_hWnd))
    {
        m_pStickyNoteDlg->SaveIfNeeded();
    }

    UnregisterHotKey(m_hWnd, 1001);

    if (m_bTrayVisible)
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bTrayVisible = false;
    }

    // Ensure we unregister clipboard listener if we registered it in OnInitDialog.
    ::RemoveClipboardFormatListener(m_hWnd);

    // Destroy auto-clicker speed adjustment window
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }

    // Ensure autoclicker threads are stopped (C++20: using CAutoClicker class)
    m_autoClicker.Stop();

    // ensure any background volume thread is stopped (CVolumeManager handles this automatically)
    // Drop helper and file management removed - no cleanup required here

    // Remove topmost status from all managed windows
    for (HWND hWnd : m_topmostWnds)
    {
        if (IsValidWindow(hWnd))
            ::SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
    }
    m_topmostWnds.clear();

    // Unregister drag-and-drop acceptance to be tidy
    ::DragAcceptFiles(m_hWnd, FALSE);

    // Destroy any lingering overlay capture window created for locating targets
    if (m_hCaptureWnd && IsValidWindow(m_hCaptureWnd))
    {
        ::DestroyWindow(m_hCaptureWnd);
        m_hCaptureWnd = NULL;
    }

    // If we dynamically registered a window class for the overlay, unregister it.
    // This avoids leaving class registrations around across repeated runs in
    // environments that reload the module without process exit.
    WNDCLASS wc = {0};
    if (GetClassInfo(AfxGetInstanceHandle(), _T("MyCaptureOverlayClass"), &wc))
    {
        UnregisterClass(_T("MyCaptureOverlayClass"), AfxGetInstanceHandle());
    }

    // Clear global autoclick owner handle to avoid referencing freed window
    // Autoclicker cleanup now handled by m_autoClicker.Stop() above

    // Revoke UIPI allowances
    AllowUIPIMessage(m_hWnd, WM_DROPFILES, FALSE);
    AllowUIPIMessage(m_hWnd, WM_COPYDATA, FALSE);
    AllowUIPIMessage(m_hWnd, 0x0049, FALSE);

    // Ensure we clear execution state if we had prevented lock
    if (m_bPreventLockScreen)
    {
        // clear previously set flags by calling with ES_CONTINUOUS only
        SetThreadExecutionState(ES_CONTINUOUS);
        m_bPreventLockScreen = false;
    }

    CDialogEx::OnDestroy();
}

void CMFCApplication1Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon. For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMFCApplication1Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // Device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client area rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMFCApplication1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

BOOL CMFCApplication1Dlg::PreTranslateMessage(MSG* pMsg)
{
    // ===== Global hotkeys =====

    if (pMsg->message == WM_KEYDOWN)
    {
        // F5: Refresh current tab list
        if (pMsg->wParam == VK_F5)
        {
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab)
            {
                int nSel = pTab->GetCurSel();
                if (nSel == 0) { RefreshProcessList(); return TRUE; }
                if (nSel == 1) { RefreshStartupList(); return TRUE; }
            }
        }

        // Ctrl+Alt+D: Window locate
        if (pMsg->wParam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_MENU) & 0x8000))
        {
            OnWindowLocate();
            return TRUE;
        }

        // Enter: Handle when focus is in edit box
        if (pMsg->wParam == VK_RETURN)
        {
            CWnd* pFocus = CWnd::FromHandle(::GetFocus());
            if (pFocus)
            {
                int nID = pFocus->GetDlgCtrlID();

                // Volume adjustment input: trigger apply volume
                if (nID == IDC_EDIT5)
                {
                    OnBnClickedButton12();
                    return TRUE;
                }

                // Command input: run command
                if (nID == IDC_EDIT6)
                {
                    OnBnClickedButton17();
                    return TRUE;
                }

                // AI input: send message
                if (nID == IDC_EDIT_AI_INPUT)
                {
                    OnBnClickedAiSend();
                    return TRUE;
                }

                // Other edit boxes: swallow Enter to prevent exiting
                TCHAR className[64] = {0};
                ::GetClassName(pFocus->GetSafeHwnd(), className, 64);
                if (_tcsstr(className, _T("Edit")) || _tcsstr(className, _T("edit")))
                {
                    return TRUE;
                }
            }
        }
    }

    // Alt+1~6: Switch tabs
    if (pMsg->message == WM_SYSKEYDOWN)
    {
        if (pMsg->wParam >= '1' && pMsg->wParam <= '6')
        {
            int nTab = static_cast<int>(pMsg->wParam - '1');
            CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
            if (pTab)
            {
                pTab->SetCurSel(nTab);
                UpdateTabVisibility(nTab);
            }
            return TRUE;
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

// Handle clipboard update messages
LRESULT CMFCApplication1Dlg::OnClipboardUpdate(WPARAM wParam, LPARAM lParam)
{
    // Only handle text
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
        return 0;

    if (!::OpenClipboard(m_hWnd))
        return 0;

    HGLOBAL hData = ::GetClipboardData(CF_UNICODETEXT);
    if (hData)
    {
        LPCWSTR pszText = (LPCWSTR)::GlobalLock(hData);
        if (pszText)
        {
            CString s = pszText;
            ::GlobalUnlock(hData);

            // Trim and ignore empty
            s.Trim();
            if (!s.IsEmpty())
            {
                // Avoid duplicate consecutive entries
                if (m_clipHistory.empty() || m_clipHistory.front() != s)
                {
                    m_clipHistory.insert(m_clipHistory.begin(), s);
                    if ((int)m_clipHistory.size() > CLIP_HISTORY_MAX)
                        m_clipHistory.pop_back();

                    // update list control
                    CListCtrl* pList3 = (CListCtrl*)GetDlgItem(IDC_LIST3);
                    if (pList3)
                    {
                        pList3->DeleteAllItems();
                        int idx = 0;
                        for (auto &item : m_clipHistory)
                        {
                            pList3->InsertItem(idx, item);
                            idx++;
                        }
                    }
                }
            }
        }
    }

    ::CloseClipboard();
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnRefreshProcessesDone(WPARAM wParam, LPARAM lParam)
{
    auto vec = reinterpret_cast<std::vector<ProcInfo>*>(wParam);
    m_processes = std::move(*vec);
    delete vec;

    m_nSortColumn = -1;  // Reset sort state
    ApplyProcessFilter();
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnVolumeUpdated(WPARAM wParam, LPARAM lParam)
{
    int pct = (int)wParam;
    CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
    CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
    if (pSlider) pSlider->SetPos(pct);
    if (pEditVol)
    {
        CString s; s.Format(_T("%d"), pct);
        pEditVol->SetWindowText(s);
    }
    return 0;
}

afx_msg LRESULT CMFCApplication1Dlg::OnRefreshStartupsDone(WPARAM wParam, LPARAM lParam)
{
    auto vec = reinterpret_cast<std::vector<StartupInfo>*>(wParam);
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST2);
    if (pList)
    {
        pList->DeleteAllItems();
        int idx = 0;
        for (auto &si : *vec)
        {
            pList->InsertItem(idx, si.name);
            pList->SetItemText(idx, 1, si.cmd);
            idx++;
        }
    }
    delete vec;
    return 0;
}

void CMFCApplication1Dlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
    CListCtrl* pList1 = (CListCtrl*)GetDlgItem(IDC_LIST1);
    CListCtrl* pList2 = (CListCtrl*)GetDlgItem(IDC_LIST2);

    HWND hClicked = pWnd ? pWnd->GetSafeHwnd() : ::WindowFromPoint(point);

    // Right-click on process list
    if (pList1 && hClicked == pList1->GetSafeHwnd())
    {
        int nSel = pList1->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 32771, _T("结束进程"));
            menu.AppendMenu(MF_STRING, 32774, _T("打开程序所在位置"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        }
        return;
    }

    // Right-click on startup management list
    if (pList2 && hClicked == pList2->GetSafeHwnd())
    {
        int nSel = pList2->GetNextItem(-1, LVNI_SELECTED);
        CMenu menu;
        menu.CreatePopupMenu();
        // Add and delete commands
        menu.AppendMenu(MF_STRING, 32772, _T("添加启动项"));
        if (nSel != -1)
        {
            menu.AppendMenu(MF_STRING, 32773, _T("删除启动项"));
            menu.AppendMenu(MF_STRING, 32806, _T("复制路径"));
        }

        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }


    // Right-click on window handling list (copy value)
    CListCtrl* pList5 = (CListCtrl*)GetDlgItem(IDC_LIST5);
    if (pList5 && hClicked == pList5->GetSafeHwnd())
    {
        int nSel = pList5->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 40002, _T("复制值"));
            int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
            if (cmd == 40002)
            {
                CString val = pList5->GetItemText(nSel, 1);
                if (!val.IsEmpty()) CopyToClipboard(m_hWnd, val);
            }
        }
        return;
    }
    // Right-click on topmost window list (untopmost / delete)
    CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
    if (pList6 && hClicked == pList6->GetSafeHwnd())
    {
        int nSel = pList6->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenu(MF_STRING, 32805, _T("取消置顶"));
            menu.AppendMenu(MF_STRING, 32807, _T("删除"));
            menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        }
        return;
    }

    // Right-click on history window list (topmost / delete)
    CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
    if (pList7 && hClicked == pList7->GetSafeHwnd())
    {
        int nSel = pList7->GetNextItem(-1, LVNI_SELECTED);
        if (nSel != -1)
        {
            size_t idx = static_cast<size_t>(pList7->GetItemData(nSel));
            if (idx < m_historyWnds.size())
            {
                HWND hWnd = m_historyWnds[idx];
                bool bAlreadyTopmost = IsValidWindow(hWnd) &&
                    std::find(m_topmostWnds.begin(), m_topmostWnds.end(), hWnd) != m_topmostWnds.end();

                CMenu menu;
                menu.CreatePopupMenu();
                if (bAlreadyTopmost)
                    menu.AppendMenu(MF_STRING, 32810, _T("取消置顶"));
                else
                    menu.AppendMenu(MF_STRING, 32809, _T("置顶"));
                menu.AppendMenu(MF_STRING, 32808, _T("删除"));
                menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
            }
        }
        return;
    }
    // Right-click on git tools list (copy command)
    CWnd* pList4 = GetDlgItem(IDC_LIST4);
    if (pList4 && hClicked == pList4->GetSafeHwnd())
    {
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 40001, _T("复制指令"));
        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
        return;
    }
}

// File management helper removed

// Named pipe listener: receives one-line file paths from DropHelper and posts to main window
// DropHelper and named pipe listener removed
















void CMFCApplication1Dlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CWnd* pWnd = CWnd::FromHandle(pScrollBar ? pScrollBar->m_hWnd : NULL);
    if (pWnd && pScrollBar && pScrollBar->GetSafeHwnd() == GetDlgItem(IDC_SLIDER1)->GetSafeHwnd())
    {
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        if (pSlider && pEditVol)
        {
            int pos = pSlider->GetPos();
            CString s; s.Format(_T("%d"), pos);
            pEditVol->SetWindowText(s);
            CVolumeManager::SetMasterVolumePercent(pos);
        }
    }
    else if (pScrollBar && pScrollBar->GetSafeHwnd() == GetDlgItem(IDC_SLIDER2)->GetSafeHwnd())
    {
        CSliderCtrl* pSlider2 = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER2));
        if (pSlider2)
        {
            int pos = pSlider2->GetPos();
            int percent = (pos * 100) / 255;
            CString label;
            label.Format(_T("透明度: %d%%"), percent);
            SetDlgItemText(IDC_STATIC18, label);

            if (m_hSelectedWnd && IsValidWindow(m_hSelectedWnd))
            {
                // Set layered window transparency
                LONG style = ::GetWindowLong(m_hSelectedWnd, GWL_EXSTYLE);
                ::SetWindowLong(m_hSelectedWnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
                ::SetLayeredWindowAttributes(m_hSelectedWnd, 0, static_cast<BYTE>(pos), LWA_ALPHA);
            }
        }
    }

    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}













// ===== Auto-clicker unified start/stop =====

void CMFCApplication1Dlg::StartAutoClicker()
{
    int interval = AfxGetApp()->GetProfileInt(_T("AutoClicker"), _T("IntervalMs"), 100);

    // Read trigger key config, default A/B
    CString strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStart"), _T("A"));
    char keyStart = static_cast<char>(strKey.IsEmpty() ? 'A' : strKey[0]);
    strKey = AfxGetApp()->GetProfileString(_T("AutoClicker"), _T("KeyStop"), _T("B"));
    char keyStop = static_cast<char>(strKey.IsEmpty() ? 'B' : strKey[0]);
    m_autoClicker.SetKeys(keyStart, keyStop);
    m_autoClicker.Start(interval, m_hWnd);

    // Show speed adjustment window
    if (!m_pSpeedDlg)
    {
        auto pDlg = std::make_unique<CAutoClickerSpeedDlg>(&m_autoClicker);
        pDlg->SetInterval(interval);
        pDlg->Create(IDD_CLICK_SPEED_DIALOG, this);
        pDlg->ShowWindow(SW_SHOW);
        m_pSpeedDlg = std::move(pDlg);
    }
}

void CMFCApplication1Dlg::StopAutoClicker()
{
    m_autoClicker.Stop();

    // Destroy speed adjustment window
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }
}

// Handler for autoclick checkbox in main dialog (C++20: using CAutoClicker class)
void CMFCApplication1Dlg::OnBnClickedCheck4()
{
    CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK4));
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
        StartAutoClicker();
    else
        StopAutoClicker();
}

// Message handler invoked when autoclick stops due to B key
afx_msg LRESULT CMFCApplication1Dlg::OnAutoClickStopped(WPARAM wParam, LPARAM lParam)
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK4);
    if (pCheck) pCheck->SetCheck(BST_UNCHECKED);

    // Destroy speed adjustment window
    if (m_pSpeedDlg)
    {
        m_pSpeedDlg->DestroyWindow();
        m_pSpeedDlg.reset();
    }

    // Tray bubble notification (only send if tray is initialized)
    if (m_bTrayVisible && m_nid.hWnd != NULL)
    {
        m_nid.uFlags = NIF_INFO;
        m_nid.dwInfoFlags = NIIF_INFO;
        _tcscpy_s(m_nid.szInfoTitle, _T("连点器"));
        _tcscpy_s(m_nid.szInfo, _T("连点已停止"));
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

    return 0;
}

// Speed dialog close callback: clear pointer
afx_msg LRESULT CMFCApplication1Dlg::OnSpeedDlgClosed(WPARAM wParam, LPARAM lParam)
{
    // Sync CHECK4 state
    CButton* pCheck = static_cast<CButton*>(GetDlgItem(IDC_CHECK4));
    if (pCheck) pCheck->SetCheck(BST_UNCHECKED);

    m_pSpeedDlg.reset();
    return 0;
}






















/////////////////////////////////////////////////////////////////////////////
// CMFCApplication1Dlg menu command handlers

void CMFCApplication1Dlg::OnFileSettings()
{
    auto* pDlg = new CSettingsDlg(nullptr);
    pDlg->Create(IDD_SETTINGS_DIALOG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnFileExit()
{
    DestroyWindow();
}

void CMFCApplication1Dlg::OnHelpAbout()
{
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
}

void CMFCApplication1Dlg::OnHelpRegexGuide()
{
    auto* pDlg = new CRegexGuideDlg(nullptr);
    pDlg->Create(IDD_REGEX_GUIDE_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsQRCode()
{
    auto* pDlg = new CQRCodeGenDlg(nullptr);
    pDlg->Create(IDD_QRCODE_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsScreenshotOCR()
{
	auto* pDlg = new CScreenshotOCRDlg(this, &m_autoClicker);
	pDlg->Create(IDD_SCREENSHOT_OCR_DLG, this);
	pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsBatchRename()
{
    auto* pDlg = new CBatchRenameDlg(nullptr);
    pDlg->Create(IDD_BATCH_RENAME_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::OnToolsStickyNote()
{
    if (m_pStickyNoteDlg && ::IsWindow(m_pStickyNoteDlg->m_hWnd))
    {
        // Toggle: if visible, hide it; if hidden, show it
        if (m_pStickyNoteDlg->IsWindowVisible())
            m_pStickyNoteDlg->ShowWindow(SW_HIDE);
        else
        {
            m_pStickyNoteDlg->ShowWindow(SW_SHOW);
            m_pStickyNoteDlg->SetForegroundWindow();
        }
    }
    else
    {
        m_pStickyNoteDlg = new CStickyNoteDlg(nullptr);
        m_pStickyNoteDlg->Create(IDD_STICKY_NOTE_DLG, nullptr);

        // Position left edge at 3/5 of screen width, top at 10px
        CRect rcDlg;
        m_pStickyNoteDlg->GetWindowRect(&rcDlg);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int x = screenW * 3 / 5;
        int y = 10;
        m_pStickyNoteDlg->SetWindowPos(nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

        m_pStickyNoteDlg->ShowWindow(SW_SHOW);
    }
}

void CMFCApplication1Dlg::OnToolsMarkdown()
{
	auto* pDlg = new CMarkdownDlg(nullptr);
	if (!pDlg->Create(IDD_MARKDOWN_DLG, nullptr))
	{
		DWORD dwErr = GetLastError();
		CString msg;
		msg.Format(_T("Failed to create Markdown Preview dialog (error %lu)"), dwErr);
		MessageBox(msg, _T("Error"), MB_ICONERROR);
		delete pDlg;
		return;
	}
	pDlg->ShowWindow(SW_SHOW);
	pDlg->SetForegroundWindow();
}

void CMFCApplication1Dlg::OnToolsEncoding()
{
	auto* pDlg = new CEncodingConverterDlg(nullptr);
	if (!pDlg->Create(IDD_ENCODING_CONVERTER_DLG, nullptr))
	{
		DWORD dwErr = GetLastError();
		CString msg;
		msg.Format(_T("Failed to create Encoding Converter dialog (error %lu)"), dwErr);
		MessageBox(msg, _T("Error"), MB_ICONERROR);
		delete pDlg;
		return;
	}
	pDlg->ShowWindow(SW_SHOW);
	pDlg->SetForegroundWindow();
}

void CMFCApplication1Dlg::OnToolsContextMenu()
{
	auto* pDlg = new CContextMenuDlg(nullptr);
	if (!pDlg->Create(IDD_CONTEXT_MENU_DLG, nullptr))
	{
		DWORD dwErr = GetLastError();
		CString msg;
		msg.Format(_T("Failed to create Context Menu Manager dialog (error %lu)"), dwErr);
		MessageBox(msg, _T("Error"), MB_ICONERROR);
		delete pDlg;
		return;
	}
	pDlg->ShowWindow(SW_SHOW);
	pDlg->SetForegroundWindow();
}

void CMFCApplication1Dlg::OnToolsEnvVar()
{
	auto* pDlg = new CEnvVarDlg(nullptr);
	if (!pDlg->Create(IDD_ENVVAR_DLG, nullptr))
	{
		DWORD dwErr = GetLastError();
		CString msg;
		msg.Format(_T("Failed to create Environment Variable Manager dialog (error %lu)"), dwErr);
		MessageBox(msg, _T("Error"), MB_ICONERROR);
		delete pDlg;
		return;
	}
	pDlg->ShowWindow(SW_SHOW);
	pDlg->SetForegroundWindow();
}

void CMFCApplication1Dlg::OnToolsFileLock()
{
	auto* pDlg = new CFileLockDlg(nullptr);
	if (!pDlg->Create(IDD_FILELOCK_DLG, nullptr))
	{
		DWORD dwErr = GetLastError();
		CString msg;
		msg.Format(_T("Failed to create File Lock Viewer dialog (error %lu)"), dwErr);
		MessageBox(msg, _T("Error"), MB_ICONERROR);
		delete pDlg;
		return;
	}
	pDlg->ShowWindow(SW_SHOW);
	pDlg->SetForegroundWindow();
}

// ========== AI Assistant ==========

void CMFCApplication1Dlg::InitAIControls()
{
    // Initialize vendor combo box
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_AI_VENDOR));
    if (pCombo)
    {
        pCombo->ResetContent();
        for (const auto& v : CAIApiClient::GetVendors())
            pCombo->AddString(v.name);
        CString savedVendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
        int idx = pCombo->FindStringExact(-1, savedVendor);
        pCombo->SetCurSel(idx != CB_ERR ? idx : 0);
    }

    // Set history edit to read-only
    CEdit* pHistory = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_HISTORY));
    if (pHistory)
    {
        pHistory->SetReadOnly(TRUE);
    }
}

CString CMFCApplication1Dlg::BuildSystemPrompt()
{
    return _T("You are an AI assistant integrated into a Windows MFC toolbox application. ")
        _T("Your role is to help users understand and use this toolbox, troubleshoot issues, and answer related questions.\n\n")

        _T("=== APPLICATION OVERVIEW ===\n\n")
        _T("This is a multifunctional Windows toolbox with 6 left-side tab pages, ")
        _T("3 right-side quick-action sub-tabs, and 9 tools accessible from the menu bar.\n")
        _T("The application runs with administrator privileges and supports system tray minimization.\n\n")

        _T("=== LEFT TAB PAGES (6 tabs, switchable via Alt+1~6 or View menu) ===\n\n")

        _T("1. Process Management (Tab 1)\n")
        _T("   - Displays all running processes: name, PID, full path, memory usage (KB)\n")
        _T("   - Click column headers to sort ascending/descending (arrow indicators)\n")
        _T("   - Filter box: type keywords to filter processes; check 'Regex' for regex filtering\n")
        _T("   - Right-click a process: 'End Process' (WM_CLOSE then TerminateProcess) or 'End All Same-Name Processes'\n")
        _T("   - Right-click a process: 'Open File Location' opens Explorer at the executable\n")
        _T("   - F5 refreshes the process list\n")
        _T("   - Help button opens regex reference guide\n\n")

        _T("2. Startup Management (Tab 2)\n")
        _T("   - Shows current user's startup items from HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\n")
        _T("   - Displays startup name and command line\n")
        _T("   - Right-click: 'Add Startup Item' (select .exe via file dialog), 'Delete Startup Item', 'Copy Path'\n")
        _T("   - Double-click copies the startup item's command line\n")
        _T("   - F5 refreshes the startup list\n\n")

        _T("3. Clipboard (Tab 3)\n")
        _T("   - Real-time clipboard monitoring: auto-records last 10 copied text items\n")
        _T("   - Double-click any item to re-copy it to clipboard\n")
        _T("   - Duplicate consecutive entries are automatically deduplicated\n\n")

        _T("4. Window Handling (Tab 4)\n")
        _T("   - 'Locate Window' button (or Ctrl+Alt+D): click a target window to capture its details\n")
        _T("   - LIST5 shows window details: handle, title, process name, PID, executable path, position/size\n")
        _T("   - LIST6 (topmost list): managed topmost windows; right-click to untopmost or delete\n")
        _T("   - LIST7 (history list): previously located windows; right-click to topmost/untopmost or delete\n")
        _T("   - Click items in LIST6/LIST7 to load their details into LIST5\n")
        _T("   - Transparency slider: adjust selected window's opacity (10%-100%)\n")
        _T("   - 'Force Kill Process': terminate the process owning the selected window\n")
        _T("   - 'Window Screenshot': capture selected window as PNG, save to configured directory and clipboard\n")
        _T("   - 'Window Topmost' checkbox: keep the toolbox itself always on top\n")
        _T("   - Window menu: Locate, Un-topmost All, Close Window\n\n")

        _T("5. File Management (Tab 5)\n")
        _T("   - Drag and drop a file to display its path; auto-fills filename and extension for rename\n")
        _T("   - Drag and drop a folder to open Batch Rename dialog\n")
        _T("   - 'Make Copy': creates a copy in the same directory (auto-adds '_copy' suffix to avoid conflicts)\n")
        _T("   - 'Rename': validates filename (no illegal characters), checks target doesn't exist, then renames\n")
        _T("   - 'Delete': moves file to Recycle Bin (not permanent delete)\n")
        _T("   - 'Copy To' / 'Move To': select destination folder, then copy or move the dragged file\n")
        _T("   - 'Clear Path' button resets to default state\n\n")

        _T("6. Git Toolbox (Tab 6)\n")
        _T("   - Pre-loaded with 20 common Git commands (init, add, commit, push, pull, clone, status, branch, checkout, merge, log, restore, etc.)\n")
        _T("   - Drag files/folders to set the Git working directory\n")
        _T("   - Double-click a command to execute it in the working directory\n")
        _T("   - Right-click to copy the command text\n")
        _T("   - 'Git Bash' button opens Git Bash in the current working directory\n")
        _T("   - Commands can be customized in config.ini under [GitCommands] section (Cmd1~Cmd99, format: 'Description|Command')\n\n")

        _T("=== RIGHT SIDE QUICK ACTIONS (3 sub-tabs) ===\n\n")

        _T("Common tab:\n")
        _T("   - Quick launch: WeChat (Ctrl+Alt+W if running), QQ (Ctrl+Alt+X if running), Bilibili, Yuanbao, VS, VSCode\n")
        _T("   - Open folders: Study, Downloads\n")
        _T("   - Open URLs: MOOC, SDUCS, LeetCode, GitHub\n")
        _T("   - Bilibili 'Next Track': sends ']' key to Bilibili player window, or global media next track\n\n")

        _T("System tab:\n")
        _T("   - Shutdown/Restart: dropdown with '1min restart', '3min shutdown', 'Custom time shutdown' (set H/M/S)\n")
        _T("   - 'Execute' button triggers the shutdown/restart; 'Cancel' button aborts it\n")
        _T("   - Volume: slider (0-100), input box (Enter to apply), 'Apply' button, 'Mute' (0%), '10%' button\n")
        _T("   - 'Task Manager' button opens Windows Task Manager\n\n")

        _T("Tools tab:\n")
        _T("   - 'PowerShell': choose normal or admin mode\n")
        _T("   - 'WSL': launch WSL terminal\n")
        _T("   - Run command input box: type exe path, URL, or cmd command, press Enter to execute\n")
        _T("   - 'Clear' button clears the command input\n\n")

        _T("=== MENU BAR TOOLS (9 independent dialogs) ===\n\n")

        _T("1. 二维码生成\n")
        _T("   - Enter text or URL, click '生成二维码' to create a QR code image\n")
        _T("   - 'Copy' copies the QR image to clipboard; 'Save' exports as PNG or BMP\n")
        _T("   - QR code has 4px white margin\n\n")

        _T("2. 截图OCR\n")
        _T("   - Click '开始截图' to hide the window, then drag a screen region to capture\n")
        _T("   - Automatically runs OCR on the captured area (small images are 2x upscaled for accuracy)\n")
        _T("   - Language dropdown: Chinese, English, Japanese, Korean\n")
        _T("   - 'Translate' button translates OCR result via MyMemory API (free, 10s timeout)\n")
        _T("   - 'Copy' copies the translated text (or original OCR text if no translation)\n")
        _T("   - Press ESC to cancel capture\n\n")

        _T("3. 文件夹处理\n")
        _T("   - Tab 1 'Folder Operations': list subfolders, rename/move/delete selected folders\n")
        _T("   - Tab 2 'File Batch Processing':\n")
        _T("     * Rename rules: add prefix/suffix, find & replace (regex supported)\n")
        _T("     * Auto-numbering: start number, zero-padded, place before or after extension\n")
        _T("     * Delete matching: regex-based file deletion to Recycle Bin, with invert option\n")
        _T("     * Ignore rules: by extension or filename pattern (regex), manual ignore/unignore\n")
        _T("     * Track rules: only process tracked files (overrides ignore)\n")
        _T("   - File list supports drag-and-drop reordering with blue insertion line\n")
        _T("   - Right-click menu: ignore, track, mark for deletion, modify extension, forward/backward move, locate in Explorer\n")
        _T("   - 'Preview' shows rename results; 'Execute' applies changes; 'Undo' reverts last rename\n")
        _T("   - 'Reset All' clears all rules and marks; F5 refreshes the file list\n\n")

        _T("4. 简易便签\n")
        _T("   - Auto-starts at application launch, positioned at right 3/5 of screen\n")
        _T("   - Initially collapsed to title bar only; double-click title bar to expand\n")
        _T("   - In expanded state: X button collapses to title bar; minimize button collapses to title bar\n")
        _T("   - In collapsed state: X button exits; double-click title bar expands\n")
        _T("   - Right-click title bar: 'Exit Sticky Note'\n")
        _T("   - Content auto-saves to sticky_note.txt (UTF-8) in the configured save folder\n")
        _T("   - 'Browse' button to change save folder\n\n")

        _T("5. Markdown 预览\n")
        _T("   - Left editor panel + right rendered preview, splitter is draggable\n")
        _T("   - 'Open' button or drag-drop .md files\n")
        _T("   - Real-time preview updates as you type\n")
        _T("   - Supports: headings, bold, italic, inline code, code blocks, links, blockquotes, strikethrough, lists, tables, horizontal rules\n")
        _T("   - GitHub-style CSS rendering; max file size 10MB\n\n")

        _T("6. 编码转换\n")
        _T("   - 'Open' or drag-drop a text file (txt, md, csv, log, etc.)\n")
        _T("   - Auto-detects source encoding (BOM check → UTF-8 validation → GBK fallback)\n")
        _T("   - Left panel shows source encoding interpretation; right panel shows target encoding interpretation\n")
        _T("   - Supported encodings: UTF-8, UTF-8 BOM, UTF-16LE, UTF-16LE BOM, UTF-16BE, GBK, Big5, Shift-JIS, Latin-1\n")
        _T("   - 'Save As' exports with target encoding; 'Overwrite' replaces original (moves original to Recycle Bin first)\n")
        _T("   - Max file size 10MB\n\n")

        _T("7. 右键菜单管理\n")
        _T("   - Scan and manage Windows right-click context menu items\n")
        _T("   - Scene dropdown: 28+ scenarios (All, File, Folder, Directory Background, Desktop, Drive, etc.)\n")
        _T("   - 14 common extension presets (.jpg, .png, .txt, .pdf, etc.) + custom extension query\n")
        _T("   - List shows: location, display name, type (Static/ShellEx), visibility, key name, command\n")
        _T("   - Right-click: enable/disable items, custom name resolution, locate in registry\n")
        _T("   - 'Folder Right-Click Menu' checkbox: add/remove this tool from folder context menu\n")
        _T("   - 'Win11 Classic Menu' checkbox: toggle Win11 old/new right-click style (requires Explorer restart)\n")
        _T("   - 'Rebuild Dictionary': query ShellEx display names via COM and cache them\n")
        _T("   - 'Dictionary Path': configure custom dictionary folder; 'Open Dictionary' opens it in Explorer\n")
        _T("   - F5 refreshes; disabled items use LegacyDisable + ProgrammaticAccessOnly mechanism\n\n")

        _T("8. 环境变量管理\n")
        _T("   - Top list: system variables; bottom list: user variables\n")
        _T("   - Search box: real-time filtering across both lists\n")
        _T("   - 'Add': choose system or user scope, enter variable name and value\n")
        _T("   - 'Edit' or double-click: PATH variable opens dedicated editor; others open simple input dialog\n")
        _T("   - 'Delete': removes selected variable (with confirmation)\n")
        _T("   - 'Export': save all variables to .txt or .env file\n")
        _T("   - Right-click: edit, delete, copy name, copy value\n")
        _T("   - PATH Editor: list entries as individual rows; add/remove/reorder (up/down) entries\n")
        _T("   - Auto-backup: before any modification, current values are backed up to temp folder with timestamp\n")
        _T("   - F5 refreshes; broadcasts WM_SETTINGCHANGE after modifications\n\n")

        _T("9. 文件占用查看\n")
        _T("   - Drag and drop files to see which processes are locking them (uses Restart Manager API)\n")
        _T("   - List shows: file path, process name, PID, process type, process path\n")
        _T("   - 'End' terminates selected process; 'End All' terminates all listed processes\n")
        _T("   - 'Locate' opens the process's folder in Explorer\n")
        _T("   - 'Refresh' re-queries; 'Clear' empties the list\n")
        _T("   - Double-click or right-click for context menu\n")
        _T("   - Confirmation dialog before killing any process\n\n")

        _T("=== OTHER FEATURES ===\n\n")
        _T("   - Auto-clicker: check 'Auto Clicker' box to enable; press start key to begin clicking, stop key to stop\n")
        _T("     Configurable interval (ms) and start/stop keys in Settings; speed adjustment window appears when active\n")
        _T("   - Prevent auto-lock: check 'Prevent Lock' to keep screen on (SetThreadExecutionState)\n")
        _T("   - Auto-start with Windows: check 'Auto Start' to add to registry Run key\n")
        _T("   - 'Window Topmost' checkbox: keep toolbox always on top\n")
        _T("   - 'Minimize to Tray' checkbox: X button minimizes to system tray instead of closing\n")
        _T("   - System tray: double-click icon to restore; right-click for 'Show Window' or 'Exit'\n\n")

        _T("=== KEYBOARD SHORTCUTS ===\n\n")
        _T("   - Ctrl+Alt+Space: show/hide main window (global hotkey)\n")
        _T("   - Alt+1~6: switch to left tab pages 1-6\n")
        _T("   - Ctrl+Alt+D: start window locate mode\n")
        _T("   - F5: refresh current tab's list (process, startup, or other)\n")
        _T("   - Enter: apply volume / execute command when focus is in those input boxes\n\n")

        _T("=== CONFIGURATION ===\n\n")
        _T("   - File > Settings: configure all app paths (Bilibili, WeChat, QQ, VSCode, VS, Git Bash, Yuanbao), ")
        _T("folder paths (Study, Downloads, Screenshot, Sticky Note), URLs (MOOC, SDUCS), ")
        _T("auto-clicker interval and start/stop keys, AI vendor and API key\n")
        _T("   - Config file: config.ini in the same directory as the executable\n")
        _T("   - The AI vendor and API key can be configured in Settings > 'AI Assistant' section\n\n")

        _T("When answering user questions:\n")
        _T("   - Primary language: respond in Chinese by default, as the application UI is in Chinese\n")
        _T("   - If the user asks in English, respond in English; if the user asks in other languages, respond in that language\n")
        _T("   - If the user mixes languages, use the dominant language of their question\n")
        _T("   - Be concise and direct; provide step-by-step instructions when needed\n")
        _T("   - If the user asks about a feature you're unsure about, ask them to check the actual UI\n")
        _T("   - If the user encounters an error, suggest checking the config.ini file and file permissions\n")
        _T("   - This application requires administrator privileges for most operations\n");
}

void CMFCApplication1Dlg::OnBnClickedAiSend()
{
    CEdit* pInput = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_INPUT));
    if (!pInput) return;

    CString userMsg;
    pInput->GetWindowText(userMsg);
    userMsg.Trim();
    if (userMsg.IsEmpty()) return;

    pInput->SetWindowText(_T(""));

    if (m_aiHistory.empty())
    {
        m_aiHistory.push_back({ _T("system"), BuildSystemPrompt() });
    }

    m_aiHistory.push_back({ _T("user"), userMsg });

    CEdit* pHistory = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_HISTORY));
    if (pHistory)
    {
        CString current;
        pHistory->GetWindowText(current);
        current += _T("You: ") + userMsg + _T("\r\n");
        pHistory->SetWindowText(current);
        int nLen = pHistory->GetWindowTextLength();
        pHistory->SetSel(nLen, nLen);
    }

    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        if (pHistory)
        {
            CString current;
            pHistory->GetWindowText(current);
            current += _T("AI: [Error] Please configure API Key in File > Settings > AI Assistant.\r\n");
            pHistory->SetWindowText(current);
            int nLen = pHistory->GetWindowTextLength();
            pHistory->SetSel(nLen, nLen);
        }
        m_aiHistory.pop_back();
        return;
    }

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(FALSE);

    CAIApiClient::SendAsync(m_aiHistory, vendor, apiKey, model, m_hWnd);
}

void CMFCApplication1Dlg::OnBnClickedAiClear()
{
    m_aiHistory.clear();
    CEdit* pHistory = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_HISTORY));
    if (pHistory)
        pHistory->SetWindowText(_T(""));
}

LRESULT CMFCApplication1Dlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);

    CString* pResult = reinterpret_cast<CString*>(lParam);
    if (!pResult) return 0;

    CString response = *pResult;
    delete pResult;

    bool bSuccess = (wParam == 1);

    if (bSuccess)
    {
        m_aiHistory.push_back({ _T("assistant"), response });
    }
    else
    {
        if (!m_aiHistory.empty() && m_aiHistory.back().first == _T("user"))
            m_aiHistory.pop_back();
    }

    CEdit* pHistory = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_HISTORY));
    if (pHistory)
    {
        CString current;
        pHistory->GetWindowText(current);
        current += _T("AI: ") + response + _T("\r\n");
        pHistory->SetWindowText(current);
        int nLen = pHistory->GetWindowTextLength();
        pHistory->SetSel(nLen, nLen);
    }

    return 0;
}






