// TerminalView.h: ConPTY-based terminal view for PowerBox
#pragma once

#include <windows.h>
#include <string>
#include <deque>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include "TerminalSession.h"
#include "resource.h"

class CTerminalView : public CWnd
{
public:
    CTerminalView();
    virtual ~CTerminalView();

    BOOL CreateTerminal(CWnd* pParent, UINT nID, const CRect& rc);
    BOOL AttachToPlaceholder(UINT nID, CWnd* pParent);
    BOOL StartShell(const CString& shellName);
    BOOL StartCommandSession(const CString& cmdLine, const CString& shellName);
    bool AdoptSession(std::unique_ptr<CTerminalSession> session, const CString& shellName);
    void StopSession();
    void ClearScreen();
    void RestartShell();
    void ScrollLines(short zDelta);
    void StartSelectionFromScreen(CPoint screenPt);
    void ContinueSelectionFromScreen(CPoint screenPt);
    void FinishSelection();
    void ShowContextMenu(CPoint screenPt);
    void StartAiCapture(UINT_PTR id, const CString& startMarker, const CString& endMarker, HWND notifyHwnd);
    void StopAiCapture();
    void WriteString(const std::wstring& text);
    void WriteUtf8(const std::string& text);

    bool IsRunning() const { return m_session && m_session->IsRunning(); }
    CString GetShellName() const { return m_shellName; }
    static constexpr UINT WM_AI_CAPTURE_DONE = WM_APP + 50;
    struct AiCaptureResult
    {
        UINT_PTR id = 0;
        std::string output;
        DWORD exitCode = 0;
    };

protected:
    struct TermCell
    {
        wchar_t ch = L' ';
        BYTE fg = 7;
        BYTE bg = 0;
        BYTE flags = 0; // 1 bold, 2 underline, 4 reverse
    };

    enum class EscState { Text, Esc, Csi, Osc };

    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnTermOutput(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTermExited(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnImeComposition(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnImeCharMsg(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTerminalCopy();
    afx_msg void OnTerminalPaste();
    afx_msg void OnTerminalRestart();

    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<CTerminalSession> m_session;
    CString m_shellName;

    // Screen buffer
    std::deque<std::vector<TermCell>> m_rows;
    int m_cols = 80;
    int m_rowsCount = 24;
    size_t m_cursorRow = 0;
    int m_cursorCol = 0;
    bool m_pendingWrap = false;
    int m_scrollOffset = 0;
    BYTE m_curFg = 7;
    BYTE m_curBg = 0;
    BYTE m_curFlags = 0;
    size_t m_savedRow = 0;
    int m_savedCol = 0;

    // ANSI parser
    EscState m_escState = EscState::Text;
    std::wstring m_escParam;
    std::string m_byteBuffer;

    // AI command capture between start/end markers
    bool m_bAiCapturing = false;
    UINT_PTR m_aiCaptureId = 0;
    std::string m_aiCaptureBuffer;
    std::string m_aiStartMarker;
    std::string m_aiEndMarker;
    HWND m_aiNotifyHwnd = nullptr;

    // Rendering
    CFont m_font;
    int m_cellW = 8;
    int m_cellH = 16;
    bool m_bFocused = false;

    // Selection
    bool m_bSelecting = false;
    int m_selAnchorRow = -1;
    int m_selAnchorCol = -1;
    int m_selStartRow = -1;
    int m_selStartCol = -1;
    int m_selEndRow = -1;
    int m_selEndCol = -1;

    void Feed(const char* data, size_t len);
    void FeedText(const std::wstring& text);
    void ProcessChar(wchar_t ch);
    void ExecuteCsi(wchar_t final, const std::wstring& params);
    void ApplySgr(const std::vector<int>& params);
    void PutChar(wchar_t ch);
    void LineFeed();
    void ReverseIndex();
    void EnsureRow(size_t row);
    std::vector<TermCell> NewRow() const;
    size_t ScreenStart() const;
    int FirstVisibleRow() const;
    void ClampCursorToScreen();
    void ClearRow(size_t row, int fromCol, int toColExclusive);
    void ResetScreen();
    void ResizeGrid();
    void RebuildFont();
    void SetSelectionFromPoint(CPoint point);
    void UpdateSelectionEnd(CPoint point);
    void ClearSelection();
    void CheckAiCapture(const std::string& data);
    void FinishAiCapture(DWORD exitCode);
    void CopySelection();
    void PasteClipboard();
    std::wstring GetSelectedText() const;
    bool CellInSelection(int row, int col) const;
    COLORREF PaletteColor(int index, bool bold) const;
    static void ParseParams(const std::wstring& s, std::vector<int>& out);
};
