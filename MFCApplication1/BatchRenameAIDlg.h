#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include <vector>
#include <filesystem>

struct AIRenameMapping {
    CString oldName;                  // current name sent to AI
    CString newName;                  // AI-generated name
    std::filesystem::path fullPath;   // for matching back to entries
};

class CBatchRenameAIDlg : public CDialogEx
{
public:
    // files: list of {oldName, fullPath} pairs for all files in the batch
    CBatchRenameAIDlg(const std::vector<std::pair<CString, std::filesystem::path>>& files,
                      CWnd* pParent = nullptr);
    enum { IDD = IDD_BATCH_RENAME_AI_DLG };

    // Get the AI-generated mappings (only files that need renaming)
    const std::vector<AIRenameMapping>& GetMappings() const { return m_mappings; }
    // Get reverse mapping (newName -> oldName) for undo support
    const std::vector<std::pair<CString, CString>>& GetReverseMappings() const { return m_reverseMappings; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnCancel() override;
    void TranslateUI();

    DECLARE_MESSAGE_MAP()

private:
    std::vector<std::pair<CString, std::filesystem::path>> m_files;
    std::vector<AIRenameMapping> m_mappings;
    std::vector<std::pair<CString, CString>> m_reverseMappings; // {newName, oldName} for undo
    bool m_bAiDone{false};
    bool m_bDestroying{false};

    afx_msg void OnBnClickedAiSend();
    afx_msg void OnBnClickedAiApply();
    afx_msg LRESULT OnAiResponse(WPARAM wParam, LPARAM lParam);

    void RefreshPreview();
    bool ParseAIResponse(const CString& response);
    CString SanitizeFilename(const CString& name);
    bool HasIllegalChars(const CString& name);
    void ShowStatus(const CString& text, bool bError = false);
};