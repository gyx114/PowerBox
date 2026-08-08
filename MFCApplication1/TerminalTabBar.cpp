// TerminalTabBar.cpp: flat custom tab strip for terminal sessions
#include "pch.h"
#include "framework.h"
#include "TerminalTabBar.h"
#include "LocalizationManager.h"
#include "resource.h"
#include <algorithm>

BEGIN_MESSAGE_MAP(CTerminalTabBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

BOOL CTerminalTabBar::AttachToPlaceholder(UINT nID, CWnd* pParent)
{
    if (!SubclassDlgItem(nID, pParent))
        return FALSE;

    CClientDC dc(this);
    int height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
    if (height == 0)
        height = -14;
    LOGFONT lf{};
    lf.lfHeight = height;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    m_font.CreateFontIndirect(&lf);

    Layout();
    return TRUE;
}

void CTerminalTabBar::AddTab(const CString& title)
{
    m_titles.push_back(title);
    Layout();
    Invalidate(FALSE);
}

void CTerminalTabBar::RemoveTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_titles.size()))
        return;
    m_titles.erase(m_titles.begin() + index);
    if (m_active > index)
        m_active--;
    else if (m_active == index)
        m_active = static_cast<int>(m_titles.size()) - 1;
    Layout();
    Invalidate(FALSE);
}

void CTerminalTabBar::SetActive(int index)
{
    m_active = index;
    Invalidate(FALSE);
}

void CTerminalTabBar::SetTabTitle(int index, const CString& title)
{
    if (index < 0 || index >= static_cast<int>(m_titles.size()))
        return;
    m_titles[index] = title;
    Invalidate(FALSE);
}

void CTerminalTabBar::Layout()
{
    CRect rc;
    GetClientRect(&rc);
    m_rcTabs.clear();
    m_rcPlus = CRect(0, 0, 0, 0);
    m_rcOverflow = CRect(0, 0, 0, 0);

    int top = 2;
    int h = std::max(14, rc.Height() - 4);
    m_tabH = h;

    int avail = std::max(0, rc.Width() - 44);
    int canFit = 0;
    while (canFit < static_cast<int>(m_titles.size()) &&
        (canFit + 1) * m_tabW <= avail)
    {
        canFit++;
    }
    m_visibleCount = canFit;

    int x = 2;
    for (int i = 0; i < m_visibleCount; i++)
    {
        m_rcTabs.push_back(CRect(x, top, x + m_tabW, top + h));
        x += m_tabW;
    }
    m_rcPlus = CRect(x + 2, top, x + 2 + 18, top + h);
    if (m_visibleCount < static_cast<int>(m_titles.size()))
    {
        int ox = m_rcPlus.right + 2;
        m_rcOverflow = CRect(ox, top, ox + 18, top + h);
    }
}

int CTerminalTabBar::HitTest(CPoint pt) const
{
    if (m_rcPlus.PtInRect(pt))
        return -1;
    if (m_rcOverflow.PtInRect(pt))
        return -2;
    for (int i = 0; i < static_cast<int>(m_rcTabs.size()); i++)
    {
        if (m_rcTabs[i].PtInRect(pt))
            return i;
    }
    return -3;
}

void CTerminalTabBar::Notify(UINT msg, WPARAM wParam) const
{
    CWnd* pParent = GetParent();
    if (pParent && pParent->GetSafeHwnd())
        ::PostMessage(pParent->GetSafeHwnd(), msg, wParam, 0);
}

BOOL CTerminalTabBar::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CTerminalTabBar::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0)
        return;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOld = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(rc, RGB(16, 16, 16));
    CFont* pOldFont = memDC.SelectObject(&m_font);
    memDC.SetBkMode(TRANSPARENT);

    for (int i = 0; i < static_cast<int>(m_rcTabs.size()); i++)
    {
        const CRect& tab = m_rcTabs[i];
        bool active = (i == m_active);
        bool hover = (i == m_hover);
        COLORREF bg = active ? RGB(48, 48, 48) : (hover ? RGB(36, 36, 36) : RGB(24, 24, 24));
        memDC.FillSolidRect(tab, bg);

        RECT rcBorder = tab;
        ::DrawEdge(memDC.GetSafeHdc(), &rcBorder, EDGE_ETCHED, BF_RECT);

        memDC.SetTextColor(active ? RGB(255, 255, 255) : RGB(190, 190, 190));
        CRect textRect = tab;
        textRect.DeflateRect(4, 1, 16, 1);
        memDC.DrawText(m_titles[i], textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Close button
        CRect closeRect(tab.right - 14, tab.top + 3, tab.right - 3, tab.bottom - 3);
        if (active || hover)
        {
            memDC.SetTextColor(RGB(220, 220, 220));
            memDC.DrawText(_T("x"), closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    auto drawSmallButton = [&](const CRect& r, LPCTSTR text, bool hover) {
        RECT rc = r;
        memDC.FillSolidRect(r, hover ? RGB(40, 40, 40) : RGB(24, 24, 24));
        ::DrawEdge(memDC.GetSafeHdc(), &rc, EDGE_ETCHED, BF_RECT);
        memDC.SetTextColor(RGB(210, 210, 210));
        memDC.DrawText(text, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawSmallButton(m_rcPlus, _T("+"), m_hover == -1);
    if (!m_rcOverflow.IsRectEmpty())
        drawSmallButton(m_rcOverflow, _T("v"), m_hover == -2);

    memDC.SelectObject(pOldFont);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOld);
}

void CTerminalTabBar::HandleClick(CPoint point)
{
    int hit = HitTest(point);
    if (hit >= 0)
    {
        if (point.x >= m_rcTabs[hit].right - 16)
            Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(hit));
        else
            Notify(WM_TERM_TAB_SELECT, static_cast<WPARAM>(hit));
    }
    else if (hit == -1)
    {
        Notify(WM_TERM_TAB_NEW, 0);
    }
    else if (hit == -2)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        ShowOverflowMenu(pt);
    }
}

void CTerminalTabBar::HandleRightClick(CPoint point)
{
    int hit = HitTest(point);
    CPoint pt = point;
    ClientToScreen(&pt);
    ShowContextMenu(pt, hit >= 0 ? hit : -1);
}

void CTerminalTabBar::OnLButtonDown(UINT, CPoint point)
{
    HandleClick(point);
}

void CTerminalTabBar::OnRButtonUp(UINT, CPoint point)
{
    HandleRightClick(point);
}

void CTerminalTabBar::ShowContextMenu(CPoint screenPt, int hitIndex)
{
    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_TERMINAL_NEW, loc.GetString(_T("Terminal"), _T("New")));
    if (hitIndex >= 0)
        menu.AppendMenu(MF_STRING, ID_TERMINAL_CLOSE_TAB, loc.GetString(_T("Terminal"), _T("CloseTab")));

    UINT cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, this);
    if (cmd == ID_TERMINAL_NEW)
        Notify(WM_TERM_TAB_NEW, 0);
    else if (cmd == ID_TERMINAL_CLOSE_TAB && hitIndex >= 0)
        Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(hitIndex));
}

void CTerminalTabBar::ShowOverflowMenu(CPoint screenPt)
{
    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();

    const int baseId = 2000;
    for (int i = 0; i < static_cast<int>(m_titles.size()); i++)
    {
        UINT flags = MF_STRING;
        if (i == m_active)
            flags |= MF_CHECKED;
        menu.AppendMenu(flags, baseId + i, m_titles[i]);
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 2999, loc.GetString(_T("Terminal"), _T("New")));

    UINT cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, this);
    if (cmd == 2999)
    {
        Notify(WM_TERM_TAB_NEW, 0);
    }
    else if (cmd >= baseId && cmd < baseId + static_cast<UINT>(m_titles.size()))
    {
        Notify(WM_TERM_TAB_SELECT, cmd - baseId);
    }
}

void CTerminalTabBar::OnMouseMove(UINT, CPoint point)
{
    int hit = HitTest(point);
    if (hit != m_hover)
    {
        m_hover = hit;
        Invalidate(FALSE);
    }

    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
    _TrackMouseEvent(&tme);
}

void CTerminalTabBar::OnMouseLeave()
{
    m_hover = -3;
    Invalidate(FALSE);
}
