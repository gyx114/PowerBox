// QuickLaunchDlg.h: Quick launch configuration dialog
#pragma once
#include "afxdialogex.h"
#include <vector>

// Registered message: sent from CQuickLaunchDlg to parent when items change
extern const UINT WM_QL_CHANGED;
// Registered message: sent from CQuickLaunchDlg to parent when dialog closes
extern const UINT WM_QL_CLOSED;

struct QLItem
{
    enum Type { Executable = 0, Folder, Url, OtherFile };
    CString name;
    CString path;
    int type = Executable;
};

class CQuickLaunchDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CQuickLaunchDlg)

public:
    explicit CQuickLaunchDlg(std::vector<QLItem>& items, CWnd* pParent = nullptr);
    virtual ~CQuickLaunchDlg();

    static bool ResolveShortcut(const CString& path, CString& outTarget, int& outType);

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

    void RefreshList();
    void OnAdd();
    void OnEdit();
    void OnDelete();
    void OnMoveUp();
    void OnMoveDown();
    bool EditItem(QLItem& item, bool bNew);
    void NotifyParent();

    afx_msg void OnBnClickedQlAdd();
    afx_msg void OnBnClickedQlEdit();
    afx_msg void OnBnClickedQlDelete();
    afx_msg void OnBnClickedQlUp();
    afx_msg void OnBnClickedQlDown();
    afx_msg void OnNMDblclkQlList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMRclickQlList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg void OnClose();
    virtual void OnCancel();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
};