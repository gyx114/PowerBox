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
#include "GitCmdResultDlg.h"
#include "ProcessScanDlg.h"
#include "ConversationHistoryDlg.h"
#include "LocalizationManager.h"
#include "json.hpp"
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
#include <MsHTML.h>
#include <ExDisp.h>
#pragma comment(lib, "Ole32.lib")

#include <windows.h>
#include <processthreadsapi.h>
#include <userenv.h>
#include <shlwapi.h>
#pragma comment(lib, "Userenv.lib")

// ============================================================================
// WebBrowser event sink: intercepts BeforeNavigate2 to handle AI executable
// commands via the custom "http://127.0.0.1:1/exec/" URL scheme.
// ============================================================================
class CWebBrowserEventSink : public IDispatch
{
public:
    CWebBrowserEventSink(HWND hTargetWnd) : m_hTargetWnd(hTargetWnd) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IDispatch)
        {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) { delete this; return 0; }
        return ref;
    }

    // IDispatch (minimal implementation — only Invoke needed)
    STDMETHODIMP GetTypeInfoCount(UINT*) override { return E_NOTIMPL; }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }

    // Helper: extract URL from rgvarg[N] which may be VT_VARIANT|VT_BYREF or VT_BSTR directly
    static CString GetUrlParam(DISPPARAMS* pParams, int index)
    {
        if (index < 0 || index >= (int)pParams->cArgs)
            return CString();
        VARIANTARG& v = pParams->rgvarg[index];
        if (v.vt == (VT_VARIANT | VT_BYREF) && v.pvarVal)
        {
            if (v.pvarVal->vt == VT_BSTR && v.pvarVal->bstrVal)
                return CString(v.pvarVal->bstrVal);
        }
        else if (v.vt == VT_BSTR && v.bstrVal)
        {
            return CString(v.bstrVal);
        }
        return CString();
    }

    // Helper: cancel the navigation at rgvarg[0] (Cancel parameter)
    static void CancelNavigation(DISPPARAMS* pParams)
    {
        if (pParams->cArgs >= 1 &&
            pParams->rgvarg[0].vt == (VT_BOOL | VT_BYREF) &&
            pParams->rgvarg[0].pboolVal)
        {
            *pParams->rgvarg[0].pboolVal = VARIANT_TRUE;
        }
    }

    // Helper: handle http://127.0.0.1:1/exec/ URL — decode payload and post to main window
    static const CString kExecPrefix()
    {
        return CString(_T("http://127.0.0.1:1/exec/"));
    }

    bool HandleAppExecUrl(const CString& url)
    {
        int prefixLen = kExecPrefix().GetLength();
        if (url.Left(prefixLen) != kExecPrefix())
            return false;

        // URL-decode using UrlUnescapeA + UTF-8 (not UrlUnescapeW which uses ANSI code page)
        CString encoded = url.Mid(prefixLen);
        CStringA encodedUtf8((LPCSTR)CT2A(encoded, CP_UTF8));
        DWORD dwSize = (DWORD)encodedUtf8.GetLength() + 1;
        char* buf = encodedUtf8.GetBuffer((int)dwSize);
        UrlUnescapeA(buf, buf, &dwSize, URL_UNESCAPE_INPLACE);
        encodedUtf8.ReleaseBuffer((int)dwSize);
        CString decoded((LPCWSTR)CA2T(encodedUtf8, CP_UTF8));

        // Post to main window for processing
        ::PostMessage(m_hTargetWnd, WM_AI_EXECUTE_COMMAND, 0,
            reinterpret_cast<LPARAM>(_tcsdup(decoded)));
        return true;
    }

    STDMETHODIMP Invoke(DISPID dispid, REFIID, LCID, WORD, DISPPARAMS* pParams,
        VARIANT*, EXCEPINFO*, UINT*) override
    {
        // DISPID_BEFORENAVIGATE2 = 250 — fires before navigation
        //   rgvarg[0] = Cancel (VT_BOOL|VT_BYREF)
        //   rgvarg[5] = URL (VT_VARIANT|VT_BYREF or VT_BSTR)
        //   rgvarg[6] = pDisp (IDispatch*)
        if (dispid == 250 && pParams && pParams->cArgs >= 6)
        {
            CString url = GetUrlParam(pParams, 5);
            if (!url.IsEmpty() && url.Find(kExecPrefix()) == 0)
            {
                CancelNavigation(pParams);
                HandleAppExecUrl(url);
                return S_OK;
            }
        }

        // DISPID_NAVIGATEERROR = 271 — fires when navigation fails
        //   rgvarg[0] = Cancel (VT_BOOL|VT_BYREF)
        //   rgvarg[3] = URL (VT_VARIANT|VT_BYREF or VT_BSTR)
        //   rgvarg[4] = StatusCode (VT_I4)
        if (dispid == 271 && pParams && pParams->cArgs >= 4)
        {
            CString url = GetUrlParam(pParams, 3);
            if (!url.IsEmpty() && url.Find(kExecPrefix()) == 0)
            {
                CancelNavigation(pParams);
                // Try to handle the URL in case BeforeNavigate2 missed it
                HandleAppExecUrl(url);
                return S_OK;
            }
        }

        return S_OK;
    }

private:
    HWND m_hTargetWnd;
    LONG m_refCount = 1;
};

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
    ON_NOTIFY(NM_DBLCLK, IDC_LIST4, &CMFCApplication1Dlg::OnNMDblclkList4)
    ON_NOTIFY(NM_RCLICK, IDC_LIST4, &CMFCApplication1Dlg::OnNMRclickList4)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST5, &CMFCApplication1Dlg::OnNMDblclkList5)
    ON_BN_CLICKED(IDC_BTN_GIT_LOCATE, &CMFCApplication1Dlg::OnBnClickedGitLocate)
    ON_BN_CLICKED(IDC_BTN_GIT_CMD_WINDOW, &CMFCApplication1Dlg::OnBnClickedGitCmdWindow)
    ON_STN_CLICKED(IDC_STATIC_GIT_PATH, &CMFCApplication1Dlg::OnStnClickedGitPath)
    ON_COMMAND(ID_FILE_SETTINGS, &CMFCApplication1Dlg::OnFileSettings)
    ON_COMMAND(ID_FILE_EXIT, &CMFCApplication1Dlg::OnFileExit)
    ON_COMMAND(ID_HELP_ABOUT, &CMFCApplication1Dlg::OnHelpAbout)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST1, &CMFCApplication1Dlg::OnProcessColumnClick)
    ON_EN_CHANGE(IDC_EDIT_PROCESS_FILTER, &CMFCApplication1Dlg::OnProcessFilterChange)
    ON_BN_CLICKED(IDC_CHECK_PROCESS_REGEX, &CMFCApplication1Dlg::OnProcessFilterChange)
    ON_BN_CLICKED(IDC_BTN_PROCESS_REGEX_HELP, &CMFCApplication1Dlg::OnProcessRegexHelp)
    ON_COMMAND(IDM_PROCESS_AI_ANALYZE, &CMFCApplication1Dlg::OnProcessAiAnalyze)
    ON_BN_CLICKED(IDC_BTN_PROCESS_AI_SCAN, &CMFCApplication1Dlg::OnBnClickedProcessAiScan)
    // AI Assistant
    ON_BN_CLICKED(IDC_BUTTON_AI_SEND, &CMFCApplication1Dlg::OnBnClickedAiSend)
    ON_BN_CLICKED(IDC_BUTTON_AI_STOP, &CMFCApplication1Dlg::OnBnClickedAiStop)
    ON_BN_CLICKED(IDC_BUTTON_AI_CLEAR, &CMFCApplication1Dlg::OnBnClickedAiClear)
    ON_BN_CLICKED(IDC_BUTTON_AI_HISTORY, &CMFCApplication1Dlg::OnBnClickedAiHistory)
    ON_MESSAGE(WM_CONV_LOADED, &CMFCApplication1Dlg::OnConvLoaded)
    ON_MESSAGE(WM_AI_RESPONSE, &CMFCApplication1Dlg::OnAiResponse)
    ON_MESSAGE(WM_AI_STREAM_CHUNK, &CMFCApplication1Dlg::OnAiStreamChunk)
    ON_MESSAGE(WM_AI_STREAM_DONE, &CMFCApplication1Dlg::OnAiStreamDone)
    ON_MESSAGE(WM_AI_EXECUTE_COMMAND, &CMFCApplication1Dlg::OnAiExecuteCommand)
    ON_MESSAGE(WM_PROCESS_SCAN_START, &CMFCApplication1Dlg::OnProcessScanStart)
    ON_WM_TIMER()
END_MESSAGE_MAP()


// CMFCApplication1Dlg message handlers


BOOL CMFCApplication1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Load language setting
	CString langId = AfxGetApp()->GetProfileString(_T("Settings"), _T("Language"), _T("zh-CN"));
	CLocalizationManager::GetInstance().LoadLanguage(langId);

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
		auto& loc = CLocalizationManager::GetInstance();
		pCombo->ResetContent();
		pCombo->AddString(loc.GetString(_T("Shutdown"), _T("Restart1Min")));
		pCombo->AddString(loc.GetString(_T("Shutdown"), _T("Shutdown3Min")));
		pCombo->AddString(loc.GetString(_T("Shutdown"), _T("CustomTime")));
		CString defaultItem = loc.GetString(_T("Shutdown"), _T("Shutdown3Min"));
		int idx = pCombo->FindStringExact(-1, defaultItem);
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
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("NeedAdminRestart")), CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Warning")), MB_OK | MB_ICONWARNING);
		}
	}

	// Global hotkey
	CString strTitle;
	GetWindowText(strTitle);
	strTitle += CLocalizationManager::GetInstance().GetString(_T("MainDlg"), _T("WindowTitleSuffix"));
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
	TranslateUI();
	return TRUE;
}

// ========== Right-side quick-action tab ==========

void CMFCApplication1Dlg::InitQuickTab()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB_QUICK));
    if (!pTab) return;

    auto& loc = CLocalizationManager::GetInstance();
    pTab->InsertItem(0, loc.GetString(_T("MainDlg"), _T("QuickTab1")));
    pTab->InsertItem(1, loc.GetString(_T("MainDlg"), _T("QuickTab2")));
    pTab->InsertItem(2, loc.GetString(_T("MainDlg"), _T("QuickTab3")));

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

    // AI Assistant controls (right tab 0: Common)
    // WebBrowser is handled separately to avoid internal state loss on hide/show
    static const int kAiIds[] = {
        IDC_STATIC_AI_SEP, IDC_STATIC_AI_LABEL, IDC_COMBO_AI_VENDOR,
        IDC_EDIT_AI_INPUT, IDC_BUTTON_AI_SEND, IDC_BUTTON_AI_STOP, IDC_BUTTON_AI_CLEAR,
        IDC_BUTTON_AI_HISTORY
    };
    showGroup(kAiIds, _countof(kAiIds), nTab == 0);

    // WebBrowser: move off-screen instead of hiding to preserve internal state
    if (m_aiBrowser.m_hWnd && ::IsWindow(m_aiBrowser.m_hWnd))
    {
        if (nTab == 0)
        {
            // Restore to original position
            m_aiBrowser.SetWindowPos(nullptr,
                m_aiBrowserRect.left, m_aiBrowserRect.top,
                m_aiBrowserRect.Width(), m_aiBrowserRect.Height(),
                SWP_NOZORDER | SWP_NOACTIVATE);
            m_aiBrowser.ShowWindow(SW_SHOW);
        }
        else
        {
            // Move off-screen (hide without losing internal state)
            m_aiBrowser.SetWindowPos(nullptr,
                -10000, -10000,
                m_aiBrowserRect.Width(), m_aiBrowserRect.Height(),
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        }
    }
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

    // Disconnect WebBrowser event sink
    DisconnectAiBrowserEvents();

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

    // Auto-save current conversation
    SaveCurrentConversation();

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
        // Delete: End selected process
        if (pMsg->wParam == VK_DELETE)
        {
            CWnd* pFocus = CWnd::FromHandle(::GetFocus());
            if (pFocus)
            {
                int nID = pFocus->GetDlgCtrlID();
                // Only handle Delete in process list (IDC_LIST1)
                if (nID == IDC_LIST1)
                {
                    OnKillProcess();
                    return TRUE;
                }
            }
        }

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
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList2 = (CListCtrl*)GetDlgItem(IDC_LIST2);

    HWND hClicked = pWnd ? pWnd->GetSafeHwnd() : ::WindowFromPoint(point);

    // Process list right-click is handled by OnRclickProcessList (NM_RCLICK) which has the full menu
    // Skip to avoid duplicate popup menus

    // Right-click on startup management list
    if (pList2 && hClicked == pList2->GetSafeHwnd())
    {
        int nSel = pList2->GetNextItem(-1, LVNI_SELECTED);
        CMenu menu;
        menu.CreatePopupMenu();
        // Add and delete commands
        menu.AppendMenu(MF_STRING, 32772, loc.GetString(_T("Menu"), _T("AddStartup")));
        if (nSel != -1)
        {
            menu.AppendMenu(MF_STRING, 32773, loc.GetString(_T("Menu"), _T("DeleteStartup")));
            menu.AppendMenu(MF_STRING, 32806, loc.GetString(_T("Menu"), _T("CopyPath")));
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
            menu.AppendMenu(MF_STRING, 40002, loc.GetString(_T("Menu"), _T("CopyValue")));
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
            menu.AppendMenu(MF_STRING, 32805, loc.GetString(_T("Menu"), _T("Untopmost")));
            menu.AppendMenu(MF_STRING, 32807, loc.GetString(_T("Menu"), _T("Delete")));
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
                    menu.AppendMenu(MF_STRING, 32810, loc.GetString(_T("Menu"), _T("Untopmost")));
                else
                    menu.AppendMenu(MF_STRING, 32809, loc.GetString(_T("Menu"), _T("Topmost")));
                menu.AppendMenu(MF_STRING, 32808, loc.GetString(_T("Menu"), _T("Delete")));
                menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
            }
        }
        return;
    }
    // Note: Git tools list (IDC_LIST4) right-click is handled by OnNMRclickList4 (NM_RCLICK)
    // which provides the full menu (Execute/Copy/Edit/Delete). No OnContextMenu handler needed.
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
            auto& loc = CLocalizationManager::GetInstance();
            label.Format(loc.GetString(_T("WindowTab"), _T("TransparencyLabel")), percent);
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
        auto& loc = CLocalizationManager::GetInstance();
        m_nid.uFlags = NIF_INFO;
        m_nid.dwInfoFlags = NIIF_INFO;
        _tcscpy_s(m_nid.szInfoTitle, loc.GetString(_T("AutoClicker"), _T("StoppedTitle")));
        _tcscpy_s(m_nid.szInfo, loc.GetString(_T("AutoClicker"), _T("StoppedMsg")));
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

    // Create WebBrowser ActiveX control for AI chat rendering
    // Follow the same pattern as MarkdownDlg which works reliably
    CWnd* pPlaceholder = GetDlgItem(IDC_AI_BROWSER);
    if (pPlaceholder)
    {
        CRect rc;
        pPlaceholder->GetWindowRect(&rc);
        ScreenToClient(&rc);
        pPlaceholder->DestroyWindow();

        // Match MarkdownDlg: WS_VISIBLE|WS_CHILD only, no WS_BORDER, no WS_EX_CLIENTEDGE
        if (m_aiBrowser.CreateControl(CLSID_WebBrowser, nullptr,
            WS_VISIBLE | WS_CHILD, rc, this, IDC_AI_BROWSER))
        {
            // Save original rect for off-screen restore when switching tabs
            m_aiBrowserRect = rc;

            // Give the browser its real size immediately — a zero-size control
            // will never render even if content is written to its document
            m_aiBrowser.SetWindowPos(nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);

            LPUNKNOWN pUnk = m_aiBrowser.GetControlUnknown();
            if (pUnk)
            {
                IWebBrowser2* pWeb2 = nullptr;
                if (SUCCEEDED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
                {
                    BSTR bstrBlank = SysAllocString(L"about:blank");
                    pWeb2->Navigate(bstrBlank, nullptr, nullptr, nullptr, nullptr);
                    SysFreeString(bstrBlank);
                    pWeb2->Release();
                }
            }

            // Bring browser to front — tab control may cover it otherwise
            m_aiBrowser.BringWindowToTop();

            // Navigate("about:blank") is async — the document won't be ready yet.
            // Use a retry timer until SetAiBrowserHtml succeeds.
            // Preload a welcome message so the timer has actual content to write,
            // matching MarkdownDlg's approach (always passes real content to SetBrowserHtml).
            m_aiPendingHtml = BuildAiHtmlPage(
                _T("<div style='color:#888;text-align:center;padding-top:20px;'>")
                _T("AI Assistant Ready<br>")
                _T("<span style='font-size:13px;'>Ask me anything about this toolbox!</span>")
                _T("</div>"));
            SetTimer(1, 100, nullptr);
        }

        // Connect WebBrowser event sink for AI executable commands
        ConnectAiBrowserEvents();
    }

    // Stop button is initially disabled (no request to cancel)
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
}

// Translate menu items by command ID (recursive, also handles submenu popup headers)
// Popup menu header translations are done in TranslateUI() where CWnd::GetMenu() is available
static void TranslateMenuItemsOnly(CMenu* pMenu, const CLocalizationManager& loc)
{
    if (!pMenu) return;

    static const std::map<UINT, LPCTSTR> cmdMap = {
        {ID_FILE_SETTINGS, _T("Settings")},
        {ID_FILE_EXIT, _T("Exit")},
        {ID_VIEW_PROCESS, _T("ViewProcess")},
        {ID_VIEW_STARTUP, _T("ViewStartup")},
        {ID_VIEW_CLIPBOARD, _T("ViewClipboard")},
        {ID_VIEW_WINDOW, _T("ViewWindow")},
        {ID_VIEW_FILE, _T("ViewFile")},
        {ID_VIEW_GIT, _T("ViewGit")},
        {ID_TOOLS_WECHAT, _T("OpenWeChat")},
        {ID_TOOLS_QQ, _T("OpenQQ")},
        {ID_TOOLS_VSCODE, _T("OpenVSCode")},
        {ID_TOOLS_VS, _T("OpenVS")},
        {ID_TOOLS_BILIBILI, _T("OpenBilibili")},
        {ID_TOOLS_STUDY, _T("OpenStudy")},
        {ID_TOOLS_DOWNLOAD, _T("OpenDownload")},
        {ID_TOOLS_POWERSHELL, _T("OpenPowerShell")},
        {ID_TOOLS_WSL, _T("OpenWSL")},
        {ID_TOOLS_GITBASH, _T("OpenGitBash")},
        {ID_WINDOW_LOCATE, _T("WindowLocate")},
        {ID_WINDOW_UNTOPMOST, _T("WindowUntopmost")},
        {ID_WINDOW_CLOSE, _T("WindowClose")},
        {ID_TOOLS_MARKDOWN, _T("ToolsMarkdown")},
        {ID_TOOLS_ENCODING, _T("ToolsEncoding")},
        {ID_TOOLS_QRCODE, _T("ToolsQRCode")},
        {ID_TOOLS_SCREENSHOT_OCR, _T("ToolsScreenshotOCR")},
        {ID_TOOLS_BATCH_RENAME, _T("ToolsBatchRename")},
        {ID_TOOLS_CONTEXT_MENU, _T("ToolsContextMenu")},
        {ID_TOOLS_ENVVAR, _T("ToolsEnvVar")},
        {ID_TOOLS_FILELOCK, _T("ToolsFileLock")},
        {ID_TOOLS_STICKY_NOTE, _T("ToolsStickyNote")},
        {ID_HELP_ABOUT, _T("HelpAbout")},
        {ID_HELP_SHORTCUTS, _T("HelpShortcuts")},
        {ID_HELP_REGEX_GUIDE, _T("HelpRegex")},
        {ID_HELP_GITHUB, _T("HelpGitHub")},
    };

    // Map submenu content to popup header translation keys
    // Identifies a POPUP submenu by the first recognizable command ID inside it
    static const std::map<UINT, LPCTSTR> submenuPopupKeys = {
        {ID_TOOLS_MARKDOWN, _T("ToolsText")},
        {ID_TOOLS_QRCODE, _T("ToolsImage")},
        {ID_TOOLS_BATCH_RENAME, _T("ToolsFile")},
        {ID_TOOLS_CONTEXT_MENU, _T("ToolsSystem")},
    };

    int count = pMenu->GetMenuItemCount();
    for (int i = 0; i < count; i++)
    {
        CMenu* pSub = pMenu->GetSubMenu(i);
        if (pSub)
        {
            // Translate submenu popup header (POPUP items have no command ID)
            UINT id = pMenu->GetMenuItemID(i);
            if (id == (UINT)-1)
            {
                // Identify the submenu by scanning its content for a known command ID
                for (int j = 0; j < pSub->GetMenuItemCount(); j++)
                {
                    UINT subId = pSub->GetMenuItemID(j);
                    auto it = submenuPopupKeys.find(subId);
                    if (it != submenuPopupKeys.end())
                    {
                        CString text = loc.GetString(_T("Menu"), it->second);
                        if (!text.IsEmpty())
                            pMenu->ModifyMenu(i, MF_BYPOSITION, (UINT)-1, text);
                        break;
                    }
                }
            }
            TranslateMenuItemsOnly(pSub, loc);
        }
        else
        {
            UINT id = pMenu->GetMenuItemID(i);
            if (id != 0 && id != (UINT)-1)
            {
                auto it = cmdMap.find(id);
                if (it != cmdMap.end())
                {
                    CString text = loc.GetString(_T("Menu"), it->second);
                    pMenu->ModifyMenu(id, MF_BYCOMMAND, id, text);
                }
            }
        }
    }
}

void CMFCApplication1Dlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();

    // Window title
    SetWindowText(loc.GetString(_T("DlgCaption"), _T("MainDlg")));

    // Translate menu
    CMenu* pMenu = GetMenu();
    if (pMenu)
    {
        // Translate popup menu headers
        static const LPCTSTR popupKeys[] = {
            _T("File"), _T("View"), _T("Open"), _T("Window"),
            _T("Tools"), _T("Help")
        };
        int count = pMenu->GetMenuItemCount();
        for (int i = 0; i < count && i < _countof(popupKeys); i++)
        {
            CString text = loc.GetString(_T("Menu"), popupKeys[i]);
            if (!text.IsEmpty())
                pMenu->ModifyMenu(i, MF_BYPOSITION, pMenu->GetMenuItemID(i), text);
        }
        // Translate menu items by command ID
        for (int i = 0; i < count; i++)
        {
            CMenu* pSub = pMenu->GetSubMenu(i);
            if (pSub)
                TranslateMenuItemsOnly(pSub, loc);
        }
    }

    // ===== Process tab =====
    SetDlgItemText(IDC_CHECK_PROCESS_REGEX, loc.GetString(_T("MainCtrl"), _T("BtnRegex")));
    SetDlgItemText(IDC_BTN_PROCESS_AI_SCAN, loc.GetString(_T("MainCtrl"), _T("BtnAiScan")));
    SetDlgItemText(IDC_BTN_PROCESS_REGEX_HELP, loc.GetString(_T("MainCtrl"), _T("BtnRegexHelp")));

    // ===== Window tab =====
    SetDlgItemText(IDC_STATIC12, loc.GetString(_T("MainCtrl"), _T("LabelLocateHint")));
    SetDlgItemText(IDC_BUTTON19, loc.GetString(_T("MainCtrl"), _T("BtnLocateWindow")));
    SetDlgItemText(IDC_BUTTON15, loc.GetString(_T("WindowTab"), _T("ForceKillBtn")));
    SetDlgItemText(IDC_BUTTON16, loc.GetString(_T("WindowTab"), _T("ScreenshotBtn")));

    // ===== File tab =====
    SetDlgItemText(IDC_STATIC_PATH, loc.GetString(_T("MainCtrl"), _T("DropHint")));
    SetDlgItemText(IDC_BUTTON3, loc.GetString(_T("MainCtrl"), _T("BtnGenerate")));
    SetDlgItemText(IDC_BUTTON23, loc.GetString(_T("MainCtrl"), _T("BtnModify")));
    SetDlgItemText(IDC_BUTTON24, loc.GetString(_T("MainCtrl"), _T("BtnDelete")));
    SetDlgItemText(IDC_BUTTON25, loc.GetString(_T("MainCtrl"), _T("BtnCopyTo")));
    SetDlgItemText(IDC_BUTTON26, loc.GetString(_T("MainCtrl"), _T("BtnMoveTo")));
    // Group boxes (set via SetWindowText on the control)
    GetDlgItem(IDC_STATIC7)->SetWindowText(loc.GetString(_T("MainCtrl"), _T("GroupGenerate")));
    GetDlgItem(IDC_STATIC13)->SetWindowText(loc.GetString(_T("MainCtrl"), _T("GroupRename")));
    GetDlgItem(IDC_STATIC14)->SetWindowText(loc.GetString(_T("MainCtrl"), _T("GroupCopyMove")));

    // ===== Checkboxes (bottom area) =====
    SetDlgItemText(IDC_CHECK1, loc.GetString(_T("MainCtrl"), _T("CheckAutoStart")));
    SetDlgItemText(IDC_CHECK3, loc.GetString(_T("MainCtrl"), _T("CheckTopmost")));
    SetDlgItemText(IDC_CHECK4, loc.GetString(_T("MainCtrl"), _T("CheckAutoClicker")));
    SetDlgItemText(IDC_CHECK2, loc.GetString(_T("MainCtrl"), _T("CheckMinimizeToTray")));
    SetDlgItemText(IDC_CHECK5, loc.GetString(_T("MainCtrl"), _T("CheckPreventLock")));

    // ===== Git tab =====
    SetDlgItemText(IDC_STATIC_GIT_PATH, loc.GetString(_T("MainCtrl"), _T("GitDropHint")));
    SetDlgItemText(IDC_BUTTON30, loc.GetString(_T("GitTab"), _T("OpenGitHub")));
    SetDlgItemText(IDC_BUTTON32, loc.GetString(_T("GitTab"), _T("ClearPath")));
    SetDlgItemText(IDC_BUTTON31, loc.GetString(_T("GitTab"), _T("GitBash")));
    SetDlgItemText(IDC_BTN_GIT_CMD_WINDOW, loc.GetString(_T("GitTab"), _T("CmdWindow")));
    SetDlgItemText(IDC_BTN_GIT_LOCATE, loc.GetString(_T("GitTab"), _T("Locate")));

    // ===== Quick tab 1 - Favorites =====
    SetDlgItemText(IDC_BUTTON4, loc.GetString(_T("MainCtrl"), _T("BtnWeChat")));
    SetDlgItemText(IDC_BUTTON5, loc.GetString(_T("MainCtrl"), _T("BtnQQ")));
    SetDlgItemText(IDC_BUTTON8, loc.GetString(_T("MainCtrl"), _T("BtnBilibili")));
    SetDlgItemText(IDC_BUTTON22, loc.GetString(_T("MainCtrl"), _T("BtnYuanbao")));
    SetDlgItemText(IDC_BUTTON7, loc.GetString(_T("MainCtrl"), _T("BtnVS")));
    SetDlgItemText(IDC_BUTTON6, loc.GetString(_T("MainCtrl"), _T("BtnVSCode")));
    SetDlgItemText(IDC_BUTTON9, loc.GetString(_T("MainCtrl"), _T("BtnStudy")));
    SetDlgItemText(IDC_BUTTON11, loc.GetString(_T("MainCtrl"), _T("BtnMOOC")));
    SetDlgItemText(IDC_BUTTON21, loc.GetString(_T("MainCtrl"), _T("BtnDownloads")));
    SetDlgItemText(IDC_BUTTON10, loc.GetString(_T("MainCtrl"), _T("BtnSDUCS")));
    SetDlgItemText(IDC_BUTTON29, loc.GetString(_T("MainCtrl"), _T("BtnLeetCode")));
    SetDlgItemText(IDC_BUTTON33, loc.GetString(_T("MainCtrl"), _T("BtnNextTrack")));

    // ===== Quick tab 2 - System =====
    SetDlgItemText(IDC_STATIC_QUICK_SHUTDOWN, loc.GetString(_T("MainCtrl"), _T("LabelShutdown")));
    SetDlgItemText(IDC_BUTTON1, loc.GetString(_T("MainCtrl"), _T("BtnExecute")));
    SetDlgItemText(IDC_BUTTON2, loc.GetString(_T("MainCtrl"), _T("BtnCancelShutdown")));
    SetDlgItemText(IDC_STATIC_QUICK_HOUR, loc.GetString(_T("MainCtrl"), _T("LabelHourUnit")));
    SetDlgItemText(IDC_STATIC_QUICK_MIN, loc.GetString(_T("MainCtrl"), _T("LabelMinuteUnit")));
    SetDlgItemText(IDC_STATIC_QUICK_SEC, loc.GetString(_T("MainCtrl"), _T("LabelSecondUnit")));
    SetDlgItemText(IDC_STATIC_QUICK_VOLUME, loc.GetString(_T("MainCtrl"), _T("LabelVolume")));
    SetDlgItemText(IDC_BUTTON12, loc.GetString(_T("MainCtrl"), _T("BtnApply")));
    SetDlgItemText(IDC_BUTTON13, loc.GetString(_T("MainCtrl"), _T("BtnMute")));
    SetDlgItemText(IDC_STATIC_QUICK_SYSMGMT, loc.GetString(_T("MainCtrl"), _T("LabelSystem")));
    SetDlgItemText(IDC_BUTTON20, loc.GetString(_T("MainCtrl"), _T("BtnTaskManager")));

    // ===== Quick tab 3 - Tools =====
    SetDlgItemText(IDC_STATIC_QUICK_CMDLINE, loc.GetString(_T("MainCtrl"), _T("LabelCmdLine")));
    SetDlgItemText(IDC_STATIC_QUICK_RUNCMD, loc.GetString(_T("MainCtrl"), _T("LabelRunCmd")));
    SetDlgItemText(IDC_BUTTON17, loc.GetString(_T("MainCtrl"), _T("BtnRun")));
    SetDlgItemText(IDC_BUTTON18, loc.GetString(_T("MainCtrl"), _T("BtnClear")));

    // ===== AI assistant section =====
    SetDlgItemText(IDC_STATIC_AI_LABEL, loc.GetString(_T("Settings"), _T("TabAI")));
    SetDlgItemText(IDC_BUTTON_AI_SEND, loc.GetString(_T("MainCtrl"), _T("BtnSend")));
    SetDlgItemText(IDC_BUTTON_AI_STOP, loc.GetString(_T("MainCtrl"), _T("BtnStop")));
    SetDlgItemText(IDC_BUTTON_AI_CLEAR, loc.GetString(_T("MainCtrl"), _T("BtnNewChat")));
    SetDlgItemText(IDC_BUTTON_AI_HISTORY, loc.GetString(_T("MainCtrl"), _T("BtnHistory")));
}

CString CMFCApplication1Dlg::BuildSystemPrompt()
{
    CString lang = CLocalizationManager::GetInstance().GetCurrentLanguage();
    if (lang == _T("en-US"))
    {
        return _T("You are an AI assistant integrated into a Windows MFC toolbox application. ")
        _T("Your role is to help users understand and use this toolbox, troubleshoot issues, and answer related questions.\n\n")

        _T("=== Application Overview ===\n\n")
        _T("This is a multi-functional Windows toolbox with 6 left-side tabs, ")
        _T("3 right-side quick-action sub-tabs (Favorites/System/Tools), and 9 tools in the menu bar.\n")
        _T("The application runs with administrator privileges and supports minimizing to system tray.\n\n")

        _T("=== Left-Side Tabs (6 tabs, switch via Alt+1~6 or View menu) ===\n\n")

        _T("1. Process Management (Tab 1)\n")
        _T("   - Shows all running processes: name, PID, full path, memory usage (KB), CPU usage (%)\n")
        _T("   - Click column headers to sort ascending/descending (arrow indicator); CPU% supports sorting\n")
        _T("   - Filter box: enter keywords to filter by name and path (CPU% is not searchable); check \"Regex\" to enable regex filtering\n")
        _T("   - Right-click process: \"End Process\" (WM_CLOSE first, then TerminateProcess), \"End All Same Name\", \"Locate\" (open exe directory in Explorer), \"AI Analyze\" (analyze process security via AI)\n")
        _T("   - \"AI Scan\" button: scans all processes via AI, opens a new window listing suspicious/unnecessary processes with risk levels and AI analysis results\n")
        _T("   - AI Scan window supports: end process, locate, batch end all, right-click menu (end/locate/copy path)\n")
        _T("   - AI analysis checks: process name, path, digital signature status, determines if malicious/unnecessary\n")
        _T("   - F5 to refresh process list\n")
        _T("   - \"Help\" button opens regex reference guide\n\n")

        _T("2. Startup Management (Tab 2)\n")
        _T("   - Shows current user startup entries (from HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run)\n")
        _T("   - Displays startup name and command line\n")
        _T("   - Right-click menu: \"Add Startup Entry\" (select .exe via file dialog), \"Remove Startup Entry\", \"Copy Path\"\n")
        _T("   - Double-click to copy startup command line\n")
        _T("   - F5 to refresh startup list\n\n")

        _T("3. Clipboard (Tab 3)\n")
        _T("   - Real-time clipboard monitoring: automatically records last 10 copied text items\n")
        _T("   - Double-click any entry to re-copy to clipboard\n")
        _T("   - Consecutive duplicate entries are automatically deduplicated\n\n")

        _T("4. Window Tools (Tab 4)\n")
        _T("   - \"Locate Window\" button (or Ctrl+Alt+D): click target window to capture its details\n")
        _T("   - Window details list shows: handle, title, process name, PID, executable path, position/size\n")
        _T("   - Topmost list: manage topmost windows; right-click to un-topmost or delete\n")
        _T("   - History list: previously located windows; right-click to topmost/un-topmost or delete\n")
        _T("   - Click items in topmost or history list to load their info in the details list\n")
        _T("   - Transparency slider: adjust selected window opacity (10%-100%)\n")
        _T("   - \"Force Kill\": terminate the process of the selected window\n")
        _T("   - \"Capture to Clipboard\": capture selected window as PNG, save to configured directory and clipboard\n")
        _T("   - \"Window Topmost\" checkbox: keep the toolbox itself always on top\n")
        _T("   - Popup menu when locating: locate, cancel all topmost, close window\n\n")

        _T("5. File Management (Tab 5)\n")
        _T("   - Drop files to show their path; auto-fills filename and extension for renaming\n")
        _T("   - Drop folders to open batch rename dialog\n")
        _T("   - \"Generate\" button: creates a copy in the same directory (copy name from edit box, default configurable in File > Settings)\n")
        _T("   - \"Modify\" button: validates filename (no illegal characters), checks target doesn't exist, then renames\n")
        _T("   - \"Delete\": moves file to recycle bin (not permanent deletion)\n")
        _T("   - \"Copy to\"/\"Move to\": select target folder, then copy or move the dropped file\n\n")

        _T("6. Git Toolbox (Tab 6)\n")
        _T("   - Preloaded with 20 common Git commands (init, add, commit, push, pull, clone, status, branch, checkout, merge, log, restore, etc.)\n")
        _T("   - Drop files/folders to set Git working directory\n")
        _T("   - Shows current working directory and repo status (branch name or 'Not a Git repo')\n")
        _T("   - \"Locate\" button: browse for a folder to use as Git working directory\n")
        _T("   - \"Command Window\" button: opens Git command result dialog (AI command generation + command list + execution output)\n")
        _T("   - \"Open GitHub\": opens https://github.com/ in default browser\n")
        _T("   - \"Clear Path\": clears Git working directory\n")
        _T("   - \"Git Bash\" button: launches Git Bash in the current working directory\n")
        _T("   - Double-click a command in the list to copy to clipboard\n")
        _T("   - Right-click command: execute/copy/edit/delete\n")
        _T("   - Execute: runs commands via bash.exe (derived from GitBashPath config) in a modeless result window\n")
        _T("   - Git Command Result dialog: modeless window with AI command generation, temporary command list, and execution output\n")
        _T("   - Command list in result dialog is temporary — commands are not saved to config\n")
        _T("   - Commands can be configured in config.ini under [GitCommands] section (Cmd1~Cmd99, format: 'description|command')\n")
        _T("   - When Git working directory is set, AI command generation automatically injects real-time git status (branch, status, log, remote, config)\n\n")

        _T("=== Right-Side Quick Actions (3 sub-tabs: Favorites / System / Tools) ===\n\n")

        _T("\"Favorites\" tab:\n")
        _T("   - Quick launch buttons: WeChat, QQ, Bilibili, Yuanbao, VS, VSCode\n")
        _T("   - Open folders: Study, Downloads\n")
        _T("   - Open URLs: MOOC, SDUCS, LeetCode, GitHub\n")
        _T("   - \"Next Track\" button: sends ']' key to Bilibili player window, or global media next track\n\n")

        _T("\"System\" tab:\n")
        _T("   - Shutdown/Restart: dropdown with \"Restart in 1 Minute\", \"Shutdown in 3 Minutes\", \"Shutdown at Set Time\" (set hours/min/sec)\n")
        _T("   - \"Execute\" button triggers shutdown/restart; \"Cancel Shutdown\" button aborts the operation\n")
        _T("   - Volume: slider (0-100), input box (Enter to apply), \"Apply\" button, \"Mute\" (0%)\n")
        _T("   - \"Task Manager\" button opens Windows Task Manager\n\n")

        _T("\"Tools\" tab:\n")
        _T("   - \"PowerShell\": select normal or admin mode\n")
        _T("   - \"WSL\": launches WSL terminal\n")
        _T("   - Run command input: enter exe path, URL, or cmd command, press Enter to execute\n")
        _T("   - \"Clear\" button clears the command input\n\n")

        _T("=== Menu Bar: Tools(&T) (9 tools, divided into 4 sub-menus + 1 direct item) ===\n\n")
        _T("Menu hierarchy: Tools > Text Tools / Image Tools / File Tools / System Tools / Sticky Note\n\n")

        _T("Text Tools(&T) submenu:\n")
        _T("  1. Markdown Preview\n")
        _T("     - Left editing panel + right rendered preview with draggable splitter\n")
        _T("     - \"Open\" button or drop .md files\n")
        _T("     - Real-time preview, updates as you type\n")
        _T("     - Supports: headings, bold, italic, inline code, code blocks, links, blockquotes, strikethrough, lists, tables, horizontal rules\n")
        _T("     - GitHub-style CSS rendering; max file size 10MB\n\n")
        _T("  2. Encoding Converter\n")
        _T("     - \"Open\" or drop text files (txt, md, csv, log, etc.)\n")
        _T("     - Auto-detect source encoding (BOM check → UTF-8 validation → GBK fallback)\n")
        _T("     - Left panel shows source encoding interpretation; right panel shows target encoding interpretation\n")
        _T("     - Supported encodings: UTF-8, UTF-8 BOM, UTF-16LE, UTF-16LE BOM, UTF-16BE, GBK, Big5, Shift-JIS, Latin-1\n")
        _T("     - \"Save As\" exports with target encoding; \"Overwrite\" replaces original file (moves original to recycle bin first)\n")
        _T("     - Max file size 10MB\n\n")

        _T("Image Tools(&I) submenu:\n")
        _T("  3. QR Code Generator\n")
        _T("     - Enter text or URL, click \"Generate QR Code\" to create a QR code image\n")
        _T("     - \"Copy to Clipboard\" copies the QR code image; \"Save\" exports as PNG or BMP\n")
        _T("     - QR code has 4px white margin\n\n")
        _T("  4. Screenshot OCR\n")
        _T("     - Click \"Start Capture\" to hide the window, then drag to select a screen region for capture\n")
        _T("     - Automatically runs OCR on the captured region (small images are 2x upscaled for accuracy)\n")
        _T("     - Language dropdown: Chinese, English, Japanese, Korean\n")
        _T("     - \"Translate >>\" button translates OCR results via MyMemory API (free, 10-second timeout)\n")
        _T("     - \"Copy Result\" copies translated text (or original OCR text if no translation)\n")
        _T("     - Press ESC to cancel capture\n\n")

        _T("File Tools(&F) submenu:\n")
        _T("  5. Folder Processing\n")
        _T("     - Tab 1 \"Folder Operations\": list subfolders, rename/move/delete selected folders\n")
        _T("     - Tab 2 \"Batch File Processing\":\n")
        _T("       * Rename rules: add prefix/suffix, find & replace (supports regex)\n")
        _T("       * Auto numbering: start number, before or after extension\n")
        _T("       * Match delete: regex-based file deletion to recycle bin, supports invert selection\n")
        _T("       * Ignore rules: by extension or filename pattern (regex), manually ignore/unignore\n")
        _T("       * Track rules: only process tracked files (overrides ignore settings)\n")
        _T("     - File list supports drag-to-reorder with blue insertion line\n")
        _T("     - Right-click menu: ignore, track, mark for deletion, modify extension, move up/down, locate in Explorer\n")
        _T("     - \"Preview\" shows rename results; \"Execute\" applies changes; \"Undo\" reverts last rename\n")
        _T("     - \"Reset All\" clears all rules and marks; F5 refreshes file list\n")
        _T("     - \"AI Assistant\" opens the AI batch rename helper, allowing natural language rename requests, AI generates filename mappings, can apply to batch file processing, supports stacking with other batch operations\n\n")

        _T("System Tools(&S) submenu:\n")
        _T("  6. Context Menu Manager\n")
        _T("     - Scan and manage Windows context menu items\n")
        _T("     - Scene dropdown: 28+ scenes (All, Files, Folders, Directory Background, Desktop, Drives, etc.)\n")
        _T("     - 14 common extension presets (.jpg, .png, .txt, .pdf, etc.) + custom extension query\n")
        _T("     - List shows: location, display name, type (Static/ShellEx), visibility, key name, command\n")
        _T("     - Right-click: enable/disable items, custom name parsing, locate in registry\n")
        _T("     - \"Folder Context Menu: Open with this Program\" checkbox: adds/removes this tool from folder context menu\n")
        _T("     - \"Win11 Classic Menu (Shift+Right-click)\" checkbox: toggles Win11 old/new right-click menu style (requires Explorer restart)\n")
        _T("     - \"Rebuild Dictionary\": queries ShellEx display names via COM and caches them\n")
        _T("     - \"Dictionary Path\": configure custom dictionary folder; \"Open Dictionary\" opens it in Explorer\n")
        _T("     - F5 to refresh; disabled items use LegacyDisable + ProgrammaticAccessOnly mechanism\n")
        _T("     - Supports AI parsing for items not translated by the dictionary\n\n")
        _T("  7. Environment Variable Manager\n")
        _T("     - Top list: system variables; bottom list: user variables\n")
        _T("     - Search box: real-time filtering of both lists\n")
        _T("     - \"Add\": select system or user scope, enter variable name and value\n")
        _T("     - \"Edit\" or double-click: PATH variable opens dedicated editor; other variables open simple input dialog\n")
        _T("     - \"Delete\": removes selected variable (requires confirmation)\n")
        _T("     - \"Export\": saves all variables to .txt or .env file\n")
        _T("     - Right-click: edit, delete, copy name, copy value\n")
        _T("     - PATH editor: displays entries as separate lines; supports add/delete/move up/move down\n")
        _T("     - Auto-backup: automatically backs up current value to temp folder with timestamp before modification\n")
        _T("     - F5 to refresh\n\n")
        _T("  8. File Lock Viewer\n")
        _T("     - Drop files to view which processes are locking them (uses Restart Manager API)\n")
        _T("     - List shows: file path, process name, PID, process type, process path\n")
        _T("     - \"End\" terminates the selected process; \"End All\" terminates all listed processes\n")
        _T("     - \"Locate\" opens the process folder in Explorer\n")
        _T("     - \"Refresh\" re-queries; \"Clear\" clears the list\n")
        _T("     - Right-click for context menu\n")
        _T("     - Confirmation dialog shown before terminating processes\n\n")

        _T("Direct menu items (not in submenus):\n")
        _T("  9. Sticky Note\n")
        _T("     - Auto-starts on program launch, positioned at right 3/5 of screen\n")
        _T("     - Initial state: title bar only; double-click title bar to expand\n")
        _T("     - Expanded state: X button collapses to title bar; minimize button collapses to title bar\n")
        _T("     - Collapsed state: X button exits; double-click title bar to expand\n")
        _T("     - Right-click title bar: \"Exit Sticky Note\"\n")
        _T("     - Content auto-saves to sticky_note.txt in the configured folder (UTF-8)\n")
        _T("     - \"Browse\" button changes the save folder\n\n")

        _T("=== Other Features ===\n\n")
        _T("Located at the bottom-left:\n\n")
        _T("   - Auto Clicker: check \"Auto Clicker\" to enable; press start key to start clicking, stop key to stop\n")
        _T("     Configurable in Settings: interval (ms) and start/stop keys; shows speed adjustment window when enabled\n")
        _T("   - Prevent Auto Lock: check \"Prevent Auto Lock\" to keep screen on (SetThreadExecutionState)\n")
        _T("   - Auto Start: check \"Auto Start\" to add to registry Run key\n")
        _T("   - \"Window Topmost\" checkbox: keep the toolbox always on top\n\n")

        _T("Located at the top-right, near the close button:\n\n")
        _T("   - \"Minimize to Tray\" checkbox: clicking X minimizes to system tray instead of closing\n\n")

        _T("Located in the system tray:\n\n")
        _T("   - System tray: double-click icon to restore window; right-click menu \"Show Window\" or \"Exit\"\n\n")

        _T("=== Shortcuts ===\n\n")
        _T("   - Ctrl+Alt+Space: show/hide main window (global hotkey)\n")
        _T("   - Alt+1~6: switch to left-side tabs 1-6\n")
        _T("   - Ctrl+Alt+D: start window locate mode\n")
        _T("   - F5: refresh current tab's list (processes, startups, etc.)\n")
        _T("   - Enter: apply in volume input box or execute in command input box\n\n")

        _T("=== Configuration ===\n\n")
        _T("   - File > Settings: configure all application paths (Bilibili, WeChat, QQ, VSCode, VS, Git Bash, Yuanbao), ")
        _T("folder paths (Study, Downloads, Screenshot, Sticky Note), URLs (MOOC, SDUCS), ")
        _T("auto-clicker interval and start/stop keys, AI vendor and API key\n")
        _T("   - Config file: config.ini in the executable directory\n")
        _T("   - AI vendor and API key can be configured in Settings > \"AI Assistant\" section\n\n")

        _T("When answering questions:\n")
        _T("   - Answer in the same language the user uses\n")
        _T("   - If the user mixes languages, use the primary language of the question\n")
        _T("   - Keep answers concise and direct, provide step-by-step guidance when needed\n")
        _T("   - If unsure about a feature, suggest the user check the actual interface\n")
        _T("   - If the user encounters an error, suggest checking the config.ini file and file permissions\n")
        _T("   - Most operations in this application require administrator privileges\n")
        _T("   - If the user requests executing system commands, managing files, managing processes, etc., you can return executable commands\n\n")

        _T("=== Executable Command Protocol ===\n\n")
        _T("When you need to return an executable command, use the following format:\n\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"command to execute\",\n")
        _T("  \"purpose\": \"purpose description\",\n")
        _T("  \"risk\": \"low/medium/high\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("Risk level description:\n")
        _T("- low: harmless operations (e.g., opening folders, displaying info)\n")
        _T("- medium: operations with potential impact (e.g., modifying files, restarting processes)\n")
        _T("- high: high-risk operations (e.g., deleting files, modifying registry, formatting disk)\n\n")
        _T("Note: Commands containing keywords like del, format, reg delete, net user are automatically escalated to high risk.\n")
        _T("High-risk commands require the user to type \"确认执行\" (Confirm Execute) to proceed.\n\n")
        _T("Command execution feedback:\n")
        _T("- After execution, stdout/stderr output is captured and displayed in the WebBrowser\n")
        _T("- Output includes: command stdout, stderr, exit code\n")
        _T("- For commands like echo, dir, ipconfig that produce output, users can see the results directly in the conversation\n")
        _T("- Execution timeout is 30 seconds; the process will be terminated after timeout\n");
    }

    return _T("你是一个集成在 Windows MFC 工具箱应用程序中的 AI 助手。")
        _T("你的职责是帮助用户理解和使用这个工具箱，排查问题，并回答相关问题。\n\n")

        _T("=== 应用概述 ===\n\n")
        _T("这是一个多功能 Windows 工具箱，包含 6 个左侧标签页、")
        _T("3 个右侧快捷操作子标签页（常用/系统/工具），以及菜单栏中的 9 个工具。\n")
        _T("应用程序以管理员权限运行，支持最小化到系统托盘。\n\n")

        _T("=== 左侧标签页（6 个标签页，可通过 Alt+1~6 或视图菜单切换） ===\n\n")

        _T("1. 进程管理（标签页 1）\n")
        _T("   - 显示所有运行中的进程：名称、PID、完整路径、内存占用（KB）、CPU 占用（%）\n")
        _T("   - 点击列标题可升序/降序排序（显示箭头指示器）；CPU% 支持排序\n")
        _T("   - 过滤框：输入关键字按名称和路径过滤进程（CPU% 不作为搜索条件）；勾选\"正则\"启用正则过滤\n")
        _T("   - 右键进程：\"结束进程\"（先 WM_CLOSE 再 TerminateProcess）、\"结束所有同名进程\"、\"定位\"（在资源管理器中打开可执行文件所在目录）、\"AI分析\"（通过 AI 分析进程安全性）\n")
        _T("   - \"AI扫描\"按钮：通过 AI 扫描所有进程，打开新窗口列出可疑/无用进程，附带风险等级和 AI 分析结果\n")
        _T("   - AI扫描窗口支持：结束进程、定位、批量结束全部、右键菜单（结束/定位/复制路径）\n")
        _T("   - AI分析检查：进程名称、路径、数字签名状态，判断是否为恶意/无用进程\n")
        _T("   - F5 刷新进程列表\n")
        _T("   - \"帮助\"按钮打开正则表达式参考指南\n\n")

        _T("2. 启动项管理（标签页 2）\n")
        _T("   - 显示当前用户的启动项（来自 HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run）\n")
        _T("   - 显示启动项名称和命令行\n")
        _T("   - 右键菜单：\"添加启动项\"（通过文件对话框选择 .exe）、\"删除启动项\"、\"复制路径\"\n")
        _T("   - 双击复制启动项的命令行\n")
        _T("   - F5 刷新启动项列表\n\n")

        _T("3. 剪贴板（标签页 3）\n")
        _T("   - 实时剪贴板监控：自动记录最近 10 次复制的文本\n")
        _T("   - 双击任意条目可重新复制到剪贴板\n")
        _T("   - 连续重复的条目自动去重\n\n")

        _T("4. 窗口处理（标签页 4）\n")
        _T("   - \"定位窗口\"按钮（或 Ctrl+Alt+D）：点击目标窗口以捕获其详细信息\n")
        _T("   - 窗口详情列表显示：句柄、标题、进程名、PID、可执行文件路径、位置/尺寸\n")
        _T("   - 置顶列表：管理已置顶的窗口；右键可取消置顶或删除\n")
        _T("   - 历史列表：之前定位过的窗口；右键可置顶/取消置顶或删除\n")
        _T("   - 点击置顶列表或历史列表中的条目，可在详情列表中加载其信息\n")
        _T("   - 透明度滑块：调整选中窗口的不透明度（10%-100%）\n")
        _T("   - \"强制结束进程\"：终止选中窗口所属的进程\n")
        _T("   - \"截图到剪贴板\"：将选中窗口截取为 PNG，保存到配置目录和剪贴板\n")
        _T("   - \"窗口置顶\"复选框：保持工具箱本身始终在最前\n")
        _T("   - 定位时窗口弹出菜单：定位、取消全部置顶、关闭窗口\n\n")

        _T("5. 文件管理（标签页 5）\n")
        _T("   - 拖放文件以显示其路径；自动填充文件名和扩展名以便重命名\n")
        _T("   - 拖放文件夹以打开批量重命名对话框\n")
        _T("   - \"生成\"按钮：在同一目录下创建副本（副本名称为编辑框中内容，默认值可在文件 > 设置中配置）\n")
        _T("   - \"修改\"按钮：验证文件名（无非法字符）、检查目标不存在、然后重命名\n")
        _T("   - \"删除\"：将文件移入回收站（非永久删除）\n")
        _T("   - \"复制到\"/\"移动到\"：选择目标文件夹，然后复制或移动拖放的文件\n")

        _T("6. git工具箱（标签页 6）\n")
        _T("   - 预载 20 条常用 Git 命令（init、add、commit、push、pull、clone、status、branch、checkout、merge、log、restore 等）\n")
        _T("   - 拖放文件/文件夹以设置 Git 工作目录\n")
        _T("   - 显示当前工作目录和仓库状态（分支名或'非Git仓库'）\n")
        _T("   - \"定位\"按钮：浏览文件夹以用作 Git 工作目录\n")
        _T("   - \"命令窗口\"按钮：打开 Git 命令结果对话框（AI 命令生成 + 命令列表 + 执行输出）\n")
        _T("   - \"打开github\"：在默认浏览器中打开 https://github.com/\n")
        _T("   - \"清空路径\"：清除 Git 工作目录\n")
        _T("   - \"git bash\"按钮：在当前工作目录中启动 Git Bash\n")
        _T("   - 双击列表中的命令可复制到剪贴板\n")
        _T("   - 右键命令：执行/复制/编辑/删除\n")
        _T("   - 执行：通过 bash.exe（从 GitBashPath 配置推导）在非模态结果窗口中运行命令\n")
        _T("   - Git 命令结果对话框：非模态窗口，包含 AI 命令生成、临时命令列表和执行输出\n")
        _T("   - 结果对话框中的命令列表为临时显示——命令不会保存到配置\n")
        _T("   - 命令可在 config.ini 的 [GitCommands] 节中配置（Cmd1~Cmd99，格式：'说明|命令'）\n")
        _T("   - 设置了 Git 工作目录后，AI 命令生成时会自动注入实时 git 状态（分支、状态、日志、远程、配置）\n\n")

        _T("=== 右侧快捷操作（3 个子标签页：常用 / 系统 / 工具） ===\n\n")

        _T("「常用」标签页：\n")
        _T("   - 快速启动按钮：微信、QQ、B站、元宝、VS、VSCode\n")
        _T("   - 打开文件夹：学习、下载\n")
        _T("   - 打开网址：MOOC、SDUCS、LeetCode、GitHub\n")
        _T("   - \"下一首\"按钮：向B站播放器窗口发送 ']' 键，或全局媒体下一曲\n\n")

        _T("「系统」标签页：\n")
        _T("   - 关机/重启：下拉菜单包含\"1分钟后重启\"、\"3分钟后关机\"、\"自定义时间关机\"（可设置时/分/秒）\n")
        _T("   - \"执行\"按钮触发关机/重启；\"解除关机\"按钮中止操作\n")
        _T("   - 音量：滑块（0-100）、输入框（回车应用）、\"应用\"按钮、\"静音\"（0%）\n")
        _T("   - \"任务管理器\"按钮打开 Windows 任务管理器\n\n")

        _T("「工具」标签页：\n")
        _T("   - \"PowerShell\"：选择普通或管理员模式\n")
        _T("   - \"WSL\"：启动 WSL 终端\n")
        _T("   - 运行命令输入框：输入 exe 路径、URL 或 cmd 命令，按回车执行\n")
        _T("   - \"清空\"按钮清除命令输入\n\n")

        _T("=== 菜单栏：工具(&T)（9 个工具，分 4 个子菜单 + 1 个直接项） ===\n\n")
        _T("菜单层级：工具 > 文本工具 / 图像工具 / 文件工具 / 系统工具 / 简易便签\n\n")

        _T("文本工具(&T) 子菜单：\n")
        _T("  1. Markdown 预览\n")
        _T("     - 左侧编辑面板 + 右侧渲染预览，分隔条可拖动\n")
        _T("     - \"打开\"按钮或拖放 .md 文件\n")
        _T("     - 实时预览，随输入更新\n")
        _T("     - 支持：标题、粗体、斜体、行内代码、代码块、链接、引用、删除线、列表、表格、水平分割线\n")
        _T("     - GitHub 风格 CSS 渲染；最大文件大小 10MB\n\n")
        _T("  2. 编码转换\n")
        _T("     - \"打开\"或拖放文本文件（txt、md、csv、log 等）\n")
        _T("     - 自动检测源编码（BOM 检查 → UTF-8 验证 → GBK 回退）\n")
        _T("     - 左侧面板显示源编码解读；右侧面板显示目标编码解读\n")
        _T("     - 支持的编码：UTF-8、UTF-8 BOM、UTF-16LE、UTF-16LE BOM、UTF-16BE、GBK、Big5、Shift-JIS、Latin-1\n")
        _T("     - \"另存为\"以目标编码导出；\"覆盖\"替换原文件（先将原文件移至回收站）\n")
        _T("     - 最大文件大小 10MB\n\n")

        _T("图像工具(&I) 子菜单：\n")
        _T("  3. 二维码生成\n")
        _T("     - 输入文本或 URL，点击\"生成二维码\"创建二维码图片\n")
        _T("     - \"复制到剪贴板\"将二维码图片复制到剪贴板；\"保存\"导出为 PNG 或 BMP\n")
        _T("     - 二维码带有 4px 白色边距\n\n")
        _T("  4. 截图OCR\n")
        _T("     - 点击\"开始截图\"隐藏窗口，然后拖拽选择屏幕区域进行截图\n")
        _T("     - 自动对截取区域运行 OCR（小图片会 2 倍放大以提高准确率）\n")
        _T("     - 语言下拉框：中文、英文、日文、韩文\n")
        _T("     - \"翻译 >>\"按钮通过 MyMemory API（免费，10 秒超时）翻译 OCR 结果\n")
        _T("     - \"复制结果\"复制翻译后的文本（若无翻译则复制原始 OCR 文本）\n")
        _T("     - 按 ESC 取消截图\n\n")

        _T("文件工具(&F) 子菜单：\n")
        _T("  5. 文件夹处理\n")
        _T("     - 标签页 1\"文件夹操作\"：列出子文件夹、重命名/移动/删除选中的文件夹\n")
        _T("     - 标签页 2\"文件批量处理\"：\n")
        _T("       * 重命名规则：添加前缀/后缀、查找替换（支持正则）\n")
        _T("       * 自动编号：起始编号、位于扩展名前或后\n")
        _T("       * 匹配删除：基于正则表达式的文件删除到回收站，支持反选\n")
        _T("       * 忽略规则：按扩展名或文件名模式（正则），手动忽略/取消忽略\n")
        _T("       * 跟踪规则：只处理被跟踪的文件（覆盖忽略设置）\n")
        _T("     - 文件列表支持拖放排序，带有蓝色插入线\n")
        _T("     - 右键菜单：忽略、跟踪、标记删除、修改扩展名、前移/后移、在资源管理器中定位\n")
        _T("     - \"预览\"显示重命名结果；\"执行\"应用更改；\"撤销\"还原上次重命名\n")
        _T("     - \"全部重置\"清除所有规则和标记；F5 刷新文件列表\n")
        _T("     - \"AI助手\"打开AI批量重命名助手，可以用自然语言描述重命名需求，ai自动生成文件名映射，可以应用到文件批量处理，支持与其他文件批量处理操作叠加\n\n")

        _T("系统工具(&S) 子菜单：\n")
        _T("  6. 右键菜单管理\n")
        _T("     - 扫描和管理 Windows 右键菜单项\n")
        _T("     - 场景下拉框：28+ 种场景（全部、文件、文件夹、目录背景、桌面、驱动器等）\n")
        _T("     - 14 个常见扩展名预设（.jpg、.png、.txt、.pdf 等）+ 自定义扩展名查询\n")
        _T("     - 列表显示：位置、显示名称、类型（静态/ShellEx）、可见性、键名、命令\n")
        _T("     - 右键：启用/禁用项、自定义名称解析、在注册表中定位\n")
        _T("     - \"文件夹右键菜单: 用本程序打开\"复选框：在文件夹右键菜单中添加/移除此工具\n")
        _T("     - \"Win11经典菜单(Shift右键效果)\"复选框：切换 Win11 新旧右键菜单样式（需重启资源管理器）\n")
        _T("     - \"重建字典\"：通过 COM 查询 ShellEx 显示名称并缓存\n")
        _T("     - \"字典路径\"：配置自定义字典文件夹；\"打开字典\"在资源管理器中打开\n")
        _T("     - F5 刷新；禁用项使用 LegacyDisable + ProgrammaticAccessOnly 机制\n")
        _T("     - 支持ai解析，对于某些未被字典翻译的项给出ai的判断\n\n")
        _T("  7. 环境变量管理\n")
        _T("     - 上方列表：系统变量；下方列表：用户变量\n")
        _T("     - 搜索框：实时过滤两个列表\n")
        _T("     - \"添加\"：选择系统或用户范围，输入变量名和值\n")
        _T("     - \"编辑\"或双击：PATH 变量打开专用编辑器；其他变量打开简单输入对话框\n")
        _T("     - \"删除\"：移除选中的变量（需确认）\n")
        _T("     - \"导出\"：将所有变量保存到 .txt 或 .env 文件\n")
        _T("     - 右键：编辑、删除、复制名称、复制值\n")
        _T("     - PATH 编辑器：以独立行形式列出条目；支持添加/删除/上移/下移\n")
        _T("     - 自动备份：修改前自动将当前值备份到临时文件夹，带时间戳\n")
        _T("     - F5刷新\n\n")
        _T("  8. 文件占用查看\n")
        _T("     - 拖放文件以查看哪些进程正在锁定它们（使用 Restart Manager API）\n")
        _T("     - 列表显示：文件路径、进程名、PID、进程类型、进程路径\n")
        _T("     - \"结束\"终止选中的进程；\"全部结束\"终止所有列出的进程\n")
        _T("     - \"定位\"在资源管理器中打开进程所在文件夹\n")
        _T("     - \"刷新\"重新查询；\"清除\"清空列表\n")
        _T("     - 右键可弹出上下文菜单\n")
        _T("     - 结束进程前会弹出确认对话框\n\n")

        _T("直接菜单项（不在子菜单中）：\n")
        _T("  9. 简易便签\n")
        _T("     - 程序启动时自动启动，定位在屏幕右侧 3/5 位置\n")
        _T("     - 初始状态仅显示标题栏；双击标题栏展开\n")
        _T("     - 展开状态下：X 按钮折叠到标题栏；最小化按钮折叠到标题栏\n")
        _T("     - 折叠状态下：X 按钮退出；双击标题栏展开\n")
        _T("     - 右键标题栏：\"退出便签\"\n")
        _T("     - 内容自动保存到配置文件夹中的 sticky_note.txt（UTF-8 编码）\n")
        _T("     - \"浏览\"按钮更改保存文件夹\n\n")

        _T("=== 其他功能 ===\n\n")
        _T("位于左下角的\n\n")
        _T("   - 连点器：勾选\"连点器\"启用；按开始键开始点击，停止键停止\n")
        _T("     可在设置中配置间隔（毫秒）和开始/停止键；启用时显示速度调节窗口\n")
        _T("   - 禁止自动锁屏：勾选\"禁止自动锁屏\"保持屏幕常亮（SetThreadExecutionState）\n")
        _T("   - 开机自启动：勾选\"开机自启动\"添加到注册表 Run 键\n")
        _T("   - \"窗口置顶\"复选框：保持工具箱始终在最前\n\n")

        _T("位于右上角，靠近关闭按钮\n\n")
        _T("   - \"最小化到托盘\"复选框：点击 X 按钮最小化到系统托盘而非关闭\n\n")

        _T("位于系统托盘区\n\n")
        _T("   - 系统托盘：双击图标恢复窗口；右键菜单\"显示窗口\"或\"退出\"\n\n")

        _T("=== 快捷键 ===\n\n")
        _T("   - Ctrl+Alt+Space：显示/隐藏主窗口（全局热键）\n")
        _T("   - Alt+1~6：切换到左侧标签页 1-6\n")
        _T("   - Ctrl+Alt+D：启动窗口定位模式\n")
        _T("   - F5：刷新当前标签页的列表（进程、启动项等）\n")
        _T("   - 回车：在音量输入框或命令输入框中应用/执行\n\n")

        _T("=== 配置 ===\n\n")
        _T("   - 文件 > 设置：配置所有应用路径（B站、微信、QQ、VSCode、VS、Git Bash、元宝），")
        _T("文件夹路径（学习、下载、截图、便签），网址（MOOC、SDUCS），")
        _T("连点器间隔和开始/停止键，AI 供应商和 API 密钥\n")
        _T("   - 配置文件：可执行文件所在目录下的 config.ini\n")
        _T("   - AI 供应商和 API 密钥可在设置 > \"AI 助手\"区域配置\n\n")

        _T("回答问题时：\n")
        _T("   - 默认使用中文回答，因为应用程序界面是中文的\n")
        _T("   - 如果用户用英文提问，用英文回答；如果用户用其他语言提问，用该语言回答\n")
        _T("   - 如果用户混合使用多种语言，使用问题中的主要语言\n")
        _T("   - 回答要简洁直接，必要时提供分步指导\n")
        _T("   - 如果不确定某个功能，建议用户查看实际界面\n")
        _T("   - 如果用户遇到错误，建议检查 config.ini 配置文件和文件权限\n")
        _T("   - 此应用程序的大多数操作需要管理员权限\n")
        _T("   - 如果用户请求执行系统命令、操作文件、管理进程等，你可以返回可执行命令\n\n")

        _T("=== 可执行命令协议 ===\n\n")
        _T("当你需要返回可执行命令时，使用以下格式：\n\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"要执行的命令\",\n")
        _T("  \"purpose\": \"用途说明\",\n")
        _T("  \"risk\": \"low/medium/high\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("风险等级说明：\n")
        _T("- low：无害操作（如打开文件夹、显示信息）\n")
        _T("- medium：有潜在影响的操作（如修改文件、重启进程）\n")
        _T("- high：高危操作（如删除文件、修改注册表、格式化磁盘）\n\n")
        _T("注意：包含 del、format、reg delete、net user 等关键词的命令会自动升级为高风险。\n")
        _T("高风险命令需要用户输入\"确认执行\"才能执行。\n\n")
        _T("命令执行结果反馈：\n")
        _T("- 命令执行后，stdout/stderr 输出会被捕获并在 WebBrowser 中显示\n")
        _T("- 输出内容包括：命令的标准输出、标准错误输出、退出代码\n")
        _T("- 对于 echo、dir、ipconfig 等产生输出的命令，用户可以直接在对话中看到执行结果\n")
        _T("- 执行超时限制为 30 秒，超时后进程将被终止\n");
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

    // Display full conversation history in browser
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        CString msg;
        msg.Format(_T("<div style='color:red;'>[Error] No API Key configured for %s. Please go to File > Settings > AI Assistant to configure it.</div>"), vendor.GetString());
        SetAiBrowserHtml(BuildAiHtmlPage(msg));
        m_aiHistory.pop_back();
        return;
    }

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(FALSE);
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(TRUE);

    // Use streaming API
    m_aiStreamingContent.Empty();
    CAIApiClient::SendAsyncStreaming(m_aiHistory, vendor, apiKey, model, m_hWnd);
}

void CMFCApplication1Dlg::OnBnClickedAiClear()
{
    // Auto-save current conversation before clearing
    SaveCurrentConversation();
    m_strConvTitle.Empty();
    m_strConvPath.Empty();
    m_strConvCreated.Empty();
    m_aiHistory.clear();
    m_aiStreamingContent.Empty();
    m_aiPendingHtml.Empty();
    SetAiBrowserHtml(BuildAiHtmlPage(_T("")));
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
}

void CMFCApplication1Dlg::OnBnClickedAiStop()
{
    CAIApiClient::Cancel();
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);
}

LRESULT CMFCApplication1Dlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    CString* pResult = reinterpret_cast<CString*>(lParam);
    if (!pResult) return 0;

    CString response = *pResult;
    delete pResult;

    bool bSuccess = (wParam == 1);

    // Normal AI assistant chat response
    if (bSuccess)
    {
        m_aiHistory.push_back({ _T("assistant"), response });
    }
    else
    {
        if (!m_aiHistory.empty() && m_aiHistory.back().first == _T("user"))
            m_aiHistory.pop_back();
    }

    // Render full conversation history
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);

    return 0;
}

LRESULT CMFCApplication1Dlg::OnAiStreamChunk(WPARAM /*wParam*/, LPARAM lParam)
{
    CString* pChunk = reinterpret_cast<CString*>(lParam);
    if (!pChunk) return 0;

    m_aiStreamingContent += *pChunk;
    delete pChunk;

    // Render full history + streaming content
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(m_aiStreamingContent)));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    return 0;
}

LRESULT CMFCApplication1Dlg::OnAiStreamDone(WPARAM wParam, LPARAM lParam)
{
    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);

    CString* pResult = reinterpret_cast<CString*>(lParam);
    bool bSuccess = (wParam == 1);

    if (bSuccess && pResult)
    {
        m_aiHistory.push_back({ _T("assistant"), *pResult });

        // Final render with full conversation history
        SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
        ScrollAiBrowserToAnchor(_T("scroll-anchor"));
    }
    else if (!bSuccess && pResult)
    {
        CString errBody = _T("<div style='color:red;'>") + CMarkdownDlg::EscapeHtml(*pResult) + _T("</div>");
        SetAiBrowserHtml(BuildAiHtmlPage(errBody));
        if (!m_aiHistory.empty() && m_aiHistory.back().first == _T("user"))
            m_aiHistory.pop_back();
    }

    if (pResult) delete pResult;
    return 0;
}

CString CMFCApplication1Dlg::BuildAiHtmlPage(const CString& bodyContent)
{
    return _T("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><style>")
        _T("body{font-family:Consolas,'Microsoft YaHei',sans-serif;font-size:18px;")
        _T("background:#1e1e1e;color:#d4d4d4;padding:8px;margin:0;line-height:1.5;}")
        _T("code{background:#2d2d2d;padding:1px 4px;border-radius:3px;font-family:Consolas,monospace;}")
        _T("pre{background:#2d2d2d;padding:8px;border-radius:4px;overflow-x:auto;}")
        _T("pre code{background:none;padding:0;}")
        _T("a{color:#569cd6;}")
        _T("h1,h2,h3{color:#dcdcaa;margin:8px 0 4px;}")
        _T("table{border-collapse:collapse;}")
        _T("th,td{border:1px solid #444;padding:4px 8px;}")
        _T("th{background:#2d2d2d;}")
        _T("blockquote{border-left:3px solid #569cd6;margin:4px 0;padding-left:12px;color:#888;}")
        // Action card styles
        _T(".action-card{border:2px solid #555;border-radius:8px;padding:12px 16px;margin:12px 0;background:#2a2a2a;font-family:Consolas,monospace;}")
        _T(".action-card.action-level-low{border-color:#2da44e;}")
        _T(".action-card.action-level-medium{border-color:#d4a72c;}")
        _T(".action-card.action-level-high{border-color:#cf222e;}")
        _T(".action-purpose{font-size:16px;color:#d4d4d4;margin-bottom:4px;}")
        _T(".action-risk{font-size:14px;font-weight:600;margin-bottom:8px;}")
        _T(".action-risk.level-low{color:#2da44e;}")
        _T(".action-risk.level-medium{color:#d4a72c;}")
        _T(".action-risk.level-high{color:#cf222e;}")
        _T(".action-btn{background:#2da44e;color:#fff;border:none;border-radius:6px;padding:6px 14px;font-size:16px;cursor:pointer;margin-bottom:6px;}")
        _T(".action-btn:hover{background:#218838;}")
        _T(".action-card.action-level-high .action-btn{background:#cf222e;}")
        _T(".action-card.action-level-high .action-btn:hover{background:#a11c26;}")
        _T(".action-card.action-level-medium .action-btn{background:#d4a72c;}")
        _T(".action-card.action-level-medium .action-btn:hover{background:#b88a1f;}")
        _T(".action-command{font-size:15px;color:#888;background:#333;padding:4px 8px;border-radius:4px;word-break:break-all;}")
        _T("</style><script>")
        _T("function execCmd(btn){")
        _T("var data=btn.getAttribute('data-cmd');")
        _T("if(!data)return;")
        _T("location.href='http://127.0.0.1:1/exec/'+encodeURIComponent(data);")
        _T("}")
        _T("</script></head><body>") + bodyContent + _T("</body></html>");
}

CString CMFCApplication1Dlg::BuildAiBodyFromHistory(const CString& streamingContent, const CString& scrollToCommand)
{
    CString body;

    // Pre-build a map: command → list of system result messages
    // This allows RenderAssistantWithResults to match action cards to their results
    // by extracting the command from the action card JSON and looking up the result.
    std::map<CString, std::vector<CString>> cmdResults;
    std::map<CString, int> cmdResultIndex;
    for (auto& msg : m_aiHistory)
    {
        if (msg.first == _T("system") && msg.second.Find(_T("【命令执行结果】")) == 0)
        {
            // Extract command from: "【命令执行结果】\n命令：<command>\n..."
            CString cmdPrefix = _T("【命令执行结果】\n命令：");
            int cmdStart = cmdPrefix.GetLength();
            int cmdEnd = msg.second.Find(_T("\n"), cmdStart);
            if (cmdEnd > cmdStart)
            {
                CString cmd = msg.second.Mid(cmdStart, cmdEnd - cmdStart);
                cmdResults[cmd].push_back(msg.second);
            }
        }
    }

    for (auto& msg : m_aiHistory)
    {
        if (msg.first == _T("system") && msg.second.Find(_T("【命令执行结果】")) == 0)
        {
            // Command results are now rendered inline within the assistant message,
            // so skip them here to avoid duplication.
            continue;
        }
        else if (msg.first == _T("system"))
        {
            continue; // Skip system prompt and other system messages
        }
        else if (msg.first == _T("user"))
        {
            body += _T("<div style='color:#888;margin-bottom:4px;'>You: </div>")
                + CMarkdownDlg::EscapeHtml(msg.second) + _T("<br>");
        }
        else if (msg.first == _T("assistant"))
        {
            body += _T("<div style='color:#c8c8c8;margin-bottom:4px;'>AI:</div>");
            // Render assistant message with action cards interleaved with results
            body += RenderAssistantWithResults(msg.second, cmdResults, cmdResultIndex);
        }
    }

    // Append streaming content if present
    if (!streamingContent.IsEmpty())
    {
        body += _T("<div style='color:#c8c8c8;margin-bottom:4px;'>AI:</div>")
            + CMarkdownDlg::MarkdownToBody(streamingContent);
    }

    // Always add scroll anchor at the end so the browser can scroll to the latest content
    body += _T("<div id=\"scroll-anchor\"></div>");

    // If a specific command result should be scrolled to, embed an inline script
    // that executes during document parsing (after all markers are in the DOM).
    // This is more reliable than execScript() after SetAiBrowserHtml().
    if (!scrollToCommand.IsEmpty())
    {
        // Escape the command for JavaScript single-quoted string literal
        CString escCmd;
        for (int i = 0; i < scrollToCommand.GetLength(); i++)
        {
            wchar_t c = scrollToCommand[i];
            if (c == L'\\')      escCmd += L"\\\\";
            else if (c == L'\'') escCmd += L"\\'";
            else if (c == L'\n') escCmd += L"\\n";
            else if (c == L'\r') escCmd += L"\\r";
            else if (c == L'\t') escCmd += L"\\t";
            else                 escCmd += c;
        }

        body += _T("<script>")
            _T("(function(){")
            _T("var m=document.querySelectorAll('.cmd-result-marker');")
            _T("for(var i=0;i<m.length;i++){")
            _T("if(m[i].getAttribute('data-command')==='") + escCmd + _T("'){")
            _T("m[i].scrollIntoView(true);return;")
            _T("}")
            _T("}")
            _T("if(m.length>0)m[m.length-1].scrollIntoView(true);")
            _T("})()")
            _T("</script>");
    }

    return body;
}

CString CMFCApplication1Dlg::RenderAssistantWithResults(const CString& content,
    std::map<CString, std::vector<CString>>& cmdResults,
    std::map<CString, int>& cmdResultIndex)
{
    CString html;
    int pos = 0;

    while (true)
    {
        // Find the next ```action block
        int actionStart = content.Find(_T("```action"), pos);
        if (actionStart < 0)
        {
            // No more action cards, render the remaining text
            CString rest = content.Mid(pos);
            if (!rest.IsEmpty())
                html += CMarkdownDlg::MarkdownToBody(rest);
            break;
        }

        // Render text before this action card (if any)
        if (actionStart > pos)
        {
            CString beforeText = content.Mid(pos, actionStart - pos);
            if (!beforeText.IsEmpty())
                html += CMarkdownDlg::MarkdownToBody(beforeText);
        }

        // Find the closing ``` of this code block
        int codeEnd = content.Find(_T("```"), actionStart + 9);
        if (codeEnd < 0 || codeEnd <= actionStart + 9)
        {
            // No closing ``` or empty block, render the rest as markdown and bail out
            html += CMarkdownDlg::MarkdownToBody(content.Mid(actionStart));
            break;
        }

        // Render the action card itself (including the ```action ... ``` wrapper)
        CString actionBlock = content.Mid(actionStart, codeEnd + 3 - actionStart);
        html += CMarkdownDlg::MarkdownToBody(actionBlock);

        // Extract the JSON content inside the action card to get the command
        CString jsonContent = content.Mid(actionStart + 9, codeEnd - actionStart - 9);
        jsonContent.Trim();

        if (!jsonContent.IsEmpty())
        {
            CString matchedCommand;
            try {
                std::string s = (LPCSTR)CT2A(jsonContent, CP_UTF8);
                nlohmann::json j = nlohmann::json::parse(s);
                matchedCommand = CString(CA2T(j["command"].get<std::string>().c_str(), CP_UTF8));
            }
            catch (...) { }

            if (!matchedCommand.IsEmpty())
            {
                // Look up the next unconsumed result for this command
                auto& results = cmdResults[matchedCommand];
                int idx = cmdResultIndex[matchedCommand];
                if (idx < (int)results.size())
                {
                    html += _T("<div style='color:#569cd6;font-size:15px;border-left:3px solid #569cd6;padding-left:8px;margin:4px 0;'>")
                        + CMarkdownDlg::MarkdownToBody(results[idx]) + _T("</div>")
                        + _T("<div class=\"cmd-result-marker\" data-command=\"") + CMarkdownDlg::EscapeHtml(matchedCommand) + _T("\"></div>");
                    cmdResultIndex[matchedCommand] = idx + 1;
                }
            }
        }

        pos = codeEnd + 3;
    }

    return html;
}

bool CMFCApplication1Dlg::SetAiBrowserHtml(const CString& html)
{
    // Follow the same pattern as CMarkdownDlg::SetBrowserHtml which works reliably.
    // The caller is responsible for providing a complete HTML document.
    if (!m_aiBrowser.m_hWnd || !::IsWindow(m_aiBrowser.m_hWnd))
    {
        m_aiPendingHtml = html;
        return false;
    }

    LPUNKNOWN pUnk = m_aiBrowser.GetControlUnknown();
    if (!pUnk)
    {
        m_aiPendingHtml = html;
        return false;
    }

    IWebBrowser2* pWeb2 = nullptr;
    if (FAILED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
    {
        m_aiPendingHtml = html;
        return false;
    }

    IDispatch* pDocDisp = nullptr;
    if (FAILED(pWeb2->get_Document(&pDocDisp)) || !pDocDisp)
    {
        // Document not ready yet (Navigate is async)
        pWeb2->Release();
        m_aiPendingHtml = html;
        return false;
    }

    IHTMLDocument2* pDoc = nullptr;
    if (FAILED(pDocDisp->QueryInterface(IID_IHTMLDocument2, (void**)&pDoc)))
    {
        pDocDisp->Release();
        pWeb2->Release();
        m_aiPendingHtml = html;
        return false;
    }

    BSTR bstrHtml = html.AllocSysString();
    SAFEARRAY* psa = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (psa)
    {
        VARIANT* pv = nullptr;
        if (SUCCEEDED(SafeArrayAccessData(psa, (void**)&pv)))
        {
            pv->vt = VT_BSTR;
            pv->bstrVal = bstrHtml;
            SafeArrayUnaccessData(psa);
            pDoc->close();
            pDoc->write(psa);
            pDoc->close();
        }
        SafeArrayDestroy(psa);
    }
    else
    {
        SysFreeString(bstrHtml);
    }

    pDoc->Release();
    pDocDisp->Release();
    pWeb2->Release();
    m_aiPendingHtml.Empty();
    // Kill the readiness timer and mark ready - prevents timer from
    // overwriting content written by streaming handlers
    if (!m_aiBrowserReady)
    {
        m_aiBrowserReady = true;
        KillTimer(1);
    }
    return true;
}

void CMFCApplication1Dlg::ScrollAiBrowserToAnchor(const CString& elementId)
{
    // Use IHTMLDocument3::getElementById + IHTMLElement::scrollIntoView to scroll the browser.
    // This runs after the document is fully written, so it's more reliable than inline <script>.
    if (!m_aiBrowser.m_hWnd || !::IsWindow(m_aiBrowser.m_hWnd))
        return;

    LPUNKNOWN pUnk = m_aiBrowser.GetControlUnknown();
    if (!pUnk) return;

    IWebBrowser2* pWeb2 = nullptr;
    if (FAILED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
        return;

    IDispatch* pDocDisp = nullptr;
    if (FAILED(pWeb2->get_Document(&pDocDisp)) || !pDocDisp)
    {
        pWeb2->Release();
        return;
    }

    IHTMLDocument3* pDoc3 = nullptr;
    if (FAILED(pDocDisp->QueryInterface(IID_IHTMLDocument3, (void**)&pDoc3)))
    {
        pDocDisp->Release();
        pWeb2->Release();
        return;
    }

    BSTR bstrId = elementId.AllocSysString();
    IHTMLElement* pElement = nullptr;
    if (SUCCEEDED(pDoc3->getElementById(bstrId, &pElement)) && pElement)
    {
        VARIANT vTop = { 0 };
        vTop.vt = VT_BOOL;
        vTop.boolVal = VARIANT_TRUE; // align to top
        pElement->scrollIntoView(vTop);
        pElement->Release();
    }
    SysFreeString(bstrId);

    pDoc3->Release();
    pDocDisp->Release();
    pWeb2->Release();
}

void CMFCApplication1Dlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        if (m_aiBrowserReady)
        {
            KillTimer(1);
            return;
        }
        // Retry until WebBrowser document is ready (about:blank is async)
        if (SetAiBrowserHtml(m_aiPendingHtml.IsEmpty() ? CString(_T("")) : m_aiPendingHtml))
        {
            // SetAiBrowserHtml already kills timer and sets m_aiBrowserReady on success
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CMFCApplication1Dlg::OnBnClickedAiHistory()
{
    CConversationHistoryDlg* pDlg = new CConversationHistoryDlg(this);
    if (!pDlg->Create(IDD_CONVERSATION_HISTORY_DLG, this))
    {
        delete pDlg;
        return;
    }
    pDlg->ShowWindow(SW_SHOW);
}

void CMFCApplication1Dlg::SaveCurrentConversation()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_aiHistory.empty()) return;

    CTime now = CTime::GetCurrentTime();

    // If we loaded an existing conversation, overwrite the original file
    if (!m_strConvPath.IsEmpty())
    {
        CFile file;
        if (!file.Open(m_strConvPath, CFile::modeCreate | CFile::modeWrite))
            return;

        CArchive ar(&file, CArchive::store);
        int nCount = (int)m_aiHistory.size();
        CString created = m_strConvCreated.IsEmpty() ? now.Format(_T("%Y-%m-%d %H:%M:%S")) : m_strConvCreated;
        CString updated = now.Format(_T("%Y-%m-%d %H:%M:%S"));

        ar << nCount;
        ar << m_strConvTitle;
        ar << created;
        ar << updated;
        for (auto& msg : m_aiHistory)
            ar << msg.first << msg.second;

        ar.Close();
        file.Close();
        return;
    }

    // New conversation: generate filename from title
    CString convDir = GetConversationsFolder();
    CreateDirectory(convDir, nullptr);

    // Generate title from first user message
    if (m_strConvTitle.IsEmpty())
    {
        for (auto& msg : m_aiHistory)
        {
            if (msg.first == _T("user"))
            {
                m_strConvTitle = msg.second.Left(50);
                m_strConvTitle.Trim();
                if (m_strConvTitle.IsEmpty()) m_strConvTitle = loc.GetString(_T("ConvHistory"), _T("UnnamedTitle"));
                break;
            }
        }
    }

    // Generate filename from title
    CString safeTitle = m_strConvTitle;
    for (int i = 0; i < safeTitle.GetLength(); i++)
    {
        if (_tcschr(_T("\\/:*?\"<>|"), safeTitle[i]))
            safeTitle.SetAt(i, '_');
    }

    CString fileName;
    fileName.Format(_T("%s_%s.conv"), now.Format(_T("%Y%m%d_%H%M%S")), safeTitle.GetString());

    CString filePath = convDir + fileName;

    CFile file;
    if (!file.Open(filePath, CFile::modeCreate | CFile::modeWrite))
        return;

    CArchive ar(&file, CArchive::store);
    int nCount = (int)m_aiHistory.size();
    CString created = now.Format(_T("%Y-%m-%d %H:%M:%S"));
    CString updated = now.Format(_T("%Y-%m-%d %H:%M:%S"));

    ar << nCount;
    ar << m_strConvTitle;
    ar << created;
    ar << updated;
    for (auto& msg : m_aiHistory)
        ar << msg.first << msg.second;

    ar.Close();
    file.Close();

    m_strConvPath = filePath;
}

void CMFCApplication1Dlg::LoadConversation(const CString& filePath)
{
    CFile file;
    if (!file.Open(filePath, CFile::modeRead))
        return;

    CArchive ar(&file, CArchive::load);
    int nCount;
    CString title, created, updated;
    ar >> nCount >> title >> created >> updated;

    m_aiHistory.clear();
    for (int i = 0; i < nCount; i++)
    {
        CString role, content;
        ar >> role >> content;
        m_aiHistory.push_back({role, content});
    }
    ar.Close();
    file.Close();

    m_strConvTitle = title;
    m_strConvPath = filePath;
    m_strConvCreated = created;

    // Render the full conversation in the browser
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));
}

LRESULT CMFCApplication1Dlg::OnConvLoaded(WPARAM wParam, LPARAM)
{
    CString* pFilePath = reinterpret_cast<CString*>(wParam);
    if (pFilePath)
    {
        LoadConversation(*pFilePath);
        delete pFilePath;
    }
    return 0;
}

CString CMFCApplication1Dlg::GetExeDir()
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(nullptr, szPath, MAX_PATH);
    CString path(szPath);
    int pos = path.ReverseFind('\\');
    if (pos >= 0)
        path = path.Left(pos + 1);
    return path;
}

CString CMFCApplication1Dlg::GetConversationsFolder()
{
    // Get config.ini path
    TCHAR szExePath[MAX_PATH] = {0};
    GetModuleFileName(nullptr, szExePath, MAX_PATH);
    CString exePath = szExePath;
    int nLastSlash = exePath.ReverseFind(_T('\\'));
    CString configPath = (nLastSlash >= 0) ? exePath.Left(nLastSlash + 1) + _T("config.ini") : CString(_T("config.ini"));

    // Try configured conversation directory
    TCHAR szPath[MAX_PATH] = {0};
    GetPrivateProfileString(_T("Paths"), _T("ConversationDir"), _T(""), szPath, MAX_PATH, configPath);
    CString convDir = szPath;
    convDir.Trim();

    if (convDir.IsEmpty())
    {
        // Fallback to AppData
        TCHAR szAppData[MAX_PATH] = {0};
        SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, szAppData);
        convDir = CString(szAppData) + _T("\\PowerBox\\conversations");
    }

    // Ensure trailing backslash
    if (convDir.Right(1) != _T("\\"))
        convDir += _T("\\");

    CreateDirectory(convDir, nullptr);
    return convDir;
}

// ============================================================================
// Connect WebBrowser event sink to intercept BeforeNavigate2
// ============================================================================
void CMFCApplication1Dlg::ConnectAiBrowserEvents()
{
    if (m_pAiEventSink || m_dwAiEventCookie != 0)
        return;

    LPUNKNOWN pUnk = m_aiBrowser.GetControlUnknown();
    if (!pUnk) return;

    IConnectionPointContainer* pCPC = nullptr;
    if (FAILED(pUnk->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC)))
        return;

    IConnectionPoint* pCP = nullptr;
    if (SUCCEEDED(pCPC->FindConnectionPoint(DIID_DWebBrowserEvents2, &pCP)))
    {
        m_pAiEventSink = new CWebBrowserEventSink(m_hWnd);
        if (SUCCEEDED(pCP->Advise(m_pAiEventSink, &m_dwAiEventCookie)))
        {
            // Success — event sink is now connected
        }
        else
        {
            // Failed to advise — clean up
            delete m_pAiEventSink;
            m_pAiEventSink = nullptr;
            m_dwAiEventCookie = 0;
        }
        pCP->Release();
    }
    pCPC->Release();
}

// ============================================================================
// Disconnect WebBrowser event sink
// ============================================================================
void CMFCApplication1Dlg::DisconnectAiBrowserEvents()
{
    if (!m_pAiEventSink || m_dwAiEventCookie == 0)
        return;

    LPUNKNOWN pUnk = m_aiBrowser.GetControlUnknown();
    if (pUnk)
    {
        IConnectionPointContainer* pCPC = nullptr;
        if (SUCCEEDED(pUnk->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC)))
        {
            IConnectionPoint* pCP = nullptr;
            if (SUCCEEDED(pCPC->FindConnectionPoint(DIID_DWebBrowserEvents2, &pCP)))
            {
                pCP->Unadvise(m_dwAiEventCookie);
                pCP->Release();
            }
            pCPC->Release();
        }
    }

    m_pAiEventSink->Release(); // Release our ref — sink deletes itself at ref=0
    m_pAiEventSink = nullptr;
    m_dwAiEventCookie = 0;
}

// ============================================================================
// High-risk confirmation dialog — sets prompt text in OnInitDialog
// ============================================================================
class CHighRiskConfirmDlg : public CDialogEx
{
public:
    CString m_input;

    CHighRiskConfirmDlg() : CDialogEx(IDD_INPUT_DLG) {}

protected:
    BOOL OnInitDialog() override
    {
        CDialogEx::OnInitDialog();
        auto& loc = CLocalizationManager::GetInstance();
        SetWindowText(loc.GetString(_T("Msg"), _T("HighRiskWarningTitle")));
        SetDlgItemText(IDC_INPUT_PROMPT, loc.GetString(_T("Msg"), _T("HighRiskPrompt")));
        SetDlgItemText(IDC_INPUT_EDIT, _T(""));
        return TRUE;
    }

    void DoDataExchange(CDataExchange* pDX) override
    {
        CDialogEx::DoDataExchange(pDX);
        DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
    }
};

// ============================================================================
// Handle AI executable command: validate, confirm, execute
// ============================================================================
LRESULT CMFCApplication1Dlg::OnAiExecuteCommand(WPARAM /*wParam*/, LPARAM lParam)
{
    auto& loc = CLocalizationManager::GetInstance();
    // lParam is a TCHAR* allocated by _tcsdup in HandleAppExecUrl
    TCHAR* pJsonStr = reinterpret_cast<TCHAR*>(lParam);
    if (!pJsonStr) return 0;

    CString json = pJsonStr;
    free(pJsonStr);  // _tcsdup uses malloc, must use free()

    // Parse JSON using nlohmann::json
    CString command, purpose, risk;
    try
    {
        std::string s = (LPCSTR)CT2A(json, CP_UTF8);
        nlohmann::json j = nlohmann::json::parse(s);
        command = CString(CA2T(j["command"].get<std::string>().c_str(), CP_UTF8));
		purpose = CString(CA2T(j["purpose"].get<std::string>().c_str(), CP_UTF8));
		risk = CString(CA2T(j["risk"].get<std::string>().c_str(), CP_UTF8));
    }
    catch (const nlohmann::json::parse_error&)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("InvalidJsonCmd")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return 0;
    }

    risk.MakeLower();
    if (risk.IsEmpty()) risk = _T("medium");

    // Risk level escalation for dangerous keywords
    CString cmdLower = command;
    cmdLower.MakeLower();
    struct { const wchar_t* keyword; } dangerous[] = {
        { L"del " }, { L"rd /s" }, { L"rmdir /s" },
        { L"format " },
        { L"reg delete" }, { L"reg add" },
        { L"net user" }, { L"net localgroup" },
        { L"takeown" }, { L"icacls" },
        { L"schtasks" },
        { L"bcdedit" }
    };
    for (const auto& d : dangerous)
    {
        if (cmdLower.Find(d.keyword) >= 0)
        {
            risk = _T("high");
            break;
        }
    }

    // Build confirmation message
    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("AiCmdConfirmFmt")),
        command.GetString(), purpose.GetString(), risk.GetString());

    // Confirm based on risk level
    bool bExecute = false;

    if (risk == _T("high"))
    {
        // Use CHighRiskConfirmDlg which sets prompt text in OnInitDialog
        CHighRiskConfirmDlg dlg;
        if (dlg.DoModal() == IDOK)
        {
            dlg.m_input.Trim();
            bExecute = (dlg.m_input == loc.GetString(_T("Msg"), _T("HighRiskConfirmText")));
            if (!bExecute)
                MessageBox(loc.GetString(_T("Msg"), _T("InputMismatchCancel")), loc.GetString(_T("Msg"), _T("Cancelled")), MB_OK | MB_ICONINFORMATION);
        }
    }
    else // low or medium
    {
        UINT nType = (risk == _T("medium")) ? MB_ICONWARNING : MB_ICONINFORMATION;
        bExecute = (MessageBox(msg, loc.GetString(_T("Msg"), _T("ExecConfirm")), MB_YESNO | nType | MB_DEFBUTTON2) == IDYES);
    }

    if (!bExecute)
    {
        // Record cancellation in conversation
        CString resultMsg;
        resultMsg.Format(loc.GetString(_T("Msg"), _T("CmdCancelledFmt")),
            command.GetString());
        // Insert after the last assistant message so results stay with their action cards
        {
            int insertPos = (int)m_aiHistory.size();
            for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
            {
                if (m_aiHistory[i].first == _T("assistant"))
                {
                    insertPos = i + 1;
                    // Skip past any system results that already follow this assistant
                    while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                        insertPos++;
                    break;
                }
            }
            m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
        }
        SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));
        return 0;
    }

    // Execute the command
    CString resultMsg;
    resultMsg.Format(_T("【命令执行结果】\n命令：%s\n状态：已执行\n\n"),
        command.GetString());

    CString cmdTrimmed = command;
    cmdTrimmed.Trim();

    // Use CreateProcess with pipes to capture stdout/stderr and show results in WebBrowser.
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    CString outputStr;
    DWORD exitCode = 0;

    if (CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        // Build command line
        CString cmdLine;
        if (cmdTrimmed.Find(_T("powershell ")) == 0 || cmdTrimmed.Find(_T("PowerShell ")) == 0)
        {
            cmdLine = _T("powershell.exe ") + cmdTrimmed.Mid(10).Trim();
        }
        else
        {
            cmdLine = _T("cmd.exe /c ") + cmdTrimmed;
        }

        STARTUPINFO si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        PROCESS_INFORMATION pi = { 0 };

        CString cmdLineCopy = cmdLine;
        LPTSTR pCmdLine = cmdLineCopy.GetBuffer(cmdLineCopy.GetLength() + 1);

        if (CreateProcess(nullptr, pCmdLine, nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            CloseHandle(hWritePipe);
            hWritePipe = nullptr;

            // Read output while process runs (30s timeout, then terminate)
            char readBuf[4096];
            DWORD bytesRead;
            std::string output;
            const DWORD timeoutMs = 30000;
            DWORD waitResult;

            do {
                waitResult = WaitForSingleObject(pi.hProcess, 100);
                // Drain any available data from pipe
                DWORD available = 0;
                while (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0)
                {
                    DWORD toRead = __min(available, (DWORD)sizeof(readBuf) - 1);
                    if (ReadFile(hReadPipe, readBuf, toRead, &bytesRead, nullptr) && bytesRead > 0)
                    {
                        readBuf[bytesRead] = '\0';
                        output.append(readBuf, bytesRead);
                    }
                    else break;
                }
            } while (waitResult == WAIT_TIMEOUT);

            // Read any remaining output
            while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                readBuf[bytesRead] = '\0';
                output.append(readBuf, bytesRead);
            }

            GetExitCodeProcess(pi.hProcess, &exitCode);
            if (waitResult == WAIT_TIMEOUT)
            {
                TerminateProcess(pi.hProcess, 1);
                output += "\r\n[执行超时（30秒），进程已终止]";
                exitCode = 1;
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            // Convert output: try UTF-8 first, fallback to system ANSI (cmd.exe uses code page)
            int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, output.c_str(), (int)output.size(), nullptr, 0);
            if (wlen > 0)
            {
                std::wstring wstr(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.size(), &wstr[0], wlen);
                outputStr = wstr.c_str();
            }
            else
            {
                // Not valid UTF-8 → likely system ANSI (e.g. GBK on Chinese Windows)
                outputStr = CString(output.c_str());
            }
            outputStr.Trim();
        }
        else
        {
            DWORD err = GetLastError();
            CString errStr;
            errStr.Format(loc.GetString(_T("Msg"), _T("ExecFailedErrCode")), err);
            outputStr = errStr;
            exitCode = err;
        }
        cmdLineCopy.ReleaseBuffer();
    }
    else
    {
        outputStr = loc.GetString(_T("Msg"), _T("CannotCreatePipe"));
        exitCode = GetLastError();
    }

    if (hWritePipe) CloseHandle(hWritePipe);
    if (hReadPipe) CloseHandle(hReadPipe);

    // Append output to result message
    if (!outputStr.IsEmpty())
    {
        resultMsg += loc.GetString(_T("Msg"), _T("OutputLabel")) + outputStr + _T("\n\n");
    }
    CString exitStr;
    exitStr.Format(loc.GetString(_T("Msg"), _T("ExitCodeFmt")), exitCode);
    resultMsg += exitStr;

    // Record result in conversation history — insert after the last assistant message
    {
        int insertPos = (int)m_aiHistory.size();
        for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
        {
            if (m_aiHistory[i].first == _T("assistant"))
            {
                insertPos = i + 1;
                // Skip past any system results that already follow this assistant
                while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                    insertPos++;
                break;
            }
        }
        m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
    }

    // Re-render full conversation with command result, auto-scroll to result
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));

    // Re-enable send button
    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);

    return 0;
}






