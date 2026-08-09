// TerminalTabBar.h: adaptive session switcher for terminal views
#pragma once

#include <vector>

constexpr UINT WM_TERM_TAB_SELECT = WM_APP + 60;
constexpr UINT WM_TERM_TAB_CLOSE = WM_APP + 61;
constexpr UINT WM_TERM_TAB_NEW = WM_APP + 62;

class CTerminalTabBar : public CWnd
{
public:
    CTerminalTabBar() = default;

    BOOL AttachToPlaceholder(UINT nID, CWnd* pParent);
    void AddTab(const CString& title);
    void RemoveTab(int index);
    void SetActive(int index);
    void SetTabTitle(int index, const CString& title);

    int GetActive() const { return m_active; }
    int GetTabCount() const { return static_cast<int>(m_titles.size()); }
    void HandleClick(CPoint clientPt);
    void HandleMiddleClick(CPoint clientPt);
    void HandleRightClick(CPoint clientPt);
    void HandleWheel(short zDelta);
    void Relayout();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    std::vector<CString> m_titles;
    std::vector<CRect> m_rcTabs;
    CRect m_rcSwitcher;
    CRect m_rcClose;
    int m_active = -1;
    int m_hover = -1;
    int m_visibleCount = 0;
    CRect m_rcPlus;
    CRect m_rcOverflow;
    CFont m_font;
    bool m_compact = false;
    int m_dpiX = 96;
    int m_tabW = 110;
    int m_tabH = 20;
    int m_minTabW = 92;
    int m_maxTabW = 160;

    void Layout();
    int HitTest(CPoint pt) const;
    CString MakeUniqueTitle(const CString& base, int ignoreIndex = -1) const;
    void ShowOverflowMenu(CPoint screenPt);
    void ShowContextMenu(CPoint screenPt, int hitIndex);
    void Notify(UINT msg, WPARAM wParam) const;
};
