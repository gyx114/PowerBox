// MFCApplication1.cpp: Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "AIApiClient.h"
#include <afxole.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMFCApplication1App

BEGIN_MESSAGE_MAP(CMFCApplication1App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CMFCApplication1App construction

CMFCApplication1App::CMFCApplication1App()
{
	// Support restart manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: Add construction code here,
	// place all significant initialization in InitInstance
}


// The one and only CMFCApplication1App object

CMFCApplication1App theApp;


// CMFCApplication1App initialization

BOOL CMFCApplication1App::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles. Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls{
		.dwSize = sizeof(InitCtrls),
		.dwICC = ICC_WIN95_CLASSES
	};
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);

    AfxEnableControlContainer();

    // Initialize RichEdit 4.1 for Markdown preview
    AfxInitRichEdit2();

#ifdef _DEBUG
	// Enable CRT debug heap leak checking and automatic dump at program exit.
	int dbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	dbgFlag |= _CRTDBG_ALLOC_MEM_DF;
	_CrtSetDbgFlag(dbgFlag);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);

   // Debug: allocation tracking enabled; no automatic breakpoints set here.

	// Optional: allow setting a CRT allocation breakpoint via environment variable
	TCHAR envBuf[64]{};
	DWORD r = GetEnvironmentVariable(_T("CRT_BREAK_ALLOC"), envBuf, _countof(envBuf));
	if (r > 0 && r < _countof(envBuf))
	{
		int alloc = _ttoi(envBuf);
		if (alloc > 0)
		{
			_CrtSetBreakAlloc(static_cast<size_t>(alloc));
		}
	}
#endif

    // Initialize OLE libraries for drag/drop and other OLE features
	if (!AfxOleInit())
	{
		AfxMessageBox(_T("初始化 OLE 失败，无法使用拖放功能。"));
		return FALSE;
	}

    // Create shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	auto pShellManager = std::make_unique<CShellManager>();

    // Activate "Windows Native" visual manager to enable themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove the following
	// specific initialization routines you do not need.
	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate,
	// such as the name of your company or organization.
	// SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	// Get the current program path and set the ini file path
	TCHAR szAppPath[MAX_PATH]{};
	GetModuleFileName(nullptr, szAppPath, MAX_PATH);
	namespace fs = std::filesystem;
	fs::path appPath(szAppPath);
	fs::path iniPath = appPath.parent_path() / L"config.ini";
	CString strIniPath(iniPath.c_str());

	// Release the default m_pszProfileName and point it to the new .ini file path
	if (m_pszProfileName != nullptr)
	{
		free(const_cast<void*>(static_cast<const void*>(m_pszProfileName)));
	}
	m_pszProfileName = _tcsdup(strIniPath);

    // Create and run the main dialog inside its own scope so that its
    // destructor (which destroys all child controls, including any OLE
    // related controls) runs before we call AfxOleTerm and free the
    // profile string. This prevents leaked OLE objects reported by CRT.
	{
		CMFCApplication1Dlg dlg;

		// Parse command line: if a folder path is passed, open batch rename
		CString strCmdLine = m_lpCmdLine;
		strCmdLine.Trim();
		if (!strCmdLine.IsEmpty() && strCmdLine[0] == _T('"'))
		{
			// Remove surrounding quotes
			strCmdLine = strCmdLine.Mid(1);
			int nLastQuote = strCmdLine.ReverseFind(_T('"'));
			if (nLastQuote >= 0)
				strCmdLine = strCmdLine.Left(nLastQuote);
		}

		// Single-instance check: if the main dialog is already running,
		// forward the folder path via WM_COPYDATA and exit this instance.
		{
			HWND hWndExisting = nullptr;
			HWND hWnd = FindWindowEx(nullptr, nullptr, _T("#32770"), nullptr);
			while (hWnd)
			{
				TCHAR szTitle[256] = { 0 };
				GetWindowText(hWnd, szTitle, 256);
				if (CString(szTitle).Find(_T("ctrl+alt+空格唤起此窗口")) >= 0)
				{
					hWndExisting = hWnd;
					break;
				}
				hWnd = FindWindowEx(nullptr, hWnd, _T("#32770"), nullptr);
			}

			if (hWndExisting)
			{
				// Forward the folder path to the existing instance
				if (!strCmdLine.IsEmpty())
				{
					COPYDATASTRUCT cds;
					cds.dwData = 1;
					cds.cbData = (strCmdLine.GetLength() + 1) * sizeof(TCHAR);
					cds.lpData = (PVOID)strCmdLine.GetString();
					SendMessage(hWndExisting, WM_COPYDATA, 0, (LPARAM)&cds);
				}
				// Bring the existing window to the foreground
				if (IsIconic(hWndExisting))
					ShowWindow(hWndExisting, SW_RESTORE);
				SetForegroundWindow(hWndExisting);
				return FALSE;
			}
		}

		// If launched from folder context menu, set initial folder for batch rename
		if (!strCmdLine.IsEmpty())
		{
			DWORD attrs = GetFileAttributes(strCmdLine);
			if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
			{
				dlg.m_strInitialFolder = strCmdLine;
			}
		}

		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK)
		{
			// TODO: Place code here to handle when the dialog is
			//  dismissed with OK
		}
		else if (nResponse == IDCANCEL)
		{
			// TODO: Place code here to handle when the dialog is
			//  dismissed with Cancel
		}
		else if (nResponse == -1)
		{
			TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
			TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		}
	}

	// pShellManager will be auto-deleted by unique_ptr when it goes out of scope

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Clean up OLE and any app-allocated profile string to avoid memory leaks reported by CRT.
	if (m_pszProfileName != nullptr)
	{
		free(const_cast<void*>(static_cast<const void*>(m_pszProfileName)));
		m_pszProfileName = nullptr;
	}
    // Ensure OLE is properly terminated. Pass TRUE to fully clean up OLE-related
	// MFC/ATL objects that may have been created by controls (e.g. CMFCEditBrowseCtrl).
	AfxOleTerm(TRUE);

#ifdef _DEBUG
	// In debug builds explicitly dump leaks after we've terminated OLE and
	// cleaned up application objects. This avoids false-positive leak reports
	// caused by objects that are destroyed during AfxOleTerm/other cleanup.
	_CrtDumpMemoryLeaks();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CMFCApplication1App::ExitInstance()
{
    // Clean up empty "log" folder created by MFC CMFCVisualManager diagnostics.
    // MFC creates this folder in the module directory for tracing output;
    // if empty, it's safe to remove.
    TCHAR szModulePath[MAX_PATH]{};
    GetModuleFileName(nullptr, szModulePath, MAX_PATH);
    fs::path logPath = fs::path(szModulePath).parent_path() / L"log";
    std::error_code ec;
    if (fs::exists(logPath, ec) && fs::is_directory(logPath, ec))
    {
        if (fs::is_empty(logPath, ec))
            fs::remove(logPath, ec);
    }

    // Wait for all AI thread pool tasks to finish
    CAIApiClient::DestroyThreadPool();

    Gdiplus::GdiplusShutdown(m_gdiplusToken);
    return CWinApp::ExitInstance();
}