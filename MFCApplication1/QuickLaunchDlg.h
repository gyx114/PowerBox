// QuickLaunchDlg.h: Quick launch configuration dialog
#pragma once
#include "afxdialogex.h"
#include <vector>

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

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_QUICK_LAUNCH_DLG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

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

    afx_msg void OnBnClickedQlAdd();
    afx_msg void OnBnClickedQlEdit();
    afx_msg void OnBnClickedQlDelete();
    afx_msg void OnBnClickedQlUp();
    afx_msg void OnBnClickedQlDown();
    afx_msg void OnNMDblclkQlList(NMHDR* pNMHDR, LRESULT* pResult);
};