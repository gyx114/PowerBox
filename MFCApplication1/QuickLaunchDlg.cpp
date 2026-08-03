// QuickLaunchDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "QuickLaunchDlg.h"
#include "LocalizationManager.h"
#include "Utils.h"
#include "afxdialogex.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// Local item edit dialog for name/path input
// ============================================================================
class CQLItemEditDlg : public CDialog
{
public:
    CString m_name;
    CString m_path;
    CString m_title;
    int m_type = QLItem::Executable;

    CQLItemEditDlg(CWnd* pParent = nullptr)
        : CDialog(IDD_QL_ITEM_DLG, pParent) {}

    static CString TypeLabel(int t)
    {
        auto& loc = CLocalizationManager::GetInstance();
        switch (t) {
            case QLItem::Executable: return loc.GetString(_T("QuickLaunch"), _T("TypeExecutable"));
            case QLItem::Folder:     return loc.GetString(_T("QuickLaunch"), _T("TypeFolder"));
            case QLItem::Url:        return loc.GetString(_T("QuickLaunch"), _T("TypeUrl"));
            case QLItem::OtherFile:  return loc.GetString(_T("QuickLaunch"), _T("TypeOtherFile"));
            default: return _T("");
        }
    }

protected:
    virtual BOOL OnInitDialog()
    {
        CDialog::OnInitDialog();
        SetWindowText(m_title);

        auto& loc = CLocalizationManager::GetInstance();
        SetDlgItemText(IDC_QL_LABEL_NAME, loc.GetString(_T("QuickLaunch"), _T("LabelName")));
        SetDlgItemText(IDC_QL_LABEL_TYPE, loc.GetString(_T("QuickLaunch"), _T("LabelType")));
        SetDlgItemText(IDC_QL_LABEL_PATH, loc.GetString(_T("QuickLaunch"), _T("LabelPath")));

        SetDlgItemText(IDC_QL_EDIT_NAME, m_name);

        // Init type combo
        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        if (pType)
        {
            pType->AddString(TypeLabel(QLItem::Executable));
            pType->AddString(TypeLabel(QLItem::Folder));
            pType->AddString(TypeLabel(QLItem::Url));
            pType->AddString(TypeLabel(QLItem::OtherFile));
            pType->SetCurSel(m_type);
        }

        // Localize buttons
        SetDlgItemText(IDC_QL_BROWSE, loc.GetString(_T("QuickLaunch"), _T("BtnBrowse")));
        SetDlgItemText(IDOK, loc.GetString(_T("QuickLaunch"), _T("BtnOK")));
        SetDlgItemText(IDCANCEL, loc.GetString(_T("QuickLaunch"), _T("BtnCancel")));

        SetDlgItemText(IDC_QL_EDIT_PATH, m_path);
        return TRUE;
    }

    virtual void OnOK()
    {
        GetDlgItemText(IDC_QL_EDIT_NAME, m_name);
        GetDlgItemText(IDC_QL_EDIT_PATH, m_path);
        m_name.Trim();
        m_path.Trim();
        if (m_name.IsEmpty() || m_path.IsEmpty())
            return;

        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        if (pType) m_type = pType->GetCurSel();
        if (m_type < 0) m_type = QLItem::Executable;

        CDialog::OnOK();
    }

    afx_msg void OnBnClickedQlBrowse()
    {
        auto& loc = CLocalizationManager::GetInstance();

        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        int type = pType ? pType->GetCurSel() : QLItem::Executable;

        if (type == QLItem::Folder)
        {
            CFolderPickerDialog dlg(NULL, 0, this);
            dlg.m_ofn.lpstrTitle = loc.GetString(_T("QuickLaunch"), _T("BrowseFolder"));
            if (dlg.DoModal() == IDOK)
                SetDlgItemText(IDC_QL_EDIT_PATH, dlg.GetPathName());
        }
        else if (type == QLItem::Url)
        {
            // For URL, just prompt for the URL string
            CString curUrl;
            GetDlgItemText(IDC_QL_EDIT_PATH, curUrl);
            // No file dialog needed for URLs
        }
        else
        {
            CString path;
            GetDlgItemText(IDC_QL_EDIT_PATH, path);
            path.Trim();

            CFileDialog dlg(TRUE, NULL, path, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                _T("All Files (*.*)|*.*||"), this);
            if (dlg.DoModal() == IDOK)
                SetDlgItemText(IDC_QL_EDIT_PATH, dlg.GetPathName());
        }
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CQLItemEditDlg, CDialog)
    ON_BN_CLICKED(IDC_QL_BROWSE, &CQLItemEditDlg::OnBnClickedQlBrowse)
END_MESSAGE_MAP()

// ============================================================================
// CQuickLaunchDlg implementation
// ============================================================================

IMPLEMENT_DYNAMIC(CQuickLaunchDlg, CDialogEx)

CQuickLaunchDlg::CQuickLaunchDlg(std::vector<QLItem>& items, CWnd* pParent)
    : CDialogEx(IDD_QUICK_LAUNCH_DLG, pParent), m_items(items)
{
}

CQuickLaunchDlg::~CQuickLaunchDlg()
{
}

void CQuickLaunchDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CQuickLaunchDlg, CDialogEx)
    ON_BN_CLICKED(IDC_QL_ADD, &CQuickLaunchDlg::OnBnClickedQlAdd)
    ON_BN_CLICKED(IDC_QL_EDIT, &CQuickLaunchDlg::OnBnClickedQlEdit)
    ON_BN_CLICKED(IDC_QL_DELETE, &CQuickLaunchDlg::OnBnClickedQlDelete)
    ON_BN_CLICKED(IDC_QL_UP, &CQuickLaunchDlg::OnBnClickedQlUp)
    ON_BN_CLICKED(IDC_QL_DOWN, &CQuickLaunchDlg::OnBnClickedQlDown)
    ON_NOTIFY(NM_DBLCLK, IDC_QL_LIST, &CQuickLaunchDlg::OnNMDblclkQlList)
    ON_NOTIFY(NM_RCLICK, IDC_QL_LIST, &CQuickLaunchDlg::OnNMRclickQlList)
END_MESSAGE_MAP()

BOOL CQuickLaunchDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(loc.GetString(_T("QuickLaunch"), _T("DlgTitle")));

    // Initialize list control with proportional column widths
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        // Get actual client width from the control (RC defines width=260 at 7,7,260,200)
        CRect rcList;
        pList->GetClientRect(&rcList);
        int totalWidth = rcList.Width();
        // RC width is 260, distribute proportionally: name 30%, type 20%, path 50%
        pList->InsertColumn(0, loc.GetString(_T("QuickLaunch"), _T("ColName")), LVCFMT_LEFT, totalWidth * 30 / 100);
        pList->InsertColumn(1, loc.GetString(_T("QuickLaunch"), _T("ColType")), LVCFMT_LEFT, totalWidth * 20 / 100);
        pList->InsertColumn(2, loc.GetString(_T("QuickLaunch"), _T("ColPath")), LVCFMT_LEFT, totalWidth * 50 / 100);
    }

    // Translate buttons
    SetDlgItemText(IDC_QL_ADD, loc.GetString(_T("QuickLaunch"), _T("BtnAdd")));
    SetDlgItemText(IDC_QL_EDIT, loc.GetString(_T("QuickLaunch"), _T("BtnEdit")));
    SetDlgItemText(IDC_QL_DELETE, loc.GetString(_T("QuickLaunch"), _T("BtnDelete")));
    SetDlgItemText(IDC_QL_UP, loc.GetString(_T("QuickLaunch"), _T("BtnUp")));
    SetDlgItemText(IDC_QL_DOWN, loc.GetString(_T("QuickLaunch"), _T("BtnDown")));
    SetDlgItemText(IDOK, loc.GetString(_T("QuickLaunch"), _T("BtnOK")));

    RefreshList();
    return TRUE;
}

void CQuickLaunchDlg::RefreshList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    pList->DeleteAllItems();

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        int idx = pList->InsertItem((int)i, m_items[i].name);
        pList->SetItemText(idx, 1, CQLItemEditDlg::TypeLabel(m_items[i].type));
        pList->SetItemText(idx, 2, m_items[i].path);
    }
}

bool CQuickLaunchDlg::EditItem(QLItem& item, bool bNew)
{
    auto& loc = CLocalizationManager::GetInstance();

    CQLItemEditDlg dlg(this);
    dlg.m_name = item.name;
    dlg.m_path = item.path;
    dlg.m_type = item.type;
    dlg.m_title = bNew ? loc.GetString(_T("QuickLaunch"), _T("AddTitle"))
                       : loc.GetString(_T("QuickLaunch"), _T("EditTitle"));

    if (dlg.DoModal() == IDOK)
    {
        item.name = dlg.m_name;
        item.path = dlg.m_path;
        item.type = dlg.m_type;
        return true;
    }
    return false;
}

void CQuickLaunchDlg::OnAdd()
{
    QLItem item;
    if (EditItem(item, true))
    {
        m_items.push_back(item);
        RefreshList();
    }
}

void CQuickLaunchDlg::OnEdit()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    int sel = pList->GetSelectionMark();
    if (sel < 0 || sel >= (int)m_items.size())
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("QuickLaunch"), _T("NoSelection")), loc.GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
        return;
    }
    if (EditItem(m_items[sel], false))
    {
        pList->SetItemText(sel, 0, m_items[sel].name);
        pList->SetItemText(sel, 1, m_items[sel].path);
    }
}

void CQuickLaunchDlg::OnDelete()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    int sel = pList->GetSelectionMark();
    if (sel < 0 || sel >= (int)m_items.size()) return;

    auto& loc = CLocalizationManager::GetInstance();
    CString msg;
    msg.Format(loc.GetString(_T("QuickLaunch"), _T("ConfirmDelete")), m_items[sel].name.GetString());
    if (MessageBox(msg, loc.GetString(_T("QuickLaunch"), _T("ConfirmDeleteTitle")), MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        m_items.erase(m_items.begin() + sel);
        RefreshList();
    }
}

void CQuickLaunchDlg::OnMoveUp()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    int sel = pList->GetSelectionMark();
    if (sel <= 0 || sel >= (int)m_items.size()) return;
    std::swap(m_items[sel], m_items[sel - 1]);
    RefreshList();
    pList->SetSelectionMark(sel - 1);
    pList->SetItemState(sel - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void CQuickLaunchDlg::OnMoveDown()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    int sel = pList->GetSelectionMark();
    if (sel < 0 || sel >= (int)m_items.size() - 1) return;
    std::swap(m_items[sel], m_items[sel + 1]);
    RefreshList();
    pList->SetSelectionMark(sel + 1);
    pList->SetItemState(sel + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void CQuickLaunchDlg::OnBnClickedQlAdd() { OnAdd(); }
void CQuickLaunchDlg::OnBnClickedQlEdit() { OnEdit(); }
void CQuickLaunchDlg::OnBnClickedQlDelete() { OnDelete(); }
void CQuickLaunchDlg::OnBnClickedQlUp() { OnMoveUp(); }
void CQuickLaunchDlg::OnBnClickedQlDown() { OnMoveDown(); }

void CQuickLaunchDlg::OnNMDblclkQlList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    if (pItem->iItem >= 0)
        OnEdit();
    *pResult = 0;
}

void CQuickLaunchDlg::OnNMRclickQlList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    auto& loc = CLocalizationManager::GetInstance();

    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;

    // Select the item under cursor if any
    int sel = pItem->iItem;
    if (sel >= 0)
    {
        pList->SetSelectionMark(sel);
        pList->SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }

    CMenu menu;
    menu.CreatePopupMenu();

    menu.AppendMenu(MF_STRING, ID_QL_RCLICK_ADD, loc.GetString(_T("QuickLaunch"), _T("RClickAdd")));
    if (sel >= 0)
    {
        menu.AppendMenu(MF_STRING, ID_QL_RCLICK_EDIT, loc.GetString(_T("QuickLaunch"), _T("RClickEdit")));
        menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DELETE, loc.GetString(_T("QuickLaunch"), _T("RClickDelete")));
        menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
        menu.AppendMenu(MF_STRING, ID_QL_RCLICK_UP, loc.GetString(_T("QuickLaunch"), _T("RClickUp")));
        menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DOWN, loc.GetString(_T("QuickLaunch"), _T("RClickDown")));
    }

    CPoint pt;
    GetCursorPos(&pt);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

BOOL CQuickLaunchDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    UINT id = LOWORD(wParam);
    if (id == ID_QL_RCLICK_ADD)
    {
        OnAdd();
        return TRUE;
    }
    else if (id == ID_QL_RCLICK_EDIT)
    {
        OnEdit();
        return TRUE;
    }
    else if (id == ID_QL_RCLICK_DELETE)
    {
        OnDelete();
        return TRUE;
    }
    else if (id == ID_QL_RCLICK_UP)
    {
        OnMoveUp();
        return TRUE;
    }
    else if (id == ID_QL_RCLICK_DOWN)
    {
        OnMoveDown();
        return TRUE;
    }
    return CDialogEx::OnCommand(wParam, lParam);
}