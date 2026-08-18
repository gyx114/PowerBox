// MarkdownDlg.h: header file
//

#pragma once
#include "afxdialogex.h"
#include "WebView2Ctrl.h"
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
	CWebView2Ctrl m_webview2;
	int m_splitPos;        // current x position of the splitter
	bool m_bDragging;      // whether the splitter is being dragged
	int m_dragOffset;      // offset from mouse to splitter during drag
	int m_btnLeft;         // button left (from RC)
	int m_btnTop;          // button top (from RC)
	int m_btnWidth;        // button width (from RC)
	int m_btnHeight;       // button height (from RC)
	int m_contentTop;      // top of edit/preview area (from RC)
	int m_previewRight;    // right edge of the preview area, from RC (fixed, does not move on resize)
	int m_pathLabelLeft;    // path label left (from RC)
	int m_pathLabelTop;     // path label top (from RC)
	int m_pathLabelHeight;  // path label height (from RC)
	CString m_baseDir;     // directory of the loaded markdown file (for relative image/link resolution)
	bool m_pageReady;      // reader.html has finished loading and accepts web messages

	void ResizeControls();
	void RefreshPreview();
	void LoadFile(const CString& path);
	void SendContentToPreview();
	CString PreviewTemplateUrl() const;
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
