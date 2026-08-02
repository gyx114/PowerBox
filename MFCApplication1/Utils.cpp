// Utils.cpp: Utility function implementations
#include "pch.h"
#include "framework.h"
#include "Utils.h"
#include "resource.h"
#include "LocalizationManager.h"
#include <userenv.h>
#pragma comment(lib, "Userenv.lib")

// Format Windows error code to readable string
[[nodiscard]] CString FormatLastError(DWORD err)
{
    LPVOID msgBuf = nullptr;
    DWORD sz = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&msgBuf), 0, nullptr);
    CString res;
    if (sz && msgBuf)
    {
        res = static_cast<LPCTSTR>(msgBuf);
        LocalFree(msgBuf);
    }
    else
    {
        res = std::format(L"Unknown error: {}", err).c_str();
    }
    return res;
}

// Copy text to clipboard
void CopyToClipboard(HWND hwnd, const CString& text)
{
    if (text.IsEmpty()) return;
    if (!OpenClipboard(hwnd)) return;
    ::EmptyClipboard();
    int len = (text.GetLength() + 1);
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
    if (hMem)
    {
        LPWSTR p = static_cast<LPWSTR>(::GlobalLock(hMem));
        if (p)
        {
            wcscpy_s(p, len, text);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        else
        {
            ::GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

// Check if current process is running with admin privileges
[[nodiscard]] bool IsProcessElevated()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;

    TOKEN_ELEVATION elev{};
    DWORD dwSize = 0;
    BOOL ok = GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &dwSize);
    CloseHandle(hToken);
    if (!ok) return false;
    return elev.TokenIsElevated != 0;
}

// Prompt user to restart with admin privileges, returns true if restart initiated
[[nodiscard]] bool PromptRestartElevated()
{
    auto& loc = CLocalizationManager::GetInstance();
    int r = MessageBox(nullptr, loc.GetString(_T("Msg"), _T("PromptRestartAdminMsg")), loc.GetString(_T("Msg"), _T("NeedPermission")), MB_YESNO | MB_ICONQUESTION);
    if (r == IDYES)
    {
        TCHAR szPath[MAX_PATH]{};
        GetModuleFileName(nullptr, szPath, MAX_PATH);
        SHELLEXECUTEINFO sei{};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = _T("runas");
        sei.lpFile = szPath;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteEx(&sei))
            return true;
    }
    return false;
}

// Get saved path from ini, prompt user to select if invalid, then save
[[nodiscard]] CString GetOrAskPath(CWnd* pParent, LPCTSTR pszKey, LPCTSTR pszTitle, bool bFolder)
{
    CString path = AfxGetApp()->GetProfileString(_T("Paths"), pszKey, _T(""));
    if (!path.IsEmpty() && GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        return path;

    if (bFolder)
    {
        CFolderPickerDialog dlg;
        if (dlg.DoModal() == IDOK)
        {
            path = dlg.GetPathName();
            AfxGetApp()->WriteProfileString(_T("Paths"), pszKey, path);
            return path;
        }
    }
    else
    {
        CFileDialog dlg(TRUE, _T("exe"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, _T("Executable Files (*.exe)|*.exe||"), pParent);
        dlg.m_ofn.lpstrTitle = pszTitle;
        if (dlg.DoModal() == IDOK)
        {
            path = dlg.GetPathName();
            AfxGetApp()->WriteProfileString(_T("Paths"), pszKey, path);
            return path;
        }
    }

    return _T("");
}

// Parse total seconds from three edit boxes (hours, minutes, seconds)
[[nodiscard]] int ParseShutdownSeconds(CDialogEx* pDlg)
{
    ASSERT(pDlg != nullptr);
    int h = 0, m = 0, s = 0;
    CString sh, sm, ss;
    CEdit* pEdit1 = static_cast<CEdit*>(pDlg->GetDlgItem(IDC_EDIT1));
    CEdit* pEdit2 = static_cast<CEdit*>(pDlg->GetDlgItem(IDC_EDIT2));
    CEdit* pEdit3 = static_cast<CEdit*>(pDlg->GetDlgItem(IDC_EDIT3));

    if (pEdit1) pEdit1->GetWindowText(sh);
    if (pEdit2) pEdit2->GetWindowText(sm);
    if (pEdit3) pEdit3->GetWindowText(ss);

    if (sh.IsEmpty()) sh = _T("0");
    if (sm.IsEmpty()) sm = _T("0");
    if (ss.IsEmpty()) ss = _T("0");

    h = _ttoi(sh);
    m = _ttoi(sm);
    s = _ttoi(ss);

    if (h < 0 || m < 0 || s < 0) return 180;
    if (m >= 60 || s >= 60) return 180;

    long total = static_cast<long>(h) * 3600 + m * 60 + s;
    if (total <= 0) return 180;
    if (total > 7LL * 24 * 3600) return 180;
    return static_cast<int>(total);
}

// Force-release stuck modifier keys by injecting a complete key-down + key-up cycle.
// Only releases keys that are actually stuck to avoid triggering side effects
// (e.g. pressing Alt would open the menu bar in the active window).
void ForceReleaseModifierKeys()
{
    static const int vkeys[] = { VK_MENU, VK_CONTROL, VK_SHIFT };

    for (int vk : vkeys)
    {
        if (::GetAsyncKeyState(vk) & 0x8000)
        {
            // Key is stuck: inject a complete key-down + key-up cycle
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = vk;          // key-down (dwFlags = 0)
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = vk;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            ::SendInput(2, inputs, sizeof(INPUT));
        }
    }
}

// Allow UIPI messages (for cross-privilege window communication)
void AllowUIPIMessage(HWND hwnd, UINT msg, BOOL allow)
{
    typedef BOOL(WINAPI* PFN_CHANGE_WINDOW_MESSAGE_FILTER)(UINT, DWORD);
    HMODULE hUser32 = GetModuleHandle(_T("user32.dll"));
    if (hUser32)
    {
        auto pfn = reinterpret_cast<PFN_CHANGE_WINDOW_MESSAGE_FILTER>(GetProcAddress(hUser32, "ChangeWindowMessageFilter"));
        if (pfn)
            pfn(msg, allow ? 1 : 0); // MSGFLT_ADD / MSGFLT_REMOVE
    }
}

// Launch process as current desktop user (non-admin)
bool LaunchProcessAsShellUser(LPCTSTR exe, LPCTSTR params, CString* pError)
{
    auto& loc = CLocalizationManager::GetInstance();
    if (pError) *pError = _T("");

    HWND hShell = GetShellWindow();
    if (!hShell)
    {
        if (pError) *pError = loc.GetString(_T("Msg"), _T("ShellWindowFailed"));
        return false;
    }

    DWORD dwPid = 0;
    GetWindowThreadProcessId(hShell, &dwPid);
    if (dwPid == 0)
    {
        if (pError) *pError = loc.GetString(_T("Msg"), _T("ShellPidFailed"));
        return false;
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPid);
    if (!hProc)
    {
        if (pError) *pError = FormatLastError(GetLastError());
        return false;
    }

    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY, &hToken))
    {
        if (pError) *pError = FormatLastError(GetLastError());
        CloseHandle(hProc);
        return false;
    }

    HANDLE hUserToken = nullptr;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, nullptr, SecurityIdentification, TokenPrimary, &hUserToken))
    {
        if (pError) *pError = FormatLastError(GetLastError());
        CloseHandle(hToken);
        CloseHandle(hProc);
        return false;
    }

    CString cmdLine;
    if (CString(params).IsEmpty())
        cmdLine.Format(_T("\"%s\""), exe);
    else
        cmdLine.Format(_T("\"%s\" %s"), exe, params);

    STARTUPINFO si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessWithTokenW(hUserToken, LOGON_WITH_PROFILE, nullptr,
        const_cast<LPWSTR>(static_cast<LPCTSTR>(cmdLine)),
        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);

    if (ok)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        if (pError) *pError = FormatLastError(GetLastError());
    }

    CloseHandle(hUserToken);
    CloseHandle(hToken);
    CloseHandle(hProc);
    return ok != FALSE;
}