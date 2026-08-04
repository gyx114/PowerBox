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
#include <ShlObj.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <atlbase.h>

#pragma comment(lib, "Shlwapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Registered message definitions
const UINT WM_QL_CHANGED = ::RegisterWindowMessage(_T("WM_QL_CHANGED_QUICKLAUNCH"));
const UINT WM_QL_CLOSED = ::RegisterWindowMessage(_T("WM_QL_CLOSED_QUICKLAUNCH"));

// ============================================================================
// Static helper: extract icon for a quick launch item
// ============================================================================
HICON CQuickLaunchDlg::ExtractIconForItem(const QLItem& item)
{
    // 1. Try custom icon path
    if (!item.customIconPath.IsEmpty())
    {
        HICON hIcon = NULL;
        ExtractIconEx(item.customIconPath, 0, &hIcon, NULL, 1);
        if (hIcon) return hIcon;
    }

    // 2. For URL items, get the default browser icon
    if (item.type == QLItem::Url)
    {
        // Look up the default browser executable for HTTP protocol
        TCHAR browserPath[MAX_PATH] = {};
        DWORD bufSize = MAX_PATH;
        if (SUCCEEDED(AssocQueryString(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, _T("http"), _T("open"), browserPath, &bufSize)))
        {
            SHFILEINFO shfi = {};
            if (SHGetFileInfo(browserPath, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON))
            {
                return shfi.hIcon;
            }
        }
        // Fallback: try to use the URL itself with SHGetFileInfo
        SHFILEINFO shfi = {};
        if (SHGetFileInfo(item.path, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES))
        {
            return shfi.hIcon;
        }
    }

    // 3. Try SHGetFileInfo for items with a path
    if (item.type != QLItem::HotkeyOnly && !item.path.IsEmpty())
    {
        SHFILEINFO shfi = {};
        if (SHGetFileInfo(item.path, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_LARGEICON))
        {
            return shfi.hIcon;
        }
    }

    // 4. Fallback: default hotkey icon
    HMODULE hRes = AfxGetResourceHandle();
    return (HICON)LoadImage(hRes, MAKEINTRESOURCE(IDI_HOTKEY_DEFAULT), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
}

CString CQuickLaunchDlg::GetIconsDir()
{
    TCHAR szAppData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szAppData)))
    {
        CString dir = CString(szAppData) + _T("\\PowerBox\\icons");
        CreateDirectory(dir, NULL);
        return dir;
    }
    return _T("");
}

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
    HotkeyInfo m_hotkey;
    CString m_customIconPath;
    HICON m_hPreviewIcon = NULL;

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
            case QLItem::HotkeyOnly: return loc.GetString(_T("QuickLaunch"), _T("TypeHotkeyOnly"));
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
        SetDlgItemText(IDC_QL_DROP_HINT, loc.GetString(_T("QuickLaunch"), _T("DropHintAdd")));

        // Localize hotkey controls
        SetDlgItemText(IDC_QL_LABEL_HOTKEY, loc.GetString(_T("QuickLaunch"), _T("LabelHotkey")));
        SetDlgItemText(IDC_QL_BTN_HOTKEY, loc.GetString(_T("QuickLaunch"), _T("BtnHotkey")));

        // Localize icon label
        SetDlgItemText(IDC_QL_LABEL_ICON, loc.GetString(_T("QuickLaunch"), _T("LabelIcon")));

        SetDlgItemText(IDC_QL_EDIT_NAME, m_name);

        // Init type combo
        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        if (pType)
        {
            pType->AddString(TypeLabel(QLItem::Executable));
            pType->AddString(TypeLabel(QLItem::Folder));
            pType->AddString(TypeLabel(QLItem::Url));
            pType->AddString(TypeLabel(QLItem::OtherFile));
            pType->AddString(TypeLabel(QLItem::HotkeyOnly));
            pType->SetCurSel(m_type);
        }

        // Init hotkey display
        SetDlgItemText(IDC_QL_EDIT_HOTKEY, m_hotkey.ToDisplay());

        // Localize buttons
        SetDlgItemText(IDC_QL_BROWSE, loc.GetString(_T("QuickLaunch"), _T("BtnBrowse")));
        SetDlgItemText(IDOK, loc.GetString(_T("QuickLaunch"), _T("BtnOK")));
        SetDlgItemText(IDCANCEL, loc.GetString(_T("QuickLaunch"), _T("BtnCancel")));

        SetDlgItemText(IDC_QL_EDIT_PATH, m_path);

        // Enable/disable path controls based on type
        UpdatePathControls(m_type);

        // Localize icon controls
        SetDlgItemText(IDC_BTN_QL_CHANGE_ICON, loc.GetString(_T("QuickLaunch"), _T("BtnChangeIcon")));
        SetDlgItemText(IDC_QL_BTN_RESET_ICON, loc.GetString(_T("QuickLaunch"), _T("BtnResetIcon")));

        // Load and display icon preview
        UpdateIconPreview();

        // Accept drag-drop
        DragAcceptFiles(TRUE);

        return TRUE;
    }

    virtual void OnOK()
    {
        GetDlgItemText(IDC_QL_EDIT_NAME, m_name);
        GetDlgItemText(IDC_QL_EDIT_PATH, m_path);
        m_name.Trim();
        m_path.Trim();
        if (m_name.IsEmpty()) return;

        // Read type from combo FIRST, then validate path
        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        if (pType) m_type = pType->GetCurSel();
        if (m_type < 0) m_type = QLItem::Executable;

        // HotkeyOnly type: path can be empty
        if (m_type != QLItem::HotkeyOnly && m_path.IsEmpty())
            return;

        // Save hotkey (m_hotkey already set via capture dialog)

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

    // Hotkey capture button
    afx_msg void OnBnClickedQlBtnHotkey()
    {
        CHotkeyCaptureDlg dlg(this, m_hotkey);
        if (dlg.DoModal() == IDOK)
        {
            m_hotkey = dlg.m_result;
            SetDlgItemText(IDC_QL_EDIT_HOTKEY, m_hotkey.ToDisplay());
        }
    }

    // Type combo change: enable/disable path controls
    afx_msg void OnCbnSelchangeQlComboType()
    {
        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        int type = pType ? pType->GetCurSel() : QLItem::Executable;
        UpdatePathControls(type);
    }

    // Enable/disable path edit and browse button based on type
    void UpdatePathControls(int type)
    {
        BOOL enable = (type != QLItem::HotkeyOnly);
        GetDlgItem(IDC_QL_LABEL_PATH)->EnableWindow(enable);
        GetDlgItem(IDC_QL_EDIT_PATH)->EnableWindow(enable);
        GetDlgItem(IDC_QL_BROWSE)->EnableWindow(enable);
        GetDlgItem(IDC_QL_DROP_HINT)->EnableWindow(enable);
    }

    afx_msg void OnDropFiles(HDROP hDropInfo)
    {
        TCHAR buf[MAX_PATH] = {};
        DragQueryFile(hDropInfo, 0, buf, MAX_PATH);
        CString path(buf);

        // Resolve shortcut
        CString target;
        int type = QLItem::OtherFile;
        if (CQuickLaunchDlg::ResolveShortcut(path, target, type))
        {
            path = target;
        }
        else
        {
            DWORD attr = GetFileAttributes(path);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                type = QLItem::Folder;
            else if (path.Right(4).CompareNoCase(_T(".exe")) == 0)
                type = QLItem::Executable;
        }

        // Extract file name as default name
        CString name = path;
        int pos = name.ReverseFind(_T('\\'));
        if (pos != -1) name = name.Mid(pos + 1);
        int dot = name.ReverseFind(_T('.'));
        if (dot != -1) name = name.Left(dot);

        SetDlgItemText(IDC_QL_EDIT_PATH, path);
        SetDlgItemText(IDC_QL_EDIT_NAME, name);

        CComboBox* pType = (CComboBox*)GetDlgItem(IDC_QL_COMBO_TYPE);
        if (pType) pType->SetCurSel(type);

        DragFinish(hDropInfo);
    }

    // Update icon preview in the edit dialog
    void UpdateIconPreview()
    {
        if (m_hPreviewIcon) { DestroyIcon(m_hPreviewIcon); m_hPreviewIcon = NULL; }

        QLItem tmp;
        tmp.name = m_name;
        tmp.path = m_path;
        tmp.type = m_type;
        tmp.customIconPath = m_customIconPath;
        m_hPreviewIcon = CQuickLaunchDlg::ExtractIconForItem(tmp);

        CWnd* pPreview = GetDlgItem(IDC_QL_ICON_PREVIEW);
        if (pPreview && m_hPreviewIcon)
        {
            pPreview->SendMessage(STM_SETICON, (WPARAM)m_hPreviewIcon, 0);
        }
    }

    // Change icon: open file dialog to pick .ico/.exe/.dll
    afx_msg void OnBnClickedChangeIcon()
    {
        auto& loc = CLocalizationManager::GetInstance();
        CFileDialog dlg(TRUE, _T("ico"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
            _T("Icon Files (*.ico)|*.ico|Executable Files (*.exe;*.dll)|*.exe;*.dll|All Files (*.*)|*.*||"), this);
        dlg.m_ofn.lpstrTitle = loc.GetString(_T("QuickLaunch"), _T("SelectIconFile"));
        if (dlg.DoModal() == IDOK)
        {
            CString srcPath = dlg.GetPathName();
            CString ext = srcPath.Mid(srcPath.ReverseFind(_T('.')));
            ext.MakeLower();

            if (ext == _T(".ico"))
            {
                // Copy .ico to icons directory for persistence
                CString iconsDir = CQuickLaunchDlg::GetIconsDir();
                if (!iconsDir.IsEmpty())
                {
                    CString safeName = m_name;
                    safeName.Replace(_T('\\'), _T('_'));
                    safeName.Replace(_T('/'), _T('_'));
                    safeName.Replace(_T(':'), _T('_'));
                    safeName.Replace(_T('|'), _T('_'));
                    safeName.Replace(_T('*'), _T('_'));
                    safeName.Replace(_T('?'), _T('_'));
                    safeName.Replace(_T('"'), _T('_'));
                    safeName.Replace(_T('<'), _T('_'));
                    safeName.Replace(_T('>'), _T('_'));
                    CString dstPath = iconsDir + _T("\\") + safeName + _T(".ico");
                    CopyFile(srcPath, dstPath, FALSE);
                    m_customIconPath = dstPath;
                }
                else
                {
                    m_customIconPath = srcPath;
                }
            }
            else
            {
                // For .exe/.dll, save path as-is
                m_customIconPath = srcPath;
            }
            UpdateIconPreview();
        }
    }

    // Reset icon to default
    afx_msg void OnBnClickedResetIcon()
    {
        m_customIconPath.Empty();
        UpdateIconPreview();
    }

    // Destructor: clean up preview icon (must be public for stack allocation)
public:
    ~CQLItemEditDlg()
    {
        if (m_hPreviewIcon) DestroyIcon(m_hPreviewIcon);
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CQLItemEditDlg, CDialog)
    ON_BN_CLICKED(IDC_QL_BROWSE, &CQLItemEditDlg::OnBnClickedQlBrowse)
    ON_BN_CLICKED(IDC_QL_BTN_HOTKEY, &CQLItemEditDlg::OnBnClickedQlBtnHotkey)
    ON_BN_CLICKED(IDC_BTN_QL_CHANGE_ICON, &CQLItemEditDlg::OnBnClickedChangeIcon)
    ON_BN_CLICKED(IDC_QL_BTN_RESET_ICON, &CQLItemEditDlg::OnBnClickedResetIcon)
    ON_CBN_SELCHANGE(IDC_QL_COMBO_TYPE, &CQLItemEditDlg::OnCbnSelchangeQlComboType)
    ON_WM_DROPFILES()
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
    ON_BN_CLICKED(IDC_BTN_QL_CHANGE_ICON, &CQuickLaunchDlg::OnBnClickedQlChangeIcon)
    ON_NOTIFY(NM_DBLCLK, IDC_QL_LIST, &CQuickLaunchDlg::OnNMDblclkQlList)
    ON_NOTIFY(NM_RCLICK, IDC_QL_LIST, &CQuickLaunchDlg::OnNMRclickQlList)
    ON_NOTIFY(LVN_BEGINDRAG, IDC_QL_LIST, &CQuickLaunchDlg::OnLvnBeginDrag)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_QL_LIST, &CQuickLaunchDlg::OnCustomDrawList)
    ON_WM_DROPFILES()
    ON_WM_CLOSE()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

BOOL CQuickLaunchDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(loc.GetString(_T("QuickLaunch"), _T("DlgTitle")));

    // Drop hint
    SetDlgItemText(IDC_QL_DROP_HINT, loc.GetString(_T("QuickLaunch"), _T("DropHintManage")));

    // Initialize list control with proportional column widths
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        // Get actual client width from the control (RC defines width=260 at 7,7,260,185)
        CRect rcList;
        pList->GetClientRect(&rcList);
        int totalWidth = rcList.Width();
        // RC width is 260, distribute proportionally: name 25%, type 20%, path 40%, hotkey 15%
        pList->InsertColumn(0, loc.GetString(_T("QuickLaunch"), _T("ColName")), LVCFMT_LEFT, totalWidth * 25 / 100);
        pList->InsertColumn(1, loc.GetString(_T("QuickLaunch"), _T("ColType")), LVCFMT_LEFT, totalWidth * 20 / 100);
        pList->InsertColumn(2, loc.GetString(_T("QuickLaunch"), _T("ColPath")), LVCFMT_LEFT, totalWidth * 40 / 100);
        pList->InsertColumn(3, loc.GetString(_T("QuickLaunch"), _T("ColHotkey")), LVCFMT_LEFT, totalWidth * 15 / 100);
    }

    // Translate buttons
    SetDlgItemText(IDC_QL_ADD, loc.GetString(_T("QuickLaunch"), _T("BtnAdd")));
    SetDlgItemText(IDC_QL_EDIT, loc.GetString(_T("QuickLaunch"), _T("BtnEdit")));
    SetDlgItemText(IDC_QL_DELETE, loc.GetString(_T("QuickLaunch"), _T("BtnDelete")));
    SetDlgItemText(IDC_QL_UP, loc.GetString(_T("QuickLaunch"), _T("BtnUp")));
    SetDlgItemText(IDC_QL_DOWN, loc.GetString(_T("QuickLaunch"), _T("BtnDown")));
    SetDlgItemText(IDC_BTN_QL_CHANGE_ICON, loc.GetString(_T("QuickLaunch"), _T("BtnChangeIcon")));
    SetDlgItemText(IDCANCEL, loc.GetString(_T("QuickLaunch"), _T("BtnClose")));

    // Initialize image list for list icons (16x16 small icons for report view)
    if (pList && m_imgList.GetSafeHandle() == NULL)
    {
        m_imgList.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 32);
        pList->SetImageList(&m_imgList, LVSIL_SMALL);
    }

    // Accept drag-drop
    DragAcceptFiles(TRUE);

    RefreshList();
    // Place window centered on parent
    CenterWindow();
    return TRUE;
}

void CQuickLaunchDlg::PostNcDestroy()
{
    delete this;
}

void CQuickLaunchDlg::OnClose()
{
    // X button → WM_CLOSE → OnClose(): notify parent before destroying
    NotifyParent();
    CWnd* pParent = GetParent();
    if (pParent && ::IsWindow(pParent->m_hWnd))
        pParent->SendMessage(WM_QL_CLOSED, 0, 0);
    DestroyWindow();
}

void CQuickLaunchDlg::OnCancel()
{
    // "关闭" button → IDCANCEL → OnCancel(): notify parent before destroying
    NotifyParent();
    CWnd* pParent = GetParent();
    if (pParent && ::IsWindow(pParent->m_hWnd))
        pParent->SendMessage(WM_QL_CLOSED, 0, 0);
    CDialogEx::OnCancel();
}

void CQuickLaunchDlg::NotifyParent()
{
    // Notify parent (main dialog) to save and refresh
    CWnd* pParent = GetParent();
    if (pParent && ::IsWindow(pParent->m_hWnd))
    {
        pParent->SendMessage(WM_QL_CHANGED, 0, 0);
    }
}

void CQuickLaunchDlg::RefreshList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    pList->DeleteAllItems();

    // Rebuild image list
    if (m_imgList.GetSafeHandle()) m_imgList.DeleteImageList();
    m_imgList.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 32);
    pList->SetImageList(&m_imgList, LVSIL_SMALL);

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        // Extract icon and add to image list
        HICON hIcon = ExtractIconForItem(m_items[i]);
        int iconIdx = -1;
        if (hIcon)
        {
            iconIdx = m_imgList.Add(hIcon);
            DestroyIcon(hIcon);
        }

        int idx = pList->InsertItem((int)i, m_items[i].name, iconIdx);
        pList->SetItemText(idx, 1, CQLItemEditDlg::TypeLabel(m_items[i].type));
        pList->SetItemText(idx, 2, m_items[i].path);
        pList->SetItemText(idx, 3, m_items[i].hotkey.ToDisplay());
    }
}

bool CQuickLaunchDlg::EditItem(QLItem& item, bool bNew)
{
    auto& loc = CLocalizationManager::GetInstance();

    CQLItemEditDlg dlg(this);
    dlg.m_name = item.name;
    dlg.m_path = item.path;
    dlg.m_type = item.type;
    dlg.m_hotkey = item.hotkey;
    dlg.m_customIconPath = item.customIconPath;
    dlg.m_title = bNew ? loc.GetString(_T("QuickLaunch"), _T("AddTitle"))
                       : loc.GetString(_T("QuickLaunch"), _T("EditTitle"));

    if (dlg.DoModal() == IDOK)
    {
        item.name = dlg.m_name;
        item.path = dlg.m_path;
        item.type = dlg.m_type;
        item.hotkey = dlg.m_hotkey;
        item.customIconPath = dlg.m_customIconPath;
        return true;
    }
    return false;
}

// Static: open the item-specific edit dialog without the overview management window
bool CQuickLaunchDlg::EditSingleItem(QLItem& item, bool bNew, CWnd* pParent)
{
    auto& loc = CLocalizationManager::GetInstance();

    CQLItemEditDlg dlg(pParent);
    dlg.m_name = item.name;
    dlg.m_path = item.path;
    dlg.m_type = item.type;
    dlg.m_hotkey = item.hotkey;
    dlg.m_customIconPath = item.customIconPath;
    dlg.m_title = bNew ? loc.GetString(_T("QuickLaunch"), _T("AddTitle"))
                       : loc.GetString(_T("QuickLaunch"), _T("EditTitle"));

    if (dlg.DoModal() == IDOK)
    {
        item.name = dlg.m_name;
        item.path = dlg.m_path;
        item.type = dlg.m_type;
        item.hotkey = dlg.m_hotkey;
        item.customIconPath = dlg.m_customIconPath;
        return true;
    }
    return false;
}

void CQuickLaunchDlg::OnAdd()
{
    auto& loc = CLocalizationManager::GetInstance();
    if ((int)m_items.size() >= MAX_QL_ITEMS)
    {
        CString msg;
        msg.Format(loc.GetString(_T("QuickLaunch"), _T("MaxItemsReached")), MAX_QL_ITEMS);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    QLItem item;
    if (EditItem(item, true))
    {
        m_items.push_back(item);
        RefreshList();
        NotifyParent();
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
        RefreshList();
        NotifyParent();
    }
}

std::vector<int> CQuickLaunchDlg::GetSelectedIndices()
{
    std::vector<int> indices;
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return indices;

    POSITION pos = pList->GetFirstSelectedItemPosition();
    while (pos)
    {
        int idx = pList->GetNextSelectedItem(pos);
        indices.push_back(idx);
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void CQuickLaunchDlg::OnDelete()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    auto sel = GetSelectedIndices();
    if (sel.empty())
    {
        auto& loc = CLocalizationManager::GetInstance();
        MessageBox(loc.GetString(_T("QuickLaunch"), _T("NoSelection")), loc.GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
        return;
    }

    auto& loc = CLocalizationManager::GetInstance();
    CString msg;
    if (sel.size() == 1)
    {
        msg.Format(loc.GetString(_T("QuickLaunch"), _T("ConfirmDelete")), m_items[sel[0]].name.GetString());
    }
    else
    {
        msg.Format(loc.GetString(_T("QuickLaunch"), _T("ConfirmDeleteMultiple")), (int)sel.size());
    }
    if (MessageBox(msg, loc.GetString(_T("QuickLaunch"), _T("ConfirmDeleteTitle")), MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        // Sort in reverse to avoid index shifting
        for (auto it = sel.rbegin(); it != sel.rend(); ++it)
        {
            int idx = *it;
            if (idx >= 0 && idx < (int)m_items.size())
                m_items.erase(m_items.begin() + idx);
        }
        RefreshList();
        NotifyParent();
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
    NotifyParent();
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
    NotifyParent();
}

void CQuickLaunchDlg::OnMoveUpSelected()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    auto sel = GetSelectedIndices();
    if (sel.empty()) return;

    // Move each selected item up by one slot. Process from top so each
    // swap only affects adjacent pairs.
    for (int idx : sel)
    {
        if (idx <= 0) continue;
        if (idx >= (int)m_items.size()) continue;
        std::swap(m_items[idx], m_items[idx - 1]);
    }

    RefreshList();

    // Re-select items at their new positions (each moved up by 1)
    pList->SetItemState(-1, 0, LVIS_SELECTED);
    for (int idx : sel)
    {
        int newPos = (idx > 0) ? idx - 1 : 0;
        pList->SetItemState(newPos, LVIS_SELECTED, LVIS_SELECTED);
    }
    if (!sel.empty())
    {
        int focusPos = (sel[0] > 0) ? sel[0] - 1 : 0;
        pList->SetSelectionMark(focusPos);
        pList->SetItemState(focusPos, LVIS_FOCUSED, LVIS_FOCUSED);
    }
    NotifyParent();
}

void CQuickLaunchDlg::OnMoveDownSelected()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    auto sel = GetSelectedIndices();
    if (sel.empty()) return;
    int nCount = (int)m_items.size();

    // Process from bottom to top so adjacent swaps don't interfere
    for (auto it = sel.rbegin(); it != sel.rend(); ++it)
    {
        int idx = *it;
        if (idx < 0 || idx >= nCount - 1) continue;
        std::swap(m_items[idx], m_items[idx + 1]);
    }

    RefreshList();

    // Re-select items at their new positions (each moved down by 1)
    pList->SetItemState(-1, 0, LVIS_SELECTED);
    for (int idx : sel)
    {
        int newPos = (idx < nCount - 1) ? idx + 1 : nCount - 1;
        pList->SetItemState(newPos, LVIS_SELECTED, LVIS_SELECTED);
    }
    if (!sel.empty())
    {
        int lastIdx = sel.back();
        int focusPos = (lastIdx < nCount - 1) ? lastIdx + 1 : nCount - 1;
        pList->SetSelectionMark(focusPos);
        pList->SetItemState(focusPos, LVIS_FOCUSED, LVIS_FOCUSED);
    }
    NotifyParent();
}

void CQuickLaunchDlg::MoveSelectedItemsTo(int nTargetIndex)
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    auto sel = GetSelectedIndices();
    if (sel.empty()) return;

    int nCount = (int)m_items.size();
    if (nTargetIndex < 0 || nTargetIndex > nCount) return;

    // Collect selected entries
    std::vector<QLItem> picked;
    for (int idx : sel)
    {
        if (idx >= 0 && idx < (int)m_items.size())
            picked.push_back(m_items[idx]);
    }

    // Remove them from the vector (back-to-front)
    for (auto it = sel.rbegin(); it != sel.rend(); ++it)
    {
        int idx = *it;
        if (idx >= 0 && idx < (int)m_items.size())
            m_items.erase(m_items.begin() + idx);
    }

    // Adjust target index: if target was after removed block, decrement
    int adjusted = nTargetIndex;
    for (int idx : sel)
    {
        if (idx < nTargetIndex) adjusted--;
    }
    if (adjusted < 0) adjusted = 0;
    if (adjusted > (int)m_items.size()) adjusted = (int)m_items.size();

    m_items.insert(m_items.begin() + adjusted, picked.begin(), picked.end());

    RefreshList();

    // Re-select the moved items
    pList->SetItemState(-1, 0, LVIS_SELECTED);
    for (size_t i = 0; i < picked.size(); i++)
    {
        int newPos = adjusted + (int)i;
        pList->SetItemState(newPos, LVIS_SELECTED, LVIS_SELECTED);
    }
    pList->SetSelectionMark(adjusted);
    pList->SetItemState(adjusted, LVIS_FOCUSED, LVIS_FOCUSED);
    NotifyParent();
}

void CQuickLaunchDlg::OnBnClickedQlAdd() { OnAdd(); }
void CQuickLaunchDlg::OnBnClickedQlEdit() { OnEdit(); }
void CQuickLaunchDlg::OnBnClickedQlDelete() { OnDelete(); }
void CQuickLaunchDlg::OnBnClickedQlUp() { OnMoveUp(); }
void CQuickLaunchDlg::OnBnClickedQlDown() { OnMoveDown(); }

void CQuickLaunchDlg::OnBnClickedQlChangeIcon()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;
    int sel = pList->GetSelectionMark();
    if (sel < 0 || sel >= (int)m_items.size())
    {
        MessageBox(loc.GetString(_T("QuickLaunch"), _T("NoSelection")), loc.GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
        return;
    }

    CFileDialog dlg(TRUE, _T("ico"), NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
        _T("Icon Files (*.ico)|*.ico|Executable Files (*.exe;*.dll)|*.exe;*.dll|All Files (*.*)|*.*||"), this);
    dlg.m_ofn.lpstrTitle = loc.GetString(_T("QuickLaunch"), _T("SelectIconFile"));
    if (dlg.DoModal() == IDOK)
    {
        CString srcPath = dlg.GetPathName();
        CString ext = srcPath.Mid(srcPath.ReverseFind(_T('.')));
        ext.MakeLower();

        if (ext == _T(".ico"))
        {
            CString iconsDir = GetIconsDir();
            if (!iconsDir.IsEmpty())
            {
                CString safeName = m_items[sel].name;
                safeName.Replace(_T('\\'), _T('_'));
                safeName.Replace(_T('/'), _T('_'));
                safeName.Replace(_T(':'), _T('_'));
                safeName.Replace(_T('|'), _T('_'));
                safeName.Replace(_T('*'), _T('_'));
                safeName.Replace(_T('?'), _T('_'));
                safeName.Replace(_T('"'), _T('_'));
                safeName.Replace(_T('<'), _T('_'));
                safeName.Replace(_T('>'), _T('_'));
                CString dstPath = iconsDir + _T("\\") + safeName + _T(".ico");
                CopyFile(srcPath, dstPath, FALSE);
                m_items[sel].customIconPath = dstPath;
            }
            else
            {
                m_items[sel].customIconPath = srcPath;
            }
        }
        else
        {
            m_items[sel].customIconPath = srcPath;
        }
        RefreshList();
        NotifyParent();
    }
}

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

    int nSelCount = pList->GetSelectedCount();
    int sel = pItem->iItem;

    // If right-clicking on an unselected item, select it (preserve multi-select if Ctrl/Shift held)
    // For simplicity, if clicking on a non-selected item, select only that item
    if (sel >= 0)
    {
        // Check if the clicked item is already selected
        bool bAlreadySelected = (pList->GetItemState(sel, LVIS_SELECTED) & LVIS_SELECTED) != 0;
        if (!bAlreadySelected || nSelCount == 0)
        {
            pList->SetItemState(-1, 0, LVIS_SELECTED);
            pList->SetSelectionMark(sel);
            pList->SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            nSelCount = 1;
        }
    }

    CMenu menu;
    menu.CreatePopupMenu();

    menu.AppendMenu(MF_STRING, ID_QL_RCLICK_ADD, loc.GetString(_T("QuickLaunch"), _T("RClickAdd")));
    if (sel >= 0)
    {
        menu.AppendMenu(MF_STRING, ID_QL_RCLICK_EDIT, loc.GetString(_T("QuickLaunch"), _T("RClickEdit")));
        if (nSelCount > 1)
        {
            // Multi-select: show batch operations
            CString strDeleteSel, strUpSel, strDownSel;
            strDeleteSel.Format(loc.GetString(_T("QuickLaunch"), _T("RClickDeleteSelected")), nSelCount);
            strUpSel.Format(loc.GetString(_T("QuickLaunch"), _T("RClickUpSelected")), nSelCount);
            strDownSel.Format(loc.GetString(_T("QuickLaunch"), _T("RClickDownSelected")), nSelCount);
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DELETE_SEL, strDeleteSel);
            menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_UP_SEL, strUpSel);
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DOWN_SEL, strDownSel);
        }
        else
        {
            // Single item: show individual operations
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DELETE, loc.GetString(_T("QuickLaunch"), _T("RClickDelete")));
            menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_UP, loc.GetString(_T("QuickLaunch"), _T("RClickUp")));
            menu.AppendMenu(MF_STRING, ID_QL_RCLICK_DOWN, loc.GetString(_T("QuickLaunch"), _T("RClickDown")));
        }
    }

    CPoint pt;
    GetCursorPos(&pt);
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

BOOL CQuickLaunchDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    UINT id = LOWORD(wParam);
    switch (id)
    {
    case ID_QL_RCLICK_ADD:         OnAdd(); return TRUE;
    case ID_QL_RCLICK_EDIT:        OnEdit(); return TRUE;
    case ID_QL_RCLICK_DELETE:      OnDelete(); return TRUE;
    case ID_QL_RCLICK_UP:          OnMoveUp(); return TRUE;
    case ID_QL_RCLICK_DOWN:        OnMoveDown(); return TRUE;
    case ID_QL_RCLICK_DELETE_SEL:  OnDelete(); return TRUE;
    case ID_QL_RCLICK_UP_SEL:      OnMoveUpSelected(); return TRUE;
    case ID_QL_RCLICK_DOWN_SEL:    OnMoveDownSelected(); return TRUE;
    }
    return CDialogEx::OnCommand(wParam, lParam);
}

// ============================================================================
// Shortcut (.lnk) resolution
// ============================================================================
bool CQuickLaunchDlg::ResolveShortcut(const CString& path, CString& outTarget, int& outType)
{
    if (path.Right(4).CompareNoCase(_T(".lnk")) != 0)
        return false;

    CComPtr<IShellLink> psl;
    if (FAILED(psl.CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER)))
        return false;

    CComQIPtr<IPersistFile> ppf(psl);
    if (!ppf) return false;

    if (FAILED(ppf->Load(path, STGM_READ)))
        return false;

    if (FAILED(psl->Resolve(NULL, SLR_ANY_MATCH | SLR_NO_UI)))
        return false;

    TCHAR buf[MAX_PATH] = {};
    WIN32_FIND_DATA wfd = {};
    if (FAILED(psl->GetPath(buf, MAX_PATH, &wfd, 0)))
        return false;

    outTarget = buf;
    DWORD attr = GetFileAttributes(outTarget);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        outType = QLItem::Folder;
    else if (outTarget.Right(4).CompareNoCase(_T(".exe")) == 0)
        outType = QLItem::Executable;
    else
        outType = QLItem::OtherFile;

    return true;
}

// ============================================================================
// Drag-drop handler for the management dialog
// ============================================================================
void CQuickLaunchDlg::OnDropFiles(HDROP hDropInfo)
{
    auto& loc = CLocalizationManager::GetInstance();
    TCHAR buf[MAX_PATH] = {};
    UINT count = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);

    for (UINT i = 0; i < count; ++i)
    {
        if ((int)m_items.size() >= MAX_QL_ITEMS)
        {
            CString msg;
            msg.Format(loc.GetString(_T("QuickLaunch"), _T("MaxItemsReached")), MAX_QL_ITEMS);
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
            break;
        }

        DragQueryFile(hDropInfo, i, buf, MAX_PATH);
        CString path(buf);

        // Try to resolve shortcut, otherwise determine type from path
        CString target;
        int type = QLItem::OtherFile;
        if (ResolveShortcut(path, target, type))
        {
            path = target;
        }
        else
        {
            DWORD attr = GetFileAttributes(path);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                type = QLItem::Folder;
            else if (path.Right(4).CompareNoCase(_T(".exe")) == 0)
                type = QLItem::Executable;
        }

        // Extract name from path
        CString name = path;
        int pos = name.ReverseFind(_T('\\'));
        if (pos != -1) name = name.Mid(pos + 1);
        int dot = name.ReverseFind(_T('.'));
        if (dot != -1) name = name.Left(dot);

        QLItem item;
        item.name = name;
        item.path = path;
        item.type = type;
        if (EditItem(item, true))
        {
            m_items.push_back(item);
            NotifyParent();
        }
    }

    RefreshList();
    DragFinish(hDropInfo);
}

// ============================================================================
// Drag-and-drop reordering (list item drag)
// ============================================================================
void CQuickLaunchDlg::OnLvnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
    if (!pList) return;

    int nSel = pList->GetSelectedCount();
    if (nSel == 0) { *pResult = 0; return; }

    m_nDragSourceIndex = pNMLV->iItem;
    m_nDropTargetIndex = -1;
    m_nDropLineY = -1;
    m_bDragging = true;

    pList->SetFocus();
    SetCapture();
    *pResult = 1;
}

void CQuickLaunchDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
        if (pList)
        {
            CPoint ptList = point;
            ClientToScreen(&ptList);
            pList->ScreenToClient(&ptList);

            int nHover = pList->HitTest(ptList);
            int nItemCount = pList->GetItemCount();
            int nNewLineY = -1;
            int nNewTarget = -1;

            if (nHover >= 0)
            {
                CRect rcItem;
                pList->GetItemRect(nHover, &rcItem, LVIR_BOUNDS);
                if (ptList.y < rcItem.CenterPoint().y)
                {
                    nNewLineY = rcItem.top;
                    nNewTarget = nHover;
                }
                else
                {
                    nNewLineY = rcItem.bottom;
                    nNewTarget = nHover + 1;
                }
            }
            else if (nItemCount > 0)
            {
                CRect rcLast;
                pList->GetItemRect(nItemCount - 1, &rcLast, LVIR_BOUNDS);
                if (ptList.y >= rcLast.bottom)
                {
                    nNewLineY = rcLast.bottom;
                    nNewTarget = nItemCount;
                }
            }

            int nNewLineScreenY = -1;
            if (nNewLineY >= 0)
            {
                CPoint ptLine(0, nNewLineY);
                pList->ClientToScreen(&ptLine);
                nNewLineScreenY = ptLine.y;
            }

            if (nNewLineScreenY != m_nDropLineY)
            {
                m_nDropLineY = nNewLineScreenY;
                m_nDropTargetIndex = nNewTarget;
                pList->Invalidate();
            }
        }
    }
    CDialogEx::OnMouseMove(nFlags, point);
}

void CQuickLaunchDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        ReleaseCapture();

        CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
        if (pList && m_nDropTargetIndex >= 0)
        {
            MoveSelectedItemsTo(m_nDropTargetIndex);
        }

        m_bDragging = false;
        m_nDragSourceIndex = -1;
        m_nDropTargetIndex = -1;
        m_nDropLineY = -1;
        if (pList) pList->Invalidate();
    }
    CDialogEx::OnLButtonUp(nFlags, point);
}

void CQuickLaunchDlg::OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVCUSTOMDRAW pCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    if (pCD->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
        *pResult = CDRF_NOTIFYPOSTPAINT;
        return;
    }

    if (pCD->nmcd.dwDrawStage == CDDS_POSTPAINT)
    {
        if (!m_bDragging || m_nDropLineY < 0) return;

        CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_QL_LIST);
        if (!pList) return;

        CDC dc;
        dc.Attach(pCD->nmcd.hdc);

        // Convert screen-space Y back to list-client space
        CPoint ptLine(0, m_nDropLineY);
        pList->ScreenToClient(&ptLine);
        int nY = ptLine.y;

        // Draw a 2-pixel-high blue line at the insertion position
        CRect rcClient;
        pList->GetClientRect(&rcClient);
        CPen pen(PS_SOLID, 2, RGB(0, 100, 255));
        CPen* pOldPen = dc.SelectObject(&pen);
        dc.MoveTo(rcClient.left, nY);
        dc.LineTo(rcClient.right, nY);
        dc.SelectObject(pOldPen);
        pen.DeleteObject();

        dc.Detach();
        return;
    }
}