// TerminalTabBar.cpp: adaptive session switcher for terminal views
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
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_MBUTTONUP()
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
    m_titles.push_back(MakeUniqueTitle(title));
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
    Layout();
    Invalidate(FALSE);
}

void CTerminalTabBar::SetTabTitle(int index, const CString& title)
{
    if (index < 0 || index >= static_cast<int>(m_titles.size()))
        return;
    m_titles[index] = MakeUniqueTitle(title, index);
    Invalidate(FALSE);
}

CString CTerminalTabBar::MakeUniqueTitle(const CString& base, int ignoreIndex) const
{
    CString prefix = base;
    if (prefix.IsEmpty())
        prefix = _T("Terminal");
    CString display = prefix;

    int suffix = 1;
    auto exists = [&](const CString& candidate) {
        for (int i = 0; i < static_cast<int>(m_titles.size()); i++)
        {
            if (i != ignoreIndex && m_titles[i] == candidate)
                return true;
        }
        return false;
    };

    while (exists(display))
    {
        suffix++;
        display.Format(_T("%s %d"), prefix, suffix);
    }
    return display;
}

void CTerminalTabBar::Relayout()
{
    Layout();
    Invalidate(FALSE);
}

void CTerminalTabBar::Layout()
{
    CRect rc;
    GetClientRect(&rc);
    m_rcTabs.clear();
    m_rcSwitcher = CRect(0, 0, 0, 0);
    m_rcClose = CRect(0, 0, 0, 0);
    m_rcPlus = CRect(0, 0, 0, 0);
    m_rcOverflow = CRect(0, 0, 0, 0);

    int top = 2;
    int h = std::max(12, rc.Height() - 4);
    m_tabH = h;

    // The main window's AI pane should still use a real horizontal tab strip.
    // Compact mode is only an extreme-narrow fallback for very small panels.
    CClientDC dc(this);
    int dpiX = dc.GetDeviceCaps(LOGPIXELSX);
    if (dpiX <= 0)
        dpiX = 96;
    m_dpiX = dpiX;
    m_compact = MulDiv(rc.Width(), 96, dpiX) < 220;
    int minTabW = MulDiv(m_minTabW, dpiX, 96);
    int maxTabW = MulDiv(m_maxTabW, dpiX, 96);
    minTabW = std::max(80, minTabW);
    maxTabW = std::max(minTabW, maxTabW);

    if (m_compact)
    {
        m_firstVisible = 0;
        int btnW = std::max(20, MulDiv(22, dpiX, 96));
        m_rcPlus = CRect(rc.right - btnW - 2, top, rc.right - 2, top + h);
        if (static_cast<int>(m_titles.size()) > 1)
            m_rcClose = CRect(m_rcPlus.left - btnW - 2, top, m_rcPlus.left - 2, top + h);
        int switcherRight = m_rcClose.IsRectEmpty() ? m_rcPlus.left : m_rcClose.left;
        m_rcSwitcher = CRect(2, top, switcherRight - 2, top + h);
        m_visibleCount = 0;
        return;
    }

    int btnW = std::max(20, MulDiv(22, dpiX, 96));
    int count = static_cast<int>(m_titles.size());
    int avail = std::max(0, rc.Width() - btnW - 6);
    int overflowW = 0;
    m_visibleCount = count;

    if (count > 0 && avail / minTabW < count)
    {
        overflowW = btnW + 2;
        avail = std::max(0, avail - overflowW);
        m_visibleCount = std::min(count, avail / minTabW);
        m_visibleCount = std::max(1, m_visibleCount);
    }

    int tabW = minTabW;
    if (m_visibleCount > 0)
    {
        tabW = std::clamp(avail / m_visibleCount, minTabW, maxTabW);
    }
    m_tabW = tabW;

    if (m_active >= 0 && m_active < count && m_visibleCount > 0)
    {
        int start = m_active - (m_visibleCount - 1) / 2;
        start = std::clamp(start, 0, std::max(0, count - m_visibleCount));
        m_firstVisible = start;
    }
    else
    {
        m_firstVisible = 0;
    }

    int x = 2;
    for (int i = 0; i < m_visibleCount; i++)
    {
        m_rcTabs.push_back(CRect(x, top, x + m_tabW, top + h));
        x += m_tabW;
    }

    int buttonX = x + 2;
    m_rcPlus = CRect(buttonX, top, buttonX + btnW, top + h);
    if (m_visibleCount < count ||
        (m_visibleCount > 0 && m_firstVisible > 0))
    {
        m_rcOverflow = CRect(m_rcPlus.right + 2, top,
            m_rcPlus.right + 2 + btnW, top + h);
    }
}

int CTerminalTabBar::HitTest(CPoint pt) const
{
    if (m_rcPlus.PtInRect(pt))
        return -1;
    if (m_rcClose.PtInRect(pt))
        return -4;
    if (m_rcSwitcher.PtInRect(pt) || m_rcOverflow.PtInRect(pt))
        return -2;
    for (int i = 0; i < static_cast<int>(m_rcTabs.size()); i++)
    {
        if (m_rcTabs[i].PtInRect(pt))
            return m_firstVisible + i;
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

    int dpiX = m_dpiX;
    if (dpiX <= 0)
        dpiX = 96;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOld = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(rc, ::GetSysColor(COLOR_BTNFACE));
    CFont* pOldFont = memDC.SelectObject(&m_font);
    memDC.SetBkMode(TRANSPARENT);

    auto drawPlus = [&](const CRect& r, bool hover) {
        COLORREF bg = hover ? RGB(40, 40, 40) : RGB(24, 24, 24);
        memDC.FillSolidRect(r, bg);
        COLORREF fg = RGB(220, 220, 220);
        int cx = r.CenterPoint().x;
        int cy = r.CenterPoint().y;
        int s = std::max(4, MulDiv(5, dpiX, 96));
        int t = std::max(2, MulDiv(2, dpiX, 96));
        s = std::min(s, std::min(r.Width(), r.Height()) / 2);
        memDC.FillSolidRect(cx - s, cy - t / 2, s * 2, t, fg);
        memDC.FillSolidRect(cx - t / 2, cy - s, t, s * 2, fg);
    };

    auto drawChevron = [&](const CRect& r) {
        int cx = r.CenterPoint().x;
        int cy = r.CenterPoint().y;
        int w = std::max(4, MulDiv(5, dpiX, 96));
        int h = std::max(3, MulDiv(4, dpiX, 96));
        POINT pts[3] = {
            { cx - w, cy - h / 2 },
            { cx + w, cy - h / 2 },
            { cx, cy + h / 2 }
        };
        CBrush brush(RGB(210, 210, 210));
        CBrush* pOldBrush = memDC.SelectObject(&brush);
        CGdiObject* pOldPen = memDC.SelectStockObject(NULL_PEN);
        memDC.Polygon(pts, 3);
        memDC.SelectObject(pOldBrush);
        memDC.SelectObject(pOldPen);
    };

    auto drawOverflow = [&](const CRect& r, bool hover) {
        memDC.FillSolidRect(r, hover ? RGB(40, 40, 40) : RGB(24, 24, 24));
        int cx = r.CenterPoint().x;
        int cy = r.CenterPoint().y;
        int dot = std::max(2, MulDiv(2, dpiX, 96));
        int gap = std::max(3, MulDiv(3, dpiX, 96));
        CBrush brush(RGB(210, 210, 210));
        CBrush* pOldBrush = memDC.SelectObject(&brush);
        CGdiObject* pOldPen = memDC.SelectStockObject(NULL_PEN);
        memDC.Ellipse(cx - gap - dot, cy - dot, cx - gap + dot, cy + dot);
        memDC.Ellipse(cx - dot, cy - dot, cx + dot, cy + dot);
        memDC.Ellipse(cx + gap - dot, cy - dot, cx + gap + dot, cy + dot);
        memDC.SelectObject(pOldBrush);
        memDC.SelectObject(pOldPen);
    };

    auto drawClose = [&](const CRect& r) {
        int cx = r.CenterPoint().x;
        int cy = r.CenterPoint().y;
        int s = std::max(3, MulDiv(4, dpiX, 96));
        int line = std::max(1, MulDiv(2, dpiX, 96));
        s = std::min(s, std::min(r.Width(), r.Height()) / 2);
        CPen pen(PS_SOLID, line, RGB(235, 235, 235));
        CPen* pOldPen = memDC.SelectObject(&pen);
        memDC.MoveTo(cx - s, cy - s);
        memDC.LineTo(cx + s, cy + s);
        memDC.MoveTo(cx + s, cy - s);
        memDC.LineTo(cx - s, cy + s);
        memDC.SelectObject(pOldPen);
    };

    auto drawCloseBadge = [&](const CRect& r, bool hover) {
        CRect circle = r;
        circle.DeflateRect(2, 2);
        CBrush brush(hover ? RGB(195, 62, 62) : RGB(165, 52, 52));
        CBrush* pOldBrush = memDC.SelectObject(&brush);
        CGdiObject* pOldPen = memDC.SelectStockObject(NULL_PEN);
        memDC.Ellipse(circle);
        memDC.SelectObject(pOldBrush);
        memDC.SelectObject(pOldPen);

        int line = std::max(1, MulDiv(2, dpiX, 96));
        int s = std::max(3, MulDiv(4, dpiX, 96));
        s = std::min(s, std::min(r.Width(), r.Height()) / 2);
        int cx = r.CenterPoint().x;
        int cy = r.CenterPoint().y;

        CPen pen(PS_SOLID, line, RGB(255, 255, 255));
        CPen* pOldLinePen = memDC.SelectObject(&pen);
        memDC.MoveTo(cx - s, cy - s);
        memDC.LineTo(cx + s, cy + s);
        memDC.MoveTo(cx + s, cy - s);
        memDC.LineTo(cx - s, cy + s);
        memDC.SelectObject(pOldLinePen);
    };

    if (m_compact)
    {
        memDC.FillSolidRect(rc, RGB(16, 16, 16));
        if (!m_rcSwitcher.IsRectEmpty())
        {
            COLORREF bg = (m_hover == -2) ? RGB(32, 32, 32) : RGB(24, 24, 24);
            memDC.FillSolidRect(m_rcSwitcher, bg);
            memDC.FillSolidRect(m_rcSwitcher.left, m_rcSwitcher.top, 2,
                m_rcSwitcher.Height(), RGB(0, 122, 204));

            CString label;
            int count = static_cast<int>(m_titles.size());
            if (m_active >= 0 && m_active < count)
                label = m_titles[m_active];
            if (count > 1 && m_active >= 0)
                label.AppendFormat(_T("  [%d/%d]"), m_active + 1, count);

            int chevronW = std::max(16, MulDiv(16, dpiX, 96));
            CRect textRect = m_rcSwitcher;
            textRect.DeflateRect(6, 1, chevronW + 2, 1);
            memDC.SetTextColor(RGB(235, 235, 235));
            memDC.DrawText(label, textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            int chevronPad = std::max(4, MulDiv(4, dpiX, 96));
            CRect chevronRect(m_rcSwitcher.right - chevronW, m_rcSwitcher.top,
                m_rcSwitcher.right - chevronPad, m_rcSwitcher.bottom);
            drawChevron(chevronRect);
        }
        if (!m_rcClose.IsRectEmpty())
        {
            memDC.FillSolidRect(m_rcClose, RGB(16, 16, 16));
            CRect closeRect = m_rcClose;
            closeRect.DeflateRect(2, 1, 2, 1);
            if (m_hover == -4)
                drawCloseBadge(closeRect, true);
            else
                drawClose(closeRect);
        }
        drawPlus(m_rcPlus, m_hover == -1);
    }
    else
    {
        memDC.FillSolidRect(rc, RGB(16, 16, 16));

        for (int i = 0; i < static_cast<int>(m_rcTabs.size()); i++)
        {
            const CRect& tab = m_rcTabs[i];
            int titleIndex = m_firstVisible + i;
            bool active = (titleIndex == m_active);
            bool hover = (titleIndex == m_hover);
            COLORREF bg = active ? RGB(34, 34, 34) : (hover ? RGB(27, 27, 27) : RGB(20, 20, 20));
            memDC.FillSolidRect(tab, bg);
            if (active)
                memDC.FillSolidRect(tab.left, tab.top, tab.Width(), 2, RGB(0, 122, 204));
            if (i > 0)
                memDC.FillSolidRect(tab.left, tab.top + 4, 1, tab.Height() - 8, RGB(55, 55, 55));

            int closePadRight = std::max(16, MulDiv(16, dpiX, 96));
            memDC.SetTextColor(active ? RGB(255, 255, 255) : RGB(170, 170, 170));
            CRect textRect = tab;
            textRect.DeflateRect(6, 1, closePadRight, 1);
            memDC.DrawText(m_titles[titleIndex], textRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (active || hover)
            {
                int closePadLeft = std::max(3, MulDiv(3, dpiX, 96));
                int closePadTop = std::max(3, MulDiv(3, dpiX, 96));
                int closePadBottom = std::max(3, MulDiv(3, dpiX, 96));
                CRect closeRect(tab.right - closePadRight, tab.top + closePadTop,
                    tab.right - closePadLeft, tab.bottom - closePadBottom);
                drawClose(closeRect);
            }
        }

        drawPlus(m_rcPlus, m_hover == -1);
        if (!m_rcOverflow.IsRectEmpty())
            drawOverflow(m_rcOverflow, m_hover == -2);
    }

    memDC.SelectObject(pOldFont);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOld);
}

void CTerminalTabBar::HandleClick(CPoint point)
{
    int hit = HitTest(point);
    if (hit >= 0)
    {
        int rectIndex = hit - m_firstVisible;
        if (rectIndex >= 0 && rectIndex < static_cast<int>(m_rcTabs.size()))
        {
            int closePadRight = std::max(16, MulDiv(16, m_dpiX, 96));
            if (point.x >= m_rcTabs[rectIndex].right - closePadRight)
                Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(hit));
            else
                Notify(WM_TERM_TAB_SELECT, static_cast<WPARAM>(hit));
        }
    }
    else if (hit == -1)
    {
        Notify(WM_TERM_TAB_NEW, 0);
    }
    else if (hit == -4 && m_active >= 0)
    {
        Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(m_active));
    }
    else if (hit == -2)
    {
        CPoint pt;
        ::GetCursorPos(&pt);
        ShowOverflowMenu(pt);
    }
}

void CTerminalTabBar::HandleMiddleClick(CPoint point)
{
    int hit = HitTest(point);
    if (hit >= 0)
        Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(hit));
    else if (m_compact && (hit == -2 || hit == -4) && m_active >= 0)
        Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(m_active));
}

void CTerminalTabBar::HandleRightClick(CPoint point)
{
    int hit = HitTest(point);
    CPoint pt = point;
    ClientToScreen(&pt);
    int closeIndex = hit;
    if (hit < 0 && static_cast<int>(m_titles.size()) > 1 &&
        (m_compact || hit == -2))
        closeIndex = m_active;
    ShowContextMenu(pt, closeIndex);
}

void CTerminalTabBar::HandleDoubleClick(CPoint point)
{
    if (HitTest(point) == -3)
        Notify(WM_TERM_TAB_NEW, 0);
}

void CTerminalTabBar::HandleWheel(short zDelta)
{
    int count = static_cast<int>(m_titles.size());
    if (count == 0)
        return;

    // Stop at the first/last session instead of wrapping around.
    int current = (m_active < 0) ? 0 : m_active;
    int next = current + ((zDelta < 0) ? 1 : -1);
    next = std::clamp(next, 0, count - 1);
    if (next != current)
        Notify(WM_TERM_TAB_SELECT, static_cast<WPARAM>(next));
}

BOOL CTerminalTabBar::OnMouseWheel(UINT, short zDelta, CPoint)
{
    HandleWheel(zDelta);
    return TRUE;
}

void CTerminalTabBar::OnMButtonUp(UINT, CPoint point)
{
    HandleMiddleClick(point);
}

void CTerminalTabBar::OnLButtonDown(UINT, CPoint point)
{
    HandleClick(point);
}

void CTerminalTabBar::OnLButtonDblClk(UINT, CPoint point)
{
    HandleDoubleClick(point);
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
        if (i >= m_firstVisible && i < m_firstVisible + m_visibleCount)
            continue;

        UINT flags = MF_STRING;
        if (i == m_active)
            flags |= MF_CHECKED;
        menu.AppendMenu(flags, baseId + i, m_titles[i]);
    }

    int count = static_cast<int>(m_titles.size());
    if (m_active >= 0 && count > 1)
    {
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 2998, loc.GetString(_T("Terminal"), _T("CloseTab")));
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 2999, loc.GetString(_T("Terminal"), _T("New")));

    UINT cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, this);
    if (cmd == 2999)
    {
        Notify(WM_TERM_TAB_NEW, 0);
    }
    else if (cmd == 2998 && m_active >= 0)
    {
        Notify(WM_TERM_TAB_CLOSE, static_cast<WPARAM>(m_active));
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
