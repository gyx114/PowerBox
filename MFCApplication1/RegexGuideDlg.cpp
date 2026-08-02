#include "pch.h"
#include "framework.h"
#include "RegexGuideDlg.h"
#include "resource.h"
#include "LocalizationManager.h"

CRegexGuideDlg::CRegexGuideDlg(CWnd* pParent) : CDialogEx(IDD_REGEX_GUIDE_DLG, pParent)
{
}

void CRegexGuideDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CRegexGuideDlg, CDialogEx)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CRegexGuideDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("RegexGuideDlg")));

    CString guide = CLocalizationManager::GetInstance().GetString(_T("RegexGuide"), _T("GuideContent"));

    SetDlgItemText(IDC_STATIC, guide);
    return TRUE;
}

void CRegexGuideDlg::OnCancel()
{
    PostMessage(WM_CLOSE);
}

void CRegexGuideDlg::OnClose()
{
    DestroyWindow();
}

void CRegexGuideDlg::PostNcDestroy()
{
    delete this;
}