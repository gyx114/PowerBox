// GitCmdResultDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "GitCmdResultDlg.h"
#include "afxdialogex.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CGitCmdResultDlg, CDialogEx)

CGitCmdResultDlg::CGitCmdResultDlg(const CString& strWorkDir, CWnd* pParent)
    : CDialogEx(IDD_GIT_CMD_RESULT_DLG, pParent)
    , m_strWorkDir(strWorkDir)
    , m_workDirLeft(0), m_workDirTop(0), m_workDirWidth(0)
    , m_statusLeft(0), m_statusTop(0), m_statusWidth(0)
    , m_aiInputLeft(0), m_aiInputTop(0), m_aiInputWidth(0)
    , m_aiBtnLeft(0), m_aiBtnTop(0), m_aiBtnWidth(0), m_aiBtnHeight(0)
    , m_addBtnLeft(0), m_clearBtnLeft(0), m_btnCmdTop(0)
    , m_listLeft(0), m_listTop(0), m_listHeight(0)
    , m_outputLabelTop(0)
    , m_outputLeft(0), m_outputTop(0), m_outputHeight(0)
    , m_copyBtnLeft(0), m_closeBtnLeft(0), m_bottomBtnTop(0), m_bottomBtnWidth(0), m_bottomBtnHeight(0)
{
}

CGitCmdResultDlg::~CGitCmdResultDlg()
{
    if (m_execThread.joinable())
        m_execThread.join();
}

void CGitCmdResultDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CGitCmdResultDlg, CDialogEx)
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_BTN_GIT_OUTPUT_COPY, &CGitCmdResultDlg::OnBnClickedCopyOutput)
    ON_BN_CLICKED(IDC_BTN_GIT_OUTPUT_CLOSE, &CGitCmdResultDlg::OnBnClickedClose)
    ON_BN_CLICKED(IDC_BTN_GIT_AI_ASK, &CGitCmdResultDlg::OnBnClickedAiAsk)
    ON_BN_CLICKED(IDC_BTN_GIT_ADD_CMD, &CGitCmdResultDlg::OnBnClickedAddCmd)
    ON_BN_CLICKED(IDC_BTN_GIT_CLEAR_CMDS, &CGitCmdResultDlg::OnBnClickedClearCmds)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_GIT_CMDS, &CGitCmdResultDlg::OnNMRclickCmdList)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_GIT_CMDS, &CGitCmdResultDlg::OnNMDblclkCmdList)
    ON_MESSAGE(CGitCmdResultDlg::WM_GIT_CMD_OUTPUT, &CGitCmdResultDlg::OnGitCmdOutput)
    ON_MESSAGE(CGitCmdResultDlg::WM_GIT_CMD_DONE, &CGitCmdResultDlg::OnGitCmdDone)
    ON_MESSAGE(WM_AI_RESPONSE, &CGitCmdResultDlg::OnAiGitResponse)
END_MESSAGE_MAP()

BOOL CGitCmdResultDlg::PreTranslateMessage(MSG* pMsg)
{
    // Prevent Enter/Esc from closing the modeless dialog
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
        return TRUE;
    // Enter key in AI ask edit box triggers ask button
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = GetFocus();
        if (pFocus && pFocus->GetDlgCtrlID() == IDC_EDIT_GIT_AI_ASK)
        {
            OnBnClickedAiAsk();
            return TRUE;
        }
        return TRUE; // block Enter for other controls too
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

BOOL CGitCmdResultDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Display working directory
    SetDlgItemText(IDC_STATIC_GIT_WORKDIR, m_strWorkDir.IsEmpty() ? CString(_T("(默认)")) : m_strWorkDir);
    SetDlgItemText(IDC_STATIC_GIT_STATUS, _T("状态: 准备就绪"));

    // Read layout anchors from RC-defined positions FIRST
    auto ReadRect = [&](int id) -> CRect {
        CRect rc(0, 0, 0, 0);
        CWnd* pWnd = GetDlgItem(id);
        if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
        return rc;
    };

    CRect rcWorkDir = ReadRect(IDC_STATIC_GIT_WORKDIR);
    m_workDirLeft = rcWorkDir.left; m_workDirTop = rcWorkDir.top; m_workDirWidth = rcWorkDir.Width();

    CRect rcStatus = ReadRect(IDC_STATIC_GIT_STATUS);
    m_statusLeft = rcStatus.left; m_statusTop = rcStatus.top; m_statusWidth = rcStatus.Width();

    CRect rcAiInput = ReadRect(IDC_EDIT_GIT_AI_ASK);
    m_aiInputLeft = rcAiInput.left; m_aiInputTop = rcAiInput.top; m_aiInputWidth = rcAiInput.Width();

    CRect rcAiBtn = ReadRect(IDC_BTN_GIT_AI_ASK);
    m_aiBtnLeft = rcAiBtn.left; m_aiBtnTop = rcAiBtn.top; m_aiBtnWidth = rcAiBtn.Width(); m_aiBtnHeight = rcAiBtn.Height();

    CRect rcAddBtn = ReadRect(IDC_BTN_GIT_ADD_CMD);
    m_addBtnLeft = rcAddBtn.left; m_btnCmdTop = rcAddBtn.top;
    m_clearBtnLeft = ReadRect(IDC_BTN_GIT_CLEAR_CMDS).left;

    CRect rcList = ReadRect(IDC_LIST_GIT_CMDS);
    m_listLeft = rcList.left; m_listTop = rcList.top; m_listHeight = rcList.Height();

    m_outputLabelTop = ReadRect(IDC_STATIC_GIT_OUTPUT_LABEL).top;

    CRect rcOutput = ReadRect(IDC_EDIT_GIT_OUTPUT);
    m_outputLeft = rcOutput.left; m_outputTop = rcOutput.top; m_outputHeight = rcOutput.Height();

    CRect rcCopy = ReadRect(IDC_BTN_GIT_OUTPUT_COPY);
    m_copyBtnLeft = rcCopy.left; m_bottomBtnTop = rcCopy.top; m_bottomBtnWidth = rcCopy.Width(); m_bottomBtnHeight = rcCopy.Height();
    m_closeBtnLeft = ReadRect(IDC_BTN_GIT_OUTPUT_CLOSE).left;

    // Initialize command list: use GetWindowRect for reliable width
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        // Get list width from the control's actual window rect (reliable in OnInitDialog)
        CRect rcList;
        pList->GetWindowRect(&rcList);
        int listWidth = rcList.Width() - 4; // subtract WS_BORDER (2px each side)
        if (listWidth < 50) listWidth = 350; // safety fallback
        pList->InsertColumn(0, _T("说明"), LVCFMT_LEFT, listWidth * 40 / 100);
        pList->InsertColumn(1, _T("命令"), LVCFMT_LEFT, listWidth * 60 / 100);
    }

    // Add any pending commands (added before dialog was created)
    for (const auto& entry : m_pendingCmds)
    {
        CListCtrl* pL = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
        if (pL)
        {
            int idx = pL->InsertItem(pL->GetItemCount(), entry.desc);
            pL->SetItemText(idx, 1, entry.cmd);
            if (entry.autoExec && !entry.cmd.IsEmpty())
                StartExecution(entry.cmd);
        }
    }
    m_pendingCmds.clear();

    m_bInitialized = true;

    return TRUE;
}

void CGitCmdResultDlg::PostNcDestroy()
{
    delete this;
}

int CGitCmdResultDlg::AddCommand(const CString& strDesc, const CString& strCmd, bool bAutoExecute)
{
    if (!IsWindow(m_hWnd))
    {
        // Dialog not yet created, queue for later
        m_pendingCmds.push_back({ strDesc, strCmd, bAutoExecute });
        return -1;
    }
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
    if (!pList) return -1;
    int idx = pList->InsertItem(pList->GetItemCount(), strDesc);
    pList->SetItemText(idx, 1, strCmd);
    if (bAutoExecute && !strCmd.IsEmpty())
        StartExecution(strCmd);
    return idx;
}

// ========== Layout ==========

void CGitCmdResultDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    // No dynamic layout: positions are defined in RC file
}

void CGitCmdResultDlg::ResizeControls()
{
    if (!IsWindow(m_hWnd)) return;
    CRect rc;
    GetClientRect(&rc);
    const int margin = 7;
    const int rightEdge = rc.Width() - margin;

    // Top labels: width adjusts
    CWnd* pWorkDir = GetDlgItem(IDC_STATIC_GIT_WORKDIR);
    if (pWorkDir)
        pWorkDir->SetWindowPos(nullptr, m_workDirLeft, m_workDirTop,
            rightEdge - m_workDirLeft, 12, SWP_NOZORDER);

    CWnd* pStatus = GetDlgItem(IDC_STATIC_GIT_STATUS);
    if (pStatus)
        pStatus->SetWindowPos(nullptr, m_statusLeft, m_statusTop,
            rightEdge - m_statusLeft, 10, SWP_NOZORDER);

    // AI input: width adjusts, ask button stays right
    CWnd* pAiInput = GetDlgItem(IDC_EDIT_GIT_AI_ASK);
    if (pAiInput)
        pAiInput->SetWindowPos(nullptr, m_aiInputLeft, m_aiInputTop,
            rightEdge - m_aiBtnWidth - margin - m_aiInputLeft, m_aiBtnHeight, SWP_NOZORDER);
    CWnd* pAiBtn = GetDlgItem(IDC_BTN_GIT_AI_ASK);
    if (pAiBtn)
        pAiBtn->SetWindowPos(nullptr, rightEdge - m_aiBtnWidth, m_aiBtnTop,
            m_aiBtnWidth, m_aiBtnHeight, SWP_NOZORDER);

    // Add/Clear buttons stay right
    CWnd* pAdd = GetDlgItem(IDC_BTN_GIT_ADD_CMD);
    if (pAdd)
        pAdd->SetWindowPos(nullptr, rightEdge - m_clearBtnLeft + m_addBtnLeft - m_clearBtnLeft,
            m_btnCmdTop, 30, 12, SWP_NOZORDER);
    CWnd* pClear = GetDlgItem(IDC_BTN_GIT_CLEAR_CMDS);
    if (pClear)
        pClear->SetWindowPos(nullptr, rightEdge - 30, m_btnCmdTop, 30, 12, SWP_NOZORDER);

    // Command list: only width adjusts
    CWnd* pList = GetDlgItem(IDC_LIST_GIT_CMDS);
    if (pList)
        pList->SetWindowPos(nullptr, m_listLeft, m_listTop,
            rightEdge - m_listLeft, m_listHeight, SWP_NOZORDER);
    // Adjust column widths to fill the list (40% desc, 60% cmd)
    CListCtrl* pListCtrl = (CListCtrl*)pList;
    if (pListCtrl)
    {
        CHeaderCtrl* pHeader = pListCtrl->GetHeaderCtrl();
        if (pHeader && pHeader->GetItemCount() >= 2)
        {
            CRect rcList;
            pListCtrl->GetClientRect(&rcList);
            int listWidth = rcList.Width();
            if (listWidth > 50)
            {
                pListCtrl->SetColumnWidth(0, listWidth * 40 / 100);
                pListCtrl->SetColumnWidth(1, listWidth * 60 / 100);
            }
        }
    }

    // Output edit: only width adjusts
    CWnd* pOutput = GetDlgItem(IDC_EDIT_GIT_OUTPUT);
    if (pOutput)
        pOutput->SetWindowPos(nullptr, m_outputLeft, m_outputTop,
            rightEdge - m_outputLeft, m_outputHeight, SWP_NOZORDER);

    // Bottom buttons: fixed size, right-aligned
    CWnd* pCopy = GetDlgItem(IDC_BTN_GIT_OUTPUT_COPY);
    if (pCopy)
        pCopy->SetWindowPos(nullptr, m_closeBtnLeft - m_copyBtnLeft - 30 + rightEdge - 60,
            m_bottomBtnTop, m_bottomBtnWidth, m_bottomBtnHeight, SWP_NOZORDER);
    CWnd* pClose = GetDlgItem(IDC_BTN_GIT_OUTPUT_CLOSE);
    if (pClose)
        pClose->SetWindowPos(nullptr, rightEdge - m_bottomBtnWidth, m_bottomBtnTop,
            m_bottomBtnWidth, m_bottomBtnHeight, SWP_NOZORDER);
}

// ========== Dynamic git context helper ==========

// Run a command and capture stdout (synchronous, short timeout)
static CString RunAndCaptureGit(const CString& exePath, const CString& args,
    const CString& workDir, DWORD timeoutMs = 3000)
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

// Find git.exe from the configured GitBashPath
static CString FindGitExe()
{
    CString gitBashPath = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (gitBashPath.IsEmpty()) return _T("");
    int pos = gitBashPath.ReverseFind(_T('\\'));
    if (pos <= 0) return _T("");
    CString gitDir = gitBashPath.Left(pos);
    CString exe = gitDir + _T("\\bin\\git.exe");
    if (GetFileAttributes(exe) == INVALID_FILE_ATTRIBUTES)
        return _T("");
    return exe;
}

// ========== Command execution helpers ==========

CString CGitCmdResultDlg::FindBashExe()
{
    CString gitBashPath = AfxGetApp()->GetProfileString(_T("Paths"), _T("GitBashPath"), _T(""));
    if (gitBashPath.IsEmpty()) return _T("");

    int pos = gitBashPath.ReverseFind(_T('\\'));
    if (pos <= 0) return _T("");
    CString gitDir = gitBashPath.Left(pos);

    CString candidates[] = {
        gitDir + _T("\\bin\\bash.exe"),
        gitDir + _T("\\usr\\bin\\bash.exe"),
        gitDir + _T("\\git-bash.exe")
    };
    for (const auto& path : candidates)
    {
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
            return path;
    }
    return _T("");
}

void CGitCmdResultDlg::StartExecution(const CString& strCommand)
{
    if (m_bRunning)
    {
        // Queue the command to execute after the current one finishes
        m_cmdQueue.push_back(strCommand);
        return;
    }

    m_bRunning = true;
    m_bCancelPending = false;

    CString status;
    status.Format(_T("状态: 执行中 - %s"), strCommand.Left(80));
    SetDlgItemText(IDC_STATIC_GIT_STATUS, status);

    // Append a header separator for this command (unless output is empty)
    if (!m_strOutput.IsEmpty())
        AppendOutput(_T("\r\n---\r\n"));
    AppendOutput(_T("$ ") + strCommand + _T("\r\n"));

    // Launch execution thread (captures this and command by value)
    CString cmd = strCommand;
    m_execThread = std::thread([this, cmd]() {
        ExecuteThread(cmd);
    });
}

void CGitCmdResultDlg::ExecuteNextInQueue()
{
    m_bRunning = false;

    if (m_execThread.joinable())
        m_execThread.join();

    if (!m_cmdQueue.empty())
    {
        CString next = m_cmdQueue.front();
        m_cmdQueue.erase(m_cmdQueue.begin());
        StartExecution(next);
    }
    else
    {
        CString status;
        status = _T("状态: 所有命令执行完毕");
        SetDlgItemText(IDC_STATIC_GIT_STATUS, status);
    }
}

void CGitCmdResultDlg::ExecuteThread(CString strCommand)
{
    HWND hWnd = m_hWnd;

    CString bashExe = FindBashExe();
    if (bashExe.IsEmpty())
    {
        CString* pMsg = new CString(_T("错误: 未找到 bash.exe。\n请在配置中设置 Git Bash 路径。"));
        ::PostMessage(hWnd, WM_GIT_CMD_OUTPUT, 0, (LPARAM)pMsg);
        ::PostMessage(hWnd, WM_GIT_CMD_DONE, 0, 0);
        return;
    }

    CString strWorkDir = m_strWorkDir;

    // Build command line: bash.exe -c "command 2>&1"
    CString cmdLine;
    if (bashExe.Right(13).CompareNoCase(_T("\\git-bash.exe")) == 0)
    {
        cmdLine.Format(_T("\"%s\" -c \"%s\""), bashExe, strCommand);
    }
    else
    {
        CString escapedCmd = strCommand;
        escapedCmd.Replace(_T("\""), _T("\\\""));
        cmdLine.Format(_T("\"%s\" --login -c \"%s 2>&1\""), bashExe, escapedCmd);
    }

    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        CString* pMsg = new CString(_T("错误: 无法创建管道。"));
        ::PostMessage(hWnd, WM_GIT_CMD_OUTPUT, 0, (LPARAM)pMsg);
        ::PostMessage(hWnd, WM_GIT_CMD_DONE, 0, 0);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi = {0};

    CString cmdLineCopy = cmdLine;
    LPTSTR pCmdLine = cmdLineCopy.GetBuffer(cmdLineCopy.GetLength() + 1);

    BOOL bSuccess = CreateProcess(
        nullptr, pCmdLine, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        strWorkDir.IsEmpty() ? nullptr : strWorkDir.GetString(),
        &si, &pi);
    cmdLineCopy.ReleaseBuffer();

    if (!bSuccess)
    {
        DWORD err = GetLastError();
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        CString* pMsg = new CString;
        pMsg->Format(_T("错误: 无法启动进程 (错误代码: %lu)\n命令行: %s"), err, cmdLine);
        ::PostMessage(hWnd, WM_GIT_CMD_OUTPUT, 0, (LPARAM)pMsg);
        ::PostMessage(hWnd, WM_GIT_CMD_DONE, 0, 0);
        return;
    }

    CloseHandle(hWritePipe);

    // Read output from pipe, convert \n to \r\n for Windows edit control
    char readBuf[4096];
    DWORD bytesRead;
    CString accumulated;

    while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        readBuf[bytesRead] = '\0';
        // Convert UTF-8 to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, readBuf, (int)bytesRead, nullptr, 0);
        if (wlen > 0)
        {
            std::wstring wstr(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, readBuf, (int)bytesRead, &wstr[0], wlen);
            CString chunk(wstr.c_str());
            // Convert lone \n to \r\n (but not existing \r\n)
            CString fixed;
            fixed.Preallocate(chunk.GetLength() * 2);
            for (int i = 0; i < chunk.GetLength(); i++)
            {
                if (chunk[i] == _T('\n') && (i == 0 || chunk[i - 1] != _T('\r')))
                    fixed += _T("\r\n");
                else
                    fixed += chunk[i];
            }
            accumulated += fixed;
            // Post incremental output
            CString* pChunk = new CString(fixed);
            ::PostMessage(hWnd, WM_GIT_CMD_OUTPUT, 0, (LPARAM)pChunk);
        }
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    ::PostMessage(hWnd, WM_GIT_CMD_DONE, 1, (LPARAM)exitCode);
}

// ========== Message handlers ==========

void CGitCmdResultDlg::AppendOutput(const CString& text)
{
    m_strOutput += text;
    SetDlgItemText(IDC_EDIT_GIT_OUTPUT, m_strOutput);
    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_GIT_OUTPUT);
    if (pEdit)
    {
        pEdit->SetSel(pEdit->GetWindowTextLength(), pEdit->GetWindowTextLength());
        pEdit->SendMessage(EM_SCROLLCARET, 0, 0);
    }
}

LRESULT CGitCmdResultDlg::OnGitCmdOutput(WPARAM wParam, LPARAM lParam)
{
    CString* pText = (CString*)lParam;
    if (pText)
    {
        AppendOutput(*pText);
        delete pText;
    }
    return 0;
}

LRESULT CGitCmdResultDlg::OnGitCmdDone(WPARAM wParam, LPARAM lParam)
{
    DWORD exitCode = (DWORD)lParam;

    CString status;
    if (wParam == 0)
    {
        status = _T("状态: 执行失败");
    }
    else if (exitCode == 0)
    {
        status = _T("状态: 执行成功 (退出码: 0)");
    }
    else
    {
        status.Format(_T("状态: 执行完成 (退出码: %lu)"), exitCode);
    }
    SetDlgItemText(IDC_STATIC_GIT_STATUS, status);

    if (m_strOutput.IsEmpty())
    {
        AppendOutput(_T("(无输出)"));
    }

    // Check and execute next queued command
    ExecuteNextInQueue();

    return 0;
}

LRESULT CGitCmdResultDlg::OnAiGitResponse(WPARAM wParam, LPARAM lParam)
{
    CString* pResult = (CString*)lParam;
    if (!pResult) return 0;

    CString response = *pResult;
    delete pResult;

    bool bSuccess = (wParam == 1);
    CWnd* pAskBtn = GetDlgItem(IDC_BTN_GIT_AI_ASK);
    if (pAskBtn) pAskBtn->EnableWindow(TRUE);

    if (!bSuccess || response.IsEmpty())
    {
        SetDlgItemText(IDC_STATIC_GIT_STATUS, _T("状态: AI生成失败"));
        AppendOutput(_T("AI生成命令失败，请检查API Key和网络连接。\r\n"));
        return 0;
    }

    // Parse response: each line is a separate command
    // Remove markdown code fences if present
    CString text = response;
    text.Trim();
    if (text.Find(_T("```")) == 0)
    {
        int end = text.Find(_T("```"), 3);
        if (end > 0)
            text = text.Mid(3, end - 3);
        else
            text = text.Mid(3);
        text.Trim();
        // Skip language tag
        int nl = text.Find(_T('\n'));
        if (nl >= 0 && text.Left(nl).Find(_T("git")) == -1)
            text = text.Mid(nl + 1);
        text.Trim();
    }

    // Split by lines and add each non-empty line as a command
    // Expected format: "description|git command"
    int added = 0;
    int pos = 0;
    CString line;
    while ((line = text.Tokenize(_T("\r\n"), pos)) != _T(""))
    {
        line.Trim();
        if (line.IsEmpty()) continue;
        // Skip lines that are purely markdown or comments
        if (line[0] == _T('#') || line.Find(_T("```")) == 0) continue;

        CString desc, cmd;
        // Try to parse as "description|command"
        int sep = line.Find(_T('|'));
        if (sep > 0)
        {
            desc = line.Left(sep);
            cmd = line.Mid(sep + 1);
            desc.Trim();
            cmd.Trim();

            // Validate: if command doesn't look like a git command,
            // the AI might have swapped or the format is wrong.
            // Try to detect: does the command start with "git"?
            if (cmd.Find(_T("git")) != 0 && cmd.Find(_T("git ")) != 0)
            {
                // Maybe the AI put description in both sides or
                // used wrong format. Try to extract git command from the
                // whole line, or use the text as-is with a generic desc.
                // Strategy: if the "command" part contains a space and the
                // description part looks like it could be part of a command,
                // try merging. Otherwise, fall back.
                CString combined = desc + _T(" ") + cmd;
                if (combined.Find(_T("git")) >= 0)
                {
                    // Extract from combined
                    int gitPos = combined.Find(_T("git"));
                    desc = combined.Left(gitPos);
                    cmd = combined.Mid(gitPos);
                    desc.Trim();
                    cmd.Trim();
                }
                else
                {
                    // Neither part looks like a git command.
                    // Keep as-is but mark the status for user awareness.
                    // We'll still add it so the user can see and correct.
                }
            }
        }
        else
        {
            // No separator found. Try to handle edge cases:
            // 1. Line IS a git command (starts with "git")
            // 2. Line is a bare description (no command)
            if (line.Find(_T("git")) == 0 || line.Find(_T("git ")) == 0)
            {
                cmd = line;
                // Generate short description from git subcommand
                int sp = line.Find(_T(' '));
                if (sp > 0)
                    desc = line.Mid(4, sp - 4); // extract subcommand name
                else
                    desc = _T("git");
            }
            else
            {
                // Bare description without command
                desc = line;
                cmd = _T(""); // mark as empty, won't be added
            }
        }

        if (!cmd.IsEmpty())
        {
            AddCommand(desc, cmd, false);
            added++;
        }
    }

    CString status;
    status.Format(_T("状态: AI生成了 %d 条命令，右键执行"), added);
    SetDlgItemText(IDC_STATIC_GIT_STATUS, status);
    return 0;
}

void CGitCmdResultDlg::OnBnClickedAiAsk()
{
    CString userInput;
    GetDlgItemText(IDC_EDIT_GIT_AI_ASK, userInput);
    userInput.Trim();
    if (userInput.IsEmpty())
    {
        MessageBox(_T("请输入问题描述。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Read AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        MessageBox(_T("请先在配置中设置 API Key。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Build system prompt: each line is "description|git command"
    // The command part MUST be a real runnable git command starting with "git"
    CString sysPrompt = _T("You are a Git command generator. ");
    sysPrompt += _T("Based on the user's description, generate the appropriate Git commands.\n\n");
    sysPrompt += _T("CRITICAL RULES:\n");
    sysPrompt += _T("1. Each output line MUST use EXACTLY this format: Chinese description|git command\n");
    sysPrompt += _T("2. The part after | MUST be a real git command that starts with 'git'\n");
    sysPrompt += _T("3. Do NOT write Chinese text in the command position\n");
    sysPrompt += _T("4. One command per line, no markdown, no explanation\n\n");
    sysPrompt += _T("CORRECT examples:\n");
    sysPrompt += _T("查看当前状态|git status\n");
    sysPrompt += _T("添加所有修改到暂存区|git add .\n");
    sysPrompt += _T("提交修改|git commit -m \"update\"\n");
    sysPrompt += _T("推送到远程仓库|git push origin main\n");
    sysPrompt += _T("克隆远程仓库|git clone https://github.com/user/repo.git\n\n");
    sysPrompt += _T("WRONG examples (DO NOT do this):\n");
    sysPrompt += _T("查看状态|查看当前分支  <- WRONG: command is not a git command\n");
    sysPrompt += _T("提交代码到github  <- WRONG: missing | separator and command\n\n");
    sysPrompt += _T("Now generate git commands for the user's request. ");
    sysPrompt += _T("Remember: the | separator is required, and text after | must start with 'git'.\n\n");

    // Add dynamic git context if a working directory is set
    if (!m_strWorkDir.IsEmpty())
    {
        sysPrompt += _T("\nWorking directory: ") + m_strWorkDir + _T("\n");

        // Try to get real-time git repo state
        CString gitExe = FindGitExe();
        if (!gitExe.IsEmpty())
        {
            CString branch = RunAndCaptureGit(gitExe, _T("branch --show-current"), m_strWorkDir);
            if (!branch.IsEmpty())
                sysPrompt += _T("Current branch: ") + branch + _T("\n");

            CString status = RunAndCaptureGit(gitExe, _T("status --short"), m_strWorkDir);
            if (!status.IsEmpty())
            {
                sysPrompt += _T("Working tree status:\n") + status + _T("\n");
            }

            CString log = RunAndCaptureGit(gitExe, _T("log --oneline -5"), m_strWorkDir);
            if (!log.IsEmpty())
            {
                sysPrompt += _T("Recent commits:\n") + log + _T("\n");
            }

            CString remote = RunAndCaptureGit(gitExe, _T("remote -v"), m_strWorkDir);
            if (!remote.IsEmpty())
            {
                sysPrompt += _T("Remote URLs:\n") + remote + _T("\n");
            }

            CString config = RunAndCaptureGit(gitExe, _T("config --list"), m_strWorkDir);
            if (!config.IsEmpty())
            {
                sysPrompt += _T("Git config:\n") + config.Left(2000) + _T("\n");
            }
        }
    }

    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), sysPrompt });
    messages.push_back({ _T("user"), userInput });

    // Send AI request; response (WM_AI_RESPONSE) is posted directly to this dialog
    CAIApiClient::SendAsync(messages, vendor, apiKey, model, m_hWnd);

    CWnd* pBtn = GetDlgItem(IDC_BTN_GIT_AI_ASK);
    if (pBtn) pBtn->EnableWindow(FALSE);
    SetDlgItemText(IDC_STATIC_GIT_STATUS, _T("状态: AI生成中..."));
}

void CGitCmdResultDlg::OnBnClickedAddCmd()
{
    // Use a simple input dialog for manual command entry
    // Format: "说明|命令"
    // Reuse the input dialog via main window
    // For simplicity, use a local CInputDialog
    class CGitInputDlg : public CDialogEx
    {
    public:
        CGitInputDlg() : CDialogEx(IDD_INPUT_DLG) {}
        CString GetInput() const { return m_input; }
    protected:
        virtual BOOL OnInitDialog() override
        {
            CDialogEx::OnInitDialog();
            SetWindowText(_T("添加命令"));
            SetDlgItemText(IDC_INPUT_PROMPT, _T("格式: 说明|命令"));
            GetDlgItem(IDC_INPUT_EDIT)->SetFocus();
            return FALSE;
        }
        virtual void DoDataExchange(CDataExchange* pDX) override
        {
            CDialogEx::DoDataExchange(pDX);
            DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
        }
        void OnOK() override { UpdateData(TRUE); CDialogEx::OnOK(); }
        CString m_input;
        enum { IDD = IDD_INPUT_DLG };
    };
    CGitInputDlg dlg;
    if (dlg.DoModal() == IDOK)
    {
        CString val = dlg.GetInput();
        int sep = val.Find(_T('|'));
        if (sep != -1)
        {
            AddCommand(val.Left(sep), val.Mid(sep + 1), false);
        }
    }
}

void CGitCmdResultDlg::OnBnClickedClearCmds()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
    if (pList) pList->DeleteAllItems();
}

void CGitCmdResultDlg::OnNMDblclkCmdList(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Double-click to copy command (prevent accidental execution)
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    *pResult = 0;
    if (pItem->iItem < 0) return;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
    if (!pList) return;
    CString cmd = pList->GetItemText(pItem->iItem, 1);
    if (!cmd.IsEmpty())
    {
        if (OpenClipboard())
        {
            EmptyClipboard();
            int nLen = (cmd.GetLength() + 1) * sizeof(TCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
            if (hMem)
            {
                memcpy(GlobalLock(hMem), cmd.GetString(), nLen);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
    }
}

void CGitCmdResultDlg::OnNMRclickCmdList(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_GIT_CMDS);
    if (!pList) return;

    int nItem = pItem->iItem;

    // Standard Windows behavior for right-click selection:
    // - If the clicked item is already selected, keep the existing selection.
    // - If not, clear all and select only the clicked item.
    if (nItem >= 0)
    {
        UINT state = pList->GetItemState(nItem, LVIS_SELECTED);
        if (!(state & LVIS_SELECTED))
        {
            for (int i = pList->GetItemCount() - 1; i >= 0; --i)
                pList->SetItemState(i, 0, LVIS_SELECTED);
            pList->SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
        }
    }

    // Count selected items
    int selCount = 0;
    if (nItem >= 0)
    {
        for (int i = pList->GetItemCount() - 1; i >= 0; --i)
        {
            if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
                selCount++;
        }
    }

    CMenu menu;
    menu.CreatePopupMenu();

    if (nItem >= 0)
    {
        menu.AppendMenu(MF_STRING, 1, selCount > 1 ? _T("执行选中命令") : _T("执行命令"));
        // Copy only makes sense for single item
        if (selCount <= 1)
            menu.AppendMenu(MF_STRING, 2, _T("复制命令"));
        menu.AppendMenu(MF_SEPARATOR);
        // Edit only makes sense for single item
        if (selCount <= 1)
            menu.AppendMenu(MF_STRING, 3, _T("编辑命令"));
        menu.AppendMenu(MF_STRING, 4, selCount > 1 ? _T("删除选中命令") : _T("删除命令"));
    }

    CPoint pt;
    GetCursorPos(&pt);
    int nCmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
    menu.DestroyMenu();

    if (nItem < 0) return;

    // Gather selected items
    std::vector<int> selItems;
    for (int i = 0; i < pList->GetItemCount(); i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
            selItems.push_back(i);
    }
    bool multiSel = (selItems.size() > 1);

    switch (nCmd)
    {
    case 1: // Execute (single or multiple)
    {
        if (m_strWorkDir.IsEmpty())
        {
            MessageBox(_T("请先设置 Git 工作目录。"), _T("提示"), MB_OK | MB_ICONWARNING);
            break;
        }

        if (multiSel)
        {
            CString msg;
            msg.Format(_T("即将依次执行 %u 条命令，工作目录: %s\n\n确认执行？"),
                (UINT)selItems.size(),
                m_strWorkDir.GetString());
            if (MessageBox(msg, _T("确认执行"), MB_YESNO | MB_ICONQUESTION) != IDYES)
                break;
            for (int idx : selItems)
            {
                CString c = pList->GetItemText(idx, 1);
                if (!c.IsEmpty())
                    StartExecution(c);
            }
        }
        else
        {
            CString c = pList->GetItemText(nItem, 1);
            if (!c.IsEmpty())
            {
                CString msg;
                msg.Format(_T("即将执行:\n\n%s\n\n工作目录: %s\n\n确认执行？"),
                    c.GetString(),
                    m_strWorkDir.GetString());
                if (MessageBox(msg, _T("确认执行"), MB_YESNO | MB_ICONQUESTION) == IDYES)
                    StartExecution(c);
            }
        }
        break;
    }
    case 2: // Copy single
    {
        CString c = pList->GetItemText(nItem, 1);
        if (!c.IsEmpty() && OpenClipboard())
        {
            EmptyClipboard();
            int nLen = (c.GetLength() + 1) * sizeof(TCHAR);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
            if (hMem)
            {
                memcpy(GlobalLock(hMem), c.GetString(), nLen);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
        break;
    }
    case 3: // Edit single
    {
        CString desc = pList->GetItemText(nItem, 0);
        CString cmd = pList->GetItemText(nItem, 1);
        class CGitEditDlg : public CDialogEx
        {
        public:
            CGitEditDlg(const CString& init) : CDialogEx(IDD_INPUT_DLG), m_init(init) {}
            CString GetInput() const { return m_input; }
        protected:
            virtual BOOL OnInitDialog() override
            {
                CDialogEx::OnInitDialog();
                SetWindowText(_T("编辑命令"));
                SetDlgItemText(IDC_INPUT_PROMPT, _T("格式: 说明|命令"));
                SetDlgItemText(IDC_INPUT_EDIT, m_init);
                GetDlgItem(IDC_INPUT_EDIT)->SetFocus();
                return FALSE;
            }
            virtual void DoDataExchange(CDataExchange* pDX) override
            {
                CDialogEx::DoDataExchange(pDX);
                DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
            }
            void OnOK() override { UpdateData(TRUE); CDialogEx::OnOK(); }
            CString m_init, m_input;
            enum { IDD = IDD_INPUT_DLG };
        };
        CGitEditDlg dlg(desc + _T("|") + cmd);
        if (dlg.DoModal() == IDOK)
        {
            CString val = dlg.GetInput();
            int sep = val.Find(_T('|'));
            if (sep != -1)
            {
                pList->SetItemText(nItem, 0, val.Left(sep));
                pList->SetItemText(nItem, 1, val.Mid(sep + 1));
            }
        }
        break;
    }
    case 4: // Delete (single or multiple)
    {
        // Delete in reverse order to preserve indices
        std::sort(selItems.begin(), selItems.end(), std::greater<int>());
        for (int idx : selItems)
            pList->DeleteItem(idx);
        break;
    }
    }
}

void CGitCmdResultDlg::OnBnClickedCopyOutput()
{
    if (m_strOutput.IsEmpty()) return;
    if (OpenClipboard())
    {
        EmptyClipboard();
        int nLen = (m_strOutput.GetLength() + 1) * sizeof(TCHAR);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nLen);
        if (hMem)
        {
            memcpy(GlobalLock(hMem), m_strOutput.GetString(), nLen);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

void CGitCmdResultDlg::OnBnClickedClose()
{
    DestroyWindow();
}
