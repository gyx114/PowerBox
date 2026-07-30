// ProcessScanDlg.h: AI process scan result dialog
#pragma once
#include "afxdialogex.h"
#include <vector>
#include "json.hpp"

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

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnBnClickedScanEnd();
    afx_msg void OnBnClickedScanLocate();
    afx_msg void OnBnClickedScanEndAll();
    afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMenuScanEnd();
    afx_msg void OnMenuScanLocate();
    afx_msg void OnMenuScanCopyPath();

private:
    std::vector<ScanEntry> m_entries;

    // Layout anchors (saved from RC file at OnInitDialog)
    int m_listLeft, m_listTop;
    int m_listRightMargin;  // original right margin of list from dialog edge
    int m_btnEndLeft, m_btnLocateLeft, m_btnEndAllLeft;
    int m_btnWidth, m_btnHeight;  // fixed button size from RC
    int m_statusLeft, m_statusTop;
    int m_statusWidth, m_statusHeight;  // fixed status label size from RC
    int m_colRatios[4];  // column width ratios for dynamic resizing

    void RefreshList();
    void ParseAIResponse(const CString& json);
    void EndProcess(int index);
    void LocateProcess(int index);
    void UpdateStatus(const CString& text);
    void ResizeControls();
    DWORD GetSelectedPid();
    CString GetSelectedPath();
};