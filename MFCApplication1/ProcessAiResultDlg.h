// ProcessAiResultDlg.h: AI process analysis result dialog
#pragma once
#include "afxdialogex.h"
#include <vector>

class CProcessAiResultDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CProcessAiResultDlg)

public:
    explicit CProcessAiResultDlg(const std::vector<CString>& procInfos, CWnd* pParent = nullptr);
    virtual ~CProcessAiResultDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PROCESS_AI_RESULT_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);
    afx_msg void OnBnClickedClose();
    afx_msg void OnBnClickedCopyResult();
    afx_msg void OnBnClickedSaveResult();

private:
    std::vector<CString> m_procInfos;  // formatted process info strings
    void SetResult(const CString& text);
    void AppendResult(const CString& text);
};