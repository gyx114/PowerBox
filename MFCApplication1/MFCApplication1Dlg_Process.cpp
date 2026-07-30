#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "AIApiClient.h"
#include "ProcessScanDlg.h"
#include "ProcessAiResultDlg.h"
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
    strMsg.Format(_T("确定要结束进程\n%s (PID: %s) 吗？"), procName, pidStr);
    if (MessageBox(strMsg, _T("确认结束进程"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwPID);
    if (hProcess)
    {
        EnumWindows(EnumWindowsCloseCallback, (LPARAM)dwPID);
        Sleep(200);

        if (TerminateProcess(hProcess, 0))
        {
            CloseHandle(hProcess);
            MessageBox(_T("进程已成功结束。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            RefreshProcessList();
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("结束进程失败：%s"), FormatLastError(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("无法打开进程以终止。错误：%s"), FormatLastError(err));
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                ::PostMessage(this->GetSafeHwnd(), WM_NULL, 0, 0);
        }
        MessageBox(msg, _T("权限不足"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnRclickProcessList(NMHDR* pNMHDR, LRESULT* pResult)
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 32771, _T("结束进程"));
    menu.AppendMenu(MF_STRING, IDM_KILL_SAME_NAME, _T("结束所有同名进程"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 32774, _T("定位"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_PROCESS_AI_ANALYZE, _T("AI分析"));

    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CMFCApplication1Dlg::OnKillSameName()
{
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
    strMsg.Format(_T("确定要结束所有 \"%s\" 进程吗？\n共 %d 个实例。"), procName, (int)sameNamePids.size());
    if (MessageBox(strMsg, _T("确认批量结束"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

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
    resultMsg.Format(_T("已结束 %d 个进程，失败 %d 个。"), success, fail);
    MessageBox(resultMsg, _T("批量结束完成"), MB_OK | MB_ICONINFORMATION);
    RefreshProcessList();
}

void CMFCApplication1Dlg::OnBnClickedButton20()
{
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
                MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnLocateProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    // Path is in column 2
    CString path = pList->GetItemText(idx, 2);
    if (path.IsEmpty())
    {
        MessageBox(_T("无法获取该进程的路径或路径为空。"), _T("提示"), MB_OK | MB_ICONWARNING);
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
        MessageBox(_T("找不到该文件，可能无权访问或进程已退出。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

// ============================================================================
// Digital signature verification for process AI analysis
// ============================================================================
bool CMFCApplication1Dlg::GetProcessSignatureInfo(const CString& path, CString& outSigner, bool& outValid)
{
    outSigner = _T("无签名");
    outValid = false;

    if (path.IsEmpty())
        return false;

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

    return true;
}

// ============================================================================
// Get file version info (CompanyName, OriginalFilename) for AI analysis
// ============================================================================
bool CMFCApplication1Dlg::GetProcessVersionInfo(const CString& path, CString& outCompany, CString& outOriginalName)
{
    outCompany = _T("无");
    outOriginalName = _T("");

    if (path.IsEmpty()) return false;

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
        MessageBox(_T("请先选择要分析的进程。"), _T("AI分析"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));

    if (apiKey.IsEmpty())
    {
        MessageBox(_T("请先在设置中配置AI API密钥。"), _T("AI分析"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Build process info strings for each selected process
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
        info.Format(_T("进程名: %s, PID: %s, ")
            _T("路径: %s, ")
            _T("数字签名: %s, ")
            _T("公司: %s, ")
            _T("原始文件名: %s"),
            procName, pidStr,
            path.IsEmpty() ? CString(_T("无")) : path,
            bValid ? (signer.IsEmpty() ? CString(_T("有效")) : CString(_T("有效 - ") + signer)) : CString(_T("无效/无签名")),
            company,
            origName.IsEmpty() ? CString(_T("无")) : origName);
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
    if (m_processes.empty())
    {
        MessageBox(_T("请先刷新进程列表。"), _T("AI扫描"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        MessageBox(_T("请先在设置中配置AI API密钥。"), _T("AI扫描"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Build process list for AI with version info
    // Deduplicate by process name (keep first occurrence), limit to 100 entries
    CString processList;
    int count = 0;
    std::set<CString> seenNames;
    for (const auto& pi : m_processes)
    {
        if (count >= 100) break;
        // Skip duplicate process names (same executable appearing multiple times)
        if (seenNames.count(pi.name) > 0) continue;
        seenNames.insert(pi.name);

        CString company, origName;
        GetProcessVersionInfo(pi.path, company, origName);
        CString line;
        line.Format(_T("%d. %s (PID: %u, 路径: %s, 公司: %s, 原始文件名: %s)\n"),
            count + 1, pi.name, pi.pid,
            pi.path.IsEmpty() ? CString(_T("无")) : pi.path,
            company,
            origName.IsEmpty() ? CString(_T("无")) : origName);
        processList += line;
        count++;
    }

    CString prompt;
    prompt.Format(_T("分析以下Windows进程列表，找出可疑进程（恶意软件、病毒、木马等）和无用进程（冗余、广告软件等）。\n")
        _T("注意：公司名和原始文件名是重要判断依据。\n")
        _T("例如：原始文件名与进程名不一致、公司名为空或可疑、无签名的系统进程名伪装等。\n\n")
        _T("进程列表：\n%s\n")
        _T("请用纯JSON数组格式返回，不要包含markdown代码块标记，只返回最多20个结果：\n")
        _T("[{\"pid\":1234,\"name\":\"xxx.exe\",\"risk\":\"可疑\",\"reason\":\"原因说明\"},...]\n\n")
        _T("risk字段取值：可疑/无用。pid必须是数字。如果未发现可疑进程，返回空数组[]。"),
        processList);

    // Create and show the scan dialog
    auto pDlg = std::make_unique<CProcessScanDlg>(this);
    CProcessScanDlg* pRaw = pDlg.release();
    pRaw->Create(IDD_PROCESS_SCAN_DLG, this);
    pRaw->ShowWindow(SW_SHOW);

    // Build messages and send async
    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), _T("你是一个Windows系统安全专家。请用中文回答。") });
    messages.push_back({ _T("user"), prompt });

    CAIApiClient::SendAsync(messages, vendor, apiKey, model, pRaw->m_hWnd);
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