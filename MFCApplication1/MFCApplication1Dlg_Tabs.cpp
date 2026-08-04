#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "LocalizationManager.h"
#include "resource.h"
#include "Utils.h"
#include <algorithm>

// ========== Tab page initialization helpers ==========

void CMFCApplication1Dlg::InitTabControl()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (!pTab) return;

    auto& loc = CLocalizationManager::GetInstance();
    pTab->InsertItem(0, loc.GetString(_T("MainDlg"), _T("Tab1")));
    pTab->InsertItem(1, loc.GetString(_T("MainDlg"), _T("Tab2")));
    pTab->InsertItem(2, loc.GetString(_T("MainDlg"), _T("Tab3")));
    pTab->InsertItem(3, loc.GetString(_T("MainDlg"), _T("Tab4")));
    pTab->InsertItem(4, loc.GetString(_T("MainDlg"), _T("Tab5")));
    pTab->InsertItem(5, loc.GetString(_T("MainDlg"), _T("Tab6")));
}

void CMFCApplication1Dlg::InitProcessTab()
{
    CListCtrl* pList1 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST1));
    if (!pList1) return;

    auto& loc = CLocalizationManager::GetInstance();
    pList1->ModifyStyle(0, LVS_REPORT);
    pList1->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
    pList1->InsertColumn(0, loc.GetString(_T("ProcessTab"), _T("ColName")), LVCFMT_LEFT, 200);
	pList1->InsertColumn(1, loc.GetString(_T("ProcessTab"), _T("ColCPU")), LVCFMT_RIGHT, 80);
	pList1->InsertColumn(2, loc.GetString(_T("ProcessTab"), _T("ColPath")), LVCFMT_LEFT, 480);
	pList1->InsertColumn(3, loc.GetString(_T("ProcessTab"), _T("ColMemory")), LVCFMT_RIGHT, 100);
	pList1->InsertColumn(4, loc.GetString(_T("ProcessTab"), _T("ColPID")), LVCFMT_LEFT, 80);

    // Force header redraw
    if (pList1->GetHeaderCtrl())
        pList1->GetHeaderCtrl()->Invalidate(TRUE);
}

void CMFCApplication1Dlg::InitStartupTab()
{
    CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
    if (!pList2) return;

    auto& loc = CLocalizationManager::GetInstance();
    pList2->ModifyStyle(0, LVS_REPORT);
    pList2->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
    pList2->InsertColumn(0, loc.GetString(_T("StartupTab"), _T("ColName")), LVCFMT_LEFT, 200);
    pList2->InsertColumn(1, loc.GetString(_T("StartupTab"), _T("ColCmd")), LVCFMT_LEFT, 840);
}

void CMFCApplication1Dlg::InitClipboardTab()
{
    CListCtrl* pList3 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST3));
    if (!pList3) return;

    auto& loc = CLocalizationManager::GetInstance();
    pList3->ModifyStyle(0, LVS_REPORT);
    pList3->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
    pList3->InsertColumn(0, loc.GetString(_T("ClipboardTab"), _T("ColText")), LVCFMT_LEFT, 780);
}

void CMFCApplication1Dlg::InitWindowTab()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));
    if (pList5)
    {
        pList5->ModifyStyle(0, LVS_REPORT);
        pList5->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        pList5->InsertColumn(0, loc.GetString(_T("WindowTab"), _T("ColField")), LVCFMT_LEFT, 84);
        pList5->InsertColumn(1, loc.GetString(_T("WindowTab"), _T("ColValue")), LVCFMT_LEFT, 980);
    }

    // Initialize transparency slider (resource control)
    CSliderCtrl* pSlider2 = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER2));
    if (pSlider2)
    {
        pSlider2->SetRange(10, 255);
        pSlider2->SetPos(255);
        pSlider2->SetTicFreq(25);
    }
    SetDlgItemText(IDC_STATIC18, loc.GetString(_T("WindowTab"), _T("TranparencyLabel")));

    // Initialize topmost window list
    CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
    if (pList6)
    {
        pList6->ModifyStyle(0, LVS_REPORT);
        pList6->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        pList6->InsertColumn(0, loc.GetString(_T("WindowTab"), _T("ColType")), LVCFMT_LEFT, 80);
        pList6->InsertColumn(1, loc.GetString(_T("WindowTab"), _T("ColTitle")), LVCFMT_LEFT, 280);
    }

    // Initialize history window list
    CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
    if (pList7)
    {
        pList7->ModifyStyle(0, LVS_REPORT);
        pList7->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        pList7->InsertColumn(0, loc.GetString(_T("WindowTab"), _T("ColIndex")), LVCFMT_LEFT, 60);
        pList7->InsertColumn(1, loc.GetString(_T("WindowTab"), _T("ColTitle")), LVCFMT_LEFT, 280);
    }
}

void CMFCApplication1Dlg::InitFileTab()
{
    // File management tab controls are defined in resource editor, placeholder here
    // Actual visibility managed by UpdateTabVisibility
}

void CMFCApplication1Dlg::InitGitTab()
{
    CListCtrl* pList4 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST4));
    if (!pList4) return;

    auto& loc = CLocalizationManager::GetInstance();
    pList4->ModifyStyle(0, LVS_REPORT);
    pList4->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
    pList4->InsertColumn(0, loc.GetString(_T("GitTab"), _T("ColDesc")), LVCFMT_LEFT, 190);
    pList4->InsertColumn(1, loc.GetString(_T("GitTab"), _T("ColCmd")), LVCFMT_LEFT, 400);

    // If config.ini does not exist, create default Git commands
    const TCHAR* section = _T("GitCommands");
    TCHAR exePath[MAX_PATH] = {0};
    GetModuleFileName(NULL, exePath, MAX_PATH);
    CString exeDir = exePath;
    int p = exeDir.ReverseFind(_T('\\'));
    CString configPath;
    if (p != -1) configPath.Format(_T("%s\\config.ini"), exeDir.Left(p));
    else configPath = _T("config.ini");

    if (GetFileAttributes(configPath) == INVALID_FILE_ATTRIBUTES)
    {
        // Write translation keys instead of translated text, so descriptions
        // always match the current language when read.
        WritePrivateProfileString(section, _T("Cmd1"),  _T("Cmd1Desc|git init"), configPath);
        WritePrivateProfileString(section, _T("Cmd2"),  _T("Cmd2Desc|git add ."), configPath);
        WritePrivateProfileString(section, _T("Cmd3"),  _T("Cmd3Desc|git commit -m \"first commit\""), configPath);
        WritePrivateProfileString(section, _T("Cmd4"),  _T("Cmd4Desc|git remote add origin <url>"), configPath);
        WritePrivateProfileString(section, _T("Cmd5"),  _T("Cmd5Desc|git branch -M main"), configPath);
        WritePrivateProfileString(section, _T("Cmd6"),  _T("Cmd6Desc|git push -u origin main"), configPath);
        WritePrivateProfileString(section, _T("Cmd7"),  _T("Cmd7Desc|git pull"), configPath);
        WritePrivateProfileString(section, _T("Cmd8"),  _T("Cmd8Desc|git clone <url>"), configPath);
        WritePrivateProfileString(section, _T("Cmd9"),  _T("Cmd9Desc|git status"), configPath);
        WritePrivateProfileString(section, _T("Cmd10"), _T("Cmd10Desc|git add ."), configPath);
        WritePrivateProfileString(section, _T("Cmd11"), _T("Cmd11Desc|git commit -m \"message\""), configPath);
        WritePrivateProfileString(section, _T("Cmd12"), _T("Cmd12Desc|git push"), configPath);
        WritePrivateProfileString(section, _T("Cmd13"), _T("Cmd13Desc|git branch -a"), configPath);
        WritePrivateProfileString(section, _T("Cmd14"), _T("Cmd14Desc|git checkout -b <branch>"), configPath);
        WritePrivateProfileString(section, _T("Cmd15"), _T("Cmd15Desc|git checkout <branch>"), configPath);
        WritePrivateProfileString(section, _T("Cmd16"), _T("Cmd16Desc|git merge <branch>"), configPath);
        WritePrivateProfileString(section, _T("Cmd17"), _T("Cmd17Desc|git branch -d <branch>"), configPath);
        WritePrivateProfileString(section, _T("Cmd18"), _T("Cmd18Desc|git log --oneline"), configPath);
        WritePrivateProfileString(section, _T("Cmd19"), _T("Cmd19Desc|git restore <file>"), configPath);
        WritePrivateProfileString(section, _T("Cmd20"), _T("Cmd20Desc|git restore --staged <file>"), configPath);
    }

    // Load command list from INI
    pList4->DeleteAllItems();
    for (int i = 1; i <= 99; ++i)
    {
        CString key; key.Format(_T("Cmd%d"), i);
        TCHAR buf[1024] = {0};
        GetPrivateProfileString(section, key, _T(""), buf, 1024, configPath);
        CString val = buf;
        if (val.IsEmpty()) break;
        int sep = val.Find(_T('|'));
        CString desc, cmd;
        if (sep != -1) { desc = val.Left(sep); cmd = val.Mid(sep + 1); }
        else { desc = val; cmd = _T(""); }

        // If desc is a translation key (e.g. "Cmd1Desc"), look up the current language text
        CString translated = loc.GetString(_T("GitCommands"), desc, _T(""));
        if (!translated.IsEmpty())
            desc = translated;

        int idx = pList4->InsertItem(i - 1, desc);
        pList4->SetItemText(idx, 1, cmd);
    }
}

// ========== Unified tab visibility management ==========

void CMFCApplication1Dlg::UpdateTabVisibility(int nTab)
{
    // List controls
    CListCtrl* pList1 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST1));
    CListCtrl* pList2 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
    CListCtrl* pList3 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST3));
    CListCtrl* pList4 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST4));
    CListCtrl* pList5 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST5));

    if (pList1) {
        pList1->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
        if (nTab == 0) {
            pList1->GetHeaderCtrl()->Invalidate(TRUE);
            pList1->RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
            RefreshProcessList();
        }
    }
    // Process search filter controls
    CWnd* pEditFilter = GetDlgItem(IDC_EDIT_PROCESS_FILTER);
    if (pEditFilter)
        pEditFilter->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    CWnd* pBtnRegex = GetDlgItem(IDC_CHECK_PROCESS_REGEX);
    if (pBtnRegex)
        pBtnRegex->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    CWnd* pBtnRegexHelp = GetDlgItem(IDC_BTN_PROCESS_REGEX_HELP);
    if (pBtnRegexHelp)
        pBtnRegexHelp->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    CWnd* pBtnAiScan = GetDlgItem(IDC_BTN_PROCESS_AI_SCAN);
    if (pBtnAiScan)
        pBtnAiScan->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    if (pList2) pList2->ShowWindow(nTab == 1 ? SW_SHOW : SW_HIDE);
    if (pList3) pList3->ShowWindow(nTab == 2 ? SW_SHOW : SW_HIDE);
    if (pList4) pList4->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
    if (pList5)
    {
        pList5->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);
        if (nTab == 3)
        {
            ::SetWindowPos(pList5->GetSafeHwnd(), HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
            pList5->BringWindowToTop();
        }
    }

    // Window handling tab buttons
    CWnd* pBtn19 = GetDlgItem(IDC_BUTTON19);
    CWnd* pStatic12 = GetDlgItem(IDC_STATIC12);
    if (pBtn19) pBtn19->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);
    if (pStatic12) pStatic12->ShowWindow(nTab == 3 ? SW_SHOW : SW_HIDE);

    // Git tools buttons
    CWnd* pBtn30 = GetDlgItem(IDC_BUTTON30);
    CWnd* pBtn31 = GetDlgItem(IDC_BUTTON31);
    CWnd* pBtn32 = GetDlgItem(IDC_BUTTON32);
    CWnd* pBtnCmdWin = GetDlgItem(IDC_BTN_GIT_CMD_WINDOW);
    if (pBtn30) pBtn30->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
    if (pBtn31) pBtn31->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
    if (pBtnCmdWin) pBtnCmdWin->ShowWindow(nTab == 5 ? SW_SHOW : SW_HIDE);
    if (pBtn32)
    {
        if (nTab == 5)
        {
            pBtn32->ShowWindow(SW_SHOW);
            pBtn32->EnableWindow(TRUE);
            pBtn32->BringWindowToTop();
            pBtn32->SetWindowPos(&wndTop, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);
        }
        else
        {
            pBtn32->ShowWindow(SW_HIDE);
        }
    }

    // File management controls (tab 4 only)
    BOOL showFile = (nTab == 4);
    BOOL showGit = (nTab == 5);
    CWnd* pStaticPath = GetDlgItem(IDC_STATIC_PATH);
    CWnd* pEdit4 = GetDlgItem(IDC_EDIT4);
    CWnd* pBtn3 = GetDlgItem(IDC_BUTTON3);
    CWnd* pStatic7 = GetDlgItem(IDC_STATIC7);
    CWnd* pStatic13 = GetDlgItem(IDC_STATIC13);
    CWnd* pEdit7 = GetDlgItem(IDC_EDIT7);
    CWnd* pEdit8 = GetDlgItem(IDC_EDIT8);
    CWnd* pBtn23 = GetDlgItem(IDC_BUTTON23);
    CWnd* pBtn24 = GetDlgItem(IDC_BUTTON24);
    CWnd* pStatic14 = GetDlgItem(IDC_STATIC14);
    CWnd* pBtn25 = GetDlgItem(IDC_BUTTON25);
    CWnd* pBtn26 = GetDlgItem(IDC_BUTTON26);
    CWnd* pBrowse = GetDlgItem(IDC_MFCEDITBROWSE2);

    if (pStaticPath) pStaticPath->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pEdit4) pEdit4->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBtn3) pBtn3->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pStatic7) pStatic7->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pStatic13) pStatic13->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pEdit7) pEdit7->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pEdit8) pEdit8->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBtn23) pBtn23->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBtn24) pBtn24->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pStatic14) pStatic14->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBtn25) pBtn25->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBtn26) pBtn26->ShowWindow(showFile ? SW_SHOW : SW_HIDE);
    if (pBrowse) pBrowse->ShowWindow(showFile ? SW_SHOW : SW_HIDE);

    if (showFile)
    {
        CString stDisplay = m_strDroppedFilePath.IsEmpty() ? CLocalizationManager::GetInstance().GetString(_T("FileTab"), _T("DropHint")) : m_strDroppedFilePath;
        SetDlgItemText(IDC_STATIC_PATH, stDisplay);
    }

    // Git tab independent path box, repo info label, and locate button (tab 5 only)
    CWnd* pGitPath = GetDlgItem(IDC_STATIC_GIT_PATH);
    CWnd* pGitRepoInfo = GetDlgItem(IDC_STATIC_GIT_REPO_INFO);
    CWnd* pGitLocate = GetDlgItem(IDC_BTN_GIT_LOCATE);
    if (pGitPath) pGitPath->ShowWindow(showGit ? SW_SHOW : SW_HIDE);
    if (pGitRepoInfo) pGitRepoInfo->ShowWindow(showGit ? SW_SHOW : SW_HIDE);
    if (pGitLocate) pGitLocate->ShowWindow(showGit ? SW_SHOW : SW_HIDE);

    // Window handling tab: populate window info
    if (nTab == 3 && pList5)
    {
        LoadWindowDetailToList5(m_hSelectedWnd);
    }

    // New control visibility for window handling tab
    BOOL showWindowTab = (nTab == 3);
    CWnd* pSlider2 = GetDlgItem(IDC_SLIDER2);
    CWnd* pStatic18 = GetDlgItem(IDC_STATIC18);
    CWnd* pBtn15 = GetDlgItem(IDC_BUTTON15);
    CWnd* pBtn16 = GetDlgItem(IDC_BUTTON16);
    if (pSlider2) pSlider2->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
    if (pStatic18) pStatic18->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
    if (pBtn15) pBtn15->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
    if (pBtn16) pBtn16->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);

    // When switching to window management tab, sync toolbox topmost checkbox state
    if (showWindowTab)
    {
        CButton* pCheck3 = static_cast<CButton*>(GetDlgItem(IDC_CHECK3));
        if (pCheck3)
        {
            auto it = std::find(m_topmostWnds.begin(), m_topmostWnds.end(), m_hWnd);
            pCheck3->SetCheck(it != m_topmostWnds.end() ? BST_CHECKED : BST_UNCHECKED);
        }
    }

    // Topmost window list (LIST6)
    CListCtrl* pList6 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST6));
    if (pList6)
    {
        pList6->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
        if (showWindowTab)
        {
            pList6->DeleteAllItems();
            int row = 0;
            for (size_t i = 0; i < m_topmostWnds.size(); ++i)
            {
                HWND h = m_topmostWnds[i];
                if (!IsValidWindow(h)) continue;

                TCHAR title[256] = {0};
                ::GetWindowText(h, title, _countof(title));

                CString label;
                label.Format(CLocalizationManager::GetInstance().GetString(_T("WindowTab"), _T("TopmostLabel")), i + 1);
                int idx = pList6->InsertItem(row, label);
                pList6->SetItemText(idx, 1, CString(title));
                pList6->SetItemData(idx, static_cast<DWORD_PTR>(i));
                ++row;
            }
        }
    }

    // History window list (LIST7)
    CListCtrl* pList7 = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST7));
    if (pList7)
    {
        pList7->ShowWindow(showWindowTab ? SW_SHOW : SW_HIDE);
        if (showWindowTab)
        {
            pList7->DeleteAllItems();
            int row = 0;
            for (size_t i = 0; i < m_historyWnds.size(); ++i)
            {
                HWND h = m_historyWnds[i];
                if (!IsValidWindow(h)) continue;

                TCHAR title[256] = {0};
                ::GetWindowText(h, title, _countof(title));

                CString label;
                label.Format(_T("%zu"), row + 1);
                int idx = pList7->InsertItem(row, label);
                pList7->SetItemText(idx, 1, CString(title));
                pList7->SetItemData(idx, static_cast<DWORD_PTR>(i));  // Store real index
                ++row;
            }
        }
    }
}

// ========== Menu bar view switching ==========

void CMFCApplication1Dlg::OnViewProcess()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(0); UpdateTabVisibility(0); }
}

void CMFCApplication1Dlg::OnViewStartup()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(1); UpdateTabVisibility(1); RefreshStartupList(); }
}

void CMFCApplication1Dlg::OnViewClipboard()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(2); UpdateTabVisibility(2); }
}

void CMFCApplication1Dlg::OnViewWindow()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(3); UpdateTabVisibility(3); }
}

void CMFCApplication1Dlg::OnViewFile()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(4); UpdateTabVisibility(4); }
}

void CMFCApplication1Dlg::OnViewGit()
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab) { pTab->SetCurSel(5); UpdateTabVisibility(5); }
}

void CMFCApplication1Dlg::OnTcnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult)
{
    CTabCtrl* pTab = static_cast<CTabCtrl*>(GetDlgItem(IDC_TAB1));
    if (pTab)
    {
        int nSel = pTab->GetCurSel();
        UpdateTabVisibility(nSel);
        if (nSel == 1)
            RefreshStartupList();
        if (nSel == 5)
            UpdateGitRepoInfo();
    }
    *pResult = 0;
}