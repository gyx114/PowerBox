// AIAssistantDlg.cpp: Standalone AI Assistant window implementation
#include "pch.h"
#include "framework.h"
#include "AIAssistantDlg.h"
#include "LocalizationManager.h"
#include "MarkdownDlg.h"
#include "SettingsDlg.h"
#include "Utils.h"
#include "json.hpp"
#include <afxdialogex.h>
#include <algorithm>
#include <MsHTML.h>
#include <ExDisp.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// WebBrowser event sink: intercepts BeforeNavigate2 to handle AI executable
// commands via the custom "http://127.0.0.1:1/exec/" URL scheme.
// ============================================================================
class CWebBrowserEventSink : public IDispatch
{
public:
    CWebBrowserEventSink(HWND hTargetWnd) : m_hTargetWnd(hTargetWnd) {}

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

    STDMETHODIMP GetTypeInfoCount(UINT*) override { return E_NOTIMPL; }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }

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

    static void CancelNavigation(DISPPARAMS* pParams)
    {
        if (pParams->cArgs >= 1 &&
            pParams->rgvarg[0].vt == (VT_BOOL | VT_BYREF) &&
            pParams->rgvarg[0].pboolVal)
        {
            *pParams->rgvarg[0].pboolVal = VARIANT_TRUE;
        }
    }

    static const CString kExecPrefix()
    {
        return CString(_T("http://127.0.0.1:1/exec/"));
    }

    bool HandleAppExecUrl(const CString& url)
    {
        int prefixLen = kExecPrefix().GetLength();
        if (url.Left(prefixLen) != kExecPrefix())
            return false;

        // The browser only sends a short command id. The full command lives in
        // the C++ side registry, so long commands never enter the URL.
        CString id = url.Mid(prefixLen);
        if (id.IsEmpty())
            return false;

        ::PostMessage(m_hTargetWnd, WM_AI_EXECUTE_COMMAND, 0,
            reinterpret_cast<LPARAM>(_tcsdup(id)));
        return true;
    }

    STDMETHODIMP Invoke(DISPID dispid, REFIID, LCID, WORD, DISPPARAMS* pParams,
        VARIANT*, EXCEPINFO*, UINT*) override
    {
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

        if (dispid == 271 && pParams && pParams->cArgs >= 4)
        {
            CString url = GetUrlParam(pParams, 3);
            if (!url.IsEmpty() && url.Find(kExecPrefix()) == 0)
            {
                CancelNavigation(pParams);
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

// ============================================================================
// Command result struct and background thread
// ============================================================================
struct CommandResult
{
    CString command;
    CString resultMsg;
    DWORD exitCode = 0;
};

static UINT AFX_CDECL CommandExecThread(LPVOID pParam)
{
    struct ThreadParam { CString command; CString exeDir; HWND hTarget; }* p = (ThreadParam*)pParam;
    CString command = p->command;
    CString exeDir = p->exeDir;
    HWND hTarget = p->hTarget;
    delete p;

    DWORD exitCode = 0;
    CString outputStr;

    CString cmdTrimmed = command;
    cmdTrimmed.Trim();

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;

    if (CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        CString cmdLine;
        if (cmdTrimmed.Find(_T("powershell ")) == 0 || cmdTrimmed.Find(_T("PowerShell ")) == 0)
            cmdLine = _T("powershell.exe ") + cmdTrimmed.Mid(10).Trim();
        else
            cmdLine = _T("cmd.exe /c ") + cmdTrimmed;

        STARTUPINFO si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        PROCESS_INFORMATION pi = { 0 };

        CString cmdLineCopy = cmdLine;
        LPTSTR pCmdLine = cmdLineCopy.GetBuffer(cmdLineCopy.GetLength() + 1);

        if (CreateProcess(nullptr, pCmdLine, nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, exeDir.IsEmpty() ? nullptr : exeDir.GetString(), &si, &pi))
        {
            CloseHandle(hWritePipe);
            hWritePipe = nullptr;

            char readBuf[4096];
            DWORD bytesRead;
            std::string output;
            const DWORD timeoutMs = 30000;
            DWORD waitResult;

            do {
                waitResult = WaitForSingleObject(pi.hProcess, 100);
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

            while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                readBuf[bytesRead] = '\0';
                output.append(readBuf, bytesRead);
            }

            GetExitCodeProcess(pi.hProcess, &exitCode);
            if (waitResult == WAIT_TIMEOUT)
            {
                TerminateProcess(pi.hProcess, 1);
                output += "\r\n[Timeout 30s, terminated]";
                exitCode = 1;
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, output.c_str(), (int)output.size(), nullptr, 0);
            if (wlen > 0)
            {
                std::wstring wstr(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, output.c_str(), (int)output.size(), &wstr[0], wlen);
                outputStr = wstr.c_str();
            }
            else
                outputStr = CString(output.c_str());
            outputStr.Trim();
        }
        else
        {
            DWORD err = GetLastError();
            outputStr.Format(_T("CreateProcess failed (error %lu)"), err);
            exitCode = err;
        }
        cmdLineCopy.ReleaseBuffer();
    }
    else
    {
        outputStr = _T("CreatePipe failed");
        exitCode = GetLastError();
    }

    if (hWritePipe) CloseHandle(hWritePipe);
    if (hReadPipe) CloseHandle(hReadPipe);

    CString resultMsg;
    resultMsg.Format(_T("【命令执行结果】\n命令：%s\n状态：已执行\n\n"), command.GetString());
    if (!outputStr.IsEmpty())
        resultMsg += _T("输出：\n```\n") + outputStr + _T("\n```\n\n");
    CString exitStr;
    exitStr.Format(_T("退出码：%lu"), exitCode);
    resultMsg += exitStr;

    auto* pResult = new CommandResult;
    pResult->command = command;
    pResult->resultMsg = resultMsg;
    pResult->exitCode = exitCode;
    ::PostMessage(hTarget, WM_AI_COMMAND_RESULT, 0, (LPARAM)pResult);

    return 0;
}

// ============================================================================
// High-risk confirmation dialog
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
// CAIAssistantDlg implementation
// ============================================================================

IMPLEMENT_DYNAMIC(CAIAssistantDlg, CDialogEx)

CAIAssistantDlg::CAIAssistantDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_AI_ASSISTANT_DLG, pParent)
{
}

CAIAssistantDlg::~CAIAssistantDlg()
{
}

void CAIAssistantDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAIAssistantDlg, CDialogEx)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_BUTTON_AI_SEND, &CAIAssistantDlg::OnBnClickedAiSend)
    ON_BN_CLICKED(IDC_BUTTON_AI_STOP, &CAIAssistantDlg::OnBnClickedAiStop)
    ON_BN_CLICKED(IDC_BUTTON_AI_CLEAR, &CAIAssistantDlg::OnBnClickedAiClear)
    ON_BN_CLICKED(IDC_BUTTON_AI_HISTORY, &CAIAssistantDlg::OnBnClickedAiHistory)
    ON_BN_CLICKED(IDC_BTN_TERMINAL_CLEAR, &CAIAssistantDlg::OnBnClickedTerminalClear)
    ON_CBN_SELCHANGE(IDC_TERMINAL_SHELL, &CAIAssistantDlg::OnCbnSelchangeTerminalShell)
    ON_WM_TIMER()
    ON_MESSAGE(WM_AI_RESPONSE, &CAIAssistantDlg::OnAiResponse)
    ON_MESSAGE(WM_AI_STREAM_CHUNK, &CAIAssistantDlg::OnAiStreamChunk)
    ON_MESSAGE(WM_AI_STREAM_DONE, &CAIAssistantDlg::OnAiStreamDone)
    ON_MESSAGE(WM_AI_EXECUTE_COMMAND, &CAIAssistantDlg::OnAiExecuteCommand)
    ON_MESSAGE(WM_AI_COMMAND_RESULT, &CAIAssistantDlg::OnAiCommandResult)
    ON_MESSAGE(WM_TERM_OUTPUT, &CAIAssistantDlg::OnAiSessionOutput)
    ON_MESSAGE(WM_TERM_EXITED, &CAIAssistantDlg::OnAiSessionExited)
    ON_MESSAGE(CTerminalView::WM_AI_CAPTURE_DONE, &CAIAssistantDlg::OnAiCaptureDone)
    ON_MESSAGE(WM_CONV_LOADED, &CAIAssistantDlg::OnConvLoaded)
    ON_MESSAGE(WM_TERM_TAB_SELECT, &CAIAssistantDlg::OnTermTabSelect)
    ON_MESSAGE(WM_TERM_TAB_CLOSE, &CAIAssistantDlg::OnTermTabClose)
    ON_MESSAGE(WM_TERM_TAB_NEW, &CAIAssistantDlg::OnTermTabNew)
END_MESSAGE_MAP()

// ============================================================================
// Initialization
// ============================================================================

BOOL CAIAssistantDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // WS_CLIPCHILDREN prevents parent background erasure under child windows,
    // eliminating ghosting/trails when resizing controls during splitter drag.
    ModifyStyle(0, WS_CLIPCHILDREN);

    auto& loc = CLocalizationManager::GetInstance();

    // Translate UI
    SetWindowText(loc.GetString(_T("DlgCaption"), _T("AIAssistantDlg")));

    InitAIAssistant();
    InitTerminal();

    CaptureInitialLayout();
    m_bLayoutReady = true;

    return TRUE;
}

void CAIAssistantDlg::PostNcDestroy()
{
    delete this;
}

void CAIAssistantDlg::OnClose()
{
    SaveCurrentConversation();
    DestroyWindow();
}

void CAIAssistantDlg::OnDestroy()
{
    DisconnectAiBrowserEvents();
    m_aiActionCommands.Clear();
    CDialogEx::OnDestroy();
}

void CAIAssistantDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED || !m_bLayoutReady || !m_aiBrowser.m_hWnd)
        return;

    if (m_rcClientInit.IsRectEmpty())
        return;

    int margin = m_rcBrowserInit.left;
    int gap = 4;

    // ===== Terminal view: bottom anchored to window bottom =====
    int termViewBottom = cy - margin;
    int termViewH = std::clamp(m_terminalViewHeight, 110,
        std::max(110, cy - 300));
    int termViewTop = termViewBottom - termViewH;

    // ===== shiftY: middle block shifts as a unit =====
    // Only browser height and terminal view height change.
    // All controls between them shift by the same amount,
    // preserving their original relative positions.
    int shiftY = termViewTop - m_rcTermViewInit.top;

    // Clamp: ensure browser doesn't get too small
    int minBrowserH = 60;
    int minShiftY = minBrowserH - m_rcBrowserInit.Height();
    if (shiftY < minShiftY)
    {
        shiftY = minShiftY;
        termViewTop = m_rcTermViewInit.top + shiftY;
        termViewH = termViewBottom - termViewTop;
    }

    // Browser: top static, bottom shifts with middle block
    int browserBottom = m_rcBrowserInit.bottom + shiftY;

    // Vendor combo: stays at original Y, adjusts X for window width
    int vendorW = m_rcVendorInit.Width();
    int vendorH = m_rcVendorInit.Height();
    bool vendorAboveBrowser =
        (m_rcVendorInit.top + m_rcButtonsInit[0].Height() <= m_rcBrowserInit.top + 4);
    CRect rcVendor;
    CRect rcBrowser;
    if (vendorAboveBrowser)
    {
        rcVendor = CRect(margin, m_rcVendorInit.top,
            margin + vendorW, m_rcVendorInit.top + vendorH);
        rcBrowser = CRect(margin, m_rcBrowserInit.top, cx - margin, browserBottom);
    }
    else
    {
        rcVendor = CRect(cx - margin - vendorW, m_rcVendorInit.top,
            cx - margin, m_rcVendorInit.top + vendorH);
        rcBrowser = CRect(margin, m_rcBrowserInit.top, rcVendor.left - gap, browserBottom);
    }

    // ===== Batch all window moves with DeferWindowPos (flicker-free) =====
    HDWP hdwp = ::BeginDeferWindowPos(12);
    if (!hdwp) return;

    hdwp = ::DeferWindowPos(hdwp, m_aiBrowser.m_hWnd, nullptr,
        rcBrowser.left, rcBrowser.top, rcBrowser.Width(), rcBrowser.Height(),
        SWP_NOZORDER);
    m_aiBrowserRect = rcBrowser;

    CWnd* pVendor = GetDlgItem(IDC_COMBO_AI_VENDOR);
    if (pVendor)
        hdwp = ::DeferWindowPos(hdwp, pVendor->m_hWnd, nullptr,
            rcVendor.left, rcVendor.top, rcVendor.Width(), rcVendor.Height(),
            SWP_NOZORDER);

    // Input: shifts by shiftY, full width
    CWnd* pInput = GetDlgItem(IDC_EDIT_AI_INPUT);
    if (pInput)
        hdwp = ::DeferWindowPos(hdwp, pInput->m_hWnd, nullptr,
            margin, m_rcInputInit.top + shiftY,
            cx - margin * 2, m_rcInputInit.Height(), SWP_NOZORDER);

    // Buttons: shift by shiftY, preserve original X/width/height
    UINT btnIds[4] = {
        IDC_BUTTON_AI_SEND, IDC_BUTTON_AI_STOP,
        IDC_BUTTON_AI_CLEAR, IDC_BUTTON_AI_HISTORY
    };
    for (int i = 0; i < 4; i++)
    {
        CWnd* pBtn = GetDlgItem(btnIds[i]);
        if (pBtn)
            hdwp = ::DeferWindowPos(hdwp, pBtn->m_hWnd, nullptr,
                m_rcButtonsInit[i].left, m_rcButtonsInit[i].top + shiftY,
                m_rcButtonsInit[i].Width(), m_rcButtonsInit[i].Height(),
                SWP_NOZORDER);
    }

    // Splitter: shifts by shiftY, full width
    CWnd* pSplitter = GetDlgItem(IDC_TERMINAL_SPLITTER);
    if (pSplitter)
        hdwp = ::DeferWindowPos(hdwp, pSplitter->m_hWnd, nullptr,
            margin, m_rcSplitterInit.top + shiftY,
            cx - margin * 2, 6, SWP_NOZORDER);

    // Tabs: shift by shiftY, full width
    if (m_terminalTabs.m_hWnd)
        hdwp = ::DeferWindowPos(hdwp, m_terminalTabs.m_hWnd, nullptr,
            margin, m_rcTermTabsInit.top + shiftY,
            cx - margin * 2, m_rcTermTabsInit.Height(), SWP_NOZORDER);

    // Label: shifts by shiftY, original X/width/height
    CWnd* pLabel = GetDlgItem(IDC_TERMINAL_LABEL);
    if (pLabel)
        hdwp = ::DeferWindowPos(hdwp, pLabel->m_hWnd, nullptr,
            m_rcTermLabelInit.left, m_rcTermLabelInit.top + shiftY,
            m_rcTermLabelInit.Width(), m_rcTermLabelInit.Height(),
            SWP_NOZORDER);

    // Clear button: shifts by shiftY, right-aligned to window width
    int clearW = m_rcTermClearInit.Width();
    int clearLeft = cx - margin - clearW;
    CWnd* pClear = GetDlgItem(IDC_BTN_TERMINAL_CLEAR);
    if (pClear)
        hdwp = ::DeferWindowPos(hdwp, pClear->m_hWnd, nullptr,
            clearLeft, m_rcTermClearInit.top + shiftY,
            clearW, m_rcTermClearInit.Height(), SWP_NOZORDER);

    // Shell dropdown: shifts by shiftY, right-aligned (left of clear)
    int shellW = m_rcTermShellInit.Width();
    int shellLeft = clearLeft - gap - shellW;
    CWnd* pShell = GetDlgItem(IDC_TERMINAL_SHELL);
    if (pShell)
        hdwp = ::DeferWindowPos(hdwp, pShell->m_hWnd, nullptr,
            shellLeft, m_rcTermShellInit.top + shiftY,
            shellW, m_rcTermShellInit.Height(), SWP_NOZORDER);

    // Terminal views: fill remaining space
    CRect rcView(margin, termViewTop, cx - margin, termViewBottom);
    for (CTerminalView* v : m_terminalTabsList)
    {
        if (v && v->m_hWnd)
            hdwp = ::DeferWindowPos(hdwp, v->m_hWnd, nullptr,
                rcView.left, rcView.top, rcView.Width(), rcView.Height(),
                SWP_NOZORDER);
    }

    ::EndDeferWindowPos(hdwp);

    // Show/hide terminal views
    if (m_pActiveTerminal && m_pActiveTerminal->m_hWnd)
    {
        for (CTerminalView* v : m_terminalTabsList)
        {
            if (v && v->m_hWnd)
                v->ShowWindow(v == m_pActiveTerminal ? SW_SHOW : SW_HIDE);
        }
    }

    m_terminalTabs.Relayout();
}

void CAIAssistantDlg::CaptureInitialLayout()
{
    GetClientRect(&m_rcClientInit);

    auto capture = [&](UINT id, CRect& out) {
        CWnd* w = GetDlgItem(id);
        if (w)
        {
            w->GetWindowRect(&out);
            ScreenToClient(&out);
        }
    };

    capture(IDC_AI_BROWSER, m_rcBrowserInit);
    capture(IDC_EDIT_AI_INPUT, m_rcInputInit);
    capture(IDC_COMBO_AI_VENDOR, m_rcVendorInit);
    capture(IDC_TERMINAL_SPLITTER, m_rcSplitterInit);
    capture(IDC_TERMINAL_LABEL, m_rcTermLabelInit);
    capture(IDC_TERMINAL_TABS, m_rcTermTabsInit);
    capture(IDC_TERMINAL_SHELL, m_rcTermShellInit);
    capture(IDC_BTN_TERMINAL_CLEAR, m_rcTermClearInit);
    capture(IDC_TERMINAL_VIEW, m_rcTermViewInit);
    capture(IDC_BUTTON_AI_SEND, m_rcButtonsInit[0]);
    capture(IDC_BUTTON_AI_STOP, m_rcButtonsInit[1]);
    capture(IDC_BUTTON_AI_CLEAR, m_rcButtonsInit[2]);
    capture(IDC_BUTTON_AI_HISTORY, m_rcButtonsInit[3]);

    if (m_rcTermViewInit.Height() > 0)
        m_terminalViewHeight = m_rcTermViewInit.Height();
}

void CAIAssistantDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 640;
    lpMMI->ptMinTrackSize.y = 520;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

BOOL CAIAssistantDlg::PreTranslateMessage(MSG* pMsg)
{
    // Splitter drag between the chat area and the terminal panel.
    if (m_bLayoutReady)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        ScreenToClient(&pt);
        CRect rcSplit;
        CWnd* pSplit = GetDlgItem(IDC_TERMINAL_SPLITTER);
        if (pSplit)
        {
            pSplit->GetWindowRect(&rcSplit);
            ScreenToClient(&rcSplit);
        }
        CRect rcHit = rcSplit;
        rcHit.InflateRect(0, 5, 0, 5);

        if (pMsg->message == WM_SETCURSOR && rcHit.PtInRect(pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;
        }
        if (pMsg->message == WM_MOUSEMOVE && !m_bTerminalResizing &&
            rcHit.PtInRect(pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;
        }

        if (pMsg->message == WM_LBUTTONDOWN && rcHit.PtInRect(pt))
        {
            m_bTerminalResizing = true;
            m_terminalSplitter.SetDragging(true);
            SetCapture();
            return TRUE;
        }
        else if (pMsg->message == WM_MOUSEMOVE && m_bTerminalResizing)
        {
            CRect rcClient;
            GetClientRect(&rcClient);

            int margin = m_rcBrowserInit.left;
            int offset = m_rcTermViewInit.top - m_rcSplitterInit.top;
            int desired = (rcClient.bottom - margin) - (pt.y + offset);
            int minH = 110;
            int maxH = std::max(minH, rcClient.Height() - 300);
            m_terminalViewHeight = std::clamp(desired, minH, maxH);

            // DeferWindowPos already batches moves; WS_CLIPCHILDREN handles clipping.
            // No SetRedraw needed — it causes ghosting with child WebBrowser/terminal views.
            OnSize(SIZE_RESTORED, rcClient.Width(), rcClient.Height());
            return TRUE;
        }
        else if (pMsg->message == WM_LBUTTONUP && m_bTerminalResizing)
        {
            m_bTerminalResizing = false;
            m_terminalSplitter.SetDragging(false);
            if (GetCapture() == this)
                ReleaseCapture();
            return TRUE;
        }
    }

    // Enter sends the AI message; Shift+Enter inserts a newline. While an IME
    // is composing, let Enter commit the composition instead of sending early.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pInput = GetDlgItem(IDC_EDIT_AI_INPUT);
        if (pInput && pMsg->hwnd == pInput->m_hWnd)
        {
            if (!(GetKeyState(VK_SHIFT) & 0x8000) && !IsImeComposing(pInput->m_hWnd))
            {
                OnBnClickedAiSend();
                return TRUE;
            }
            return FALSE;
        }
    }

    // Route mouse interaction over the terminal area at the dialog level so
    // selection works even if the tab control is above the terminal window.
    if (m_pActiveTerminal && m_pActiveTerminal->m_hWnd)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        CRect rcTerm;
        m_pActiveTerminal->GetWindowRect(&rcTerm);
        if (rcTerm.PtInRect(pt) && pMsg->hwnd != m_terminalShell.m_hWnd)
        {
            TCHAR szClass[64]{};
            ::GetClassName(pMsg->hwnd, szClass, _countof(szClass));
            bool isShellDropdown = _tcsicmp(szClass, _T("ComboLBox")) == 0;

            if (!isShellDropdown && pMsg->message == WM_LBUTTONDOWN)
            {
                m_pActiveTerminal->StartSelectionFromScreen(pt);
                SetCapture();
                return TRUE;
            }
            if (!isShellDropdown && pMsg->message == WM_MOUSEMOVE && GetCapture() == this)
            {
                m_pActiveTerminal->ContinueSelectionFromScreen(pt);
                return TRUE;
            }
            if (!isShellDropdown && pMsg->message == WM_LBUTTONUP && GetCapture() == this)
            {
                m_pActiveTerminal->FinishSelection();
                if (GetCapture() == this)
                    ReleaseCapture();
                return TRUE;
            }
            if (!isShellDropdown && pMsg->message == WM_RBUTTONUP)
            {
                m_pActiveTerminal->ShowContextMenu(pt);
                return TRUE;
            }
        }
    }

    // Route terminal tab bar mouse clicks at the dialog level so they always
    // reach the custom hit-testing, even if IsDialogMessage would swallow them.
    if (m_terminalTabs.m_hWnd)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        CRect rcTabs;
        m_terminalTabs.GetWindowRect(&rcTabs);
        if (rcTabs.PtInRect(pt))
        {
            CPoint client = pt;
            m_terminalTabs.ScreenToClient(&client);
            if (pMsg->message == WM_LBUTTONDOWN)
            {
                m_terminalTabs.HandleClick(client);
                return TRUE;
            }
            if (pMsg->message == WM_RBUTTONUP)
            {
                m_terminalTabs.HandleRightClick(client);
                return TRUE;
            }
            if (pMsg->message == WM_MBUTTONUP)
            {
                m_terminalTabs.HandleMiddleClick(client);
                return TRUE;
            }
        }
        if (pMsg->hwnd == m_terminalTabs.m_hWnd)
            return FALSE;
    }

    // Wheel over the tab strip switches sessions instead of scrolling the terminal.
    if (pMsg->message == WM_MOUSEWHEEL && m_terminalTabs.m_hWnd)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        CRect rcTabs;
        m_terminalTabs.GetWindowRect(&rcTabs);
        if (rcTabs.PtInRect(pt))
        {
            m_terminalTabs.HandleWheel(GET_WHEEL_DELTA_WPARAM(pMsg->wParam));
            return TRUE;
        }
    }

    // Ctrl+Tab / Ctrl+PageUp / Ctrl+PageDown cycle terminal sessions.
    if (pMsg->message == WM_KEYDOWN && m_pActiveTerminal &&
        m_pActiveTerminal->m_hWnd &&
        pMsg->hwnd == m_pActiveTerminal->m_hWnd &&
        (GetKeyState(VK_CONTROL) & 0x8000))
    {
        if (pMsg->wParam == VK_TAB || pMsg->wParam == VK_PRIOR ||
            pMsg->wParam == VK_NEXT)
        {
            int count = static_cast<int>(m_terminalTabsList.size());
            if (count > 1)
            {
                bool forward = (pMsg->wParam == VK_NEXT) ||
                    (pMsg->wParam == VK_TAB && !(GetKeyState(VK_SHIFT) & 0x8000));
                int cur = m_terminalTabs.GetActive();
                if (cur < 0)
                    cur = 0;
                int idx = cur + (forward ? 1 : -1);
                idx = (idx + count) % count;
                ActivateTerminalTab(idx);
                return TRUE;
            }
        }
    }

    // The terminal view handles its own keyboard input. Returning FALSE here
    // keeps the dialog's Enter/accelerator handling from closing the app.
    if (m_pActiveTerminal && m_pActiveTerminal->m_hWnd &&
        pMsg->hwnd == m_pActiveTerminal->m_hWnd)
        return FALSE;

    // Enter inside the terminal shell dropdown should only move focus back
    // to the terminal, never trigger the dialog's default close behavior.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = CWnd::FromHandle(::GetFocus());
        if (pFocus && pFocus->GetDlgCtrlID() == IDC_TERMINAL_SHELL)
        {
            if (m_pActiveTerminal && m_pActiveTerminal->m_hWnd)
                m_pActiveTerminal->SetFocus();
            return TRUE;
        }
    }

    // Terminal key handling (same as main window)
    if (pMsg->message == WM_SYSKEYDOWN && m_pActiveTerminal &&
        m_pActiveTerminal->m_hWnd &&
        pMsg->hwnd == m_pActiveTerminal->m_hWnd)
    {
        // Ctrl+Shift+letter for accessibility (e.g. text selection in terminal)
        if (GetKeyState(VK_SHIFT) & 0x8000)
        {
            m_pActiveTerminal->SetFocus();
            return FALSE;
        }
    }

    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE &&
        m_pActiveTerminal && m_pActiveTerminal->m_hWnd &&
        pMsg->hwnd == m_pActiveTerminal->m_hWnd)
    {
        // ESC in terminal: cancel selection or reset cursor
        return FALSE;
    }

    if (pMsg->message == WM_MOUSEWHEEL && m_pActiveTerminal &&
        m_pActiveTerminal->m_hWnd)
    {
        CRect rcTerm;
        m_pActiveTerminal->GetWindowRect(&rcTerm);
        CPoint pt(GET_X_LPARAM(pMsg->lParam), GET_Y_LPARAM(pMsg->lParam));
        if (rcTerm.PtInRect(pt))
        {
            short zDelta = GET_WHEEL_DELTA_WPARAM(pMsg->wParam);
            m_pActiveTerminal->ScrollLines(zDelta);
            return TRUE;
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

// ============================================================================
// AI Assistant initialization
// ============================================================================

void CAIAssistantDlg::InitAIAssistant()
{
    auto& loc = CLocalizationManager::GetInstance();

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
    CWnd* pPlaceholder = GetDlgItem(IDC_AI_BROWSER);
    if (pPlaceholder)
    {
        CRect rc;
        pPlaceholder->GetWindowRect(&rc);
        ScreenToClient(&rc);
        pPlaceholder->DestroyWindow();

        if (m_aiBrowser.CreateControl(CLSID_WebBrowser, nullptr,
            WS_VISIBLE | WS_CHILD, rc, this, IDC_AI_BROWSER))
        {
            m_aiBrowserRect = rc;
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

            m_aiBrowser.BringWindowToTop();

            m_aiPendingHtml = BuildAiHtmlPage(
                _T("<div style='color:#888;text-align:center;padding-top:20px;'>")
                _T("AI Assistant Ready<br>")
                _T("<span style='font-size:13px;'>Ask me anything about this toolbox!</span>")
                _T("</div>"));
            SetTimer(1, 100, nullptr);
        }

        ConnectAiBrowserEvents();
    }

    // Translate buttons
    SetDlgItemText(IDC_BUTTON_AI_SEND, loc.GetString(_T("MainCtrl"), _T("BtnSend")));
    SetDlgItemText(IDC_BUTTON_AI_STOP, loc.GetString(_T("MainCtrl"), _T("BtnStop")));
    SetDlgItemText(IDC_BUTTON_AI_CLEAR, loc.GetString(_T("MainCtrl"), _T("BtnNewChat")));
    SetDlgItemText(IDC_BUTTON_AI_HISTORY, loc.GetString(_T("MainCtrl"), _T("BtnHistory")));

    // Stop button is initially disabled
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
}

// ============================================================================
// Terminal initialization
// ============================================================================

void CAIAssistantDlg::InitTerminal()
{
    auto& loc = CLocalizationManager::GetInstance();

    m_terminalShell.SubclassDlgItem(IDC_TERMINAL_SHELL, this);
    m_terminalShell.AddString(_T("PowerShell"));
    m_terminalShell.AddString(_T("CMD"));
    m_terminalShell.AddString(_T("WSL"));
    m_terminalShell.AddString(_T("Git Bash"));

    m_strTerminalShell = AfxGetApp()->GetProfileString(_T("Terminal"), _T("Shell"), _T("PowerShell"));
    int idx = m_terminalShell.FindStringExact(-1, m_strTerminalShell);
    m_terminalShell.SetCurSel(idx == CB_ERR ? 0 : idx);

    m_terminalLabel.SubclassDlgItem(IDC_TERMINAL_LABEL, this);
    m_terminalLabel.SetWindowText(loc.GetString(_T("Terminal"), _T("Title")));
    m_terminalClear.SubclassDlgItem(IDC_BTN_TERMINAL_CLEAR, this);
    m_terminalClear.SetWindowText(loc.GetString(_T("Terminal"), _T("Clear")));
    m_terminalSplitter.SubclassDlgItem(IDC_TERMINAL_SPLITTER, this);

    // First tab is the resource placeholder view.
    if (m_terminalView.AttachToPlaceholder(IDC_TERMINAL_VIEW, this))
    {
        m_terminalView.StartShell(m_strTerminalShell);
        m_pActiveTerminal = &m_terminalView;
        m_terminalTabsList.push_back(&m_terminalView);
        m_terminalView.Invalidate(TRUE);
    }

    if (m_terminalTabs.AttachToPlaceholder(IDC_TERMINAL_TABS, this))
    {
        m_terminalTabs.AddTab(m_strTerminalShell);
        m_terminalTabs.SetActive(0);
        m_terminalTabs.SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_terminalTabs.Invalidate(TRUE);
    }
}

// ============================================================================
// AI Send
// ============================================================================

void CAIAssistantDlg::OnBnClickedAiSend()
{
    CEdit* pInput = static_cast<CEdit*>(GetDlgItem(IDC_EDIT_AI_INPUT));
    if (!pInput) return;
    if (IsImeComposing(pInput->m_hWnd)) return;

    CString userMsg;
    pInput->GetWindowText(userMsg);
    userMsg.Trim();
    if (userMsg.IsEmpty()) return;

    pInput->SetWindowText(_T(""));
    pInput->SetSel(0, 0);

    if (m_aiHistory.empty() || m_aiHistory.front().first == _T("system"))
    {
        if (m_aiHistory.empty())
            m_aiHistory.emplace_back(_T("system"), BuildSystemPrompt());
        else
            m_aiHistory.front() = { _T("system"), BuildSystemPrompt() };
    }
    else
    {
        m_aiHistory.insert(m_aiHistory.begin(), { _T("system"), BuildSystemPrompt() });
    }

    m_aiHistory.push_back({ _T("user"), userMsg });

    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        auto& loc = CLocalizationManager::GetInstance();
        CString msg;
        msg.Format(loc.GetString(_T("AI"), _T("NoApiKey")), vendor.GetString());
        SetAiBrowserHtml(BuildAiHtmlPage(msg));
        m_aiHistory.pop_back();
        return;
    }

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(FALSE);
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(TRUE);

    m_aiStreamingContent.Empty();
    CAIApiClient::SendAsyncStreaming(m_aiHistory, vendor, apiKey, model, m_hWnd);
}

void CAIAssistantDlg::OnBnClickedAiClear()
{
    SaveCurrentConversation();
    m_strConvTitle.Empty();
    m_strConvPath.Empty();
    m_strConvCreated.Empty();
    m_aiHistory.clear();
    m_aiActionCommands.Clear();
    m_aiStreamingContent.Empty();
    m_aiPendingHtml.Empty();
    SetAiBrowserHtml(BuildAiHtmlPage(_T("")));
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
}

void CAIAssistantDlg::OnBnClickedAiStop()
{
    if (!m_aiCommandById.empty() && m_pActiveTerminal &&
        m_pActiveTerminal->m_hWnd)
    {
        m_pActiveTerminal->WriteUtf8("\x03");
    }

    CAIApiClient::Cancel();
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);
    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);
}

void CAIAssistantDlg::OnBnClickedAiHistory()
{
    CConversationHistoryDlg* pDlg = new CConversationHistoryDlg(this);
    if (!pDlg->Create(IDD_CONVERSATION_HISTORY_DLG, this))
    {
        delete pDlg;
        return;
    }
    pDlg->ShowWindow(SW_SHOW);
}

// ============================================================================
// AI response handlers
// ============================================================================

LRESULT CAIAssistantDlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
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

    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);
    CWnd* pStop = GetDlgItem(IDC_BUTTON_AI_STOP);
    if (pStop) pStop->EnableWindow(FALSE);

    return 0;
}

LRESULT CAIAssistantDlg::OnAiStreamChunk(WPARAM /*wParam*/, LPARAM lParam)
{
    CString* pChunk = reinterpret_cast<CString*>(lParam);
    if (!pChunk) return 0;

    m_aiStreamingContent += *pChunk;
    delete pChunk;

    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(m_aiStreamingContent)));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));

    return 0;
}

LRESULT CAIAssistantDlg::OnAiStreamDone(WPARAM wParam, LPARAM lParam)
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

LRESULT CAIAssistantDlg::OnConvLoaded(WPARAM wParam, LPARAM /*lParam*/)
{
    CString* pPath = reinterpret_cast<CString*>(wParam);
    if (!pPath) return 0;
    LoadConversation(*pPath);
    delete pPath;
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory()));
    ScrollAiBrowserToAnchor(_T("scroll-anchor"));
    return 0;
}

// ============================================================================
// HTML/Browser helpers
// ============================================================================

CString CAIAssistantDlg::BuildSystemPrompt()
{
    return _T("你是一个集成在 Windows MFC 工具箱应用程序中的 AI 助手。")
        _T("你的职责是帮助用户理解和使用这个工具箱，排查问题，并回答相关问题。\n\n")
        _T("=== 应用概述 ===\n\n")
        _T("这是一个多功能 Windows 工具箱，包含 6 个左侧标签页、")
        _T("3 个右侧快捷操作子标签页（常用/系统/工具），以及菜单栏中的 9 个工具。\n")
        _T("应用程序以管理员权限运行，支持最小化到系统托盘。\n\n")

        _T("=== 可执行命令协议（重要！你必须遵守此协议） ===\n\n")
        _T("当用户请求执行系统命令、操作文件、管理进程等时，你必须使用以下 ```action 格式返回可执行命令，\n")
        _T("绝对不可以在回答中只给出纯文本的命令描述。\n\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"要执行的命令\",\n")
        _T("  \"purpose\": \"用途说明\",\n")
        _T("  \"risk\": \"low/medium/high\",\n")
        _T("  \"terminal\": \"PowerShell/CMD/WSL/Git Bash\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("terminal 字段决定命令在哪个真实终端执行；command 字段只写命令本身，")
        _T("严禁包含 powershell.exe、pwsh、cmd.exe /c、wsl.exe、bash -c 等启动前缀，否则程序会重复包装。\n")
        _T("terminal 可省略；省略时程序根据命令自动推断，默认 CMD。\n")
        _T("选择终端：\n")
        _T("- PowerShell：PowerShell cmdlet，如 Get-Process、Get-Service、Select-Object、Where-Object、Sort-Object\n")
        _T("- CMD：Windows 内置命令，如 dir、ipconfig、tasklist、findstr、reg、net\n")
        _T("- WSL：Linux 命令，如 ls、grep、awk、sed、df、ps\n")
        _T("- Git Bash：git 和 bash 风格命令，如 git log、grep、head、tail\n")
        _T("管道 | 必须原样写在 command 字段内，程序会把它交给所选终端执行。\n")
        _T("命令默认在 %USERPROFILE% 目录执行；涉及其他位置时使用完整路径。\n\n")
        _T("风险等级说明：\n")
        _T("- low：无害操作（如打开文件夹、显示信息）\n")
        _T("- medium：有潜在影响的操作（如修改文件、重启进程）\n")
        _T("- high：高危操作（如删除文件、修改注册表、格式化磁盘）\n\n")
        _T("注意：包含 del、format、reg delete、net user 等关键词的命令会自动升级为高风险。\n")
        _T("高风险命令需要用户输入\"确认执行\"才能执行。\n\n")
        _T("示例（重要！请严格按照此格式执行）：\n\n")
        _T("用户：查看当前目录有哪些文件\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"dir\",\n")
        _T("  \"purpose\": \"查看当前目录的文件列表\",\n")
        _T("  \"risk\": \"low\",\n")
        _T("  \"terminal\": \"CMD\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("用户：打开计算器\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"start calc.exe\",\n")
        _T("  \"purpose\": \"打开计算器程序\",\n")
        _T("  \"risk\": \"low\",\n")
        _T("  \"terminal\": \"CMD\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("用户：查看占用 CPU 前 5 的进程\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"Get-Process | Sort-Object CPU -Descending | Select-Object -First 5\",\n")
        _T("  \"purpose\": \"查看占用 CPU 前 5 的进程\",\n")
        _T("  \"risk\": \"low\",\n")
        _T("  \"terminal\": \"PowerShell\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("用户：查看 WSL 里的磁盘空间\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"df -h\",\n")
        _T("  \"purpose\": \"查看 WSL 里的磁盘空间\",\n")
        _T("  \"risk\": \"low\",\n")
        _T("  \"terminal\": \"WSL\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("用户：查看最近 5 条 git 提交\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"git log --oneline -5\",\n")
        _T("  \"purpose\": \"查看最近 5 条 git 提交\",\n")
        _T("  \"risk\": \"low\",\n")
        _T("  \"terminal\": \"Git Bash\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("用户：删除 C:\\temp 目录下的所有文件\n")
        _T("AI：\n")
        _T("```action\n")
        _T("{\n")
        _T("  \"command\": \"del /f /s /q C:\\temp\\*\",\n")
        _T("  \"purpose\": \"删除 C:\\temp 目录下的所有文件\",\n")
        _T("  \"risk\": \"high\",\n")
        _T("  \"terminal\": \"CMD\"\n")
        _T("}\n")
        _T("```\n\n")
        _T("命令格式规则：\n")
        _T("- command 字段严禁包含 shell 启动器或 -Command/-c 包装，例如不要写 \"powershell Get-Process\"、")
        _T("\"cmd /c dir\"、\"wsl.exe ls\"、\"bash -c git status\"；终端类型由 terminal 字段决定。\n")
        _T("- 命令参数、管道、重定向和引号必须完整保留，不要简化或丢失参数。\n")
        _T("- PowerShell 命令必须配 terminal: \"PowerShell\"，不要配成 CMD。\n")
        _T("- 控制台程序、交互程序、可执行文件（.exe/.bat/.cmd/.ps1 等）直接给出命令或带引号的完整路径，不要使用 start，因为程序会在终端 tab 中运行并支持输入输出。\n")
        _T("- 打开文件、文件夹、网址、媒体文件（mp3/mp4/文档/URL 等）使用 start \"\" \"路径\"，由系统默认程序打开。\n")
        _T("- 需要用户输入的控制台程序绝对不要用 start，否则输入输出无法回传。\n")
        _T("- 含空格的路径使用双引号包住；路径中的反斜杠按 Windows 风格保留。\n\n")
        _T("命令执行结果反馈：\n")
        _T("- 命令执行后，stdout/stderr 输出会被捕获并在 WebBrowser 中显示\n")
        _T("- 输出内容包括：命令的标准输出、标准错误输出、退出代码\n")
        _T("- 对于 echo、dir、ipconfig 等产生输出的命令，用户可以直接在对话中看到执行结果\n")
        _T("- 执行超时限制为 30 秒，超时后进程将被终止\n\n")

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
        _T("   - 显示当前用户的启动项（来自 HKCU\\\\Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run）\n")
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
        _T("   - \"复制到\"/\"移动到\"：选择目标文件夹，然后复制或移动拖放的文件\n\n")
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
        _T("=== 右侧快捷操作（3 个子标签页：快捷打开 / 系统 / 工具） ===\n\n")
        _T("「快捷打开」标签页：\n")
        _T("   - 用户可配置的快捷打开按钮（最多 32 个，存储在 config.ini [QuickLaunch] 节）\n")
        _T("   - 支持类型：可执行文件 (.exe)、文件夹、网址、其他文件、快捷键\n")
        _T("   - 可执行文件可配置可选的唤醒快捷键：若进程已运行则发送快捷键激活，否则启动程序\n")
        _T("   - 快捷键类型：点击时模拟键盘快捷键（无需路径/启动程序）\n")
        _T("   - 点击\"管理\"打开快捷打开管理对话框：\n")
        _T("     - 添加：设置名称、类型和路径/URL；可选配置唤醒快捷键\n")
        _T("     - 编辑：修改已有项目属性\n")
        _T("     - 删除：移除选中项目\n")
        _T("     - 排序：上移/下移按钮\n")
        _T("     - 拖放支持：拖放文件/文件夹快速添加\n")
        _T("     - 右键菜单：添加、编辑、删除、上移、下移\n\n")
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
        _T("  2. 编码转换\n\n")
        _T("图像工具(&I) 子菜单：\n")
        _T("  3. 二维码生成\n")
        _T("  4. 截图OCR\n\n")
        _T("文件工具(&F) 子菜单：\n")
        _T("  5. 文件夹处理\n\n")
        _T("系统工具(&S) 子菜单：\n")
        _T("  6. 右键菜单管理\n")
        _T("  7. 环境变量管理\n")
        _T("  8. 文件占用查看\n\n")
        _T("直接菜单项（不在子菜单中）：\n")
        _T("  9. 简易便签\n\n")
        _T("=== 其他功能 ===\n\n")
        _T("位于左下角的\n\n")
        _T("   - 连点器：勾选\"连点器\"启用；按开始键开始点击，停止键停止\n")
        _T("     可在设置中配置间隔（毫秒）和开始/停止键；启用时显示速度调节窗口\n")
        _T("   - 禁止自动锁屏：勾选\"禁止自动锁屏\"保持屏幕常亮（SetThreadExecutionState）\n")
        _T("   - 开机自启动：勾选\"开机自启动\"添加到注册表 Run 键\n")
        _T("   - \"窗口置顶\"复选框：保持工具箱始终在最前\n\n")
        _T("位于菜单栏右侧（帮助菜单旁）\n\n")
        _T("   - \"最小化到托盘\"复选框：勾选后点击 X 按钮最小化到系统托盘而非关闭\n\n")
        _T("位于系统托盘区\n\n")
        _T("   - 系统托盘：双击图标恢复窗口；右键菜单\"显示窗口\"或\"退出\"\n\n")
        _T("=== 终端 ===\n\n")
        _T("   - AI 助手右侧面板内置 ConPTY 终端，支持多个终端会话\n")
        _T("   - 终端会话使用横向标签栏；Ctrl+Tab/滚轮切换，中键关闭，右键可新建或关闭\n")
        _T("   - AI 执行命令时会自动打开一个新终端 tab，命令在真实终端中运行，可交互输入输出\n")
        _T("   - 命令结束后结果会回传到 AI 对话\n\n")
        _T("=== 快捷键 ===\n\n")
        _T("   - 可配置的全局热键（文件 > 设置 > 快捷键）：\n")
        _T("     - 显示/隐藏主窗口（默认：Ctrl+Alt+Space）— 显示在标题栏中\n")
        _T("     - 窗口定位（默认：Ctrl+Alt+D）— 与\"定位窗口\"按钮功能相同\n")
        _T("   - 热键捕捉：点击\"捕捉\"按钮，然后按下所需按键组合\n")
        _T("     - 支持：Ctrl/Alt/Shift/Win + 任意键（包括空格）\n")
        _T("     - \"清除\"按钮移除热键绑定\n")
        _T("     - 重复热键检测：若两个热键使用相同组合会发出警告\n")
        _T("   - 热键修改后立即生效（无需重启）\n")
        _T("   - 帮助 > 快捷键列表：显示当前热键配置（动态文本）\n")
        _T("   - Alt+1~6：切换到左侧标签页 1-6\n")
        _T("   - F5：刷新当前标签页的列表（进程、启动项等）\n")
        _T("   - 回车：在音量输入框或命令输入框中应用/执行\n\n")
        _T("=== 配置 ===\n\n")
        _T("   - 文件 > 设置：配置文件夹路径（截图、便签），网址（MOOC、SDUCS），")
        _T("连点器间隔和开始/停止键，AI 供应商和 API 密钥，全局快捷键\n")
        _T("   - 设置 > 快捷键：配置显示/隐藏主窗口和窗口定位的全局热键\n")
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
        _T("   - 如果用户请求执行系统命令、操作文件、管理进程等，你必须使用 ```action 协议返回可执行命令（格式见上方\"可执行命令协议\"章节），不得给出纯文本命令描述\n")
        _T("   - 任何需要执行的操作（如查看、打开、删除、启动、创建、关闭、复制、移动等）都应当返回 ```action 格式的可执行命令，而不是文字描述。\n\n");
        
}

CString CAIAssistantDlg::BuildAiHtmlPage(const CString& bodyContent)
{
    return _T("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><style>")
        _T("body{font-family:Consolas,'Microsoft YaHei',sans-serif;font-size:18px;")
        _T("background:#1e1e1e;color:#d4d4d4;padding:8px;margin:0;line-height:1.5;}")
        _T("code{background:#2d2d2d;padding:1px 4px;border-radius:3px;font-family:Consolas,monospace;}")
        _T("pre{background:#2d2d2d;padding:8px;border-radius:4px;overflow-x:auto;}")
        _T("pre code{background:none;padding:0;}")
        _T(".code-block{margin:8px 0;}")
        _T(".code-header{display:flex;justify-content:space-between;align-items:center;gap:8px;")
        _T("padding:4px 8px;background:#333;border-radius:4px 4px 0 0;font-size:13px;color:#888;}")
        _T(".code-header + pre{margin:0;border-radius:0 0 4px 4px;}")
        _T(".copy-code-btn{background:#3a3a3a;color:#d4d4d4;border:1px solid #555;border-radius:4px;")
        _T("padding:2px 8px;font-size:13px;cursor:pointer;}")
        _T(".copy-code-btn:hover{background:#444;}")
        _T("a{color:#569cd6;}")
        _T("h1,h2,h3{color:#dcdcaa;margin:8px 0 4px;}")
        _T("table{border-collapse:collapse;}")
        _T("th,td{border:1px solid #444;padding:4px 8px;}")
        _T("th{background:#2d2d2d;}")
        _T("blockquote{border-left:3px solid #569cd6;margin:4px 0;padding-left:12px;color:#888;}")
        _T(".action-card{border:2px solid #555;border-radius:8px;padding:12px 16px;margin:12px 0;background:#2a2a2a;font-family:Consolas,monospace;}")
        _T(".action-card.action-level-low{border-color:#2da44e;}")
        _T(".action-card.action-level-medium{border-color:#d4a72c;}")
        _T(".action-card.action-level-high{border-color:#cf222e;}")
        _T(".action-purpose{font-size:16px;color:#d4d4d4;margin-bottom:4px;}")
        _T(".action-risk{font-size:14px;font-weight:600;margin-bottom:8px;}")
        _T(".action-risk.level-low{color:#2da44e;}")
        _T(".action-risk.level-medium{color:#d4a72c;}")
        _T(".action-risk.level-high{color:#cf222e;}")
        _T(".action-terminal{font-size:14px;color:#9cdcfe;margin-bottom:6px;}")
        _T(".action-btn{background:#2da44e;color:#fff;border:none;border-radius:6px;padding:6px 14px;font-size:16px;cursor:pointer;margin-bottom:6px;}")
        _T(".action-btn:hover{background:#218838;}")
        _T(".action-card.action-level-high .action-btn{background:#cf222e;}")
        _T(".action-card.action-level-high .action-btn:hover{background:#a11c26;}")
        _T(".action-card.action-level-medium .action-btn{background:#d4a72c;}")
        _T(".action-card.action-level-medium .action-btn:hover{background:#b88a1f;}")
        _T(".action-command{font-size:15px;color:#888;background:#333;padding:4px 8px;border-radius:4px;word-break:break-all;}")
        _T("</style><script>")
        _T("function execCmd(btn){")
        _T("var id=btn.getAttribute('data-cmd-id');")
        _T("if(!id)return;")
        _T("location.href='http://127.0.0.1:1/exec/'+id;")
        _T("}")
        _T("function copyText(t){")
        _T("if(window.clipboardData&&window.clipboardData.setData){window.clipboardData.setData('Text',t);return true;}")
        _T("var ta=document.createElement('textarea');ta.value=t;ta.style.position='fixed';ta.style.opacity='0';")
        _T("document.body.appendChild(ta);ta.select();")
        _T("try{return document.execCommand('copy');}catch(e){return false;}finally{document.body.removeChild(ta);}")
        _T("}")
        _T("function copyCode(btn){")
        _T("var block=btn.parentNode.parentNode;var pre=block.querySelector('pre');")
        _T("var text=pre?pre.innerText:'';")
        _T("if(copyText(text)){var old=btn.textContent;btn.textContent=btn.getAttribute('data-copied')||'已复制';")
        _T("setTimeout(function(){btn.textContent=old;},1200);}")
        _T("}")
        _T("</script></head><body>") + bodyContent + _T("</body></html>");
}

CString CAIAssistantDlg::BuildAiBodyFromHistory(const CString& streamingContent, const CString& scrollToCommand)
{
    CString body;

    std::map<CString, std::vector<CString>> cmdResults;
    std::map<CString, int> cmdResultIndex;
    for (auto& msg : m_aiHistory)
    {
        if (msg.first == _T("system") && msg.second.Find(_T("【命令执行结果】")) == 0)
        {
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
            continue;
        }
        else if (msg.first == _T("system"))
        {
            continue;
        }
        else if (msg.first == _T("user"))
        {
            body += _T("<div style='color:#888;margin-bottom:4px;'>You: </div>");
            body += _T("<div style='white-space:pre-wrap;margin-bottom:4px;'>")
                + CMarkdownDlg::EscapeHtml(msg.second) + _T("</div>");
        }
        else if (msg.first == _T("assistant"))
        {
            body += _T("<div style='color:#c8c8c8;margin-bottom:4px;'>AI:</div>");
            body += RenderAssistantWithResults(msg.second, cmdResults, cmdResultIndex);
        }
    }

    if (!streamingContent.IsEmpty())
    {
        body += _T("<div style='color:#c8c8c8;margin-bottom:4px;'>AI:</div>")
            + CMarkdownDlg::MarkdownToBody(streamingContent, &m_aiActionCommands);
    }

    body += _T("<div id=\"scroll-anchor\"></div>");

    if (!scrollToCommand.IsEmpty())
    {
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

CString CAIAssistantDlg::RenderAssistantWithResults(const CString& content,
    std::map<CString, std::vector<CString>>& cmdResults,
    std::map<CString, int>& cmdResultIndex)
{
    CString html;
    int pos = 0;

    while (true)
    {
        int actionStart = content.Find(_T("```action"), pos);
        if (actionStart < 0)
        {
            CString rest = content.Mid(pos);
            if (!rest.IsEmpty())
                html += CMarkdownDlg::MarkdownToBody(rest, &m_aiActionCommands);
            break;
        }

        if (actionStart > pos)
        {
            CString beforeText = content.Mid(pos, actionStart - pos);
            if (!beforeText.IsEmpty())
                html += CMarkdownDlg::MarkdownToBody(beforeText, &m_aiActionCommands);
        }

        int codeEnd = content.Find(_T("```"), actionStart + 9);
        if (codeEnd < 0 || codeEnd <= actionStart + 9)
        {
            html += CMarkdownDlg::MarkdownToBody(content.Mid(actionStart), &m_aiActionCommands);
            break;
        }

        CString actionBlock = content.Mid(actionStart, codeEnd + 3 - actionStart);
        html += CMarkdownDlg::MarkdownToBody(actionBlock, &m_aiActionCommands);

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
                auto& results = cmdResults[matchedCommand];
                int idx = cmdResultIndex[matchedCommand];
                if (idx < (int)results.size())
                {
                    html += _T("<div style='color:#569cd6;font-size:15px;border-left:3px solid #569cd6;padding-left:8px;margin:4px 0;'>")
                        + CMarkdownDlg::MarkdownToBody(results[idx], &m_aiActionCommands) + _T("</div>")
                        + _T("<div class=\"cmd-result-marker\" data-command=\"") + CMarkdownDlg::EscapeHtml(matchedCommand) + _T("\"></div>");
                    cmdResultIndex[matchedCommand] = idx + 1;
                }
            }
        }

        pos = codeEnd + 3;
    }

    return html;
}

bool CAIAssistantDlg::SetAiBrowserHtml(const CString& html)
{
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
    if (!m_aiBrowserReady)
    {
        m_aiBrowserReady = true;
        KillTimer(1);
    }
    return true;
}

void CAIAssistantDlg::ScrollAiBrowserToAnchor(const CString& elementId)
{
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
        vTop.boolVal = VARIANT_TRUE;
        pElement->scrollIntoView(vTop);
        pElement->Release();
    }
    SysFreeString(bstrId);

    pDoc3->Release();
    pDocDisp->Release();
    pWeb2->Release();
}

void CAIAssistantDlg::ConnectAiBrowserEvents()
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
        }
        else
        {
            delete m_pAiEventSink;
            m_pAiEventSink = nullptr;
            m_dwAiEventCookie = 0;
        }
        pCP->Release();
    }
    pCPC->Release();
}

void CAIAssistantDlg::DisconnectAiBrowserEvents()
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

    m_pAiEventSink->Release();
    m_pAiEventSink = nullptr;
    m_dwAiEventCookie = 0;
}

void CAIAssistantDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        if (m_aiBrowserReady)
        {
            KillTimer(1);
            return;
        }
        if (SetAiBrowserHtml(m_aiPendingHtml.IsEmpty() ? CString(_T("")) : m_aiPendingHtml))
        {
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

// ============================================================================
// Conversation save/load
// ============================================================================

CString CAIAssistantDlg::GetExeDir()
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(nullptr, szPath, MAX_PATH);
    CString path(szPath);
    int pos = path.ReverseFind('\\');
    if (pos >= 0)
        path = path.Left(pos + 1);
    return path;
}

CString CAIAssistantDlg::GetConversationsFolder()
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

void CAIAssistantDlg::SaveCurrentConversation()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_aiHistory.empty()) return;

    CTime now = CTime::GetCurrentTime();

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

    CString convDir = GetConversationsFolder();
    CreateDirectory(convDir, nullptr);

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

void CAIAssistantDlg::LoadConversation(const CString& filePath)
{
    CFile file;
    if (!file.Open(filePath, CFile::modeRead))
        return;

    CArchive ar(&file, CArchive::load);
    int nCount = 0;
    CString title, created, updated;

    ar >> nCount;
    ar >> title;
    ar >> created;
    ar >> updated;

    m_aiHistory.clear();
    m_aiHistory.reserve(nCount);

    for (int i = 0; i < nCount; i++)
    {
        CString role, content;
        ar >> role >> content;
        m_aiHistory.push_back({ role, content });
    }

    ar.Close();
    file.Close();

    m_strConvTitle = title;
    m_strConvPath = filePath;
    m_strConvCreated = created;
}

// ============================================================================
// AI command execution
// ============================================================================

LRESULT CAIAssistantDlg::OnAiExecuteCommand(WPARAM /*wParam*/, LPARAM lParam)
{
    auto& loc = CLocalizationManager::GetInstance();
    // lParam is a TCHAR* command id allocated by _tcsdup in HandleAppExecUrl
    TCHAR* pCmdId = reinterpret_cast<TCHAR*>(lParam);
    if (!pCmdId) return 0;

    CString cmdId = pCmdId;
    free(pCmdId);

    CString json;
    if (!m_aiActionCommands.Get(cmdId, json))
        return 0;

    CString command, purpose, risk, terminal;
    try
    {
        std::string s = (LPCSTR)CT2A(json, CP_UTF8);
        nlohmann::json j = nlohmann::json::parse(s);
        command = CString(CA2T(j["command"].get<std::string>().c_str(), CP_UTF8));
        purpose = CString(CA2T(j["purpose"].get<std::string>().c_str(), CP_UTF8));
        risk = CString(CA2T(j["risk"].get<std::string>().c_str(), CP_UTF8));
        if (j.contains("terminal") && j["terminal"].is_string())
            terminal = CString(CA2T(j["terminal"].get<std::string>().c_str(), CP_UTF8));
    }
    catch (const nlohmann::json::parse_error&)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("InvalidJsonCmd")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return 0;
    }

    risk.MakeLower();
    if (risk.IsEmpty()) risk = _T("medium");

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

    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("AiCmdConfirmFmt")),
        command.GetString(), purpose.GetString(), risk.GetString());

    bool bExecute = false;

    if (risk == _T("high"))
    {
        CHighRiskConfirmDlg dlg;
        if (dlg.DoModal() == IDOK)
        {
            dlg.m_input.Trim();
            bExecute = (dlg.m_input == loc.GetString(_T("Msg"), _T("HighRiskConfirmText")));
            if (!bExecute)
                MessageBox(loc.GetString(_T("Msg"), _T("InputMismatchCancel")), loc.GetString(_T("Msg"), _T("Cancelled")), MB_OK | MB_ICONINFORMATION);
        }
    }
    else
    {
        UINT nType = (risk == _T("medium")) ? MB_ICONWARNING : MB_ICONINFORMATION;
        bExecute = (MessageBox(msg, loc.GetString(_T("Msg"), _T("ExecConfirm")), MB_YESNO | nType | MB_DEFBUTTON2) == IDYES);
    }

    if (!bExecute)
    {
        CString resultMsg;
        resultMsg.Format(loc.GetString(_T("Msg"), _T("CmdCancelledFmt")),
            command.GetString());
        {
            int insertPos = (int)m_aiHistory.size();
            for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
            {
                if (m_aiHistory[i].first == _T("assistant"))
                {
                    insertPos = i + 1;
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

    AddAiCommandTab(command, terminal);
    return 0;
}

LRESULT CAIAssistantDlg::OnAiCommandResult(WPARAM /*wParam*/, LPARAM lParam)
{
    CommandResult* pResult = reinterpret_cast<CommandResult*>(lParam);
    if (!pResult) return 0;

    CString command = pResult->command;
    CString resultMsg = pResult->resultMsg;
    delete pResult;

    {
        int insertPos = (int)m_aiHistory.size();
        for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
        {
            if (m_aiHistory[i].first == _T("assistant"))
            {
                insertPos = i + 1;
                while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                    insertPos++;
                break;
            }
        }
        m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
    }

    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));

    CWnd* pSend = GetDlgItem(IDC_BUTTON_AI_SEND);
    if (pSend) pSend->EnableWindow(TRUE);

    return 0;
}

LRESULT CAIAssistantDlg::OnAiSessionOutput(WPARAM wParam, LPARAM lParam)
{
    auto* session = reinterpret_cast<CTerminalSession*>(wParam);
    auto* p = reinterpret_cast<std::string*>(lParam);
    auto it = m_aiCommandContexts.find(session);
    if (p)
    {
        if (it != m_aiCommandContexts.end())
            it->second.output.append(*p);
        delete p;
    }
    return 0;
}

LRESULT CAIAssistantDlg::OnAiSessionExited(WPARAM wParam, LPARAM)
{
    auto* session = reinterpret_cast<CTerminalSession*>(wParam);
    if (m_aiCommandContexts.find(session) == m_aiCommandContexts.end())
        return 0;
    FinishAiCommand(session);
    return 0;
}

void CAIAssistantDlg::FinishAiCommand(CTerminalSession* session)
{
    auto it = m_aiCommandContexts.find(session);
    if (it == m_aiCommandContexts.end())
        return;

    CString command = it->second.command;
    CString outputStr;
    const std::string& output = it->second.output;

    int wlen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        output.data(), static_cast<int>(output.size()), nullptr, 0);
    if (wlen > 0)
    {
        std::wstring wstr(static_cast<size_t>(wlen), 0);
        ::MultiByteToWideChar(CP_UTF8, 0, output.data(),
            static_cast<int>(output.size()), &wstr[0], wlen);
        outputStr = wstr.c_str();
    }
    else if (!output.empty())
    {
        int ansiLen = ::MultiByteToWideChar(CP_ACP, 0,
            output.data(), static_cast<int>(output.size()), nullptr, 0);
        if (ansiLen > 0)
        {
            std::wstring wstr(static_cast<size_t>(ansiLen), 0);
            ::MultiByteToWideChar(CP_ACP, 0, output.data(),
                static_cast<int>(output.size()), &wstr[0], ansiLen);
            outputStr = wstr.c_str();
        }
        else
        {
            outputStr = CString(output.c_str());
        }
    }
    outputStr.Trim();

    DWORD exitCode = session->ExitCode();

    CString resultMsg;
    resultMsg.Format(_T("【命令执行结果】\n命令：%s\n状态：已执行\n\n"), command.GetString());
    if (!outputStr.IsEmpty())
        resultMsg += _T("输出：\n```\n") + outputStr + _T("\n```\n\n");
    CString exitStr;
    exitStr.Format(_T("退出码：%lu"), exitCode);
    resultMsg += exitStr;

    int insertPos = (int)m_aiHistory.size();
    for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
    {
        if (m_aiHistory[i].first == _T("assistant"))
        {
            insertPos = i + 1;
            while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                insertPos++;
            break;
        }
    }
    m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));

    m_aiCommandContexts.erase(it);
    for (auto i = m_aiSessions.begin(); i != m_aiSessions.end(); ++i)
    {
        if (i->get() == session)
        {
            m_aiSessions.erase(i);
            break;
        }
    }
}

void CAIAssistantDlg::AddAiCommandTab(const CString& command, const CString& terminal)
{
    CString cmdTrimmed = command;
    cmdTrimmed.Trim();

    if (cmdTrimmed.Left(6).CompareNoCase(_T("start ")) == 0)
    {
        CString rest = cmdTrimmed.Mid(6).Trim();
        if (rest.Left(1) == _T('"'))
        {
            int closeQuote = rest.Find(_T('"'), 1);
            if (closeQuote > 1)
                rest = rest.Mid(closeQuote + 1).Trim();
        }
        bool executable = (rest.Find(_T(".exe ")) > 0 || rest.Find(_T(".exe\"")) > 0 ||
            rest.Find(_T(".com ")) > 0 || rest.Find(_T(".bat ")) > 0 ||
            rest.Find(_T(".cmd ")) > 0 || rest.Find(_T(".ps1 ")) > 0 ||
            rest.Find(_T(".exe")) == rest.GetLength() - 4);
        if (executable && !rest.IsEmpty())
            cmdTrimmed = rest;
        else
            cmdTrimmed = command;
    }

    CString shellName = CTerminalView::NormalizeShellName(terminal);
    if (shellName.IsEmpty())
    {
        if (cmdTrimmed.Left(14).CompareNoCase(_T("powershell.exe ")) == 0 ||
            cmdTrimmed.Left(11).CompareNoCase(_T("powershell ")) == 0 ||
            cmdTrimmed.Left(5).CompareNoCase(_T("pwsh ")) == 0)
            shellName = _T("PowerShell");
        else if (cmdTrimmed.Left(8).CompareNoCase(_T("wsl.exe ")) == 0 ||
            cmdTrimmed.Left(4).CompareNoCase(_T("wsl ")) == 0)
            shellName = _T("WSL");
        else if (cmdTrimmed.Left(9).CompareNoCase(_T("bash.exe ")) == 0 ||
            cmdTrimmed.Left(5).CompareNoCase(_T("bash ")) == 0)
            shellName = _T("Git Bash");
        else
            shellName = _T("CMD");
    }
    CString cmdLine = CTerminalView::BuildAiCommandLine(shellName, cmdTrimmed);

    const int kMaxCommandLineLength = 32000;
    if (cmdLine.GetLength() > kMaxCommandLineLength)
    {
        CString resultMsg;
        resultMsg.Format(_T("【命令执行结果】\n命令：%s\n状态：已拒绝\n\n输出：\n```\n命令过长，超过 Windows 命令行 32767 字符限制，无法在终端中执行。\n```\n\n退出码：0"),
            command.GetString());
        int insertPos = (int)m_aiHistory.size();
        for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
        {
            if (m_aiHistory[i].first == _T("assistant"))
            {
                insertPos = i + 1;
                while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                    insertPos++;
                break;
            }
        }
        m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
        SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));
        return;
    }

    AddTerminalTabWithCommand(shellName, cmdLine);
    CTerminalView* view = m_pActiveTerminal;
    if (!view)
        return;

    UINT_PTR id = m_aiNextCommandId++;
    m_aiCommandById[id] = command;
    view->StartAiCapture(id, CString(), CString(), m_hWnd);

    CString runningMsg;
    runningMsg.Format(_T("【命令执行中】\n命令：%s\n终端：%s\n\n已在新终端 tab 中运行，可直接在终端里交互。"),
        command.GetString(), shellName.GetString());
    int insertPos = (int)m_aiHistory.size();
    for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
    {
        if (m_aiHistory[i].first == _T("assistant"))
        {
            insertPos = i + 1;
            while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                insertPos++;
            break;
        }
    }
    m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), runningMsg });
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));
}

LRESULT CAIAssistantDlg::OnAiCaptureDone(WPARAM, LPARAM lParam)
{
    auto* pResult = reinterpret_cast<CTerminalView::AiCaptureResult*>(lParam);
    if (!pResult)
        return 0;

    UINT_PTR id = pResult->id;
    std::string output = std::move(pResult->output);
    DWORD exitCode = pResult->exitCode;
    delete pResult;

    auto it = m_aiCommandById.find(id);
    if (it == m_aiCommandById.end())
        return 0;

    CString command = it->second;
    m_aiCommandById.erase(it);

    CString outputStr;
    int wlen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        output.data(), static_cast<int>(output.size()), nullptr, 0);
    if (wlen > 0)
    {
        std::wstring wstr(static_cast<size_t>(wlen), 0);
        ::MultiByteToWideChar(CP_UTF8, 0, output.data(),
            static_cast<int>(output.size()), &wstr[0], wlen);
        outputStr = wstr.c_str();
    }
    else if (!output.empty())
    {
        int ansiLen = ::MultiByteToWideChar(CP_ACP, 0,
            output.data(), static_cast<int>(output.size()), nullptr, 0);
        if (ansiLen > 0)
        {
            std::wstring wstr(static_cast<size_t>(ansiLen), 0);
            ::MultiByteToWideChar(CP_ACP, 0, output.data(),
                static_cast<int>(output.size()), &wstr[0], ansiLen);
            outputStr = wstr.c_str();
        }
        else
        {
            outputStr = CString(output.c_str());
        }
    }
    outputStr.Trim();

    CString resultMsg;
    resultMsg.Format(_T("【命令执行结果】\n命令：%s\n状态：已执行\n\n"), command.GetString());
    if (!outputStr.IsEmpty())
        resultMsg += _T("输出：\n```\n") + outputStr + _T("\n```\n\n");
    CString exitStr;
    exitStr.Format(_T("退出码：%lu"), exitCode);
    resultMsg += exitStr;

    int insertPos = (int)m_aiHistory.size();
    for (int i = (int)m_aiHistory.size() - 1; i >= 0; i--)
    {
        if (m_aiHistory[i].first == _T("assistant"))
        {
            insertPos = i + 1;
            while (insertPos < (int)m_aiHistory.size() && m_aiHistory[insertPos].first == _T("system"))
                insertPos++;
            break;
        }
    }
    m_aiHistory.insert(m_aiHistory.begin() + insertPos, { _T("system"), resultMsg });
    SetAiBrowserHtml(BuildAiHtmlPage(BuildAiBodyFromHistory(CString(), command)));
    return 0;
}

// ============================================================================
// Terminal helpers
// ============================================================================

CTerminalView* CAIAssistantDlg::ActiveTerminal()
{
    return m_pActiveTerminal ? m_pActiveTerminal : &m_terminalView;
}

void CAIAssistantDlg::ActivateTerminalTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_terminalTabsList.size()))
        return;

    m_pActiveTerminal = m_terminalTabsList[index];
    for (CTerminalView* v : m_terminalTabsList)
    {
        if (v && v->m_hWnd)
            v->ShowWindow(v == m_pActiveTerminal ? SW_SHOW : SW_HIDE);
    }

    if (m_terminalTabs.m_hWnd)
        m_terminalTabs.SetActive(index);

    if (m_pActiveTerminal)
    {
        m_pActiveTerminal->SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_pActiveTerminal->Invalidate(TRUE);
        m_pActiveTerminal->RedrawWindow();
        m_pActiveTerminal->SetFocus();

        CString shell = m_pActiveTerminal->GetShellName();
        if (!shell.IsEmpty())
        {
            int idx = m_terminalShell.FindStringExact(-1, shell);
            if (idx != CB_ERR)
                m_terminalShell.SetCurSel(idx);
        }
    }

    if (m_terminalTabs.m_hWnd)
    {
        m_terminalTabs.SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_terminalTabs.Invalidate(TRUE);
    }
}

void CAIAssistantDlg::AddTerminalTab(const CString& shellName)
{
    if (m_terminalTabsList.empty())
        return;

    CTerminalView* refView = m_pActiveTerminal ? m_pActiveTerminal : m_terminalTabsList.front();
    CRect rc;
    refView->GetWindowRect(&rc);
    ScreenToClient(&rc);

    auto view = std::make_unique<CTerminalView>();
    UINT nId = 4000 + static_cast<UINT>(m_terminalTabsList.size());
    if (!view->CreateTerminal(this, nId, rc))
        return;

    view->StartShell(shellName);
    view->ShowWindow(SW_HIDE);
    m_extraTerminalViews.push_back(std::move(view));
    m_terminalTabsList.push_back(m_extraTerminalViews.back().get());

    int idx = static_cast<int>(m_terminalTabsList.size()) - 1;
    if (m_terminalTabs.m_hWnd)
        m_terminalTabs.AddTab(shellName);
    ActivateTerminalTab(idx);
    CRect rcClient;
    GetClientRect(&rcClient);
    OnSize(SIZE_RESTORED, rcClient.Width(), rcClient.Height());
}

void CAIAssistantDlg::AddTerminalTabWithCommand(const CString& shellName,
    const CString& cmdLine)
{
    if (m_terminalTabsList.empty())
        return;

    CTerminalView* refView = m_pActiveTerminal ? m_pActiveTerminal : m_terminalTabsList.front();
    CRect rc;
    refView->GetWindowRect(&rc);
    ScreenToClient(&rc);

    auto view = std::make_unique<CTerminalView>();
    UINT nId = 4000 + static_cast<UINT>(m_terminalTabsList.size());
    if (!view->CreateTerminal(this, nId, rc))
        return;

    view->StartCommandSession(cmdLine, shellName);
    view->ShowWindow(SW_HIDE);
    m_extraTerminalViews.push_back(std::move(view));
    m_terminalTabsList.push_back(m_extraTerminalViews.back().get());

    int idx = static_cast<int>(m_terminalTabsList.size()) - 1;
    if (m_terminalTabs.m_hWnd)
        m_terminalTabs.AddTab(shellName);
    ActivateTerminalTab(idx);
    CRect rcClient;
    GetClientRect(&rcClient);
    OnSize(SIZE_RESTORED, rcClient.Width(), rcClient.Height());
}

void CAIAssistantDlg::CloseTerminalTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_terminalTabsList.size()))
        return;
    if (m_terminalTabsList.size() <= 1)
        return;

    CTerminalView* victim = m_terminalTabsList[index];
    m_terminalTabsList.erase(m_terminalTabsList.begin() + index);
    if (m_terminalTabs.m_hWnd)
        m_terminalTabs.RemoveTab(index);

    for (auto i = m_extraTerminalViews.begin(); i != m_extraTerminalViews.end(); ++i)
    {
        if (i->get() == victim)
        {
            m_extraTerminalViews.erase(i);
            break;
        }
    }
    if (victim == &m_terminalView)
        victim->ShowWindow(SW_HIDE);

    int newIdx = std::min(index, static_cast<int>(m_terminalTabsList.size()) - 1);
    if (newIdx >= 0)
        ActivateTerminalTab(newIdx);
    else
        m_pActiveTerminal = nullptr;
}

LRESULT CAIAssistantDlg::OnTermTabSelect(WPARAM wParam, LPARAM)
{
    ActivateTerminalTab(static_cast<int>(wParam));
    return 0;
}

LRESULT CAIAssistantDlg::OnTermTabClose(WPARAM wParam, LPARAM)
{
    CloseTerminalTab(static_cast<int>(wParam));
    return 0;
}

LRESULT CAIAssistantDlg::OnTermTabNew(WPARAM, LPARAM)
{
    AddTerminalTab(m_strTerminalShell);
    return 0;
}

void CAIAssistantDlg::OnBnClickedTerminalClear()
{
    CTerminalView* active = ActiveTerminal();
    if (!active)
        return;
    active->ClearScreen();
    active->SetFocus();
}

void CAIAssistantDlg::OnCbnSelchangeTerminalShell()
{
    int idx = m_terminalShell.GetCurSel();
    if (idx == CB_ERR)
        return;

    CString shell;
    m_terminalShell.GetLBText(idx, shell);
    m_strTerminalShell = shell;
    AfxGetApp()->WriteProfileString(_T("Terminal"), _T("Shell"), shell);

    CTerminalView* active = ActiveTerminal();
    if (active)
    {
        active->StartShell(shell);
        active->SetFocus();
    }

    if (m_terminalTabs.m_hWnd && m_pActiveTerminal)
    {
        int tabIdx = m_terminalTabs.GetActive();
        if (tabIdx >= 0)
            m_terminalTabs.SetTabTitle(tabIdx, shell);
    }
}
