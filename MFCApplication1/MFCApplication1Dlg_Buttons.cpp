#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "VolumeManager.h"
#include "ProcessManager.h"
#include "GitCmdResultDlg.h"
#include "AIApiClient.h"
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
            if (sTitle.Find(_T("哔哩")) != -1 || sTitle.Find(_T("bilibili")) != -1 || sTitle.Find(_T("b站")) != -1)
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

void CMFCApplication1Dlg::OnBnClickedButton10()
{
    CString url = AfxGetApp()->GetProfileString(_T("Sites"), _T("Sducs"), _T(""));
    ::ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
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

void CMFCApplication1Dlg::OnBnClickedButton8()
{
    CString strPath = GetOrAskPath(this, _T("BiliPath"), _T("选择哔哩哔哩可执行文件"), false);

    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("指定的哔哩哔哩可执行文件未找到或未设置。请先设置路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
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

    BOOL enable = (selText.Find(_T("设定")) != -1);

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

void CMFCApplication1Dlg::OnBnClickedButton4()
{
    // 1. Check if WeChat is running by enumerating processes
    bool bIsWeChatRunning = false;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &pe32))
        {
            do
            {
                CString strExeFile = pe32.szExeFile;
                strExeFile.MakeLower();
                if (strExeFile == _T("wechat.exe") || strExeFile == _T("weixin.exe"))
                {
                    bIsWeChatRunning = true;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    if (bIsWeChatRunning)
    {
        // WeChat is running, simulate its default global hotkey Ctrl + Alt + W
        INPUT inputs[6] = { 0 };

        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_MENU;

        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'W';

        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = 'W';
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        inputs[4].type = INPUT_KEYBOARD;
        inputs[4].ki.wVk = VK_MENU;
        inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

        inputs[5].type = INPUT_KEYBOARD;
        inputs[5].ki.wVk = VK_CONTROL;
        inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }
    else
    {
        CString path = GetOrAskPath(this, _T("WeChatPath"), _T("选择微信可执行文件"), false);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的微信可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton5()
{
    // 1. Check if QQ is running by enumerating processes
    bool bIsQQRunning = false;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &pe32))
        {
            do
            {
                CString strExeFile = pe32.szExeFile;
                strExeFile.MakeLower(); // Convert to lowercase for matching
                if (strExeFile == _T("qq.exe"))
                {
                    bIsQQRunning = true;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    if (bIsQQRunning)
    {
        // QQ is running, simulate hotkey Ctrl + Alt + X
        INPUT inputs[6] = { 0 };

        // 1. Press Ctrl
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        // 2. Press Alt
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_MENU;

        // 3. Press X
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'X';

        // 4. Release X
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = 'X';
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        // 5. Release Alt
        inputs[4].type = INPUT_KEYBOARD;
        inputs[4].ki.wVk = VK_MENU;
        inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

        // 6. Release Ctrl
        inputs[5].type = INPUT_KEYBOARD;
        inputs[5].ki.wVk = VK_CONTROL;
        inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

        // Send input
        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }
    else
    {
        CString path = GetOrAskPath(this, _T("QQPath"), _T("选择QQ可执行文件"), false);
        if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
        else MessageBox(_T("指定的QQ可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton6()
{
    CString strPath = GetOrAskPath(this, _T("VSCodePath"), _T("选择VS Code可执行文件"), false);
    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    else
        MessageBox(_T("指定的VS Code可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
}

void CMFCApplication1Dlg::OnBnClickedButton7()
{
    CString strPath = GetOrAskPath(this, _T("VSPath"), _T("选择 Visual Studio 可执行文件"), false);
    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    else
        MessageBox(_T("指定的Visual Studio可执行文件未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
}

void CMFCApplication1Dlg::OnBnClickedButton9()
{
    CString path = GetOrAskPath(this, _T("StudyFolder"), _T("选择学习文件夹"), true);
    if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
    else MessageBox(_T("指定的学习文件夹未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
}

void CMFCApplication1Dlg::OnBnClickedButton11()
{
    CString url = AfxGetApp()->GetProfileString(_T("Sites"), _T("MoocUrl"), _T("https://www.icourse163.org/home.htm?userId=1595641987#/home/course"));
    ::ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
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
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT6);
    if (!pEdit) return;

    CString cmd;
    pEdit->GetWindowText(cmd);
    if (cmd.IsEmpty())
    {
        MessageBox(_T("请输入要运行的指令。"), _T("提示"), MB_OK | MB_ICONWARNING);
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
        msg.Format(_T("执行命令失败：%u"), err);
        MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton18()
{
    SetDlgItemText(IDC_EDIT6, _T(""));
}

void CMFCApplication1Dlg::OnBnClickedButton21()
{
    CString path = GetOrAskPath(this, _T("DownloadFolder"), _T("选择下载文件夹"), true);
    if (!path.IsEmpty()) ::ShellExecute(NULL, _T("open"), path, NULL, NULL, SW_SHOWNORMAL);
    else MessageBox(_T("指定的下载文件夹未找到，请检查路径是否正确。"), _T("提示"), MB_OK | MB_ICONWARNING);
}

void CMFCApplication1Dlg::OnBnClickedButton22()
{
    CString strPath = GetOrAskPath(this, _T("YuanbaoPath"), _T("选择元宝可执行文件"), false);

    if (!strPath.IsEmpty() && GetFileAttributes(strPath) != INVALID_FILE_ATTRIBUTES)
    {
        ::ShellExecute(NULL, _T("open"), strPath, NULL, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("指定的元宝可执行文件未找到或未设置。请先设置路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

// Handler for IDC_CHECK5: prevent automatic lock/screen-off while checked
void CMFCApplication1Dlg::OnBnClickedCheck5()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK5);
    if (!pCheck) return;

    if (pCheck->GetCheck() == BST_CHECKED)
    {
        EXECUTION_STATE es = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        if (es == 0)
        {
            MessageBox(_T("无法设置防止锁屏的系统状态。"), _T("错误"), MB_OK | MB_ICONERROR);
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
    CString exe = _T("powershell.exe");
    CString params = _T("-NoExit");

    int nResult = MessageBox(
        _T("是否以管理员身份运行 PowerShell？\n\n是 - 管理员权限\n否 - 普通权限"),
        _T("启动 PowerShell"),
        MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);

    if (nResult == IDCANCEL)
        return;

    if (nResult == IDNO)
    {
        CString err;
        if (!LaunchProcessAsShellUser(exe, params, &err))
        {
            CString msg;
            msg.Format(_T("以非管理员权限启动 PowerShell 失败：%s"), err.IsEmpty() ? FormatLastError(GetLastError()) : CString(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
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
            msg.Format(_T("以管理员权限启动 PowerShell 失败：%s"), FormatLastError(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton28()
{
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
                MessageBox(_T("无法启动 WSL，请手动运行 wsl.exe"), _T("错误"), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(_T("无法启动 WSL，请手动运行 wsl.exe"), _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnBnClickedButton29()
{
    // Open LeetCode CN problemset in default browser
    CString url = _T("https://leetcode.cn/problemset/");
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        MessageBox(_T("无法打开链接，请手动访问 https://leetcode.cn/problemset/"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton30()
{
    // Open GitHub
    CString url = _T("https://github.com/");
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        MessageBox(_T("无法打开链接，请手动访问 https://github.com/"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton31()
{
    // Launch Git Bash using configured path. If a valid path is displayed in
    // IDC_STATIC_PATH, use it as the process working directory.
    CString exe = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (exe.IsEmpty() || GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("找不到 git-bash.exe，请在配置中设置 Git Bash 路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // Read displayed path from static control. It may be a file path (dropped file)
    // or a directory. If it's a file, use its parent directory.
    CString displayed;
    CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
    if (pStatic) pStatic->GetWindowText(displayed);

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
    // Determine current tab: clear the appropriate path
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    int nCurTab = pTab ? pTab->GetCurSel() : -1;

    if (nCurTab == 5)
    {
        // Git tab: clear Git work directory
        m_strGitWorkDir.Empty();
        CWnd* pGitPath = GetDlgItem(IDC_STATIC_GIT_PATH);
        if (pGitPath) pGitPath->SetWindowText(_T("拖入文件夹或文件所在目录"));
        UpdateGitRepoInfo();
    }
    else
    {
        // File tab: clear dropped file path
        m_strDroppedFilePath.Empty();
        CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
        if (pStatic && IsValidWindow(pStatic->GetSafeHwnd()))
        {
            pStatic->SetWindowText(_T("拖拽文件到此"));
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
    CString workDir = GetGitWorkDir();
    auto* pDlg = new CGitCmdResultDlg(workDir, nullptr);
    if (!pDlg->Create(IDD_GIT_CMD_RESULT_DLG, nullptr))
    {
        delete pDlg;
        MessageBox(_T("无法创建 Git 命令窗口。"), _T("错误"), MB_OK | MB_ICONERROR);
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

// Detect whether the directory is a Git repo and get the current branch.
// Uses git.exe directly with CreateProcess + lpCurrentDirectory for reliable detection.
void CMFCApplication1Dlg::DetectGitRepoInfo(const CString& strWorkDir, bool& bIsRepo, CString& strBranch) const
{
    bIsRepo = false;
    strBranch.Empty();

    if (strWorkDir.IsEmpty()) return;

    // Find git.exe from git-bash.exe path
    CString gitBashPath = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (gitBashPath.IsEmpty()) return;

    int pos = gitBashPath.ReverseFind(_T('\\'));
    if (pos <= 0) return;
    CString gitDir = gitBashPath.Left(pos);

    CString gitExe;
    CString candidates[] = {
        gitDir + _T("\\bin\\git.exe"),
        gitDir + _T("\\mingw64\\bin\\git.exe"),
        gitDir + _T("\\usr\\bin\\git.exe"),
    };
    for (const auto& path : candidates)
    {
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        {
            gitExe = path;
            break;
        }
    }
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
            info = _T("Git仓库 ( detached HEAD)");
        else
            info.Format(_T("Git仓库 分支: %s"), strBranch.GetString());
    }
    else
    {
        info = _T("非Git仓库");
    }
    SetDlgItemText(IDC_STATIC_GIT_REPO_INFO, info);
}

void CMFCApplication1Dlg::ExecuteGitCommand(const CString& strDesc, const CString& strCmd)
{
    // Check working directory
    CString workDir = GetGitWorkDir();
    if (workDir.IsEmpty())
    {
        MessageBox(_T("请先设置工作目录。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // Detect repo info
    bool bIsRepo = false;
    CString strBranch;
    DetectGitRepoInfo(workDir, bIsRepo, strBranch);

    // Confirm before execution
    CString msg;
    msg.Format(_T("即将执行以下命令:\n\n%s\n\n工作目录: %s\n%s\n确认执行？"),
        strCmd.GetString(),
        workDir.GetString(),
        bIsRepo ? (CString(_T("当前分支: ")) + strBranch).GetString() : _T("(非Git仓库)"));
    if (MessageBox(msg, _T("确认执行"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Create modeless result dialog
    auto* pDlg = new CGitCmdResultDlg(workDir, nullptr);
    pDlg->AddCommand(strDesc, strCmd, true); // auto-execute
    if (!pDlg->Create(IDD_GIT_CMD_RESULT_DLG, nullptr))
    {
        delete pDlg;
        MessageBox(_T("无法创建结果窗口。"), _T("错误"), MB_OK | MB_ICONERROR);
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
        menu.AppendMenu(MF_STRING, 1, _T("执行命令"));
        menu.AppendMenu(MF_STRING, 2, _T("复制命令"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 3, _T("编辑命令"));
        menu.AppendMenu(MF_STRING, 4, _T("删除命令"));
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
            CGitCmdInputDialog editDlg(_T("编辑命令 (格式: 说明|命令)"), _T("编辑命令"),
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

// ========== AI command generation ==========
// Note: AI command generation is now handled by CGitCmdResultDlg::OnBnClickedAiAsk
// The old OnBnClickedGitAiGen function has been removed (AI input moved to result dialog)