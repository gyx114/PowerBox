#pragma once
#include <vector>
#include <utility>

// Structure representing a saved conversation
struct ConversationInfo {
    CString filePath;      // Full path to .conv file
    CString title;         // Conversation title (auto-generated from first user message)
    CString createdTime;   // Creation time string
    CString updatedTime;   // Last update time string
    int messageCount;      // Number of messages
};

// Custom message sent to parent when a conversation is loaded
#define WM_CONV_LOADED (WM_APP + 20)

class CConversationHistoryDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CConversationHistoryDlg)

public:
    CConversationHistoryDlg(CWnd* pParent = nullptr);
    virtual ~CConversationHistoryDlg();

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void PostNcDestroy() override;
    afx_msg void OnDestroy();
    afx_msg void OnOK();
    afx_msg void OnCancel();
    afx_msg void OnBnClickedConvLoad();
    afx_msg void OnBnClickedConvRename();
    afx_msg void OnBnClickedConvDelete();
    afx_msg void OnBnClickedConvPath();
    afx_msg void OnNMDblclkConvHistory(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickConvHistory(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    CListCtrl m_list;
    CStatic m_staticPath;
    std::vector<ConversationInfo> m_conversations;

    CString GetConversationsFolder();
    void RefreshList();
    void LoadConversation(const ConversationInfo& conv);
    void DeleteConversation(const ConversationInfo& conv);
    void RenameConversation(ConversationInfo& conv);
};