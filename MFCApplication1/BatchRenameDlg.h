#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include <vector>
#include <set>
#include <filesystem>

struct RenameEntry {
        CString oldName;
        CString newName;
        std::filesystem::path fullPath;
        bool bMarkedDelete{false};
        bool bCustomExt{false};
        CString customExt;
        bool bIgnored{false};
        bool bTracked{false};
        bool bAiGenerated{false};
    };

struct FolderEntry {
    CString name;
    std::filesystem::path fullPath;
};

class CBatchRenameDlg : public CDialogEx
{
public:
    CBatchRenameDlg(CWnd* pParent = nullptr, const CString& initialFolder = _T(""));
    enum { IDD = IDD_BATCH_RENAME_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
    virtual void OnCancel() override;
    virtual void PostNcDestroy() override;
    afx_msg void OnClose();

    DECLARE_MESSAGE_MAP()

private:
    // Tab control
    CTabCtrl m_tabCtrl;
    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    void ShowTab(int nTab);

    // Shared
    CString m_folderPath;
    afx_msg void OnBnClickedBrowse();
    afx_msg void OnBnClickedClear();
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnDropFiles(HDROP hDrop);
    void SetFolderPath(const CString& path);

    // Tab 0: Folder operations
    void LoadFolders();
    void RefreshFolderList();
    void UpdateFolderSelectionCount();
    std::vector<int> GetCheckedFolders();
    afx_msg void OnBnClickedFolderRename();
    afx_msg void OnBnClickedFolderMove();
    afx_msg void OnBnClickedFolderDelete();
    afx_msg void OnBnClickedFolderSelectAll();
    afx_msg void OnBnClickedFolderDeselectAll();
    afx_msg void OnFolderListRightClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFolderListCheckChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFolderExplore();
    std::vector<FolderEntry> m_folders;

    // Tab 1: File batch processing
    void LoadFiles();
    void ApplyRules();
    void ApplyIgnoreRules();
    void ApplyTrackRules();
    static bool MatchExtension(const CString& fileName, const CString& extList);
    static bool MatchPattern(const CString& fileName, const CString& pattern, bool bRegex);
    void RefreshFileList();
    afx_msg void OnBnClickedFilePreview();
    afx_msg void OnBnClickedFileExecute();
    afx_msg void OnBnClickedRenameUndo();
    afx_msg void OnEnChangeRule();
    afx_msg void OnIgnoreRuleChanged();
    afx_msg void OnTrackRuleChanged();
    afx_msg void OnBnClickedRegexHelp();
    afx_msg void OnFileListRightClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFileMarkDelete();
    afx_msg void OnFileUnmarkDelete();
    afx_msg void OnFileUnmarkAll();
    afx_msg void OnFileResetAll();
    afx_msg void OnFileChangeExt();
    afx_msg void OnFileRestoreExt();
    afx_msg void OnFileExplore();
    afx_msg void OnCheckDeleteInvert();
    afx_msg void OnBnClickedCurrentRename();
    afx_msg void OnBnClickedCurrentMove();
    afx_msg void OnBnClickedCurrentDelete();
    afx_msg void OnBnClickedIgnoreClear();
    afx_msg void OnBnClickedTrackClear();
    afx_msg void OnFileIgnore();
    afx_msg void OnFileUnignore();
    afx_msg void OnFileTrack();
    afx_msg void OnFileUntrack();
    afx_msg void OnFileMoveUp();
    afx_msg void OnFileMoveDown();
    afx_msg void OnFileMoveToUp();
    afx_msg void OnFileMoveToDown();
    afx_msg void OnBnClickedAiAssistant();
    afx_msg void OnFileAiAnalyze();
    afx_msg void OnLvnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult);
    void MoveSelectedItemsTo(int nTargetIndex);
    void ReselectItems(const std::vector<int>& indices);
    std::vector<RenameEntry> m_entries;
    std::set<std::filesystem::path> m_manualIgnoredSet;
    std::set<std::filesystem::path> m_manualUnignoredSet;
    std::set<std::filesystem::path> m_manualTrackedSet;
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> m_undoList;
    bool m_bPreviewDone{false};
    int m_nActiveTab{0};
    HWND m_hRegexGuideWnd{nullptr};

    // Drag-and-drop reordering state
    bool m_bDragging{false};
    int m_nDragSourceIndex{-1};
    int m_nDropTargetIndex{-1};
    int m_nDropLineY{-1};  // screen-space Y of insertion line, -1 = none
};