#include "pch.h"
#include "framework.h"
#include "AutoClickerSpeedDlg.h"
#include "resource.h"
#include "LocalizationManager.h"

CAutoClickerSpeedDlg::CAutoClickerSpeedDlg(CAutoClicker* pClicker, CWnd* pParent /*= nullptr*/)
    : CDialogEx(IDD_CLICK_SPEED_DIALOG, pParent), m_pClicker(pClicker) {}

void CAutoClickerSpeedDlg::SetInterval(int ms)
{
    m_interval = ms;
    if (GetSafeHwnd())
    {
        SetDlgItemInt(IDC_EDIT_CLICK_SPEED, ms);
        CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
        if (pSlider) pSlider->SetPos(ms);
    }
}

int CAutoClickerSpeedDlg::GetInterval() const { return m_interval; }

BOOL CAutoClickerSpeedDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("ClickSpeedDlg")));
    CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
    if (pSlider)
    {
        pSlider->SetRange(10, 1000);
        pSlider->SetPos(m_interval);
        pSlider->SetTicFreq(100);
    }
    SetDlgItemInt(IDC_EDIT_CLICK_SPEED, m_interval);
    UpdateStatus();
    SetTimer(1, 200, nullptr);  // Refresh status every 200ms
    return TRUE;
}

BOOL CAutoClickerSpeedDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = CWnd::FromHandle(::GetFocus());
        if (pFocus)
        {
            TCHAR className[64] = {0};
            ::GetClassName(pFocus->GetSafeHwnd(), className, 64);
            if (_tcsstr(className, _T("Edit")) || _tcsstr(className, _T("edit")))
            {
                return TRUE;
            }
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CAutoClickerSpeedDlg::OnOK() { /* Do not close, only apply */ }

void CAutoClickerSpeedDlg::OnCancel() { OnClose(); }

void CAutoClickerSpeedDlg::OnClose()
{
    // Stop auto-clicker
    if (m_pClicker)
        m_pClicker->Stop();

    // Notify main window to clear pointer
    if (GetParent())
        ::PostMessage(GetParent()->m_hWnd, WM_SPEED_DLG_CLOSED, 0, 0);

    KillTimer(1);
    DestroyWindow();
}

void CAutoClickerSpeedDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
    if (pSlider && pScrollBar && pSlider->m_hWnd == pScrollBar->m_hWnd)
    {
        m_interval = pSlider->GetPos();
        SetDlgItemInt(IDC_EDIT_CLICK_SPEED, m_interval);
        if (m_pClicker) m_pClicker->SetInterval(m_interval);
    }
}

void CAutoClickerSpeedDlg::OnEnChangeClickSpeed()
{
    int val = GetDlgItemInt(IDC_EDIT_CLICK_SPEED);
    if (val >= 10 && val <= 1000)
    {
        m_interval = val;
        CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
        if (pSlider) pSlider->SetPos(val);
        if (m_pClicker) m_pClicker->SetInterval(val);
    }
}

void CAutoClickerSpeedDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
        UpdateStatus();
}

void CAutoClickerSpeedDlg::UpdateStatus()
{
    if (!m_pClicker) return;
    CWnd* pStatus = GetDlgItem(IDC_STATIC_CLICK_STATUS);
    if (!pStatus) return;

    if (!m_pClicker->IsRunning())
    {
        if (m_lastStatus != ClickStatus::Stopped)
        {
            m_lastStatus = ClickStatus::Stopped;
            pStatus->SetWindowText(CLocalizationManager::GetInstance().GetString(_T("AutoClicker"), _T("StatusStopped")));
            pStatus->Invalidate();
        }
        // Self-destroy: auto-clicker has stopped, close window
        KillTimer(1);
        if (GetParent())
            ::PostMessage(GetParent()->m_hWnd, WM_SPEED_DLG_CLOSED, 0, 0);
        DestroyWindow();
    }
    else if (m_pClicker->IsClicking())
    {
        if (m_lastStatus != ClickStatus::Clicking)
        {
            m_lastStatus = ClickStatus::Clicking;
            CString strMsg;
            strMsg.Format(CLocalizationManager::GetInstance().GetString(_T("AutoClicker"), _T("StatusClicking")), m_pClicker->GetKeyStop());
            pStatus->SetWindowText(strMsg);
            pStatus->Invalidate();
        }
    }
    else
    {
        if (m_lastStatus != ClickStatus::Waiting)
        {
            m_lastStatus = ClickStatus::Waiting;
            CString strMsg;
            strMsg.Format(CLocalizationManager::GetInstance().GetString(_T("AutoClicker"), _T("StatusWaiting")),
                m_pClicker->GetKeyStart(), m_pClicker->GetKeyStop());
            pStatus->SetWindowText(strMsg);
            pStatus->Invalidate();
        }
    }
}

HBRUSH CAutoClickerSpeedDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd && pWnd->GetDlgCtrlID() == IDC_STATIC_CLICK_STATUS)
    {
        pDC->SetBkMode(TRANSPARENT);
        switch (m_lastStatus)
        {
        case ClickStatus::Clicking:  // Clicking -> green
            pDC->SetTextColor(RGB(0, 170, 0));
            break;
        case ClickStatus::Waiting:   // Waiting for trigger -> orange
            pDC->SetTextColor(RGB(204, 136, 0));
            break;
        case ClickStatus::Stopped:   // Stopped -> gray
            pDC->SetTextColor(RGB(136, 136, 136));
            break;
        }
    }

    return hbr;
}

BEGIN_MESSAGE_MAP(CAutoClickerSpeedDlg, CDialogEx)
    ON_WM_HSCROLL()
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_EDIT_CLICK_SPEED, &CAutoClickerSpeedDlg::OnEnChangeClickSpeed)
END_MESSAGE_MAP()