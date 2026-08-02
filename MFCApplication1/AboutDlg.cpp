#include "pch.h"
#include "framework.h"
#include "AboutDlg.h"
#include "resource.h"
#include "LocalizationManager.h"

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CAboutDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CLocalizationManager& loc = CLocalizationManager::GetInstance();

    SetWindowText(loc.GetString(_T("DlgCaption"), _T("AboutDlg")));

    // 静态文本控件共享 IDC_STATIC ID，按 Z 序遍历子窗口以区分
    CWnd* pWnd = GetWindow(GW_CHILD);
    if (pWnd) pWnd = pWnd->GetNextWindow(); // 跳过图标控件
    if (pWnd)
    {
        pWnd->SetWindowText(loc.GetString(_T("AboutDlg"), _T("Version")));
        pWnd = pWnd->GetNextWindow();
    }
    if (pWnd)
    {
        pWnd->SetWindowText(loc.GetString(_T("AboutDlg"), _T("Copyright")));
    }

    GetDlgItem(IDOK)->SetWindowText(loc.GetString(_T("AboutDlg"), _T("BtnOK")));

    return TRUE;
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()