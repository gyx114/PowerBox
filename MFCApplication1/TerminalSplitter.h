// TerminalSplitter.h: thick draggable splitter with hover cursor and highlight
#pragma once

class CTerminalSplitter : public CStatic
{
public:
    CTerminalSplitter() = default;

    BOOL AttachToPlaceholder(UINT nID, CWnd* pParent);
    void SetDragging(bool dragging)
    {
        m_bDragging = dragging;
        Invalidate(FALSE);
    }

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();

    DECLARE_MESSAGE_MAP()

private:
    bool m_bDragging = false;
    bool m_bHover = false;
};
