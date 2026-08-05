// QuickLaunchDlg.h: Quick launch configuration dialog
#pragma once
#include "afxdialogex.h"
#include "HotkeyCaptureDlg.h"
#include <vector>

// Registered message: sent from CQuickLaunchDlg to parent when items change
extern const UINT WM_QL_CHANGED;
// Registered message: sent from CQuickLaunchDlg to parent when dialog closes
extern const UINT WM_QL_CLOSED;

struct QLItem
{
    enum Type { Executable = 0, Folder, Url, OtherFile, HotkeyOnly };
    CString name;
    CString path;
    int type = Executable;
    HotkeyInfo hotkey; // wake hotkey (only valid for Executable/HotkeyOnly)
    CString customIconPath; // custom icon file path; empty = use default extracted icon
};

// Maximum number of quick launch items
static constexpr int MAX_QL_ITEMS = 36;

class CQuickLaunchDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CQuickLaunchDlg)

public:
    explicit CQuickLaunchDlg(std::vector<QLItem>& items, CWnd* pParent = nullptr);
    virtual ~CQuickLaunchDlg();

    static bool ResolveShortcut(const CString& path, CString& outTarget, int& outType);
    // Extract icon for a quick launch item: custom icon > SHGetFileInfo > default hotkey icon
    static HICON ExtractIconForItem(const QLItem& item);
    // Open the item-specific edit dialog for a single item (modeless, no overview window)
    static bool EditSingleItem(QLItem& item, bool bNew, CWnd* pParent);
    // Get the icons directory (%APPDATA%\PowerBox\icons\), creating it if needed
    static CString GetIconsDir();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_QUICK_LAUNCH_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();

    DECLARE_MESSAGE_MAP()

private:
    std::vector<QLItem>& m_items; // reference to caller's list
    int m_nLabelMaxLen; // label truncation length for icon view (0 = no truncation)

    void RefreshList();
    void OnAdd();
    void OnEdit();
    void OnDelete();
    void OnMoveUp();
    void OnMoveDown();
    void OnMoveUpSelected();
    void OnMoveDownSelected();
    void MoveSelectedItemsTo(int nTargetIndex);
    bool EditItem(QLItem& item, bool bNew);
    void NotifyParent();
    std::vector<int> GetSelectedIndices();

    // Drag-and-drop reordering state
    bool m_bDragging{false};
    int m_nDragSourceIndex{-1};
    int m_nDropTargetIndex{-1};
    int m_nDropLineY{-1};  // screen-space Y of insertion line, -1 = none

    CImageList m_imgList; // small icons (16x16) for the config list
    CImageList m_imgListLarge; // large icons (32x32) for the config list

    afx_msg void OnBnClickedQlAdd();
    afx_msg void OnBnClickedQlEdit();
    afx_msg void OnBnClickedQlDelete();
    afx_msg void OnBnClickedQlUp();
    afx_msg void OnBnClickedQlDown();
    afx_msg void OnBnClickedQlChangeIcon();
    afx_msg void OnNMDblclkQlList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMRclickQlList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg void OnClose();
    virtual void OnCancel();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    afx_msg void OnLvnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult);
};