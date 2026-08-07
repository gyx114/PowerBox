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
        m_terminalView.SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_terminalView.Invalidate(TRUE);
    }

    // Tab strip lives between the shell dropdown and the clear button.
    CRect rcCombo;
    CRect rcClear;
    m_terminalShell.GetWindowRect(&rcCombo);
    ScreenToClient(&rcCombo);
    m_terminalClear.GetWindowRect(&rcClear);
    ScreenToClient(&rcClear);
    CRect rcTabs(rcCombo.right + 6, rcCombo.top, rcClear.left - 6, rcCombo.bottom);
    if (rcTabs.Width() > 40 && m_terminalTabs.Create(
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT | WS_TABSTOP,
        rcTabs, this, IDC_TERMINAL_TABS))
    {
        m_terminalTabs.SetItemSize(CSize(70, 16));
        m_terminalTabs.InsertItem(0, m_strTerminalShell);
        m_terminalTabs.SetCurSel(0);
    }
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
        m_terminalTabs.SetCurSel(index);

    if (m_pActiveTerminal)
    {
        m_pActiveTerminal->SetWindowPos(&wndTop, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_pActiveTerminal->SetFocus();

        CString shell = m_pActiveTerminal->GetShellName();
        if (!shell.IsEmpty())
        {
            int idx = m_terminalShell.FindStringExact(-1, shell);
            if (idx != CB_ERR)
                m_terminalShell.SetCurSel(idx);
        }
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
        m_terminalTabs.InsertItem(idx, shellName);
    ActivateTerminalTab(idx);
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
        m_terminalTabs.InsertItem(idx, shellName);
    ActivateTerminalTab(idx);
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
        m_terminalTabs.DeleteItem(index);

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

void CMFCApplication1Dlg::OnTcnSelchangeTerminalTabs(NMHDR*, LRESULT* pResult)
{
    ActivateTerminalTab(m_terminalTabs.GetCurSel());
    *pResult = 0;
}

void CMFCApplication1Dlg::OnNMRclickTerminalTabs(NMHDR*, LRESULT* pResult)
{
    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_TERMINAL_NEW, loc.GetString(_T("Terminal"), _T("New")));
    menu.AppendMenu(MF_STRING, ID_TERMINAL_CLOSE_TAB, loc.GetString(_T("Terminal"), _T("CloseTab")));

    CPoint pt;
    ::GetCursorPos(&pt);
    UINT cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, this);
    if (cmd == ID_TERMINAL_NEW)
        AddTerminalTab(m_strTerminalShell);
    else if (cmd == ID_TERMINAL_CLOSE_TAB)
        CloseTerminalTab(m_terminalTabs.GetCurSel());

    *pResult = 0;
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
        int tabIdx = m_terminalTabs.GetCurSel();
        if (tabIdx != CB_ERR)
        {
            TCITEM item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<LPTSTR>(shell.GetString());
            m_terminalTabs.SetItem(tabIdx, &item);
        }
    }
}
