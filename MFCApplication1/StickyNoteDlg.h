// StickyNoteDlg.h: header file
//

#pragma once
#include "afxdialogex.h"

// CStickyNoteDlg dialog

class CStickyNoteDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CStickyNoteDlg)

public:
	CStickyNoteDlg(CWnd* pParent = nullptr);
	virtual ~CStickyNoteDlg();
	void SaveIfNeeded();  // called by main dialog on shutdown

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_STICKY_NOTE_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	CString m_noteText;
	CRect m_editMargins;
	bool m_bCollapsed{false};
	CRect m_rcNormal;    // saved window rect before collapse (for restoring)
	CRect m_rcCollapsed; // original dialog rect at creation (for collapsed title bar width)

	CString GetSavePath();
	bool EnsureSavePath();
	void LoadNote();
	void SaveNote();
	void ResizeEdit();
public:
	void CollapseWindow();
	void ExpandWindow();
private:

	afx_msg void OnBnClickedBrowse();
	afx_msg void OnEnChangeEdit();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnNcLButtonDblClk(UINT nHitTest, CPoint point);
	afx_msg void OnNcRButtonUp(UINT nHitTest, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};