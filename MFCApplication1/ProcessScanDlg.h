// ProcessScanDlg.h: AI process scan result dialog
#pragma once
#include "afxdialogex.h"
#include <vector>
#include "json.hpp"

// Custom message: sent by scan dialog to parent when "开始扫描" is clicked
// WPARAM = scan level (0=保守, 1=标准, 2=激进), LPARAM = hWnd of scan dialog
#define WM_PROCESS_SCAN_START (WM_USER + 210)

class CProcessScanDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CProcessScanDlg)

public:
    explicit CProcessScanDlg(CWnd* pParent = nullptr);
    virtual ~CProcessScanDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PROCESS_SCAN_DLG };
#endif

    struct ScanEntry
    {
        CString name;
        DWORD pid{0};
        CString risk;       // "可疑" or "无用"
        CString reason;     // AI analysis result
    };

    // Returns current scan level: 0=保守, 1=标准, 2=激进
    int GetScanLevel() const;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamChunk(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamDone(WPARAM wParam, LPARAM lParam);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnBnClickedScanEnd();
    afx_msg void OnBnClickedScanLocate();
    afx_msg void OnBnClickedScanEndAll();
    afx_msg void OnBnClickedScanStart();
    afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMenuScanEnd();
    afx_msg void OnMenuScanLocate();
    afx_msg void OnMenuScanCopyPath();

private:
    std::vector<ScanEntry> m_entries;
    CString m_streamBuffer;  // accumulates streaming response until completion

    // Layout anchors (saved from RC file at OnInitDialog)
    int m_listLeft, m_listTop;
    int m_listHeight;       // fixed list height from RC (only width changes on resize)
    int m_listRightMargin;  // original right margin of list from dialog edge
    int m_btnEndLeft, m_btnLocateLeft, m_btnEndAllLeft, m_btnStartLeft;
    int m_btnWidth, m_btnHeight;  // fixed button size from RC
    int m_labelLevelLeft, m_labelLevelTop;
    int m_labelLevelWidth, m_labelLevelHeight;  // fixed label size from RC
    int m_cmbLevelLeft, m_cmbLevelTop;
    int m_cmbLevelWidth, m_cmbLevelHeight;  // fixed combo box size from RC
    int m_statusLeft, m_statusTop;
    int m_statusWidth, m_statusHeight;  // fixed status label size from RC
    int m_colRatios[4];  // column width ratios for dynamic resizing

    void TranslateUI();
    void RefreshList();
    void ParseAIResponse(const CString& json);
    void EndProcess(int index);
    void LocateProcess(int index);
    void UpdateStatus(const CString& text);
    void ResizeControls();
    DWORD GetSelectedPid();
    CString GetSelectedPath();
};