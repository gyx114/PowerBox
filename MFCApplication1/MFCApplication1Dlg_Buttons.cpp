#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "VolumeManager.h"
#include "ProcessManager.h"
#include "GitCmdResultDlg.h"
#include "AIApiClient.h"
#include "LocalizationManager.h"
#include "QuickLaunchDlg.h"
#include <TlHelp32.h>
#include <Shellapi.h>
#include <Psapi.h>

// Simple input dialog for editing Git commands (local to this translation unit)
class CGitCmdInputDialog : public CDialogEx
{
public:
    CGitCmdInputDialog(LPCTSTR prompt, LPCTSTR title, LPCTSTR defaultVal)
        : CDialogEx(IDD_INPUT_DLG), m_prompt(prompt), m_title(title), m_default(defaultVal) {}
    CString GetInput() const { return m_input; }
protected:
    virtual BOOL OnInitDialog() override
    {
        CDialogEx::OnInitDialog();
        SetWindowText(m_title);
        SetDlgItemText(IDC_INPUT_PROMPT, m_prompt);
        SetDlgItemText(IDC_INPUT_EDIT, m_default);
        GetDlgItem(IDC_INPUT_EDIT)->SetFocus();
        return FALSE;
    }
    virtual void DoDataExchange(CDataExchange* pDX) override
    {
        CDialogEx::DoDataExchange(pDX);
        DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
    }
    void OnOK() override { UpdateData(TRUE); CDialogEx::OnOK(); }
    DECLARE_MESSAGE_MAP()
    CString m_prompt, m_title, m_default, m_input;
    enum { IDD = IDD_INPUT_DLG };
};
BEGIN_MESSAGE_MAP(CGitCmdInputDialog, CDialogEx)
END_MESSAGE_MAP()

// Trigger next track in Bilibili player if found; otherwise send global media next key as fallback
void CMFCApplication1Dlg::OnBiliNext()
{
    // Helper: find candidate window(s) that likely belong to Bilibili. Collect all candidates
    // and prefer the one with the largest HWND value when multiple exist.
    auto FindBiliWindow = [this]() -> HWND {
        std::vector<HWND> candidates;
        for (HWND h = ::GetTopWindow(NULL); h != NULL; h = ::GetNextWindow(h, GW_HWNDNEXT))
        {
            if (!::IsWindowVisible(h)) continue;

            TCHAR title[512] = {0};
            ::GetWindowText(h, title, _countof(title));
            CString sTitle = title;
            sTitle.MakeLower();
            bool matched = false;
            if (sTitle.Find(_T("哔哩")) != -1 || sTitle.Find(_T("bilibili")) != -1 || sTitle.Find(_T("b站")) != -1 || sTitle.Find(_T("Bili")) != -1 || sTitle.Find(_T("B站")) != -1)
                matched = true;

            // check process image path for name containing bilibili
            if (!matched)
            {
                DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
                if (pid != 0)
                {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hProc)
                    {
                        TCHAR buf[MAX_PATH] = {0};
                        DWORD sz = _countof(buf);
                        if (QueryFullProcessImageName(hProc, 0, buf, &sz))
                        {
                            CString p = buf; p.MakeLower();
                            if (p.Find(_T("bilibili")) != -1 || p.Find(_T("bili")) != -1)
                                matched = true;
                        }
                        CloseHandle(hProc);
                    }
                }
            }

            if (matched)
                candidates.push_back(h);
        }

        if (candidates.empty()) return NULL;

        // If only one candidate, return it. If multiple, pick the one with smallest HWND value.
        HWND best = candidates.front();
        if (candidates.size() > 1)
        {
            for (HWND c : candidates)
            {
                if ((UINT_PTR)c < (UINT_PTR)best) best = c;
            }
        }
        return best;
    };

    // Do NOT prefer user-selected window; instead select candidate by smallest HWND value
    HWND hBili = FindBiliWindow();
    if (hBili)
    {
        // Bring the player window to foreground (restore if minimized), send ']' key, then re-minimize.
        // Try to set foreground safely by attaching thread input.
        if (::IsIconic(hBili)) ::ShowWindow(hBili, SW_RESTORE);

        DWORD curTid = GetCurrentThreadId();
        DWORD targetPid = 0; DWORD targetTid = GetWindowThreadProcessId(hBili, &targetPid);
        // attach input threads to allow SetForegroundWindow
        AttachThreadInput(curTid, targetTid, TRUE);
        ::SetForegroundWindow(hBili);
        ::SetActiveWindow(hBili);
        AttachThreadInput(curTid, targetTid, FALSE);

        // small delay to ensure window receives focus
        Sleep(120);

        // Determine virtual-key and modifier from current layout for ']'
        SHORT vkAndState = VkKeyScanW((WCHAR)']');
        BYTE vk = LOBYTE(vkAndState);
        BYTE shiftState = HIBYTE(vkAndState);

        // Build inputs: press modifiers, keydown, keyup, release modifiers
        std::vector<INPUT> inputs;
        inputs.reserve(6);
        auto pushKey = [&](WORD vkCode, DWORD flags){ INPUT in = {}; in.type = INPUT_KEYBOARD; in.ki.wVk = vkCode; in.ki.dwFlags = flags; inputs.push_back(in); };

        // modifiers: SHIFT (1), CTRL (2), ALT (4) per VkKeyScan return
        if (shiftState & 1) pushKey(VK_SHIFT, 0);
        if (shiftState & 2) pushKey(VK_CONTROL, 0);
        if (shiftState & 4) pushKey(VK_MENU, 0);

        // key down + up
        if (vk != 0xFF)
        {
            pushKey(vk, 0);
            pushKey(vk, KEYEVENTF_KEYUP);
        }

        // release modifiers in reverse order
        if (shiftState & 4) pushKey(VK_MENU, KEYEVENTF_KEYUP);
        if (shiftState & 2) pushKey(VK_CONTROL, KEYEVENTF_KEYUP);
        if (shiftState & 1) pushKey(VK_SHIFT, KEYEVENTF_KEYUP);

        if (!inputs.empty()) SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));

        // give the app a moment to process and then minimize it again
        Sleep(80);
        ::ShowWindow(hBili, SW_MINIMIZE);
        return;
    }

    // Fallback: send a global media next key via SendInput
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MEDIA_NEXT_TRACK;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_MEDIA_NEXT_TRACK;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void CMFCApplication1Dlg::OnBnClickedButton1()
{
    // Execute: determine selection
    CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO1);
    int sel = 0;
    if (pCombo) sel = pCombo->GetCurSel();

    if (sel == 0)
    {
        // Restart in 1 minute
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/r /t 60"), NULL, SW_HIDE);
    }
    else if (sel == 1)
    {
        // Default 3-minute shutdown
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/s /t 180"), NULL, SW_HIDE);
    }
    else if (sel == 2)
    {
        // Set time shutdown
        int seconds = ParseShutdownSeconds(this);
        CString cmd;
        cmd.Format(_T("/s /t %d"), seconds);
        ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), cmd, NULL, SW_HIDE);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton2()
{
    // Cancel shutdown/restart
    ::ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/a"), NULL, SW_HIDE);
}

void CMFCApplication1Dlg::OnCbnSelchangeCombo1()
{
    // Use the selected text instead of index because the combo may be
    // created with CBS_SORT or items may reorder. Enable edits only when
    // the selected string indicates "Set time".
    CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO1);
    CString selText;
    if (pCombo)
    {
        int sel = pCombo->GetCurSel();
        if (sel != CB_ERR)
            pCombo->GetLBText(sel, selText);
    }

    CEdit* pE1 = (CEdit*)GetDlgItem(IDC_EDIT1);
    CEdit* pE2 = (CEdit*)GetDlgItem(IDC_EDIT2);
    CEdit* pE3 = (CEdit*)GetDlgItem(IDC_EDIT3);

    BOOL enable = (selText.Find(_T("设定")) != -1 || selText.Find(_T("Custom")) != -1 || selText.Find(_T("Set")) != -1);

    if (pE1)
    {
        pE1->SetReadOnly(!enable);
        pE1->EnableWindow(enable);
        if (enable) pE1->SetFocus();
    }
    if (pE2)
    {
        pE2->SetReadOnly(!enable);
        pE2->EnableWindow(enable);
    }
    if (pE3)
    {
        pE3->SetReadOnly(!enable);
        pE3->EnableWindow(enable);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton12()
{
    // Apply numeric value from IDC_EDIT5 if valid
    CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
    CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
    if (!pEditVol) return;
    CString s; pEditVol->GetWindowText(s);
    int v = _ttoi(s);
    if (s.IsEmpty() || v < 0 || v > 100)
    {
        // invalid -> restore display to current volume
        int cur = CVolumeManager::GetMasterVolumePercent();
        CString cs; cs.Format(_T("%d"), cur);
        pEditVol->SetWindowText(cs);
        if (pSlider) pSlider->SetPos(cur);
        return;
    }

    // set volume
    if (CVolumeManager::SetMasterVolumePercent(v))
    {
        if (pSlider) pSlider->SetPos(v);
    }
    else
    {
        // on failure, restore displayed
        int cur = CVolumeManager::GetMasterVolumePercent();
        CString cs; cs.Format(_T("%d"), cur);
        pEditVol->SetWindowText(cs);
        if (pSlider) pSlider->SetPos(cur);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton13()
{
    // set to 0
    if (CVolumeManager::SetMasterVolumePercent(0))
    {
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        if (pEditVol) pEditVol->SetWindowText(_T("0"));
        if (pSlider) pSlider->SetPos(0);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton14()
{
    // set to 10
    if (CVolumeManager::SetMasterVolumePercent(10))
    {
        CEdit* pEditVol = (CEdit*)GetDlgItem(IDC_EDIT5);
        CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER1);
        if (pEditVol) pEditVol->SetWindowText(_T("10"));
        if (pSlider) pSlider->SetPos(10);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton17()
{
    auto& loc = CLocalizationManager::GetInstance();
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT6);
    if (!pEdit) return;

    CString cmd;
    pEdit->GetWindowText(cmd);
    if (cmd.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("EnterCommand")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Use ShellExecute to run commands similar to Win+R. Support 'runas' for elevated commands if user prefixes 'runas:'
    // Trim whitespace
    cmd.Trim();

    // If the command looks like a path with arguments, split appropriately
    // ShellExecute can accept entire string as lpParameters if lpFile is executable. We'll try ShellExecuteEx via SHELLEXECUTEINFO
    // Try to execute directly using CreateProcess for proper command line handling when target is an executable.
    CString firstToken = cmd;
    int sp = firstToken.Find(' ');
    if (sp != -1) firstToken = firstToken.Left(sp);

    bool triedCreate = false;
    // Heuristic: if it contains a backslash or ends with .exe, try CreateProcess
    int dot = firstToken.ReverseFind('.');
    CString ext = (dot != -1) ? firstToken.Mid(dot) : CString(_T(""));
    if (firstToken.Find(_T('\\')) != -1 || (!ext.IsEmpty() && ext.CompareNoCase(_T(".exe")) == 0))
    {
        triedCreate = true;
        // CreateProcess requires a modifiable buffer
        CString cmdLine = cmd;
        TCHAR* lpCmd = _tcsdup(cmdLine.GetBuffer());
        STARTUPINFO si = {0}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        BOOL ok = CreateProcess(NULL, lpCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
        free(lpCmd);
        if (ok)
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return;
        }
    }

    // Next try ShellExecute (handles URLs, file associations, apps without explicit .exe)
    HINSTANCE h = ShellExecute(m_hWnd, NULL, cmd, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h > 32)
    {
        return; // success
    }

    // Final fallback: run through cmd.exe so builtins and complex commands work. Quote the entire command.
    SHELLEXECUTEINFO sei = {0};
    CString params;
    params.Format(_T("/C %s"), cmd);
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.hwnd = m_hWnd;
    sei.lpVerb = NULL;
    sei.lpFile = _T("cmd.exe");
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(loc.GetString(_T("Msg"), _T("ExecCmdFailed")), err);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton18()
{
    SetDlgItemText(IDC_EDIT6, _T(""));
}

// Handler for IDC_CHECK5: prevent automatic lock/screen-off while checked
void CMFCApplication1Dlg::OnBnClickedCheck5()
{
    auto& loc = CLocalizationManager::GetInstance();
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK5);
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        EXECUTION_STATE es = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        if (es == 0)
        {
            MessageBox(loc.GetString(_T("Msg"), _T("PreventLockFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            pCheck->SetCheck(BST_UNCHECKED);
            m_bPreventLockScreen = false;
        }
        else
        {
            m_bPreventLockScreen = true;
        }
    }
    else
    {
        SetThreadExecutionState(ES_CONTINUOUS);
        m_bPreventLockScreen = false;
    }
}

void CMFCApplication1Dlg::OnBnClickedCheck6()
{
}

void CMFCApplication1Dlg::OnBnClickedButton27()
{
    auto& loc = CLocalizationManager::GetInstance();
    CString exe = _T("powershell.exe");
    CString params = _T("-NoExit");

    int nResult = MessageBox(
        loc.GetString(_T("Msg"), _T("PowerShellConfirm")),
        loc.GetString(_T("Msg"), _T("PowerShellTitle")),
        MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);

    if (nResult == IDCANCEL)
        return;

    if (nResult == IDNO)
    {
        CString err;
        if (!LaunchProcessAsShellUser(exe, params, &err))
        {
            CString msg;
            msg.Format(loc.GetString(_T("Msg"), _T("PowerShellNonAdminFailed")), err.IsEmpty() ? FormatLastError(GetLastError()) : CString(err));
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        SHELLEXECUTEINFO sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = _T("runas");
        sei.lpFile = exe;
        sei.lpParameters = params;
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteEx(&sei))
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(loc.GetString(_T("Msg"), _T("PowerShellAdminFailed")), FormatLastError(err));
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton28()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Launch WSL (wsl.exe). Try ShellExecute first; fallback to system directory path.
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), _T("wsl.exe"), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        TCHAR sysPath[MAX_PATH] = {0};
        if (GetSystemDirectory(sysPath, MAX_PATH) > 0)
        {
            CString full;
            full.Format(_T("%s\\wsl.exe"), sysPath);
            HINSTANCE h2 = ::ShellExecute(m_hWnd, _T("open"), full, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h2 <= 32)
            {
                MessageBox(loc.GetString(_T("Msg"), _T("WslLaunchFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(loc.GetString(_T("Msg"), _T("WslLaunchFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton30()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Open GitHub
    CString url = _T("https://github.com/");
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("OpenLinkFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton31()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Launch Git Bash using configured path. If a valid path is displayed in
    // IDC_STATIC_PATH, use it as the process working directory.
    CString exe = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (exe.IsEmpty() || GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("GitBashNotFound")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    // Read the Git working directory from the Git tab's path control
    // If empty, fall back to the shared path control
    CString displayed;
    CWnd* pGitPath = GetDlgItem(IDC_STATIC_GIT_PATH);
    if (pGitPath) pGitPath->GetWindowText(displayed);
    if (displayed.IsEmpty())
    {
        CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
        if (pStatic) pStatic->GetWindowText(displayed);
    }

    CString workDir;
    if (!displayed.IsEmpty())
    {
        DWORD attr = GetFileAttributes(displayed);
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY)
                workDir = displayed;
            else
            {
                int pos = displayed.ReverseFind(_T('\\'));
                if (pos != -1)
                    workDir = displayed.Left(pos);
            }
        }
    }

    // If we have a valid working directory, try CreateProcess with lpCurrentDirectory
    if (!workDir.IsEmpty() && GetFileAttributes(workDir) != INVALID_FILE_ATTRIBUTES)
    {
        STARTUPINFO si = {0}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        // CreateProcess expects writable command line buffer if provided; we pass NULL.
        BOOL ok = CreateProcess(exe, NULL, NULL, NULL, FALSE, 0, NULL, workDir, &si, &pi);
        if (ok)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return;
        }
        // on failure, fall through to ShellExecute as a fallback
    }

    // Fallback: let ShellExecute open git-bash (uses default start directory)
    ::ShellExecute(NULL, _T("open"), exe, NULL, NULL, SW_SHOWNORMAL);
}

// Clear the displayed dropped file path when IDC_BUTTON32 is clicked
void CMFCApplication1Dlg::OnBnClickedButton32()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Determine current tab: clear the appropriate path
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    int nCurTab = pTab ? pTab->GetCurSel() : -1;

    if (nCurTab == 5)
    {
        // Git tab: clear Git work directory
        m_strGitWorkDir.Empty();
        CWnd* pGitPath = GetDlgItem(IDC_STATIC_GIT_PATH);
        if (pGitPath) pGitPath->SetWindowText(loc.GetString(_T("Msg"), _T("DropGitFolderHint")));
        UpdateGitRepoInfo();
    }
    else
    {
        // File tab: clear dropped file path
        m_strDroppedFilePath.Empty();
        CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
        if (pStatic && IsValidWindow(pStatic->GetSafeHwnd()))
        {
            pStatic->SetWindowText(loc.GetString(_T("Msg"), _T("DropFileHint")));
        }
    }
}

void CMFCApplication1Dlg::OnNMDblclkList5(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST5);
    if (pList && pItem->iItem >= 0)
    {
        CString val = pList->GetItemText(pItem->iItem, 1);
        if (!val.IsEmpty()) CopyToClipboard(m_hWnd, val);
    }
    *pResult = 0;
}

// ========== Git command execution helpers ==========

CString CMFCApplication1Dlg::GetGitWorkDir() const
{
    return m_strGitWorkDir;
}

// Locate button: browse for a folder to use as Git working directory
void CMFCApplication1Dlg::OnBnClickedGitLocate()
{
    CFolderPickerDialog dlg(nullptr, 0, this);
    if (dlg.DoModal() == IDOK)
    {
        m_strGitWorkDir = dlg.GetPathName();
        SetDlgItemText(IDC_STATIC_GIT_PATH, m_strGitWorkDir);
        UpdateGitRepoInfo();
    }
}

// Click on the Git path static control: also open folder picker
void CMFCApplication1Dlg::OnStnClickedGitPath()
{
    OnBnClickedGitLocate();
}

// Open the Git command window (modeless dialog)
void CMFCApplication1Dlg::OnBnClickedGitCmdWindow()
{
    auto& loc = CLocalizationManager::GetInstance();
    CString workDir = GetGitWorkDir();
    if (workDir.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("GitNoPathError")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }
    auto* pDlg = new CGitCmdResultDlg(workDir, nullptr);
    if (!pDlg->Create(IDD_GIT_CMD_RESULT_DLG, nullptr))
    {
        delete pDlg;
        MessageBox(loc.GetString(_T("Msg"), _T("GitCmdWindowFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }
    pDlg->ShowWindow(SW_SHOW);
    pDlg->SetForegroundWindow();
}

// Helper: run a command and capture its stdout (UTF-8). Returns empty string on failure.
static CString RunAndCapture(const CString& exePath, const CString& args,
    const CString& workDir, DWORD timeoutMs = 5000)
{
    SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return _T("");
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    PROCESS_INFORMATION pi = { 0 };

    CString cmdLine = _T("\"") + exePath + _T("\" ") + args;
    CString cmdLineCopy = cmdLine;
    LPTSTR pCmdLine = cmdLineCopy.GetBuffer(cmdLineCopy.GetLength() + 1);

    BOOL bOk = CreateProcess(nullptr, pCmdLine, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        workDir.IsEmpty() ? nullptr : workDir.GetString(),
        &si, &pi);
    cmdLineCopy.ReleaseBuffer();

    if (!bOk)
    {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return _T("");
    }
    CloseHandle(hWritePipe);

    char readBuf[4096] = { 0 };
    DWORD bytesRead;
    std::string output;
    while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0)
        output.append(readBuf, bytesRead);
    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, timeoutMs);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    int wlen = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.size(), nullptr, 0);
    if (wlen <= 0) return _T("");
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.size(), &wstr[0], wlen);
    CString result(wstr.c_str());
    result.Trim();
    return result;
}

// Helper: find git.exe from configured GitBashPath or common locations
static CString FindGitExe()
{
    // 1. Try configured GitBashPath
    CString gitBashPath = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (!gitBashPath.IsEmpty())
    {
        int pos = gitBashPath.ReverseFind(_T('\\'));
        if (pos > 0)
        {
            CString gitDir = gitBashPath.Left(pos);
            CString candidates[] = {
                gitDir + _T("\\bin\\git.exe"),
                gitDir + _T("\\mingw64\\bin\\git.exe"),
                gitDir + _T("\\usr\\bin\\git.exe"),
            };
            for (const auto& path : candidates)
            {
                if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
                    return path;
            }
        }
    }

    // 2. Try PATH environment variable
    TCHAR buf[4096] = {};
    if (GetEnvironmentVariable(_T("PATH"), buf, 4096) > 0)
    {
        CString pathEnv(buf);
        int start = 0;
        while (start < pathEnv.GetLength())
        {
            int end = pathEnv.Find(_T(';'), start);
            if (end == -1) end = pathEnv.GetLength();
            CString dir = pathEnv.Mid(start, end - start);
            dir.Trim();
            if (!dir.IsEmpty())
            {
                CString exe = dir + _T("\\git.exe");
                if (GetFileAttributes(exe) != INVALID_FILE_ATTRIBUTES)
                    return exe;
            }
            start = end + 1;
        }
    }

    // 3. Try common installation directories
    CString commonPaths[] = {
        _T("C:\\Program Files\\Git\\bin\\git.exe"),
        _T("C:\\Program Files (x86)\\Git\\bin\\git.exe"),
        _T("C:\\Program Files\\Git\\mingw64\\bin\\git.exe"),
        _T("C:\\Program Files\\Git\\usr\\bin\\git.exe"),
    };
    for (const auto& path : commonPaths)
    {
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
            return path;
    }

    return _T("");
}

// Detect whether the directory is a Git repo and get the current branch.
// Uses git.exe directly with CreateProcess + lpCurrentDirectory for reliable detection.
void CMFCApplication1Dlg::DetectGitRepoInfo(const CString& strWorkDir, bool& bIsRepo, CString& strBranch) const
{
    bIsRepo = false;
    strBranch.Empty();

    if (strWorkDir.IsEmpty()) return;

    CString gitExe = FindGitExe();
    if (gitExe.IsEmpty()) return;

    // Step 1: git rev-parse --is-inside-work-tree  (checks if inside a repo)
    CString output = RunAndCapture(gitExe, _T("rev-parse --is-inside-work-tree"), strWorkDir);
    if (output.CompareNoCase(_T("true")) != 0)
        return;

    bIsRepo = true;

    // Step 2: git branch --show-current
    CString branch = RunAndCapture(gitExe, _T("branch --show-current"), strWorkDir);
    if (!branch.IsEmpty())
        strBranch = branch;
}

// Update the repo info label on the Git tab
void CMFCApplication1Dlg::UpdateGitRepoInfo()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_strGitWorkDir.IsEmpty())
    {
        SetDlgItemText(IDC_STATIC_GIT_REPO_INFO, _T(""));
        return;
    }

    bool bIsRepo = false;
    CString strBranch;
    DetectGitRepoInfo(m_strGitWorkDir, bIsRepo, strBranch);

    CString info;
    if (bIsRepo)
    {
        if (strBranch.IsEmpty())
            info = loc.GetString(_T("Msg"), _T("GitRepoDetached"));
        else
            info.Format(loc.GetString(_T("Msg"), _T("GitRepoBranch")), strBranch.GetString());
    }
    else
    {
        info = loc.GetString(_T("Msg"), _T("GitNotRepo"));
    }
    SetDlgItemText(IDC_STATIC_GIT_REPO_INFO, info);
}

void CMFCApplication1Dlg::ExecuteGitCommand(const CString& strDesc, const CString& strCmd)
{
    auto& loc = CLocalizationManager::GetInstance();
    // Check working directory
    CString workDir = GetGitWorkDir();
    if (workDir.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("GitNoWorkDir")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    // Detect repo info
    bool bIsRepo = false;
    CString strBranch;
    DetectGitRepoInfo(workDir, bIsRepo, strBranch);

    // Confirm before execution
    CString msg;
    CString branchInfo = bIsRepo
        ? CString(loc.GetString(_T("Msg"), _T("GitCurrentBranch"))) + strBranch
        : CString(loc.GetString(_T("Msg"), _T("GitNotRepo")));
    msg.Format(loc.GetString(_T("Msg"), _T("GitConfirmExec")),
        strCmd.GetString(),
        workDir.GetString(),
        branchInfo.GetString());
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("GitConfirmTitle")), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Create modeless result dialog
    auto* pDlg = new CGitCmdResultDlg(workDir, nullptr);
    pDlg->AddCommand(strDesc, strCmd, true); // auto-execute
    if (!pDlg->Create(IDD_GIT_CMD_RESULT_DLG, nullptr))
    {
        delete pDlg;
        MessageBox(loc.GetString(_T("Msg"), _T("GitResultWindowFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }
    pDlg->ShowWindow(SW_SHOW);
    pDlg->SetForegroundWindow();
}

void CMFCApplication1Dlg::SaveGitCommandsToConfig()
{
    // Get config path
    TCHAR exePath[MAX_PATH] = {0};
    GetModuleFileName(NULL, exePath, MAX_PATH);
    CString exeDir = exePath;
    int p = exeDir.ReverseFind(_T('\\'));
    CString configPath = (p != -1) ? (exeDir.Left(p) + _T("\\config.ini")) : CString(_T("config.ini"));

    const TCHAR* section = _T("GitCommands");
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST4);
    if (!pList) return;

    int count = pList->GetItemCount();
    for (int i = 0; i < count && i < 99; ++i)
    {
        CString desc = pList->GetItemText(i, 0);
        CString cmd = pList->GetItemText(i, 1);
        CString val;
        val.Format(_T("%s|%s"), desc.GetString(), cmd.GetString());
        CString key;
        key.Format(_T("Cmd%d"), i + 1);
        WritePrivateProfileString(section, key, val, configPath);
    }

    // Clear remaining entries
    for (int i = count + 1; i <= 99; ++i)
    {
        CString key;
        key.Format(_T("Cmd%d"), i);
        WritePrivateProfileString(section, key, NULL, configPath);
    }
}

void CMFCApplication1Dlg::AddGitCommandToList(const CString& strDesc, const CString& strCmd)
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST4);
    if (!pList) return;
    int idx = pList->InsertItem(pList->GetItemCount(), strDesc);
    pList->SetItemText(idx, 1, strCmd);
    SaveGitCommandsToConfig();
}

// ========== Right-click menu for Git command list ==========

void CMFCApplication1Dlg::OnNMRclickList4(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    auto& loc = CLocalizationManager::GetInstance();
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST4);
    if (!pList) return;

    int nItem = pItem->iItem;

    // Select the right-clicked item
    if (nItem >= 0)
    {
        for (int i = pList->GetItemCount() - 1; i >= 0; --i)
            pList->SetItemState(i, 0, LVIS_SELECTED);
        pList->SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
    }

    CMenu menu;
    menu.CreatePopupMenu();

    if (nItem >= 0)
    {
        menu.AppendMenu(MF_STRING, 1, loc.GetString(_T("Msg"), _T("GitMenuExec")));
        menu.AppendMenu(MF_STRING, 2, loc.GetString(_T("Msg"), _T("GitMenuCopy")));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 3, loc.GetString(_T("Msg"), _T("GitMenuEdit")));
        menu.AppendMenu(MF_STRING, 4, loc.GetString(_T("Msg"), _T("GitMenuDelete")));
    }

    CPoint pt;
    GetCursorPos(&pt);
    int nCmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
    menu.DestroyMenu();

    if (nItem < 0) return;

    CString desc = pList->GetItemText(nItem, 0);
    CString cmd = pList->GetItemText(nItem, 1);

    switch (nCmd)
    {
    case 1: // Execute
        if (!cmd.IsEmpty())
            ExecuteGitCommand(desc, cmd);
        break;
    case 2: // Copy
        if (!cmd.IsEmpty())
            CopyToClipboard(m_hWnd, cmd);
        break;
    case 3: // Edit
        {
            // Use a simple input dialog for editing
            // Format: "说明|命令"
            CGitCmdInputDialog editDlg(loc.GetString(_T("Msg"), _T("GitEditPrompt")), loc.GetString(_T("Msg"), _T("GitMenuEdit")),
                desc + _T("|") + cmd);
            if (editDlg.DoModal() == IDOK)
            {
                CString val = editDlg.GetInput();
                int sep = val.Find(_T('|'));
                if (sep != -1)
                {
                    pList->SetItemText(nItem, 0, val.Left(sep));
                    pList->SetItemText(nItem, 1, val.Mid(sep + 1));
                    SaveGitCommandsToConfig();
                }
            }
        }
        break;
    case 4: // Delete
        pList->DeleteItem(nItem);
        SaveGitCommandsToConfig();
        break;
    }
}

// ========== Quick Launch: user-configurable buttons ==========

void CMFCApplication1Dlg::LoadQuickLaunchItems()
{
    m_qlItems.clear();
    TCHAR exePath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, exePath, MAX_PATH);
    CString configPath = exePath;
    int p = configPath.ReverseFind(_T('\\'));
    if (p != -1) configPath = configPath.Left(p) + _T("\\config.ini");
    else configPath = _T("config.ini");

    for (int i = 0; i < 99; ++i)
    {
        CString key;
        key.Format(_T("Item%d"), i);
        TCHAR buf[4096] = {};
        GetPrivateProfileString(_T("QuickLaunch"), key, _T(""), buf, 4096, configPath);
        if (buf[0] == _T('\0')) break;

        CString entry(buf);
        int sep1 = entry.Find(_T('|'));
        if (sep1 != -1)
        {
            QLItem item;
            item.name = entry.Left(sep1);
            CString rest = entry.Mid(sep1 + 1);
            int sep2 = rest.Find(_T('|'));
            if (sep2 != -1)
            {
                item.path = rest.Left(sep2);
                CString typeStr = rest.Mid(sep2 + 1);
                int sep3 = typeStr.Find(_T('|'));
                if (sep3 != -1)
                {
                    item.type = _ttoi(typeStr.Left(sep3));
                    CString hotkeyAndIcon = typeStr.Mid(sep3 + 1);
                    // Format: modifier|vk|customIconPath (old) or modifier,vk|customIconPath (new)
                    int sep4 = hotkeyAndIcon.Find(_T('|'));
                    if (sep4 != -1)
                    {
                        int sep5 = hotkeyAndIcon.Find(_T('|'), sep4 + 1);
                        if (sep5 != -1)
                        {
                            // Old format: modifier|vk|customIconPath (has two pipes)
                            item.hotkey = HotkeyInfo::FromConfigString(hotkeyAndIcon.Left(sep5));
                            item.customIconPath = hotkeyAndIcon.Mid(sep5 + 1);
                        }
                        else
                        {
                            // New format: modifier,vk|customIconPath (hotkey uses comma, pipe separates customIconPath)
                            item.hotkey = HotkeyInfo::FromConfigString(hotkeyAndIcon.Left(sep4));
                            item.customIconPath = hotkeyAndIcon.Mid(sep4 + 1);
                        }
                        // Clean up garbage: Windows file paths cannot contain '|',
                        // so if customIconPath has '|' it's remnant from old corrupted format
                        if (item.customIconPath.Find(_T('|')) != -1)
                            item.customIconPath.Empty();
                    }
                    else
                    {
                        item.hotkey = HotkeyInfo::FromConfigString(hotkeyAndIcon);
                    }
                }
                else
                {
                    item.type = _ttoi(typeStr);
                }
            }
            else
            {
                item.path = rest;
                item.type = QLItem::Executable;
            }
            m_qlItems.push_back(item);
        }
    }
}

void CMFCApplication1Dlg::SaveQuickLaunchItems()
{
    TCHAR exePath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, exePath, MAX_PATH);
    CString configPath = exePath;
    int p = configPath.ReverseFind(_T('\\'));
    if (p != -1) configPath = configPath.Left(p) + _T("\\config.ini");
    else configPath = _T("config.ini");

    // Clear existing entries
    for (int i = 0; i < 99; ++i)
    {
        CString key;
        key.Format(_T("Item%d"), i);
        WritePrivateProfileString(_T("QuickLaunch"), key, NULL, configPath);
    }

    // Write current items
    for (size_t i = 0; i < m_qlItems.size(); ++i)
    {
        CString key, val;
        key.Format(_T("Item%d"), (int)i);
        val.Format(_T("%s|%s|%d|%s|%s"), m_qlItems[i].name.GetString(), m_qlItems[i].path.GetString(), m_qlItems[i].type, m_qlItems[i].hotkey.ToConfigString().GetString(), m_qlItems[i].customIconPath.GetString());
        WritePrivateProfileString(_T("QuickLaunch"), key, val, configPath);
    }
}

void CMFCApplication1Dlg::RefreshQuickLaunchList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_QUICK_LAUNCH);
    if (!pList) return;
    pList->DeleteAllItems();

    // Calculate icon size based on list width: 5 items per row
    CRect rcList;
    pList->GetClientRect(&rcList);
    int listWidth = rcList.Width();
    if (listWidth <= 0) listWidth = 211; // fallback to RC-defined width

    int cellWidth = listWidth / 5;                    // grid cell width for 5 columns
    // Icon takes ~50% of cell width, leaving ~25% padding on each side for clean spacing
    int iconSize = cellWidth / 2;
    if (iconSize < 32) iconSize = 32;
    // Vertical spacing per row: icon height + label (~16px) + inter-row padding (~20px)
    int spacingY = iconSize + 36;

    // Rebuild image list with calculated icon size
    if (m_quickLaunchImages.GetSafeHandle()) m_quickLaunchImages.DeleteImageList();
    m_quickLaunchImages.Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, 1, 36);
    pList->SetImageList(&m_quickLaunchImages, LVSIL_NORMAL);

    // Set spacing for icon view: 5 columns, vertical includes room for label + top/bottom padding
    pList->SetIconSpacing(CSize(cellWidth, spacingY));

    for (size_t i = 0; i < m_qlItems.size(); ++i)
    {
        HICON hIcon = CQuickLaunchDlg::ExtractIconForItem(m_qlItems[i]);
        int iconIdx = -1;
        if (hIcon)
        {
            // Scale icon to calculated size for the image list
            HICON hScaled = (HICON)CopyImage(hIcon, IMAGE_ICON, iconSize, iconSize, LR_COPYFROMRESOURCE);
            if (hScaled)
            {
                iconIdx = m_quickLaunchImages.Add(hScaled);
                DestroyIcon(hScaled);
            }
            else
            {
                iconIdx = m_quickLaunchImages.Add(hIcon);
            }
            DestroyIcon(hIcon);
        }
        pList->InsertItem((int)i, m_qlItems[i].name, iconIdx);
    }

    // Move list control to top of Z-order so it draws in front of the tab control
    pList->SetWindowPos(&CWnd::wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Helper: check if a process with the given executable path is running
static bool IsProcessRunning(const CString& exePath)
{
    if (exePath.IsEmpty()) return false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe = { sizeof(pe) };
    bool found = false;
    CString targetLower = exePath;
    targetLower.MakeLower();

    if (Process32First(hSnapshot, &pe))
    {
        do
        {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc)
            {
                TCHAR buf[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageName(hProc, 0, buf, &sz))
                {
                    CString procPath(buf);
                    procPath.MakeLower();
                    if (procPath == targetLower)
                    {
                        found = true;
                        CloseHandle(hProc);
                        break;
                    }
                }
                CloseHandle(hProc);
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return found;
}

// Helper: send a hotkey combination via SendInput
static void SendHotkey(const HotkeyInfo& hotkey)
{
    if (hotkey.IsEmpty()) return;

    // Build inputs: press modifiers, keydown, keyup, release modifiers
    std::vector<INPUT> inputs;
    inputs.reserve(8);
    auto pushKey = [&](WORD vkCode, DWORD flags) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vkCode;
        in.ki.dwFlags = flags;
        inputs.push_back(in);
    };

    // Press modifiers
    if (hotkey.modifier & MOD_CONTROL) pushKey(VK_CONTROL, 0);
    if (hotkey.modifier & MOD_ALT)     pushKey(VK_MENU, 0);
    if (hotkey.modifier & MOD_SHIFT)   pushKey(VK_SHIFT, 0);
    if (hotkey.modifier & MOD_WIN)     pushKey(VK_LWIN, 0);

    // Key down + up
    pushKey((WORD)hotkey.vk, 0);
    pushKey((WORD)hotkey.vk, KEYEVENTF_KEYUP);

    // Release modifiers in reverse order
    if (hotkey.modifier & MOD_WIN)     pushKey(VK_LWIN, KEYEVENTF_KEYUP);
    if (hotkey.modifier & MOD_SHIFT)   pushKey(VK_SHIFT, KEYEVENTF_KEYUP);
    if (hotkey.modifier & MOD_ALT)     pushKey(VK_MENU, KEYEVENTF_KEYUP);
    if (hotkey.modifier & MOD_CONTROL) pushKey(VK_CONTROL, KEYEVENTF_KEYUP);

    if (!inputs.empty())
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

void CMFCApplication1Dlg::OnQuickLaunchItem(int index)
{
    if (index < 0 || index >= (int)m_qlItems.size())
        return;

    const QLItem& item = m_qlItems[index];

    switch (item.type)
    {
    case QLItem::HotkeyOnly:
        // Hotkey-only: simulate the hotkey
        if (!item.hotkey.IsEmpty())
            SendHotkey(item.hotkey);
        break;

    case QLItem::Executable:
        // Executable with hotkey: check if process is running
        if (!item.hotkey.IsEmpty() && IsProcessRunning(item.path))
        {
            // Process exists: send hotkey to activate it
            SendHotkey(item.hotkey);
            break;
        }
        // No hotkey or process not running: fall through to launch
        ::ShellExecute(m_hWnd, _T("open"), item.path, NULL, NULL, SW_SHOWNORMAL);
        break;

    case QLItem::Url:
        // URL: open in default browser
        ::ShellExecute(m_hWnd, _T("open"), item.path, NULL, NULL, SW_SHOWNORMAL);
        break;

    case QLItem::Folder:
        // Directory: open in Explorer
        ::ShellExecute(m_hWnd, _T("open"), item.path, NULL, NULL, SW_SHOWNORMAL);
        break;

    case QLItem::OtherFile:
    default:
        // File: execute with ShellExecute
        if (!item.path.IsEmpty())
            ::ShellExecute(m_hWnd, _T("open"), item.path, NULL, NULL, SW_SHOWNORMAL);
        break;
    }
}

void CMFCApplication1Dlg::OnQuickLaunchManage()
{
    auto& loc = CLocalizationManager::GetInstance();

    // If already open, just bring it to front
    if (m_pQuickLaunchDlg && ::IsWindow(m_pQuickLaunchDlg->m_hWnd))
    {
        m_pQuickLaunchDlg->SetForegroundWindow();
        return;
    }

    // Load current items before opening dialog
    LoadQuickLaunchItems();

    // Create modeless dialog
    m_pQuickLaunchDlg = new CQuickLaunchDlg(m_qlItems, this);
    if (!m_pQuickLaunchDlg->Create(IDD_QUICK_LAUNCH_DLG, this))
    {
        delete m_pQuickLaunchDlg;
        m_pQuickLaunchDlg = nullptr;
        MessageBox(loc.GetString(_T("Msg"), _T("CreateDlgFailed")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }
    m_pQuickLaunchDlg->ShowWindow(SW_SHOW);
}

// List control handlers: double-click to activate, right-click for context menu
void CMFCApplication1Dlg::OnNMDblclkQuickLaunchList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    if (pItem->iItem >= 0 && pItem->iItem < (int)m_qlItems.size())
        OnQuickLaunchItem(pItem->iItem);
    *pResult = 0;
}

void CMFCApplication1Dlg::OnNMRclickQuickLaunchList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    if (pItem->iItem < 0 || pItem->iItem >= (int)m_qlItems.size())
    {
        *pResult = 0;
        return;
    }

    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, loc.GetString(_T("QuickLaunch"), _T("BtnEdit")));
    menu.AppendMenu(MF_STRING, 2, loc.GetString(_T("QuickLaunch"), _T("BtnDelete")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 3, loc.GetString(_T("QuickLaunch"), _T("BtnChangeIcon")));

    // Convert list client coordinates to screen coordinates for the popup menu
    CPoint ptScreen = pItem->ptAction;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_QUICK_LAUNCH);
    if (pList) pList->ClientToScreen(&ptScreen);

    DWORD cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        ptScreen.x, ptScreen.y, this);

    if (cmd == 1)
    {
        // Edit: open item-specific edit dialog directly
        CQuickLaunchDlg::EditSingleItem(m_qlItems[pItem->iItem], false, this);
        SaveQuickLaunchItems();
        RefreshQuickLaunchList();
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
        if (pTab) UpdateQuickTab(pTab->GetCurSel());
    }
    else if (cmd == 2)
    {
        // Delete item
        m_qlItems.erase(m_qlItems.begin() + pItem->iItem);
        SaveQuickLaunchItems();
        RefreshQuickLaunchList();
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
        if (pTab) UpdateQuickTab(pTab->GetCurSel());
    }
    else if (cmd == 3)
    {
        // Change icon: open item-specific edit dialog directly
        CQuickLaunchDlg::EditSingleItem(m_qlItems[pItem->iItem], false, this);
        SaveQuickLaunchItems();
        RefreshQuickLaunchList();
        CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB_QUICK);
        if (pTab) UpdateQuickTab(pTab->GetCurSel());
    }

    *pResult = 0;
}