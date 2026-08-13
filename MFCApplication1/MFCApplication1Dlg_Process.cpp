#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "AIApiClient.h"
#include "ProcessScanDlg.h"
#include "ProcessAiResultDlg.h"
#include "LocalizationManager.h"
#include <TlHelp32.h>
#include <Shellapi.h>
#include <Psapi.h>
#include <regex>
#include <algorithm>
#include <map>
#include <set>
#include <wintrust.h>
#include <softpub.h>
#pragma comment(lib, "wintrust.lib")

// Forward declarations for static helper functions
static UINT EnumProcessesThread(LPVOID pParam);
static BOOL CALLBACK EnumWindowsCloseCallback(HWND hWnd, LPARAM lParam);

// Helper: convert FILETIME to ULONGLONG (100ns units)
static ULONGLONG FileTimeToUInt64(const FILETIME& ft)
{
    return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

// Double-click handler for list3: copy selected item back to clipboard
void CMFCApplication1Dlg::OnNMDblclkList3(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    int nItem = pItem->iItem;
    if (nItem >= 0 && nItem < (int)m_clipHistory.size())
    {
        CString text = m_clipHistory[nItem];
        if (!text.IsEmpty())
        {
            if (::OpenClipboard(m_hWnd))
            {
                ::EmptyClipboard();
                int len = (text.GetLength() + 1);
                HGLOBAL hGlob = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
                if (hGlob)
                {
                    LPWSTR pBuf = (LPWSTR)::GlobalLock(hGlob);
                    if (pBuf)
                    {
                        wcscpy_s(pBuf, len, text);
                        ::GlobalUnlock(hGlob);
                        HGLOBAL hSet = ::SetClipboardData(CF_UNICODETEXT, hGlob);
                        if (hSet == NULL)
                        {
                            ::GlobalFree(hGlob);
                        }
                    }
                    else
                    {
                        ::GlobalFree(hGlob);
                    }
                }
                ::CloseClipboard();
            }
        }
    }
    *pResult = 0;
}

void CMFCApplication1Dlg::RefreshProcessList()
{
    AfxBeginThread(EnumProcessesThread, this);
}

void CMFCApplication1Dlg::OnKillProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString procName = pList->GetItemText(idx, 0);
    CString pidStr = pList->GetItemText(idx, 4);
    DWORD dwPID = _ttoi(pidStr);

    CString strMsg;
    auto& loc = CLocalizationManager::GetInstance();
    strMsg.Format(loc.GetString(_T("Msg"), _T("ConfirmEndProcess")), procName, pidStr);
    if (MessageBox(strMsg, loc.GetString(_T("Msg"), _T("ConfirmEndProcessTitle")), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwPID);
    if (hProcess)
    {
        EnumWindows(EnumWindowsCloseCallback, (LPARAM)dwPID);
        Sleep(200);

        if (TerminateProcess(hProcess, 0))
        {
            CloseHandle(hProcess);
            MessageBox(loc.GetString(_T("Msg"), _T("ProcessEnded")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
            RefreshProcessList();
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(loc.GetString(_T("Msg"), _T("ProcessEndFail")), FormatLastError(err));
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(loc.GetString(_T("Msg"), _T("CannotOpenProcess")), FormatLastError(err));
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                ::PostMessage(this->GetSafeHwnd(), WM_NULL, 0, 0);
        }
        MessageBox(msg, loc.GetString(_T("Msg"), _T("AccessDenied")), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnRclickProcessList(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 32771, loc.GetString(_T("ProcessMenu"), _T("EndProcess")));
    menu.AppendMenu(MF_STRING, IDM_KILL_SAME_NAME, loc.GetString(_T("ProcessMenu"), _T("EndSameName")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 32774, loc.GetString(_T("ProcessMenu"), _T("Locate")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_PROCESS_AI_ANALYZE, loc.GetString(_T("ProcessMenu"), _T("AiAnalyze")));

    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CMFCApplication1Dlg::OnKillSameName()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString procName = pList->GetItemText(idx, 0);

    std::vector<DWORD> sameNamePids;
    for (const auto& pi : m_processes)
    {
        if (pi.name.CompareNoCase(procName) == 0)
            sameNamePids.push_back(pi.pid);
    }

    if (sameNamePids.empty()) return;

    CString strMsg;
    strMsg.Format(loc.GetString(_T("Msg"), _T("ConfirmEndSameName")), procName, (int)sameNamePids.size());
    if (MessageBox(strMsg, loc.GetString(_T("Msg"), _T("ConfirmBatchEnd")), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    int success = 0, fail = 0;
    for (DWORD pid : sameNamePids)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess)
        {
            EnumWindows(EnumWindowsCloseCallback, (LPARAM)pid);
            Sleep(50);
            if (TerminateProcess(hProcess, 0))
                success++;
            else
                fail++;
            CloseHandle(hProcess);
        }
        else
        {
            fail++;
        }
    }

    CString resultMsg;
    resultMsg.Format(loc.GetString(_T("Msg"), _T("BatchEndResult")), success, fail);
    MessageBox(resultMsg, loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
    RefreshProcessList();
}

void CMFCApplication1Dlg::OnBnClickedButton20()
{
    auto& loc = CLocalizationManager::GetInstance();
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), _T("taskmgr.exe"), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        TCHAR sysPath[MAX_PATH] = {0};
        if (GetSystemDirectory(sysPath, MAX_PATH) > 0)
        {
            CString full;
            full.Format(_T("%s\\taskmgr.exe"), sysPath);
            HINSTANCE h2 = ::ShellExecute(m_hWnd, _T("open"), full, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h2 <= 32)
            {
                MessageBox(loc.GetString(_T("Msg"), _T("TaskMgrFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(loc.GetString(_T("Msg"), _T("TaskMgrFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnLocateProcess()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    // Path is in column 2
    CString path = pList->GetItemText(idx, 2);
    if (path.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("NoPathForProcess")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    {
        CString args;
        args.Format(_T("/select,\"%s\""), path);
        ::ShellExecute(NULL, _T("open"), _T("explorer.exe"), args, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFoundForProcess")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
    }
}

// ============================================================================
// Digital signature verification for process AI analysis
// Uses an in-memory cache keyed by path to avoid repeated WinVerifyTrust calls.
// ============================================================================
std::map<CString, std::pair<CString, bool>> CMFCApplication1Dlg::s_signatureCache;

void CMFCApplication1Dlg::ClearSignatureCache()
{
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_signatureCache.clear();
}

bool CMFCApplication1Dlg::GetProcessSignatureInfo(const CString& path, CString& outSigner, bool& outValid)
{
    auto& loc = CLocalizationManager::GetInstance();
    outSigner = loc.GetString(_T("Msg"), _T("NoSignature"));
    outValid = false;

    if (path.IsEmpty())
        return false;

    // Check cache first to avoid repeated WinVerifyTrust disk/chain I/O
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_signatureCache.find(path);
        if (it != s_signatureCache.end())
        {
            outSigner = it->second.first;
            outValid = it->second.second;
            return true;
        }
    }

    WINTRUST_FILE_INFO fileInfo = {0};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path;
    fileInfo.hFile = NULL;
    fileInfo.pgKnownSubject = NULL;

    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA trustData = {0};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_SAFER_FLAG;

    LONG lStatus = WinVerifyTrust(NULL, &guidAction, &trustData);
    outValid = (lStatus == ERROR_SUCCESS);

    // Try to get signer info
    if (outValid)
    {
        CRYPT_PROVIDER_DATA* pProvData = WTHelperProvDataFromStateData(trustData.hWVTStateData);
        if (pProvData)
        {
            CRYPT_PROVIDER_SGNR* pSgnr = WTHelperGetProvSignerFromChain(pProvData, 0, FALSE, 0);
            if (pSgnr)
            {
                CRYPT_PROVIDER_CERT* pCert = WTHelperGetProvCertFromChain(pSgnr, 0);
                if (pCert && pCert->pCert)
                {
                    DWORD dwSize = CertGetNameString(pCert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                        0, NULL, NULL, 0);
                    if (dwSize > 0)
                    {
                        CertGetNameString(pCert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                            0, NULL, outSigner.GetBuffer(dwSize), dwSize);
                        outSigner.ReleaseBuffer();
                    }
                }
            }
        }
    }

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &guidAction, &trustData);

    // Cache the result so subsequent scans avoid WinVerifyTrust overhead
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_signatureCache[path] = { outSigner, outValid };
    }
    return true;
}

// ============================================================================
// Local pre-filter for AI process scan.
// Returns true if the process at the given path is considered "commonly safe"
// and should be skipped (not sent to AI). Never enabled for suspicious paths.
//
// Rules (option 2 — conservative):
//   1. Files under the Windows system directory (System32/SysWOW64/SystemApps)
//      are skipped outright (zero-cost path check, no signature lookup).
//   2. Files under "Program Files" / "Program Files (x86)" are skipped only if
//      they carry a valid *Microsoft* signature; third-party signed programs
//      are still sent to AI for review.
//   3. Any other path (user dir, temp, removable drives) is kept for AI.
// ============================================================================
static bool PrefilterIsSystemDir(const CString& path)
{
    TCHAR sysDir[MAX_PATH] = {0};
    GetSystemDirectory(sysDir, _countof(sysDir));
    CString sys = sysDir;
    sys.MakeLower();
    CString p = path;
    p.MakeLower();
    if (p.GetLength() >= sys.GetLength() + 1 &&
        p.Left(sys.GetLength()) == sys &&
        p[sys.GetLength()] == _T('\\'))
        return true;
    // Also cover SystemApps under Windows dir
    TCHAR winDir[MAX_PATH] = {0};
    GetWindowsDirectory(winDir, _countof(winDir));
    CString win = winDir;
    win.MakeLower();
    CString sysApps = win + _T("\\systemapps\\");
    if (p.GetLength() >= sysApps.GetLength() &&
        p.Left(sysApps.GetLength()).CompareNoCase(sysApps) == 0)
        return true;
    // SysWOW64 is under System32's parent; GetSystemDirectory returns System32.
    // SysWOW64 is a sibling: <win>\SysWOW64. Check it explicitly.
    CString sysWow = win + _T("\\syswow64\\");
    if (p.GetLength() >= sysWow.GetLength() &&
        p.Left(sysWow.GetLength()).CompareNoCase(sysWow) == 0)
        return true;
    return false;
}

static bool PrefilterIsProgramFiles(const CString& path)
{
    CString p = path;
    p.MakeLower();
    const LPCTSTR pfDirs[] = {
        _T("\\program files\\"),
        _T("\\program files (x86)\\"),
    };
    for (auto dir : pfDirs)
    {
        if (p.Find(dir) != -1)
            return true;
    }
    return false;
}

bool CMFCApplication1Dlg::ShouldSkipProcessPrefilter(const CString& path)
{
    if (path.IsEmpty())
        return false; // unknown path -> keep for AI

    // Rule 1: system directory -> skip (zero-cost)
    if (PrefilterIsSystemDir(path))
        return true;

    // Rule 2: Program Files -> skip only if valid Microsoft signature
    if (PrefilterIsProgramFiles(path))
    {
        CString signer;
        bool bValid = false;
        GetProcessSignatureInfo(path, signer, bValid);
        if (bValid && signer.Find(_T("Microsoft")) != -1)
            return true;
        // third-party signed or unsigned -> keep for AI
        return false;
    }

    // Rule 3: other paths -> keep for AI
    return false;
}

// ============================================================================
// Get file version info (CompanyName, OriginalFilename) for AI analysis
// Uses an in-memory cache keyed by path to avoid repeated disk I/O.
// ============================================================================
std::map<CString, std::pair<CString, CString>> CMFCApplication1Dlg::s_versionInfoCache;
std::mutex CMFCApplication1Dlg::s_cacheMutex;

// ============================================================================
// Background cache prewarm thread.
// Iterates the current process list and fills the version/signature caches so
// that a subsequent AI scan is near-instant. Runs detached; never touches UI.
// ============================================================================
UINT CMFCApplication1Dlg::ScanCachePrewarmThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    if (!dlg) return 0;

    // Snapshot the current process list (copy under the main-thread lock is not
    // done here; m_processes is only mutated by the enum thread which posts back
    // to the UI thread, and the scan dialog is modal/visible during prewarm).
    std::vector<ProcInfo> list = dlg->m_processes;
    for (const auto& pi : list)
    {
        if (pi.path.IsEmpty())
            continue;

        // Prefill version info cache (disk I/O is the slow part we want pre-done)
        CString company, orig;
        GetProcessVersionInfo(pi.path, company, orig);

        // Prefill signature cache only for Program Files paths (the only place
        // the pre-filter actually queries a signature); skip system/user dirs.
        if (PrefilterIsProgramFiles(pi.path))
        {
            CString signer;
            bool bValid = false;
            GetProcessSignatureInfo(pi.path, signer, bValid);
        }
    }
    return 0;
}

void CMFCApplication1Dlg::StartScanCachePrewarm()
{
    if (m_processes.empty())
        return;
    try
    {
        AfxBeginThread(ScanCachePrewarmThread, this);
    }
    catch (...)
    {
        // Thread creation failure is non-fatal; scan will collect on demand.
    }
}

void CMFCApplication1Dlg::ClearVersionInfoCache()
{
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_versionInfoCache.clear();
}

bool CMFCApplication1Dlg::GetProcessVersionInfo(const CString& path, CString& outCompany, CString& outOriginalName)
{
    outCompany = _T("无");
    outOriginalName = _T("");

    if (path.IsEmpty()) return false;

    // Check cache first to avoid repeated GetFileVersionInfo disk I/O
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_versionInfoCache.find(path);
        if (it != s_versionInfoCache.end())
        {
            outCompany = it->second.first;
            outOriginalName = it->second.second;
            return true;
        }
    }

    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSize(path, &dwHandle);
    if (dwSize == 0) return false;

    std::vector<BYTE> versionInfo(dwSize);
    if (!GetFileVersionInfo(path, 0, dwSize, versionInfo.data()))
        return false;

    // Get translation info for language/codepage
    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } *lpTranslate = nullptr;
    UINT cbTranslate = 0;
    if (VerQueryValue(versionInfo.data(), _T("\\VarFileInfo\\Translation"),
        (LPVOID*)&lpTranslate, &cbTranslate) && cbTranslate > 0)
    {
        // Build query string for CompanyName
        CString query;
        query.Format(_T("\\StringFileInfo\\%04x%04x\\CompanyName"),
            lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);

        LPTSTR pValue = nullptr;
        UINT cbValue = 0;
        if (VerQueryValue(versionInfo.data(), query.GetBuffer(),
            (LPVOID*)&pValue, &cbValue) && pValue && cbValue > 0)
        {
            outCompany = pValue;
        }
        query.ReleaseBuffer();

        // Build query string for OriginalFilename
        query.Format(_T("\\StringFileInfo\\%04x%04x\\OriginalFilename"),
            lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);

        if (VerQueryValue(versionInfo.data(), query.GetBuffer(),
            (LPVOID*)&pValue, &cbValue) && pValue && cbValue > 0)
        {
            outOriginalName = pValue;
        }
        query.ReleaseBuffer();
    }

    // Cache the result so subsequent scans avoid disk I/O
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_versionInfoCache[path] = { outCompany, outOriginalName };
    }
    return true;
}

// ============================================================================
// AI Analysis: analyze selected processes (supports multi-select)
// ============================================================================
void CMFCApplication1Dlg::OnProcessAiAnalyze()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    // Collect all selected items
    std::vector<int> selectedIndices;
    int idx = -1;
    while ((idx = pList->GetNextItem(idx, LVNI_SELECTED)) != -1)
    {
        selectedIndices.push_back(idx);
    }

    if (selectedIndices.empty())
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Msg"), _T("NoProcessSelected")), loc.GetString(_T("Msg"), _T("AiAnalyze")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));

    if (apiKey.IsEmpty())
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("Msg"), _T("ConfigApiKeyFirst")), loc.GetString(_T("Msg"), _T("AiAnalyze")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Build process info strings for each selected process
    auto& loc = CLocalizationManager::GetInstance();
    std::vector<CString> procInfos;
    for (int i : selectedIndices)
    {
        CString procName = pList->GetItemText(i, 0);
        CString pidStr = pList->GetItemText(i, 4);
        CString path = pList->GetItemText(i, 2);

        // Check digital signature
        CString signer;
        bool bValid = false;
        GetProcessSignatureInfo(path, signer, bValid);

        // Check version info
        CString company, origName;
        GetProcessVersionInfo(path, company, origName);

        CString info;
        info.Format(loc.GetString(_T("Msg"), _T("ProcInfoFmt")),
            procName, pidStr,
            path.IsEmpty() ? loc.GetString(_T("Msg"), _T("None")) : path,
            bValid ? (signer.IsEmpty() ? loc.GetString(_T("Msg"), _T("Valid")) : loc.GetString(_T("Msg"), _T("ValidPrefix")) + signer) : loc.GetString(_T("Msg"), _T("InvalidNoSig")),
            company,
            origName.IsEmpty() ? loc.GetString(_T("Msg"), _T("None")) : origName);
        procInfos.push_back(info);
    }

    // Show the result dialog (modeless, AI request sent in OnInitDialog)
    auto pDlg = std::make_unique<CProcessAiResultDlg>(procInfos, this);
    CProcessAiResultDlg* pRaw = pDlg.release();
    pRaw->Create(IDD_PROCESS_AI_RESULT_DLG, this);
    pRaw->ShowWindow(SW_SHOW);
}

// ============================================================================
// AI Scan: scan all processes for suspicious/useless ones
// ============================================================================
void CMFCApplication1Dlg::OnBnClickedProcessAiScan()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_processes.empty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("RefreshProcessListFirst")), loc.GetString(_T("Msg"), _T("AiScan")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Create and show the scan dialog (scan starts when user clicks "开始扫描")
    auto pDlg = std::make_unique<CProcessScanDlg>(this);
    CProcessScanDlg* pRaw = pDlg.release();
    pRaw->Create(IDD_PROCESS_SCAN_DLG, this);
    pRaw->ShowWindow(SW_SHOW);

    // Prewarm version/signature caches in the background so the actual AI scan
    // (triggered by the user clicking "开始扫描") is near-instant.
    StartScanCachePrewarm();
}

LRESULT CMFCApplication1Dlg::OnProcessScanStart(WPARAM wParam, LPARAM lParam)
{
    // High bit of wParam carries the pre-filter checkbox toggle (0x10000)
    int scanLevel = (int)(wParam & 0xFFFF);
    bool bPrefilter = (wParam & 0x10000) != 0;
    HWND hScanDlg = (HWND)lParam;

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        auto& loc = CLocalizationManager::GetInstance();
        ::MessageBox(hScanDlg, loc.GetString(_T("Msg"), _T("ConfigApiKeyFirst")), loc.GetString(_T("Msg"), _T("AiScan")), MB_OK | MB_ICONWARNING);
        return 0;
    }

    // Build process list for AI with version info
    // Deduplicate by process name, skip well-known safe system processes, limit to 80
    CString processList;
    int count = 0;
    int origNameSent = 0;
    std::set<CString> seenNames;

    // Well-known Windows core system processes that are almost always safe, skip to save tokens
    static const LPCTSTR kSafeSystemProcs[] = {
        _T("smss.exe"), _T("csrss.exe"), _T("wininit.exe"), _T("winlogon.exe"),
        _T("services.exe"), _T("lsass.exe"), _T("lsm.exe"), _T("fontdrvhost.exe"),
        _T("svchost.exe"), _T("dwm.exe"), _T("explorer.exe"), _T("sihost.exe"),
        _T("RuntimeBroker.exe"), _T("SearchHost.exe"), _T("SearchIndexer.exe"),
        _T("ShellExperienceHost.exe"), _T("StartMenuExperienceHost.exe"),
        _T("SecurityHealthService.exe"), _T("SecurityHealthSystray.exe"),
        _T("audiodg.exe"), _T("spoolsv.exe"), _T("WlanSvc.dll"),
        _T("System"), _T("Registry"), _T("Idle"),
    };

    for (const auto& pi : m_processes)
    {
        if (count >= 80) break;
        if (seenNames.count(pi.name) > 0) continue;
        seenNames.insert(pi.name);

        bool bSkip = false;
        for (auto safeName : kSafeSystemProcs)
        {
            if (pi.name.CompareNoCase(safeName) == 0)
            {
                bSkip = true;
                break;
            }
        }
        if (bSkip) continue;

        // Local pre-filter: skip commonly-safe processes (system dir / MS-signed
        // Program Files) before collecting version info and sending to AI.
        if (bPrefilter && ShouldSkipProcessPrefilter(pi.path))
            continue;

        CString company, origName;
        GetProcessVersionInfo(pi.path, company, origName);

        CString origField;
        if (!origName.IsEmpty() && origName.CompareNoCase(pi.name) != 0)
        {
            origField.Format(_T(", 原名: %s"), origName);
            origNameSent++;
        }

        CString companyField;
        if (!company.IsEmpty() &&
            company.Find(_T("Microsoft")) == -1 &&
            company.Find(_T("Windows")) == -1)
        {
            companyField.Format(_T(", 公司: %s"), company);
        }

        CString line;
        line.Format(_T("%d. %s(PID:%u, 路径:%s%s%s)\n"),
            count + 1, pi.name, pi.pid,
            pi.path.IsEmpty() ? _T("无") : (LPCTSTR)pi.path,
            (LPCTSTR)companyField,
            (LPCTSTR)origField);
        processList += line;
        count++;
    }

    CString prompt;
    prompt.Format(_T("Analyze the following Windows processes, find suspicious (malicious/virus/trojan) and unnecessary (redundant adware) processes.\n")
        _T("Key indicators: path not in C:\\Windows system directory, company name missing/disreputable, process name mimics system process but path differs, original name differs significantly from display name.\n\n")
        _T("[CRITICAL: Absolute whitelist — the following core system processes must NEVER be included in results, even if their name/path seems anomalous!]\n")
        _T("Forbidden: smss.exe, csrss.exe, wininit.exe, winlogon.exe, services.exe, lsass.exe, lsm.exe, svchost.exe,\n")
        _T("fontdrvhost.exe, dwm.exe, sihost.exe, RuntimeBroker.exe, explorer.exe, ntoskrnl.exe, hal.dll,\n")
        _T("Registry, System, Idle, SearchHost.exe, SearchIndexer.exe,\n")
        _T("ShellExperienceHost.exe, StartMenuExperienceHost.exe, SecurityHealthService.exe,\n")
        _T("TextInputHost.exe, ctfmon.exe, audiodg.exe, spoolsv.exe, conhost.exe, dllhost.exe,\n")
        _T("taskhostw.exe, taskhostex.exe, SearchProtocolHost.exe, SearchFilterHost.exe,\n")
        _T("and all Microsoft-signed processes with path prefix C:\\Windows\\System32\\, C:\\Windows\\SysWOW64\\, C:\\Windows\\SystemApps\\.\n")
        _T("(If a whitelisted process seems genuinely anomalous, write \"possible false positive, manual review needed\" in the reason field and skip it — do NOT include it in the result array!)\n\n")
        _T("[Other rules]\n")
        _T("- Suspicious processes must meet at least 2 criteria; if uncertain, do not include (better to miss than false-positive)\n")
        _T("- Unnecessary processes: only include clear popup ads, autostart toolbars, useless bundled software; common software (browsers, antivirus, IME, drivers, etc.) are NOT unnecessary\n")
        _T("- Return at most 10 results, keeping only the most suspicious/unnecessary\n")
        _T("- Sort results by suspicion level, highest first\n\n")
        _T("(Already filtered svchost/explorer/smss/csrss and %d other known-safe core system processes, currently listing %d processes to review)\n")
        _T("Process list:\n%s\n")
        _T("Return JSON array. Format: [{\"pid\":number,\"name\":\"xxx.exe\",\"risk\":\"suspicious\",\"reason\":\"brief reason (max 30 chars)\"},...]")
        _T("risk is either \"suspicious\" or \"unnecessary\". If no problematic processes, return empty array []. Return ONLY JSON, no markdown, no explanations."),
        (int)(_countof(kSafeSystemProcs)), count, processList);

    // Build system prompt and user prompt based on scan level
    CString sysPrompt, userPrompt;
    switch (scanLevel)
    {
    case 0: // Conservative — daily check, extremely strict
        sysPrompt = _T("You are an extremely conservative Windows system security analyst.\n")
            _T("Core principle: System critical process protection > Threat detection (better to miss than to false-positive).\n\n")
            _T("Strict rules:\n")
            _T("1. Absolute whitelist (csrss/smss/lsass/wininit/winlogon/services/svchost/dwm/explorer/shell experience/TextInputHost/ctfmon/conhost/taskhostw/audiodg/spoolsv/SearchHost/SecurityHealth, etc.) and any process located in C:\\Windows\\System32\\ or SystemApps signed by Microsoft must NOT be included, even if suspicious — skip rather than risk it;\n")
            _T("2. To mark as \"suspicious\", must meet 3+ criteria simultaneously (e.g.: non-system directory + unsigned + suspicious company + name masquerading, etc.); insufficient evidence = do not include;\n")
            _T("3. To mark as \"unnecessary\": must be typical popup ads, browser bundle toolbars, resident adware; normal office/entertainment software, antivirus, IME, driver/power management, GPU control panels are NOT unnecessary;\n")
            _T("4. If a whitelisted process seems anomalous (e.g., similar name but wrong path), skip it in the JSON result. Leave it for manual judgment;\n")
            _T("5. Return only the top 5 most suspicious results, sorted by suspicion level descending;\n")
            _T("6. If uncertain, return an empty array [] — do not pad results.\n\n")
            _T("Output: JSON array only, no markdown, no explanatory text, no code block markers.");
        userPrompt = prompt;
        break;

    case 2: // Aggressive — virus hunting, less strict, report anything suspicious
        sysPrompt = _T("You are a Windows system security analyst in virus-hunting mode.\n")
            _T("Core principle: Better to false-positive than to miss (but NEVER touch system core processes).\n\n")
            _T("Rules:\n")
            _T("1. Absolutely forbidden processes: smss.exe, csrss.exe, wininit.exe, System, Registry, Idle. Other processes, even in System32, can be included if path/behavior is anomalous;\n")
            _T("2. To mark as \"suspicious\", only 1 clear criterion is needed (e.g.: non-system directory, unsigned, suspicious/missing company, name mimics system process, anomalous path, etc.);\n")
            _T("3. To mark as \"unnecessary\": in addition to popup ads and bundle toolbars, also include resident background processes consuming resources with no real purpose, anomalous autostart processes, suspicious third-party services;\n")
            _T("4. Common software (browsers, antivirus, IME, drivers, etc.) should only be included when anomalous behavior exists (abnormal path, unsigned, multiple anomalous copies);\n")
            _T("5. Return at most 20 results, sorted by suspicion level descending;\n")
            _T("6. Even if slightly suspicious, include it — do not be too conservative.\n\n")
            _T("Output: JSON array only, no markdown, no explanatory text, no code block markers.");
        userPrompt.Format(_T("Analyze the following Windows processes, find suspicious (malicious/virus/trojan) and unnecessary (redundant adware) processes.\n")
            _T("Key indicators: path not in system directory, company name missing/disreputable, process name mimics system process but path differs, original name differs significantly from display name.\n\n")
            _T("[Absolute forbidden: smss.exe, csrss.exe, wininit.exe, System, Registry, Idle processes must NOT be included. All other processes can be included.]\n\n")
            _T("(Already filtered smss/csrss/wininit and %d other core system processes, currently listing %d processes)\n")
            _T("Process list:\n%s\n")
            _T("Return JSON array. Format: [{\"pid\":number,\"name\":\"xxx.exe\",\"risk\":\"suspicious\",\"reason\":\"brief reason (max 30 chars)\"},...]")
            _T("risk is either \"suspicious\" or \"unnecessary\". At most 20 results. If no problematic processes, return empty array []. Return ONLY JSON, no markdown."),
            (int)(_countof(kSafeSystemProcs)), count, processList);
        break;

    default: // 1=Standard (balanced)
        sysPrompt = _T("You are an extremely conservative Windows system security analyst.\n")
            _T("Core principle: System critical process protection > Threat detection (better to miss than to false-positive).\n\n")
            _T("Strict rules:\n")
            _T("1. Absolute whitelist (csrss/smss/lsass/wininit/winlogon/services/svchost/dwm/explorer/shell experience/TextInputHost/ctfmon/conhost/taskhostw/audiodg/spoolsv/SearchHost/SecurityHealth, etc.) and any process located in C:\\Windows\\System32\\ or SystemApps signed by Microsoft must NOT be included, even if suspicious — skip rather than risk it;\n")
            _T("2. To mark as \"suspicious\", must meet 2+ criteria simultaneously (e.g.: non-system directory + unsigned + suspicious company + name masquerading, etc.);\n")
            _T("3. To mark as \"unnecessary\": must be typical popup ads, browser bundle toolbars, resident adware; normal office/entertainment software, antivirus, IME, driver/power management, GPU control panels are NOT unnecessary;\n")
            _T("4. If a whitelisted process seems anomalous (e.g., similar name but wrong path), skip it in the JSON result. Leave it for manual judgment;\n")
            _T("5. Return only the top 10 most suspicious results, sorted by suspicion level descending;\n")
            _T("6. If uncertain, return an empty array [] — do not pad results.\n\n")
            _T("Output: JSON array only, no markdown, no explanatory text, no code block markers.");
        userPrompt = prompt;
        break;
    }

    // Add language constraint based on current UI language
    CString langName = CLocalizationManager::GetInstance().GetString(_T("Language"), _T("DisplayName"));
    sysPrompt += _T("\n\nIMPORTANT: The \"risk\" and \"reason\" fields in the JSON must be written in ") + langName + _T(".");

    // Build messages and send async
    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), sysPrompt });
    messages.push_back({ _T("user"), userPrompt });

    CAIApiClient::SendAsyncStreaming(messages, vendor, apiKey, model, hScanDlg);
    return 0;
}

// ============================================================================
// Background thread: enumerate processes with CPU calculation
// ============================================================================
static UINT EnumProcessesThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::ProcInfo>* results = new std::vector<CMFCApplication1Dlg::ProcInfo>();

    // Enable SeDebugPrivilege
    auto EnableDebugPrivilege = []() {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;
        TOKEN_PRIVILEGES tp = {0};
        LUID luid;
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
        {
            CloseHandle(hToken);
            return false;
        }
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
        return (ok && GetLastError() == ERROR_SUCCESS);
    };
    EnableDebugPrivilege();

    // ---- Pass 1: collect initial CPU times and basic info ----
    std::map<DWORD, ULONGLONG> initialCpuTimes;

    FILETIME ftSysIdle1, ftSysKernel1, ftSysUser1;
    GetSystemTimes(&ftSysIdle1, &ftSysKernel1, &ftSysUser1);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe))
        {
            do
            {
                CMFCApplication1Dlg::ProcInfo pi;
                pi.name = pe.szExeFile;
                pi.pid = pe.th32ProcessID;
                pi.path = _T("");
                pi.memKB = 0;
                pi.cpuPercent = 0.0;

                // Get process path
                TCHAR bufPath[MAX_PATH] = {0};
                HANDLE hProcPath = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
                if (hProcPath)
                {
                    DWORD size = _countof(bufPath);
                    if (QueryFullProcessImageName(hProcPath, 0, bufPath, &size))
                        pi.path = bufPath;

                    // Get initial CPU time
                    FILETIME ftCreate, ftExit, ftKernel, ftUser;
                    if (GetProcessTimes(hProcPath, &ftCreate, &ftExit, &ftKernel, &ftUser))
                    {
                        ULONGLONG cpuTime = FileTimeToUInt64(ftKernel) + FileTimeToUInt64(ftUser);
                        initialCpuTimes[pi.pid] = cpuTime;
                    }

                    CloseHandle(hProcPath);
                }

                if (pi.path.IsEmpty())
                {
                    HANDLE hModSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pi.pid);
                    if (hModSnap != INVALID_HANDLE_VALUE)
                    {
                        MODULEENTRY32 me;
                        me.dwSize = sizeof(me);
                        if (Module32First(hModSnap, &me))
                            pi.path = me.szExePath;
                        CloseHandle(hModSnap);
                    }
                }

                // Get memory info
                HANDLE hProcMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.pid);
                if (hProcMem)
                {
                    PROCESS_MEMORY_COUNTERS pmc = {0};
                    pmc.cb = sizeof(pmc);
                    if (GetProcessMemoryInfo(hProcMem, &pmc, sizeof(pmc)))
                        pi.memKB = pmc.WorkingSetSize / 1024;
                    CloseHandle(hProcMem);
                }

                results->push_back(pi);
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    // ---- Wait for measurement interval ----
    Sleep(250);

    // ---- Pass 2: calculate CPU% ----
    FILETIME ftSysIdle2, ftSysKernel2, ftSysUser2;
    GetSystemTimes(&ftSysIdle2, &ftSysKernel2, &ftSysUser2);

    ULONGLONG sysTime1 = FileTimeToUInt64(ftSysKernel1) + FileTimeToUInt64(ftSysUser1);
    ULONGLONG sysTime2 = FileTimeToUInt64(ftSysKernel2) + FileTimeToUInt64(ftSysUser2);
    ULONGLONG sysDelta = (sysTime2 > sysTime1) ? (sysTime2 - sysTime1) : 0;

    for (auto& pi : *results)
    {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (hProc)
        {
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser))
            {
                ULONGLONG cpuTime2 = FileTimeToUInt64(ftKernel) + FileTimeToUInt64(ftUser);
                auto it = initialCpuTimes.find(pi.pid);
                if (it != initialCpuTimes.end())
                {
                    ULONGLONG procDelta = (cpuTime2 > it->second) ? (cpuTime2 - it->second) : 0;
                    if (sysDelta > 0)
                        pi.cpuPercent = (double)procDelta / sysDelta * 100.0;
                }
            }
            CloseHandle(hProc);
        }
    }

    // Post results to UI
    HWND hwnd = dlg->GetSafeHwnd();
    if (hwnd != NULL && IsValidWindow(hwnd))
    {
        if (!::PostMessage(hwnd, CMFCApplication1Dlg::WM_REFRESH_PROCESSES_DONE, (WPARAM)results, 0))
        {
            delete results;
        }
    }
    else
    {
        delete results;
    }
    return 0;
}

static BOOL CALLBACK EnumWindowsCloseCallback(HWND hWnd, LPARAM lParam)
{
    DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);
    if (pid == (DWORD)lParam)
    {
        ::PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

// ========== Process sorting and filtering ==========

void CMFCApplication1Dlg::PopulateProcessList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    pList->DeleteAllItems();

    for (int i = 0; i < (int)m_processes.size(); i++)
    {
        auto& pi = m_processes[i];
        CString pidStr;
        pidStr.Format(_T("%u"), pi.pid);
        CString memStr;
        memStr.Format(_T("%llu"), (unsigned long long)pi.memKB);
        CString cpuStr;
        cpuStr.Format(_T("%.1f"), pi.cpuPercent);

        LVITEM li = {0};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = const_cast<LPTSTR>((LPCTSTR)pi.name);
        pList->InsertItem(&li);
        pList->SetItemText(i, 1, cpuStr);
        pList->SetItemText(i, 2, pi.path);
        pList->SetItemText(i, 3, memStr);
        pList->SetItemText(i, 4, pidStr);
    }
}

void CMFCApplication1Dlg::SortProcessList()
{
    if (m_nSortColumn < 0 || m_nSortColumn > 4) return;

    std::sort(m_processes.begin(), m_processes.end(),
        [this](const ProcInfo& a, const ProcInfo& b) -> bool
        {
            int cmp = 0;
            switch (m_nSortColumn)
            {
            case 0: cmp = a.name.CompareNoCase(b.name); break;
            case 1: cmp = (a.cpuPercent > b.cpuPercent) ? 1 : (a.cpuPercent < b.cpuPercent) ? -1 : 0; break;
            case 2: cmp = a.path.CompareNoCase(b.path); break;
            case 3: cmp = (a.memKB > b.memKB) ? 1 : (a.memKB < b.memKB) ? -1 : 0; break;
            case 4: cmp = (a.pid > b.pid) ? 1 : (a.pid < b.pid) ? -1 : 0; break;
            }
            return m_bSortAscending ? (cmp < 0) : (cmp > 0);
        });
}

void CMFCApplication1Dlg::ApplyProcessFilter()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    if (m_processes.empty()) return;

    CString filterText;
    CEdit* pEditFilter = (CEdit*)GetDlgItem(IDC_EDIT_PROCESS_FILTER);
    if (pEditFilter)
        pEditFilter->GetWindowText(filterText);

    bool useRegex = false;
    CButton* pBtnRegex = (CButton*)GetDlgItem(IDC_CHECK_PROCESS_REGEX);
    if (pBtnRegex)
        useRegex = (pBtnRegex->GetCheck() == BST_CHECKED);

    SortProcessList();

    if (!filterText.IsEmpty())
    {
        std::vector<ProcInfo> filtered;
        try
        {
            if (useRegex)
            {
                std::wregex re(filterText.GetString(), std::regex_constants::icase);
                for (auto& pi : m_processes)
                {
                    if (std::regex_search(std::wstring(pi.name), re) ||
                        std::regex_search(std::wstring(pi.path), re))
                    {
                        filtered.push_back(pi);
                    }
                }
            }
            else
            {
                CString lower = filterText;
                lower.MakeLower();
                for (auto& pi : m_processes)
                {
                    CString name = pi.name; name.MakeLower();
                    CString path = pi.path; path.MakeLower();
                    if (name.Find(lower) >= 0 || path.Find(lower) >= 0)
                        filtered.push_back(pi);
                }
            }
        }
        catch (...)
        {
            OutputDebugString(_T("[ProcessManager] Invalid regex filter, falling back to unfiltered\n"));
            filtered = m_processes;
        }

        pList->DeleteAllItems();
        for (int i = 0; i < (int)filtered.size(); i++)
        {
            auto& pi = filtered[i];
            CString pidStr;
            pidStr.Format(_T("%u"), pi.pid);
            CString memStr;
            memStr.Format(_T("%llu"), (unsigned long long)pi.memKB);
            CString cpuStr;
            cpuStr.Format(_T("%.1f"), pi.cpuPercent);

            LVITEM li = {0};
            li.mask = LVIF_TEXT;
            li.iItem = i;
            li.pszText = const_cast<LPTSTR>((LPCTSTR)pi.name);
            pList->InsertItem(&li);
            pList->SetItemText(i, 1, cpuStr);
            pList->SetItemText(i, 2, pi.path);
            pList->SetItemText(i, 3, memStr);
            pList->SetItemText(i, 4, pidStr);
        }
    }
    else
    {
        PopulateProcessList();
    }

    // Update header sort arrows
    if (pList->GetHeaderCtrl())
    {
        for (int i = 0; i < 5; i++)
        {
            HDITEM hdi = {0};
            hdi.mask = HDI_TEXT;
            TCHAR buf[128];
            hdi.pszText = buf;
            hdi.cchTextMax = 128;
            pList->GetHeaderCtrl()->GetItem(i, &hdi);

            CString text = hdi.pszText;
            if (text.GetLength() >= 2 &&
                (text[0] == _T('\x25B2') || text[0] == _T('\x25BC') || text[0] == _T(' ')))
            {
                text = text.Mid(2);
            }

            if (i == m_nSortColumn)
            {
                text = (m_bSortAscending ? _T("▲ ") : _T("▼ ")) + text;
            }

            hdi.mask = HDI_TEXT;
            hdi.pszText = const_cast<LPTSTR>((LPCTSTR)text);
            pList->GetHeaderCtrl()->SetItem(i, &hdi);
        }
    }
}

void CMFCApplication1Dlg::OnProcessColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
    int nCol = pNMListView->iSubItem;

    if (nCol == m_nSortColumn)
        m_bSortAscending = !m_bSortAscending;
    else
    {
        m_nSortColumn = nCol;
        m_bSortAscending = true;
    }

    ApplyProcessFilter();
    *pResult = 0;
}

void CMFCApplication1Dlg::OnProcessFilterChange()
{
    ApplyProcessFilter();
}

void CMFCApplication1Dlg::OnProcessRegexHelp()
{
    OnHelpRegexGuide();
}