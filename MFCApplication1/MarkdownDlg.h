// MarkdownDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include <map>

class CActionCommandRegistry
{
public:
    CString Add(const CString& jsonAttr);
    bool Get(const CString& id, CString& jsonAttr) const;
    void Clear();

private:
    UINT_PTR m_nextId = 1;
    std::map<UINT_PTR, CString> m_commands;
};

class CMarkdownDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CMarkdownDlg)

public:
	CMarkdownDlg(CWnd* pParent = nullptr);
	virtual ~CMarkdownDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MARKDOWN_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
	CString m_markdownText;
	CFont m_fontEdit;
	CWnd m_browser;
	int m_splitPos;        // current x position of the splitter
	bool m_bDragging;      // whether the splitter is being dragged
	int m_dragOffset;      // offset from mouse to splitter during drag
	int m_btnLeft;         // button left (from RC)
	int m_btnTop;          // button top (from RC)
	int m_btnWidth;        // button width (from RC)
	int m_btnHeight;       // button height (from RC)
	int m_contentTop;      // top of edit/preview area (from RC)
	int m_pathLabelLeft;    // path label left (from RC)
	int m_pathLabelTop;     // path label top (from RC)
	int m_pathLabelHeight;  // path label height (from RC)

	void ResizeControls();
	void RefreshPreview();
	void LoadFile(const CString& path);
	bool SetBrowserHtml(const CString& html);
public:
    static CString MarkdownToHtml(const CString& markdown);
    static CString MarkdownToBody(const CString& markdown);
    static CString MarkdownToBody(const CString& markdown, CActionCommandRegistry* actionCommands);
    static CString EscapeHtml(const CString& text);
private:
	static CString FormatInline(const CString& text);
    static CString MarkdownToHtml(const CString& markdown, CActionCommandRegistry* actionCommands);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnEnChangeEdit();
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedOpen();
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
};
