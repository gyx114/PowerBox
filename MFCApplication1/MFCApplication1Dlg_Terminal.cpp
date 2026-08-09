// MFCApplication1Dlg_Terminal.cpp: ConPTY terminal integration in the AI tab
#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "LocalizationManager.h"
#include <algorithm>

void CMFCApplication1Dlg::InitTerminal()
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

    // Tab strip is defined statically in the resource file so it can be
    // fine-tuned in the resource editor.
    if (m_terminalTabs.AttachToPlaceholder(IDC_TERMINAL_TABS, this))
    {
        m_terminalTabs.AddTab(m_strTerminalShell);
        m_terminalTabs.SetActive(0);
        m_terminalTabs.SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_terminalTabs.Invalidate(TRUE);
    }

    CaptureAiLayout();
    LayoutAiTabControls();
}

void CMFCApplication1Dlg::LayoutAiTabControls()
{
    if (!::IsWindow(m_hWnd))
        return;

    if (m_rcAiTermViewInit.IsRectEmpty())
        return;

    int gap = 4;
    int left = m_rcAiBrowserInit.left;
    int right = m_rcAiBrowserInit.right;

    // ===== Terminal view: bottom anchored to original position =====
    int termViewBottom = m_rcAiTermViewInit.bottom;
    int maxH = std::max(60, termViewBottom -
        static_cast<int>(m_rcAiBrowserInit.top) - 220);
    int termH = std::clamp(m_terminalHeight, 60, maxH);
    int termViewTop = termViewBottom - termH;

    // ===== shiftY: middle block shifts as a unit =====
    // Only browser height and terminal view height change.
    // All controls between them shift by the same amount,
    // preserving their original relative positions.
    int shiftY = termViewTop - m_rcAiTermViewInit.top;

    // Clamp: ensure browser doesn't get too small
    int minBrowserH = 60;
    int minShiftY = minBrowserH - m_rcAiBrowserInit.Height();
    if (shiftY < minShiftY)
    {
        shiftY = minShiftY;
        termViewTop = m_rcAiTermViewInit.top + shiftY;
        termH = termViewBottom - termViewTop;
    }

    // Browser: top static, bottom shifts with middle block
    CRect rcBrowser(left, m_rcAiBrowserInit.top, right,
        m_rcAiBrowserInit.bottom + shiftY);

    // ===== Batch all window moves with DeferWindowPos (flicker-free) =====
    HDWP hdwp = ::BeginDeferWindowPos(12);
    if (!hdwp) return;

    CWnd* pBrowser = GetDlgItem(IDC_AI_BROWSER);
    if (pBrowser)
    {
        hdwp = ::DeferWindowPos(hdwp, pBrowser->m_hWnd, nullptr,
            rcBrowser.left, rcBrowser.top,
            rcBrowser.Width(), rcBrowser.Height(), SWP_NOZORDER);
        m_aiBrowserRect = rcBrowser;
    }

    // Input: shifts by shiftY
    CWnd* pInput = GetDlgItem(IDC_EDIT_AI_INPUT);
    if (pInput)
        hdwp = ::DeferWindowPos(hdwp, pInput->m_hWnd, nullptr,
            left, m_rcAiInputInit.top + shiftY,
            right - left, m_rcAiInputInit.Height(), SWP_NOZORDER);

    // Buttons (including standalone): shift by shiftY, preserve original X/width/height
    UINT btnIds[5] = {
        IDC_BUTTON_AI_SEND, IDC_BUTTON_AI_STOP,
        IDC_BUTTON_AI_CLEAR, IDC_BUTTON_AI_HISTORY,
        IDC_BTN_AI_STANDALONE
    };
    for (int i = 0; i < 5; i++)
    {
        CWnd* pBtn = GetDlgItem(btnIds[i]);
        if (pBtn)
        {
            int x, y, w, h;
            if (i < 4)
            {
                x = m_rcAiButtonsInit[i].left;
                y = m_rcAiButtonsInit[i].top;
                w = m_rcAiButtonsInit[i].Width();
                h = m_rcAiButtonsInit[i].Height();
            }
            else
            {
                x = m_rcAiStandaloneInit.left;
                y = m_rcAiStandaloneInit.top;
                w = m_rcAiStandaloneInit.Width();
                h = m_rcAiStandaloneInit.Height();
            }
            hdwp = ::DeferWindowPos(hdwp, pBtn->m_hWnd, nullptr,
                x, y + shiftY, w, h, SWP_NOZORDER);
        }
    }

    // Splitter: shifts by shiftY
    CWnd* pSplit = GetDlgItem(IDC_TERMINAL_SPLITTER);
    if (pSplit)
        hdwp = ::DeferWindowPos(hdwp, pSplit->m_hWnd, nullptr,
            m_rcAiSplitterInit.left, m_rcAiSplitterInit.top + shiftY,
            m_rcAiSplitterInit.Width(), 6, SWP_NOZORDER);

    // Tabs: shift by shiftY
    if (m_terminalTabs.m_hWnd)
        hdwp = ::DeferWindowPos(hdwp, m_terminalTabs.m_hWnd, nullptr,
            m_rcAiTermTabsInit.left, m_rcAiTermTabsInit.top + shiftY,
            right - m_rcAiTermTabsInit.left,
            m_rcAiTermTabsInit.Height(), SWP_NOZORDER);

    // Label: shifts by shiftY, original X/width/height
    CWnd* pTermLabel = GetDlgItem(IDC_TERMINAL_LABEL);
    if (pTermLabel)
        hdwp = ::DeferWindowPos(hdwp, pTermLabel->m_hWnd, nullptr,
            m_rcAiTermLabelInit.left, m_rcAiTermLabelInit.top + shiftY,
            m_rcAiTermLabelInit.Width(), m_rcAiTermLabelInit.Height(), SWP_NOZORDER);

    // Shell: shifts by shiftY, original X/width/height
    CWnd* pShell = GetDlgItem(IDC_TERMINAL_SHELL);
    if (pShell)
        hdwp = ::DeferWindowPos(hdwp, pShell->m_hWnd, nullptr,
            m_rcAiTermShellInit.left, m_rcAiTermShellInit.top + shiftY,
            m_rcAiTermShellInit.Width(), m_rcAiTermShellInit.Height(), SWP_NOZORDER);

    // Clear: shifts by shiftY, original X/width/height
    CWnd* pClear = GetDlgItem(IDC_BTN_TERMINAL_CLEAR);
    if (pClear)
        hdwp = ::DeferWindowPos(hdwp, pClear->m_hWnd, nullptr,
            m_rcAiTermClearInit.left, m_rcAiTermClearInit.top + shiftY,
            m_rcAiTermClearInit.Width(), m_rcAiTermClearInit.Height(), SWP_NOZORDER);

    // Terminal views: fill remaining space
    CRect rcView(left, termViewTop, right, termViewBottom);
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

    // Give the interactive AI-tab controls top z-order so nothing covers them.
    UINT topIds[] = {
        IDC_TERMINAL_SPLITTER,
        IDC_TERMINAL_LABEL,
        IDC_TERMINAL_SHELL,
        IDC_BTN_TERMINAL_CLEAR,
        IDC_COMBO_AI_VENDOR,
        IDC_EDIT_AI_INPUT,
        IDC_BUTTON_AI_SEND,
        IDC_BUTTON_AI_STOP,
        IDC_BUTTON_AI_CLEAR,
        IDC_BUTTON_AI_HISTORY,
        IDC_BTN_AI_STANDALONE
    };
    for (UINT id : topIds)
    {
        CWnd* w = GetDlgItem(id);
        if (w)
        {
            w->SetWindowPos(&wndTop, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }
    if (m_pActiveTerminal && m_pActiveTerminal->m_hWnd)
    {
        m_pActiveTerminal->SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    if (m_terminalTabs.m_hWnd)
    {
        m_terminalTabs.SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
}

void CMFCApplication1Dlg::CaptureAiLayout()
{
    auto capture = [&](UINT id, CRect& out) {
        CWnd* w = GetDlgItem(id);
        if (w)
        {
            w->GetWindowRect(&out);
            ScreenToClient(&out);
        }
    };

    capture(IDC_COMBO_AI_VENDOR, m_rcAiVendorInit);
    capture(IDC_AI_BROWSER, m_rcAiBrowserInit);
    capture(IDC_EDIT_AI_INPUT, m_rcAiInputInit);
    capture(IDC_TERMINAL_SPLITTER, m_rcAiSplitterInit);
    capture(IDC_TERMINAL_LABEL, m_rcAiTermLabelInit);
    capture(IDC_TERMINAL_TABS, m_rcAiTermTabsInit);
    capture(IDC_TERMINAL_SHELL, m_rcAiTermShellInit);
    capture(IDC_BTN_TERMINAL_CLEAR, m_rcAiTermClearInit);
    capture(IDC_TERMINAL_VIEW, m_rcAiTermViewInit);
    capture(IDC_BUTTON_AI_SEND, m_rcAiButtonsInit[0]);
    capture(IDC_BUTTON_AI_STOP, m_rcAiButtonsInit[1]);
    capture(IDC_BUTTON_AI_CLEAR, m_rcAiButtonsInit[2]);
    capture(IDC_BUTTON_AI_HISTORY, m_rcAiButtonsInit[3]);
    capture(IDC_BTN_AI_STANDALONE, m_rcAiStandaloneInit);
}

CTerminalView* CMFCApplication1Dlg::ActiveTerminal()
{
    return m_pActiveTerminal ? m_pActiveTerminal : &m_terminalView;
}

void CMFCApplication1Dlg::ActivateTerminalTab(int index)
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

void CMFCApplication1Dlg::AddTerminalTab(const CString& shellName)
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
    LayoutAiTabControls();
}

void CMFCApplication1Dlg::AddTerminalTabWithCommand(const CString& shellName,
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
    LayoutAiTabControls();
}

void CMFCApplication1Dlg::CloseTerminalTab(int index)
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

LRESULT CMFCApplication1Dlg::OnTermTabSelect(WPARAM wParam, LPARAM)
{
    ActivateTerminalTab(static_cast<int>(wParam));
    return 0;
}

LRESULT CMFCApplication1Dlg::OnTermTabClose(WPARAM wParam, LPARAM)
{
    CloseTerminalTab(static_cast<int>(wParam));
    return 0;
}

LRESULT CMFCApplication1Dlg::OnTermTabNew(WPARAM, LPARAM)
{
    AddTerminalTab(m_strTerminalShell);
    return 0;
}

void CMFCApplication1Dlg::OnBnClickedTerminalClear()
{
    CTerminalView* active = ActiveTerminal();
    if (!active)
        return;
    active->ClearScreen();
    active->SetFocus();
}

void CMFCApplication1Dlg::OnCbnSelchangeTerminalShell()
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
