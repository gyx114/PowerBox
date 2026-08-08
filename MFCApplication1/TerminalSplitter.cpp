// TerminalSplitter.cpp: thick draggable splitter with hover cursor and highlight
#include "pch.h"
#include "framework.h"
#include "TerminalSplitter.h"

BEGIN_MESSAGE_MAP(CTerminalSplitter, CStatic)
    ON_WM_PAINT()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

BOOL CTerminalSplitter::AttachToPlaceholder(UINT nID, CWnd* pParent)
{
    if (!SubclassDlgItem(nID, pParent))
        return FALSE;
    return TRUE;
}

void CTerminalSplitter::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    COLORREF color = RGB(58, 58, 58);
    if (m_bDragging)
        color = RGB(80, 150, 230);
    else if (m_bHover)
        color = RGB(95, 95, 95);

    dc.FillSolidRect(rc, color);

    // Center grip line.
    int y = rc.CenterPoint().y;
    dc.FillSolidRect(rc.left + 4, y - 1, rc.Width() - 8, 2,
        m_bDragging ? RGB(255, 255, 255) : RGB(120, 120, 120));
}

BOOL CTerminalSplitter::OnSetCursor(CWnd*, UINT, UINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
    return TRUE;
}

void CTerminalSplitter::OnMouseMove(UINT, CPoint)
{
    if (!m_bHover)
    {
        m_bHover = true;
        Invalidate(FALSE);
    }

    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
    _TrackMouseEvent(&tme);
}

void CTerminalSplitter::OnMouseLeave()
{
    m_bHover = false;
    Invalidate(FALSE);
}
