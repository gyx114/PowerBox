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
    menu.AppendMenu(MF_STRING, 32771, loc.GetString(_T("Menu"), _T("EndProcess")));
    menu.AppendMenu(MF_STRING, IDM_KILL_SAME_NAME, loc.GetString(_T("Menu"), _T("EndSameName")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 32774, loc.GetString(_T("Menu"), _T("Locate")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_PROCESS_AI_ANALYZE, loc.GetString(_T("Menu"), _T("AiAnalyze")));

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
// ============================================================================
bool CMFCApplication1Dlg::GetProcessSignatureInfo(const CString& path, CString& outSigner, bool& outValid)
{
    auto& loc = CLocalizationManager::GetInstance();
    outSigner = loc.GetString(_T("Msg"), _T("NoSignature"));
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
}

LRESULT CMFCApplication1Dlg::OnProcessScanStart(WPARAM wParam, LPARAM lParam)
{
    int scanLevel = (int)wParam;
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
    prompt.Format(_T("分析以下Windows进程，找出可疑(恶意/病毒/木马)和无用(冗余广告软件)进程。\n")
        _T("判断要点：路径不在C:\\Windows系统目录、公司名缺失/非正规、进程名伪装成系统名但路径不对、原名与显示名不一致且相差很大。\n\n")
        _T("【极其重要：绝对白名单 — 以下系统核心进程，即使名称/路径出现异常，也绝对不能列入返回结果！】\n")
        _T("禁止列入：smss.exe、csrss.exe、wininit.exe、winlogon.exe、services.exe、lsass.exe、lsm.exe、svchost.exe、\n")
        _T("fontdrvhost.exe、dwm.exe、sihost.exe、RuntimeBroker.exe、explorer.exe、ntoskrnl.exe、hal.dll、\n")
        _T("Registry、System进程、Idle进程、SearchHost.exe、SearchIndexer.exe、\n")
        _T("ShellExperienceHost.exe、StartMenuExperienceHost.exe、SecurityHealthService.exe、\n")
        _T("TextInputHost.exe、ctfmon.exe、audiodg.exe、spoolsv.exe、conhost.exe、dllhost.exe、\n")
        _T("taskhostw.exe、taskhostex.exe、SearchProtocolHost.exe、SearchFilterHost.exe、\n")
        _T("以及所有路径前缀为 C:\\Windows\\System32\\、C:\\Windows\\SysWOW64\\、C:\\Windows\\SystemApps\\ 的Microsoft签名进程。\n")
        _T("（如果某个上述白名单进程确实异常可疑，请直接在reason里写出""疑似误报请人工核实""并跳过该条，不要列入返回数组！）\n\n")
        _T("【其他规则】\n")
        _T("· 可疑进程必须满足至少2条判断依据；不确定则不列入（宁放过不杀错）\n")
        _T("· 无用进程只列入明确的弹窗广告、自启工具栏、无用捆绑软件；普通常见软件（浏览器、杀软、输入法、驱动工具等）一律不算无用\n")
        _T("· 最多返回10条结果，超过的请只保留最可疑/最无用的\n")
        _T("· 结果按可疑程度从高到低排序\n\n")
        _T("(已自动过滤svchost/explorer/smss/csrss等%d个已知安全的系统核心进程，当前仅列需关注的%d个进程)\n")
        _T("进程列表：\n%s\n")
        _T("返回JSON数组。格式：[{\"pid\":数字,\"name\":\"xxx.exe\",\"risk\":\"可疑\",\"reason\":\"简短原因（不超过30字）\"},...]")
        _T("risk取""可疑""或""无用""。若无不安全进程，返回空数组[]。只返回JSON，不要markdown、不要说明文字。"),
        (int)(_countof(kSafeSystemProcs)), count, processList);

    // Build system prompt and user prompt based on scan level
    CString sysPrompt, userPrompt;
    switch (scanLevel)
    {
    case 0: // 保守 — daily check, extremely strict
        sysPrompt = _T("你是一个极其保守的Windows系统安全分析专家。\n")
            _T("核心原则：系统关键进程保护 > 威胁检测（宁放过不杀错）。\n\n")
            _T("必须遵守以下铁律：\n")
            _T("1. 绝对白名单（csrss/smss/lsass/wininit/winlogon/services/svchost/dwm/explorer/shell experience/TextInputHost/ctfmon/conhost/taskhostw/audiodg/spoolsv/SearchHost/SecurityHealth等）以及任何位于C:\\Windows\\System32\\或SystemApps下且由Microsoft发布的进程，即便可疑也不得列入返回，宁可跳过也不冒险；\n")
            _T("2. 列入""可疑""必须同时满足3条以上判断依据（如：非系统目录 + 无签名 + 公司名可疑 + 进程名伪装等），依据不足的不列入；\n")
            _T("3. 列入""无用""必须是典型的弹窗广告、浏览器捆绑工具栏、常驻广告软件等；正常办公/娱乐软件、杀毒软件、输入法、驱动/电源管理、显卡控制面板等均不得列为无用；\n")
            _T("4. 若发现上述绝对白名单进程看似异常（如名字相似但路径不对），请在JSON结果中跳过该条目，不要返回。请交由人工判断；\n")
            _T("5. 仅返回最可疑的前5条结果，按可疑度从高到低排序；\n")
            _T("6. 若不确定，请直接返回空数组[]，不要为了凑数而列入。\n\n")
            _T("输出要求：仅JSON数组，不包含markdown、解释文字、代码块标记。");
        userPrompt = prompt;
        break;

    case 2: // 激进 — virus hunting, less strict, report anything suspicious
        sysPrompt = _T("你是一个Windows系统安全分析专家，当前为病毒查杀模式。\n")
            _T("核心原则：宁可误报也不漏报（但绝对不能动系统核心进程）。\n\n")
            _T("必须遵守的规则：\n")
            _T("1. 绝对禁止列入的进程：smss.exe、csrss.exe、wininit.exe、System、Registry、Idle。其他进程即便在System32目录下，如果路径/行为异常也可以列入；\n")
            _T("2. 列入""可疑""只需满足1条明确判断依据（如：非系统目录、无签名、公司名可疑/缺失、进程名伪装系统名、路径异常等）；\n")
            _T("3. 列入""无用""：除了弹窗广告和捆绑工具栏外，常驻后台消耗资源但无实际功用的进程、异常自启动进程、可疑的第三方服务等均可列入；\n")
            _T("4. 常见软件（浏览器、杀软、输入法、驱动工具等）仅当存在异常行为（如路径异常、无签名、多个异常副本）时才列入；\n")
            _T("5. 最多返回20条结果，按可疑度从高到低排序；\n")
            _T("6. 即使略有怀疑也应列入，不要过于保守。\n\n")
            _T("输出要求：仅JSON数组，不包含markdown、解释文字、代码块标记。");
        userPrompt.Format(_T("分析以下Windows进程，找出可疑(恶意/病毒/木马)和无用(冗余广告软件)进程。\n")
            _T("判断要点：路径非系统目录、公司名缺失/非正规、进程名伪装成系统名但路径不对、原名与显示名不一致且相差很大。\n\n")
            _T("【绝对禁止项：smss.exe、csrss.exe、wininit.exe、System、Registry、Idle进程禁止列入。其他进程均可列入。】\n\n")
            _T("(已自动过滤smss/csrss/wininit等%d个最核心系统进程，当前列%d个进程)\n")
            _T("进程列表：\n%s\n")
            _T("返回JSON数组。格式：[{\"pid\":数字,\"name\":\"xxx.exe\",\"risk\":\"可疑\",\"reason\":\"简短原因（不超过30字）\"},...]")
            _T("risk取""可疑""或""无用""。最多20条。若无问题进程，返回空数组[]。只返回JSON，不要markdown。"),
            (int)(_countof(kSafeSystemProcs)), count, processList);
        break;

    default: // 1=标准 (balanced)
        sysPrompt = _T("你是一个极其保守的Windows系统安全分析专家。\n")
            _T("核心原则：系统关键进程保护 > 威胁检测（宁放过不杀错）。\n\n")
            _T("必须遵守以下铁律：\n")
            _T("1. 绝对白名单（csrss/smss/lsass/wininit/winlogon/services/svchost/dwm/explorer/shell experience/TextInputHost/ctfmon/conhost/taskhostw/audiodg/spoolsv/SearchHost/SecurityHealth等）以及任何位于C:\\Windows\\System32\\或SystemApps下且由Microsoft发布的进程，即便可疑也不得列入返回，宁可跳过也不冒险；\n")
            _T("2. 列入""可疑""必须同时满足2条以上判断依据（如：非系统目录 + 无签名 + 公司名可疑 + 进程名伪装等）；\n")
            _T("3. 列入""无用""必须是典型的弹窗广告、浏览器捆绑工具栏、常驻广告软件等；正常办公/娱乐软件、杀毒软件、输入法、驱动/电源管理、显卡控制面板等均不得列为无用；\n")
            _T("4. 若发现上述绝对白名单进程看似异常（如名字相似但路径不对），请在JSON结果中跳过该条目，不要返回。请交由人工判断；\n")
            _T("5. 仅返回最可疑的前10条结果，按可疑度从高到低排序；\n")
            _T("6. 若不确定，请直接返回空数组[]，不要为了凑数而列入。\n\n")
            _T("输出要求：仅JSON数组，不包含markdown、解释文字、代码块标记。");
        userPrompt = prompt;
        break;
    }

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