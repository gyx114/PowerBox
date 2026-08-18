// AIAssistantDlg.h: Standalone AI Assistant window (with terminal)
#pragma once

#include <vector>
#include <utility>
#include <string>
#include <memory>
#include <map>
#include "resource.h"
#include "AIApiClient.h"
#include "ConversationHistoryDlg.h"
#include "MarkdownDlg.h"
#include "TerminalView.h"
#include "TerminalTabBar.h"
#include "TerminalSplitter.h"

class CAIAssistantDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAIAssistantDlg)

public:
    CAIAssistantDlg(CWnd* pParent = nullptr);
    virtual ~CAIAssistantDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_AI_ASSISTANT_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnClose();
    virtual void OnDestroy();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void OnOK() {}
    virtual void OnCancel() {}

    DECLARE_MESSAGE_MAP()

private:
    // AI state
    std::vector<std::pair<CString, CString>> m_aiHistory;
    CWebView2Ctrl m_webview2;
    CRect m_aiBrowserRect;
    CString m_aiPendingHtml;
    CString m_aiStreamingContent;
    CString m_aiScrollTarget;   // element to scroll to after the page finishes loading
    CString m_strConvTitle;
    CString m_strConvPath;
    CString m_strConvCreated;

    // AI command execution context
    struct AiCommandContext
    {
        CString command;
        CString exeDir;
        std::string output;
    };
    std::vector<std::unique_ptr<CTerminalSession>> m_aiSessions;
    std::map<CTerminalSession*, AiCommandContext> m_aiCommandContexts;
    std::map<UINT_PTR, CString> m_aiCommandById;
    CActionCommandRegistry m_aiActionCommands;
    UINT_PTR m_aiNextCommandId = 1;


    // Terminal
    CTerminalView m_terminalView;
    CComboBox m_terminalShell;
    CStatic m_terminalLabel;
    CButton m_terminalClear;
    CTerminalSplitter m_terminalSplitter;
    CTerminalTabBar m_terminalTabs;
    std::vector<std::unique_ptr<CTerminalView>> m_extraTerminalViews;
    std::vector<CTerminalView*> m_terminalTabsList;
    CTerminalView* m_pActiveTerminal = nullptr;
    CString m_strTerminalShell;
    bool m_bLayoutReady = false;
    int m_terminalViewHeight = 0;
    bool m_bTerminalResizing = false;

    // Initial static layout captured at startup; resize only applies anchors.
    CRect m_rcClientInit;
    CRect m_rcBrowserInit;
    CRect m_rcInputInit;
    CRect m_rcVendorInit;
    CRect m_rcButtonsInit[4];
    CRect m_rcSplitterInit;
    CRect m_rcTermLabelInit;
    CRect m_rcTermTabsInit;
    CRect m_rcTermShellInit;
    CRect m_rcTermClearInit;
    CRect m_rcTermViewInit;
    void CaptureInitialLayout();

    // Initialization
    void InitAIAssistant();
    void InitTerminal();

    // AI helpers
    CString BuildSystemPrompt();
    CString BuildAiHtmlPage(const CString& bodyContent);
    CString BuildAiBodyFromHistory(const CString& streamingContent = CString(), const CString& scrollToCommand = CString());
    CString RenderAssistantWithResults(const CString& content,
        std::map<CString, std::vector<CString>>& cmdResults,
        std::map<CString, int>& cmdResultIndex);
    bool SetAiBrowserHtml(const CString& html);
    void ScrollAiBrowserToAnchor(const CString& elementId);
    void ResizeWebView();
    void SaveCurrentConversation();
    void LoadConversation(const CString& filePath);
    CString GetExeDir();
    CString GetConversationsFolder();

    // Terminal helpers
    void AddTerminalTab(const CString& shellName);
    void AddTerminalTabWithCommand(const CString& shellName, const CString& cmdLine);
    void CloseTerminalTab(int index);
    void ActivateTerminalTab(int index);
    CTerminalView* ActiveTerminal();
    void AddAiCommandTab(const CString& command, const CString& terminal = CString());
    void FinishAiCommand(CTerminalSession* session);

    // Message handlers
    afx_msg void OnBnClickedAiSend();
    afx_msg void OnBnClickedAiClear();
    afx_msg void OnBnClickedAiStop();
    afx_msg void OnBnClickedAiHistory();
    afx_msg void OnBnClickedTerminalClear();
    afx_msg void OnCbnSelchangeTerminalShell();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    // Custom message handlers
    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamChunk(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiStreamDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiExecuteCommand(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiCommandResult(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiSessionOutput(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiSessionExited(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAiCaptureDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnConvLoaded(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTermTabSelect(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTermTabClose(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTermTabNew(WPARAM wParam, LPARAM lParam);
};
