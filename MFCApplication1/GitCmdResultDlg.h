// GitCmdResultDlg.h: Integrated Git command dialog with AI, command list, and output
#pragma once
#include "afxdialogex.h"
#include "AIApiClient.h"
#include <thread>
#include <vector>

class CGitCmdResultDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CGitCmdResultDlg)

public:
    // If strCommand is non-empty, it will be auto-added to the list and executed on init.
    // If bAutoExecute is false, the command is added but not executed (user decides).
    CGitCmdResultDlg(const CString& strWorkDir, CWnd* pParent = nullptr);
    virtual ~CGitCmdResultDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_GIT_CMD_RESULT_DLG };
#endif

    // Add a command to the list (description | command). Returns the item index.
    int AddCommand(const CString& strDesc, const CString& strCmd, bool bAutoExecute = false);

    // Get the working directory associated with this dialog.
    CString GetWorkDir() const { return m_strWorkDir; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void PostNcDestroy();

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnBnClickedCopyOutput();
    afx_msg void OnBnClickedClose();
    afx_msg void OnBnClickedAiAsk();
    afx_msg void OnBnClickedAddCmd();
    afx_msg void OnBnClickedClearCmds();
    afx_msg void OnNMRclickCmdList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkCmdList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnGitCmdOutput(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGitCmdDone(WPARAM wParam, LPARAM lParam);
    // Handle AI response posted from main dialog
    afx_msg LRESULT OnAiGitResponse(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    // Custom messages for async execution thread
    static constexpr UINT WM_GIT_CMD_OUTPUT = WM_APP + 10;
    static constexpr UINT WM_GIT_CMD_DONE   = WM_APP + 11;
    // AI response message (WM_AI_RESPONSE from AIApiClient.h, wParam=success, lParam=CString*)

    CString m_strWorkDir;
    CString m_strOutput;
    std::thread m_execThread;
    bool m_bRunning{false};
    bool m_bCancelPending{false};

    bool m_bInitialized{false};

    // Pending commands to auto-execute (added via constructor or AddCommand before init)
    struct CmdEntry { CString desc; CString cmd; bool autoExec; };
    std::vector<CmdEntry> m_pendingCmds;

    // Layout anchors (only widths adjust on horizontal resize)
    int m_workDirLeft, m_workDirTop, m_workDirWidth;
    int m_statusLeft, m_statusTop, m_statusWidth;
    int m_aiInputLeft, m_aiInputTop, m_aiInputWidth;
    int m_aiBtnLeft, m_aiBtnTop, m_aiBtnWidth, m_aiBtnHeight;
    int m_addBtnLeft, m_clearBtnLeft, m_btnCmdTop;
    int m_listLeft, m_listTop, m_listHeight;
    int m_outputLabelTop;
    int m_outputLeft, m_outputTop, m_outputHeight;
    int m_copyBtnLeft, m_closeBtnLeft, m_bottomBtnTop, m_bottomBtnWidth, m_bottomBtnHeight;

    void TranslateUI();
    void ResizeControls();
    void StartExecution(const CString& strCommand);
    void AppendOutput(const CString& text);

    // Derive bash.exe path from GitBashPath config
    static CString FindBashExe();
    // Execute a single command in a background thread
    void ExecuteThread(CString strCommand);
    // Start the next queued command, if any
    void ExecuteNextInQueue();

    // Queue of commands to execute sequentially
    std::vector<CString> m_cmdQueue;
};
