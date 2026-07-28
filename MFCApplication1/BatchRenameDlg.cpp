#include "pch.h"
#include "framework.h"
#include "BatchRenameDlg.h"
#include "RegexGuideDlg.h"
#include "resource.h"
#include <algorithm>
#include <set>
#include <regex>
#include <shellapi.h>

namespace fs = std::filesystem;

// Right-click menu command IDs
#define IDM_FILE_MARK_DELETE    32820
#define IDM_FILE_UNMARK_DELETE  32821
#define IDM_FILE_CHANGE_EXT     32822
#define IDM_FILE_RESTORE_EXT    32823
#define IDM_FOLDER_RENAME       32824
#define IDM_FOLDER_MOVE         32825
#define IDM_FOLDER_DELETE       32826
#define IDM_FOLDER_EXPLORE      32827
#define IDM_FILE_EXPLORE        32828
#define IDM_FILE_IGNORE         32829
#define IDM_FILE_UNIGNORE       32830
#define IDM_FILE_TRACK          32831
#define IDM_FILE_UNTRACK        32832
#define IDM_FILE_MOVE_UP        32833
#define IDM_FILE_MOVE_DOWN      32834
#define IDM_FILE_MOVE_TO_UP     32835
#define IDM_FILE_MOVE_TO_DOWN   32836

// Simple input dialog class
class CInputDialog : public CDialogEx
{
public:
    CInputDialog(LPCTSTR prompt, LPCTSTR title, LPCTSTR defaultVal)
        : CDialogEx(IDD_INPUT_DLG), m_prompt(prompt), m_title(title), m_default(defaultVal) {}

    CString GetInput() const { return m_input; }

protected:
    virtual BOOL OnInitDialog() override
    {
        CDialogEx::OnInitDialog();
        SetWindowText(m_title);
        SetDlgItemText(IDC_INPUT_PROMPT, m_prompt);
        SetDlgItemText(IDC_INPUT_EDIT, m_default);
        GetDlgItem(IDC_INPUT_EDIT)->SetFocus();
        return FALSE;
    }
    virtual void DoDataExchange(CDataExchange* pDX) override
    {
        CDialogEx::DoDataExchange(pDX);
        DDX_Text(pDX, IDC_INPUT_EDIT, m_input);
    }

    void OnOK() override
    {
        UpdateData(TRUE);
        CDialogEx::OnOK();
    }

    DECLARE_MESSAGE_MAP()

    CString m_prompt, m_title, m_default, m_input;
    enum { IDD = IDD_INPUT_DLG };
};

BEGIN_MESSAGE_MAP(CInputDialog, CDialogEx)
END_MESSAGE_MAP()

// Move a file or folder to the recycle bin using SHFileOperation
static bool MoveToRecycleBin(const fs::path& path)
{
    std::wstring wPath = path.wstring();
    // SHFileOperation requires double-null-terminated string
    std::wstring buffer = wPath + L'\0' + L'\0';
    SHFILEOPSTRUCTW fos = {};
    fos.wFunc = FO_DELETE;
    fos.pFrom = buffer.c_str();
    fos.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOF_SILENT;
    return ::SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted;
}

CBatchRenameDlg::CBatchRenameDlg(CWnd* pParent, const CString& initialFolder)
    : CDialogEx(IDD_BATCH_RENAME_DLG, pParent)
    , m_folderPath(initialFolder)
{
}

void CBatchRenameDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBatchRenameDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RENAME_BROWSE, &CBatchRenameDlg::OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_BTN_RENAME_PREVIEW, &CBatchRenameDlg::OnBnClickedFilePreview)
    ON_BN_CLICKED(IDC_BTN_RENAME_EXECUTE, &CBatchRenameDlg::OnBnClickedFileExecute)
    ON_BN_CLICKED(IDC_BTN_RENAME_UNDO, &CBatchRenameDlg::OnBnClickedRenameUndo)
    ON_BN_CLICKED(IDC_BTN_RENAME_CLEAR, &CBatchRenameDlg::OnBnClickedClear)
    ON_BN_CLICKED(IDC_BTN_RENAME_REFRESH, &CBatchRenameDlg::OnBnClickedRefresh)
    ON_BN_CLICKED(IDC_BTN_RENAME_REGEX_HELP, &CBatchRenameDlg::OnBnClickedRegexHelp)
    ON_EN_CHANGE(IDC_EDIT_RENAME_PREFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_SUFFIX, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_FROM, &CBatchRenameDlg::OnEnChangeRule)
    ON_EN_CHANGE(IDC_EDIT_RENAME_REPLACE_TO, &CBatchRenameDlg::OnEnChangeRule)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_FOLDER, &CBatchRenameDlg::OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_BTN_FOLDER_RENAME, &CBatchRenameDlg::OnBnClickedFolderRename)
    ON_BN_CLICKED(IDC_BTN_FOLDER_MOVE, &CBatchRenameDlg::OnBnClickedFolderMove)
    ON_BN_CLICKED(IDC_BTN_FOLDER_DELETE, &CBatchRenameDlg::OnBnClickedFolderDelete)
    ON_BN_CLICKED(IDC_BTN_FOLDER_SELECTALL, &CBatchRenameDlg::OnBnClickedFolderSelectAll)
    ON_BN_CLICKED(IDC_BTN_FOLDER_DESELECTALL, &CBatchRenameDlg::OnBnClickedFolderDeselectAll)
    ON_COMMAND(IDC_BTN_FOLDER_SELECTALL, &CBatchRenameDlg::OnBnClickedFolderSelectAll)
    ON_COMMAND(IDC_BTN_FOLDER_DESELECTALL, &CBatchRenameDlg::OnBnClickedFolderDeselectAll)
    ON_BN_CLICKED(IDC_BTN_CURRENT_RENAME, &CBatchRenameDlg::OnBnClickedCurrentRename)
    ON_BN_CLICKED(IDC_BTN_CURRENT_MOVE, &CBatchRenameDlg::OnBnClickedCurrentMove)
    ON_BN_CLICKED(IDC_BTN_CURRENT_DELETE, &CBatchRenameDlg::OnBnClickedCurrentDelete)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_FOLDERS, &CBatchRenameDlg::OnFolderListRightClick)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FOLDERS, &CBatchRenameDlg::OnFolderListCheckChanged)
    ON_COMMAND(IDM_FOLDER_RENAME, &CBatchRenameDlg::OnBnClickedFolderRename)
    ON_COMMAND(IDM_FOLDER_MOVE, &CBatchRenameDlg::OnBnClickedFolderMove)
    ON_COMMAND(IDM_FOLDER_DELETE, &CBatchRenameDlg::OnBnClickedFolderDelete)
    ON_COMMAND(IDM_FOLDER_EXPLORE, &CBatchRenameDlg::OnFolderExplore)
    ON_COMMAND(IDM_FILE_EXPLORE, &CBatchRenameDlg::OnFileExplore)
    ON_COMMAND(IDM_FILE_IGNORE, &CBatchRenameDlg::OnFileIgnore)
    ON_COMMAND(IDM_FILE_UNIGNORE, &CBatchRenameDlg::OnFileUnignore)
    ON_COMMAND(IDM_FILE_TRACK, &CBatchRenameDlg::OnFileTrack)
    ON_COMMAND(IDM_FILE_UNTRACK, &CBatchRenameDlg::OnFileUntrack)
    ON_COMMAND(IDM_FILE_MOVE_UP, &CBatchRenameDlg::OnFileMoveUp)
    ON_COMMAND(IDM_FILE_MOVE_DOWN, &CBatchRenameDlg::OnFileMoveDown)
    ON_COMMAND(IDM_FILE_MOVE_TO_UP, &CBatchRenameDlg::OnFileMoveToUp)
    ON_COMMAND(IDM_FILE_MOVE_TO_DOWN, &CBatchRenameDlg::OnFileMoveToDown)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_RENAME, &CBatchRenameDlg::OnFileListRightClick)
    ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST_RENAME, &CBatchRenameDlg::OnLvnBeginDrag)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_RENAME, &CBatchRenameDlg::OnCustomDrawList)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_COMMAND(IDM_FILE_MARK_DELETE, &CBatchRenameDlg::OnFileMarkDelete)
    ON_COMMAND(IDM_FILE_UNMARK_DELETE, &CBatchRenameDlg::OnFileUnmarkDelete)
    ON_COMMAND(IDM_FILE_CHANGE_EXT, &CBatchRenameDlg::OnFileChangeExt)
    ON_COMMAND(IDM_FILE_RESTORE_EXT, &CBatchRenameDlg::OnFileRestoreExt)
    ON_BN_CLICKED(IDC_BTN_FILE_UNMARK_ALL, &CBatchRenameDlg::OnFileUnmarkAll)
    ON_BN_CLICKED(IDC_BTN_FILE_RESET_ALL, &CBatchRenameDlg::OnFileResetAll)
    ON_BN_CLICKED(IDC_CHECK_DELETE_INVERT, &CBatchRenameDlg::OnCheckDeleteInvert)
    ON_BN_CLICKED(IDC_BTN_IGNORE_CLEAR, &CBatchRenameDlg::OnBnClickedIgnoreClear)
    ON_BN_CLICKED(IDC_CHECK_IGNORE_EXT, &CBatchRenameDlg::OnIgnoreRuleChanged)
    ON_BN_CLICKED(IDC_CHECK_IGNORE_MATCH, &CBatchRenameDlg::OnIgnoreRuleChanged)
    ON_BN_CLICKED(IDC_CHECK_IGNORE_REGEX, &CBatchRenameDlg::OnIgnoreRuleChanged)
    ON_BN_CLICKED(IDC_CHECK_TRACK_EXT, &CBatchRenameDlg::OnTrackRuleChanged)
    ON_BN_CLICKED(IDC_CHECK_TRACK_MATCH, &CBatchRenameDlg::OnTrackRuleChanged)
    ON_BN_CLICKED(IDC_CHECK_TRACK_REGEX, &CBatchRenameDlg::OnTrackRuleChanged)
    ON_BN_CLICKED(IDC_BTN_TRACK_CLEAR, &CBatchRenameDlg::OnBnClickedTrackClear)
    ON_WM_DROPFILES()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

// ========== Initialization ==========

BOOL CBatchRenameDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    DragAcceptFiles(TRUE);

    // Initialize tabs
    m_tabCtrl.SubclassDlgItem(IDC_TAB_FOLDER, this);
    m_tabCtrl.InsertItem(0, _T("文件夹操作"));
    m_tabCtrl.InsertItem(1, _T("文件批量处理"));

    // Initialize file list
    CListCtrl* pFileList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (pFileList)
    {
        pFileList->ModifyStyle(0, LVS_REPORT);
        pFileList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        CRect rcList;
        pFileList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pFileList->InsertColumn(0, _T("序号"), LVCFMT_CENTER, totalWidth * 1 / 15);
        pFileList->InsertColumn(1, _T("原文件名"), LVCFMT_LEFT, totalWidth * 4.5 / 10);
        pFileList->InsertColumn(2, _T("新文件名/状态"), LVCFMT_LEFT, totalWidth * 4.5 / 10 - totalWidth / 12 + 80);
    }

    // Initialize folder list
    CListCtrl* pFolderList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (pFolderList)
    {
        pFolderList->ModifyStyle(0, LVS_REPORT);
        pFolderList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
        CRect rcList;
        pFolderList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pFolderList->InsertColumn(0, _T("文件夹名"), LVCFMT_LEFT, totalWidth);
    }

    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);

    // If there is an initial folder path, load it
    if (!m_folderPath.IsEmpty())
    {
        SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
        LoadFolders();
        LoadFiles();
    }

    ShowTab(0);
    return TRUE;
}

// ========== Tab switching ==========

void CBatchRenameDlg::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    ShowTab(m_tabCtrl.GetCurSel());
    *pResult = 0;
}

void CBatchRenameDlg::ShowTab(int nTab)
{
    m_nActiveTab = nTab;

    // Folder operation controls (with labels)
    static const int folderIDs[] = {
        IDC_LIST_FOLDERS, IDC_BTN_FOLDER_RENAME, IDC_BTN_FOLDER_MOVE,
        IDC_BTN_FOLDER_DELETE, IDC_BTN_FOLDER_SELECTALL, IDC_BTN_FOLDER_DESELECTALL,
        IDC_BTN_CURRENT_RENAME, IDC_BTN_CURRENT_MOVE, IDC_BTN_CURRENT_DELETE,
        IDC_STATIC_CURRENT_DIR, IDC_STATIC_SUBDIR, IDC_STATIC_FOLDER_SELCOUNT
    };
    constexpr int nFolderIDs = sizeof(folderIDs) / sizeof(folderIDs[0]);

    // File batch processing controls (with labels)
    static const int fileIDs[] = {
        IDC_LIST_RENAME,
        IDC_EDIT_RENAME_PREFIX, IDC_EDIT_RENAME_SUFFIX,
        IDC_EDIT_RENAME_REPLACE_FROM, IDC_EDIT_RENAME_REPLACE_TO,
        IDC_CHECK_RENAME_REGEX, IDC_BTN_RENAME_REGEX_HELP,
        IDC_CHECK_RENAME_NUMBER, IDC_EDIT_RENAME_START_NUM,
        IDC_CHECK_DELETE_MATCH, IDC_EDIT_DELETE_PATTERN,
        IDC_CHECK_DELETE_INVERT, IDC_CHECK_NUMBER_AFTER_EXT,
        IDC_BTN_RENAME_PREVIEW, IDC_BTN_RENAME_EXECUTE, IDC_BTN_RENAME_UNDO,
        IDC_BTN_FILE_UNMARK_ALL, IDC_BTN_FILE_RESET_ALL,
        IDC_STATIC_TAB1_PREFIX, IDC_STATIC_TAB1_SUFFIX,
        IDC_STATIC_TAB1_REPLACE_FROM, IDC_STATIC_TAB1_REPLACE_TO,
        IDC_STATIC_TAB1_START_NUM, IDC_STATIC_TAB1_TIP,
        IDC_CHECK_IGNORE_EXT, IDC_EDIT_IGNORE_EXT,
        IDC_CHECK_IGNORE_MATCH, IDC_EDIT_IGNORE_PATTERN,
        IDC_CHECK_IGNORE_REGEX, IDC_BTN_IGNORE_CLEAR,
        IDC_STATIC_IGNORE_GROUP,
        IDC_CHECK_TRACK_EXT, IDC_EDIT_TRACK_EXT,
        IDC_CHECK_TRACK_MATCH, IDC_EDIT_TRACK_PATTERN,
        IDC_CHECK_TRACK_REGEX, IDC_BTN_TRACK_CLEAR,
        IDC_STATIC_TRACK_GROUP
    };
    constexpr int nFileIDs = sizeof(fileIDs) / sizeof(fileIDs[0]);

    // Hide all tab-related controls
    for (int i = 0; i < nFolderIDs; i++)
    {
        CWnd* pWnd = GetDlgItem(folderIDs[i]);
        if (pWnd) pWnd->ShowWindow(nTab == 0 ? SW_SHOW : SW_HIDE);
    }
    for (int i = 0; i < nFileIDs; i++)
    {
        CWnd* pWnd = GetDlgItem(fileIDs[i]);
        if (pWnd) pWnd->ShowWindow(nTab == 1 ? SW_SHOW : SW_HIDE);
    }

    // Update button state
    if (nTab == 1)
    {
        GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(!m_entries.empty());
        GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(m_bPreviewDone);
    }
    else
    {
        GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);
        GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    }
}

BOOL CBatchRenameDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
    {
        OnBnClickedRefresh();
        return TRUE;
    }
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = CWnd::FromHandle(::GetFocus());
        if (pFocus)
        {
            TCHAR className[64] = {0};
            ::GetClassName(pFocus->GetSafeHwnd(), className, 64);
            if (_tcsstr(className, _T("Edit")) || _tcsstr(className, _T("edit")))
            {
                return TRUE;
            }
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

// ========== Shared functionality ==========

void CBatchRenameDlg::SetFolderPath(const CString& path)
{
    m_folderPath = path;
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, m_folderPath);
    LoadFolders();
    LoadFiles();
}

void CBatchRenameDlg::OnBnClickedBrowse()
{
    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR path[MAX_PATH];
    if (::SHGetPathFromIDList(pidl, path))
    {
        SetFolderPath(path);
    }
    CoTaskMemFree(pidl);
}

void CBatchRenameDlg::OnBnClickedClear()
{
    m_folderPath.Empty();
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, _T(""));
    m_folders.clear();
    m_entries.clear();
    RefreshFolderList();
    RefreshFileList();

    SetDlgItemText(IDC_EDIT_RENAME_PREFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_SUFFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, _T(""));
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_UNCHECKED);
    SetDlgItemText(IDC_EDIT_DELETE_PATTERN, _T(""));

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnBnClickedRefresh()
{
    if (m_folderPath.IsEmpty()) return;
    LoadFolders();
    LoadFiles();
}

void CBatchRenameDlg::OnDropFiles(HDROP hDrop)
{
    UINT nFiles = ::DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (nFiles == 0)
    {
        ::DragFinish(hDrop);
        return;
    }

    TCHAR path[MAX_PATH];
    ::DragQueryFile(hDrop, 0, path, MAX_PATH);
    ::DragFinish(hDrop);

    // Check if it is a folder
    DWORD attrs = ::GetFileAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("仅支持拖入文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    SetFolderPath(path);
}

// ========== Tab 0: Folder operations ==========

void CBatchRenameDlg::LoadFolders()
{
    m_folders.clear();

    if (m_folderPath.IsEmpty()) return;

    try
    {
        std::wstring wpath = static_cast<LPCWSTR>(m_folderPath);
        for (const auto& entry : fs::directory_iterator(wpath))
        {
            if (entry.is_directory())
            {
                FolderEntry fe;
                fe.name = entry.path().filename().c_str();
                fe.fullPath = entry.path();
                m_folders.push_back(fe);
            }
        }
    }
    catch (...) { OutputDebugString(_T("[BatchRename] Failed to enumerate folder directory\n")); }

    std::sort(m_folders.begin(), m_folders.end(),
        [](const FolderEntry& a, const FolderEntry& b) {
            return a.name.CompareNoCase(b.name) < 0;
        });

    RefreshFolderList();
}

void CBatchRenameDlg::RefreshFolderList()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    pList->DeleteAllItems();
    for (size_t i = 0; i < m_folders.size(); i++)
    {
        pList->InsertItem(static_cast<int>(i), m_folders[i].name);
    }
    UpdateFolderSelectionCount();
}

std::vector<int> CBatchRenameDlg::GetCheckedFolders()
{
    std::vector<int> checked;
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return checked;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (ListView_GetCheckState(pList->m_hWnd, i))
            checked.push_back(i);
    }
    return checked;
}

void CBatchRenameDlg::UpdateFolderSelectionCount()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    int nChecked = 0;
    for (int i = 0; i < nCount; i++)
    {
        if (ListView_GetCheckState(pList->m_hWnd, i))
            nChecked++;
    }

    CString text;
    text.Format(_T("已选中: %d/%d"), nChecked, nCount);
    SetDlgItemText(IDC_STATIC_FOLDER_SELCOUNT, text);
}

void CBatchRenameDlg::OnBnClickedFolderRename()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要重命名的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Show input dialog
    CString defaultName = m_folders[selected[0]].name;
    CInputDialog dlg(_T("请输入新的文件夹名称:"), _T("重命名文件夹"), defaultName);
    if (dlg.DoModal() != IDOK) return;

    CString newName = dlg.GetInput();
    newName.Trim();
    if (newName.IsEmpty()) return;

    // Check for illegal characters
    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newName.Find(invalid[i]) != -1)
        {
            MessageBox(_T("文件夹名包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    if (selected.size() == 1)
    {
        fs::path newPath = m_folders[selected[0]].fullPath.parent_path() / static_cast<LPCWSTR>(newName);
        std::error_code ec;
        fs::rename(m_folders[selected[0]].fullPath, newPath, ec);
        if (ec)
            MessageBox(_T("重命名失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        else
            MessageBox(_T("重命名成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        int success = 0, fail = 0;
        for (size_t i = 0; i < selected.size(); i++)
        {
            CString numberedName;
            numberedName.Format(_T("%s_%d"), static_cast<LPCWSTR>(newName), (int)(i + 1));
            fs::path newPath = m_folders[selected[i]].fullPath.parent_path() / static_cast<LPCWSTR>(numberedName);
            std::error_code ec;
            fs::rename(m_folders[selected[i]].fullPath, newPath, ec);
            if (ec) fail++;
            else success++;
        }
        CString msg;
        msg.Format(_T("重命名完成：成功 %d 个，失败 %d 个。"), success, fail);
        MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);
    }

    LoadFolders();
}

void CBatchRenameDlg::OnBnClickedFolderMove()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要移动的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Show browse dialog to select target path
    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择目标文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR destPath[MAX_PATH];
    if (!::SHGetPathFromIDList(pidl, destPath))
    {
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);

    int success = 0, fail = 0;
    std::error_code ec;
    for (int idx : selected)
    {
        fs::path newPath = fs::path(destPath) / m_folders[idx].fullPath.filename();
        fs::rename(m_folders[idx].fullPath, newPath, ec);
        if (ec) fail++;
        else success++;
    }
    CString msg;
    msg.Format(_T("移动完成：成功 %d 个，失败 %d 个。"), success, fail);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    LoadFolders();
}

void CBatchRenameDlg::OnBnClickedFolderDelete()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty())
    {
        MessageBox(_T("请先勾选要删除的文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    CString msg;
	msg.Format(_T("确定要将选中的 %d 个文件夹移入回收站吗？"), (int)selected.size());
	if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	int success = 0, fail = 0;
	for (int idx : selected)
	{
		if (MoveToRecycleBin(m_folders[idx].fullPath))
			success++;
		else
			fail++;
	}
    msg.Format(_T("删除完成：成功 %d 个，失败 %d 个。"), success, fail);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    LoadFolders();
}

void CBatchRenameDlg::OnBnClickedCurrentRename()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
    CString curName = curPath.filename().c_str();

    CInputDialog dlg(_T("请输入新的目录名称:"), _T("重命名当前目录"), curName);
    if (dlg.DoModal() != IDOK) return;

    CString newName = dlg.GetInput();
    newName.Trim();
    if (newName.IsEmpty() || newName == curName) return;

    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newName.Find(invalid[i]) != -1)
        {
            MessageBox(_T("目录名包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    fs::path newPath = curPath.parent_path() / static_cast<LPCWSTR>(newName);
    std::error_code ec;
    fs::rename(curPath, newPath, ec);
    if (ec)
    {
        MessageBox(_T("重命名失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    SetFolderPath(newPath.c_str());
    MessageBox(_T("重命名成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedCurrentMove()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    BROWSEINFO bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = _T("选择目标文件夹");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
    if (!pidl) return;

    TCHAR destPath[MAX_PATH];
    if (!::SHGetPathFromIDList(pidl, destPath))
    {
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
    fs::path newPath = fs::path(destPath) / curPath.filename();

    CString msg;
    msg.Format(_T("确定将 \"%s\" 移动到 \"%s\" 吗？"), curPath.filename().c_str(), destPath);
    if (MessageBox(msg, _T("确认移动"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    std::error_code ec;
    fs::rename(curPath, newPath, ec);
    if (ec)
    {
        MessageBox(_T("移动失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    SetFolderPath(newPath.c_str());
    MessageBox(_T("移动成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedCurrentDelete()
{
    if (m_folderPath.IsEmpty())
    {
        MessageBox(_T("请先选择文件夹。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    fs::path curPath(static_cast<LPCWSTR>(m_folderPath));
	CString msg;
	msg.Format(_T("确定要将当前目录 \"%s\" 及其所有内容移入回收站吗？"), curPath.filename().c_str());
	if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONWARNING) != IDYES)
		return;

	if (!MoveToRecycleBin(curPath))
	{
		MessageBox(_T("移入回收站失败。"), _T("错误"), MB_OK | MB_ICONERROR);
		return;
	}

    // Clear current path
    m_folderPath.Empty();
    SetDlgItemText(IDC_EDIT_RENAME_FOLDER, _T(""));
    m_folders.clear();
    m_entries.clear();
    RefreshFolderList();
    RefreshFileList();

    MessageBox(_T("删除成功。"), _T("结果"), MB_OK | MB_ICONINFORMATION);
}

void CBatchRenameDlg::OnBnClickedFolderSelectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        ListView_SetCheckState(pList->m_hWnd, i, TRUE);
    UpdateFolderSelectionCount();
}

void CBatchRenameDlg::OnBnClickedFolderDeselectAll()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;
    for (int i = 0; i < pList->GetItemCount(); i++)
        ListView_SetCheckState(pList->m_hWnd, i, FALSE);
    UpdateFolderSelectionCount();
}

void CBatchRenameDlg::OnFolderListRightClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
    if (!pList) return;

    int nChecked = static_cast<int>(GetCheckedFolders().size());
    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    if (nChecked > 0)
    {
        menu.AppendMenu(MF_STRING, IDM_FOLDER_RENAME, _T("重命名"));
        menu.AppendMenu(MF_STRING, IDM_FOLDER_MOVE, _T("移动"));
        menu.AppendMenu(MF_STRING, IDM_FOLDER_DELETE, _T("删除"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FOLDER_EXPLORE, _T("在资源管理器中定位"));
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDC_BTN_FOLDER_SELECTALL, _T("全选"));
    menu.AppendMenu(MF_STRING, IDC_BTN_FOLDER_DESELECTALL, _T("取消全选"));
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CBatchRenameDlg::OnFolderListCheckChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Sync list selection state to checkbox: auto-check when user clicks/Shift-clicks/drags
    NMLISTVIEW* pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    if ((pNMLV->uChanged & LVIF_STATE) && pNMLV->iItem >= 0)
    {
        BOOL bWasSelected = (pNMLV->uOldState & LVIS_SELECTED) != 0;
        BOOL bIsSelected = (pNMLV->uNewState & LVIS_SELECTED) != 0;
        if (bWasSelected != bIsSelected)
        {
            CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_FOLDERS));
            if (pList)
            {
                BOOL bChecked = ListView_GetCheckState(pList->m_hWnd, pNMLV->iItem);
                if (bIsSelected != bChecked)
                    ListView_SetCheckState(pList->m_hWnd, pNMLV->iItem, bIsSelected);
            }
        }
    }
    UpdateFolderSelectionCount();
    *pResult = 0;
}

void CBatchRenameDlg::OnFolderExplore()
{
    std::vector<int> selected = GetCheckedFolders();
    if (selected.empty()) return;

    CString path = m_folders[selected[0]].fullPath.c_str();
    CString cmd;
    cmd.Format(_T("/select,\"%s\""), static_cast<LPCWSTR>(path));
    ShellExecute(nullptr, _T("open"), _T("explorer"), cmd, nullptr, SW_SHOW);
}

void CBatchRenameDlg::OnFileExplore()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                CString path = m_entries[i].fullPath.c_str();
                CString cmd;
                cmd.Format(_T("/select,\"%s\""), static_cast<LPCWSTR>(path));
                ShellExecute(nullptr, _T("open"), _T("explorer"), cmd, nullptr, SW_SHOW);
                return;
            }
        }
    }
}

void CBatchRenameDlg::OnFileIgnore()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_manualIgnoredSet.insert(m_entries[i].fullPath);
                // Recent operation priority: clear tracked and unignored state when manually ignoring
                m_manualTrackedSet.erase(m_entries[i].fullPath);
                m_manualUnignoredSet.erase(m_entries[i].fullPath);
                m_entries[i].bIgnored = true;
                m_entries[i].bTracked = false;
                m_entries[i].bMarkedDelete = false;
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileUnignore()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_manualIgnoredSet.erase(m_entries[i].fullPath);
                // Manual unignore: subsequent rule matching will not re-ignore
                m_manualUnignoredSet.insert(m_entries[i].fullPath);
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnBnClickedIgnoreClear()
{
    if (m_manualIgnoredSet.empty() && m_manualUnignoredSet.empty()) return;
    m_manualIgnoredSet.clear();
    m_manualUnignoredSet.clear();
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnIgnoreRuleChanged()
{
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnTrackRuleChanged()
{
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileTrack()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                // Track priority: auto clear ignore when tracking
                m_manualIgnoredSet.erase(m_entries[i].fullPath);
                m_manualTrackedSet.insert(m_entries[i].fullPath);
                m_entries[i].bTracked = true;
                m_entries[i].bIgnored = false;
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileUntrack()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_manualTrackedSet.erase(m_entries[i].fullPath);
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

// ========== Custom ordering (move up/down/top/bottom + drag) ==========

std::vector<int> CBatchRenameDlg_GetSelectedIndices(CListCtrl* pList)
{
    std::vector<int> indices;
    if (!pList) return indices;
    POSITION pos = pList->GetFirstSelectedItemPosition();
    while (pos)
    {
        int idx = pList->GetNextSelectedItem(pos);
        indices.push_back(idx);
    }
    // Sort ascending so we can process moves deterministically
    std::sort(indices.begin(), indices.end());
    return indices;
}

void CBatchRenameDlg::ReselectItems(const std::vector<int>& indices)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    pList->SetItemState(-1, 0, LVIS_SELECTED);
    for (int idx : indices)
    {
        if (idx >= 0 && idx < pList->GetItemCount())
        {
            pList->SetItemState(idx, LVIS_SELECTED, LVIS_SELECTED);
            pList->EnsureVisible(idx, FALSE);
        }
    }
}

void CBatchRenameDlg::OnFileMoveUp()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    auto sel = CBatchRenameDlg_GetSelectedIndices(pList);
    if (sel.empty()) return;

    // Move each selected item up by one slot. Process from top so each
    // swap only affects adjacent pairs.
    for (int idx : sel)
    {
        if (idx <= 0) continue;
        if (idx >= static_cast<int>(m_entries.size())) continue;
        std::swap(m_entries[idx], m_entries[idx - 1]);
    }

    ApplyRules();
    RefreshFileList();

    // Re-select items at their new positions (each moved up by 1)
    std::vector<int> newSel;
    for (int idx : sel)
    {
        int newPos = (idx > 0) ? idx - 1 : 0;
        newSel.push_back(newPos);
    }
    ReselectItems(newSel);
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileMoveDown()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    auto sel = CBatchRenameDlg_GetSelectedIndices(pList);
    if (sel.empty()) return;
    int nCount = static_cast<int>(m_entries.size());

    // Process from bottom to top so adjacent swaps don't interfere
    for (auto it = sel.rbegin(); it != sel.rend(); ++it)
    {
        int idx = *it;
        if (idx < 0 || idx >= nCount - 1) continue;
        std::swap(m_entries[idx], m_entries[idx + 1]);
    }

    ApplyRules();
    RefreshFileList();

    std::vector<int> newSel;
    for (int idx : sel)
    {
        int newPos = (idx < nCount - 1) ? idx + 1 : nCount - 1;
        newSel.push_back(newPos);
    }
    ReselectItems(newSel);
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileMoveToUp()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    auto sel = CBatchRenameDlg_GetSelectedIndices(pList);
    if (sel.empty()) return;

    int nCurFirst = sel.front() + 1;  // 1-based current position of first selected item
    int nMaxTarget = nCurFirst - 1;   // must be strictly before current position
    if (nMaxTarget < 1)
    {
        MessageBox(_T("选中项已在最前，无法继续前移。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    CString strPrompt;
    strPrompt.Format(_T("请输入移动后第一项的标号 (1 - %d)："), nMaxTarget);
    CInputDialog dlg(strPrompt, _T("前移到"), _T("1"));
    if (dlg.DoModal() != IDOK) return;

    CString strInput = dlg.GetInput();
    strInput.Trim();
    if (strInput.IsEmpty()) return;

    int nTarget = _ttoi(strInput);
    if (nTarget < 1 || nTarget > nMaxTarget)
    {
        CString msg;
        msg.Format(_T("请输入 1 到 %d 之间的数字。"), nMaxTarget);
        MessageBox(msg, _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    MoveSelectedItemsTo(nTarget - 1);
}

void CBatchRenameDlg::OnFileMoveToDown()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    auto sel = CBatchRenameDlg_GetSelectedIndices(pList);
    if (sel.empty()) return;
    int nCount = static_cast<int>(m_entries.size());
    int nSelCount = static_cast<int>(sel.size());

    int nCurFirst = sel.front() + 1;  // 1-based current position of first selected item
    int nMinTarget = nCurFirst + 1;   // must be strictly after current position

    // The block of selected items occupies [nTarget, nTarget + nSelCount - 1],
    // so the last item must fit: nTarget + nSelCount - 1 <= nCount
    int nMaxTarget = nCount - nSelCount + 1;

    if (nMinTarget > nMaxTarget)
    {
        MessageBox(_T("选中项已在最后，无法继续后移。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    CString strPrompt;
    strPrompt.Format(_T("请输入移动后第一项的标号 (%d - %d)："), nMinTarget, nMaxTarget);
    CString strDefault;
    strDefault.Format(_T("%d"), nMaxTarget);
    CInputDialog dlg(strPrompt, _T("后移到"), strDefault);
    if (dlg.DoModal() != IDOK) return;

    CString strInput = dlg.GetInput();
    strInput.Trim();
    if (strInput.IsEmpty()) return;

    int nTarget = _ttoi(strInput);
    if (nTarget < nMinTarget || nTarget > nMaxTarget)
    {
        CString msg;
        msg.Format(_T("请输入 %d 到 %d 之间的数字。"), nMinTarget, nMaxTarget);
        MessageBox(msg, _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    MoveSelectedItemsTo(nTarget - 1);
}

void CBatchRenameDlg::MoveSelectedItemsTo(int nTargetIndex)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;
    auto sel = CBatchRenameDlg_GetSelectedIndices(pList);
    if (sel.empty()) return;

    int nCount = static_cast<int>(m_entries.size());
    if (nTargetIndex < 0 || nTargetIndex > nCount) return;

    // Collect selected entries
    std::vector<RenameEntry> picked;
    for (int idx : sel)
    {
        if (idx >= 0 && idx < static_cast<int>(m_entries.size()))
            picked.push_back(m_entries[idx]);
    }
    // Remove them from the vector (back-to-front)
    for (auto it = sel.rbegin(); it != sel.rend(); ++it)
    {
        int idx = *it;
        if (idx >= 0 && idx < static_cast<int>(m_entries.size()))
            m_entries.erase(m_entries.begin() + idx);
    }

    // Adjust target index: if target was after removed block, decrement
    int adjusted = nTargetIndex;
    for (int idx : sel)
    {
        if (idx < nTargetIndex) adjusted--;
    }
    if (adjusted < 0) adjusted = 0;
    if (adjusted > static_cast<int>(m_entries.size())) adjusted = static_cast<int>(m_entries.size());

    m_entries.insert(m_entries.begin() + adjusted, picked.begin(), picked.end());

    ApplyRules();
    RefreshFileList();

    std::vector<int> newSel;
    for (size_t i = 0; i < picked.size(); i++)
        newSel.push_back(adjusted + static_cast<int>(i));
    ReselectItems(newSel);
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnLvnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nSel = pList->GetSelectedCount();
    if (nSel == 0) { *pResult = 0; return; }

    m_nDragSourceIndex = pNMLV->iItem;
    m_nDropTargetIndex = -1;
    m_nDropLineY = -1;
    m_bDragging = true;

    // No drag image is created: only the insertion line is shown during drag.
    pList->SetFocus();
    SetCapture();

    *pResult = 1;
}

void CBatchRenameDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
        if (pList)
        {
            CPoint ptList = point;
            ClientToScreen(&ptList);
            pList->ScreenToClient(&ptList);

            // Compute the insertion line position: if cursor is in the
            // upper half of an item, the line is at the top of that item;
            // if in the lower half, the line is at the bottom (i.e. top of next).
            int nHover = pList->HitTest(ptList);
            int nItemCount = pList->GetItemCount();
            int nNewLineY = -1; // client-space Y of insertion line
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
                // Below the last item: insert at the end
                CRect rcLast;
                pList->GetItemRect(nItemCount - 1, &rcLast, LVIR_BOUNDS);
                if (ptList.y >= rcLast.bottom)
                {
                    nNewLineY = rcLast.bottom;
                    nNewTarget = nItemCount;
                }
            }

            // Convert to screen space for drawing during custom draw
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
                // Force repaint to show the updated insertion line
                pList->Invalidate();
            }
        }
    }
    CDialogEx::OnMouseMove(nFlags, point);
}

void CBatchRenameDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        ReleaseCapture();

        CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
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

void CBatchRenameDlg::OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult)
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
        // Draw the insertion line after the list has painted itself
        if (!m_bDragging || m_nDropLineY < 0) return;

        CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
        if (!pList) return;

        CDC dc;
        dc.Attach(pCD->nmcd.hdc);

        // Convert screen-space Y back to list-client space
        CPoint ptLine(0, m_nDropLineY);
        pList->ScreenToClient(&ptLine);
        int nY = ptLine.y;

        // Get client rect to span the full width of the list
        CRect rcClient;
        pList->GetClientRect(&rcClient);

        // Draw a 2px thick blue insertion line across the list
        CPen pen(PS_SOLID, 2, RGB(0, 120, 215));
        CPen* pOldPen = dc.SelectObject(&pen);
        dc.MoveTo(rcClient.left, nY);
        dc.LineTo(rcClient.right, nY);
        dc.SelectObject(pOldPen);

        dc.Detach();
    }
}

void CBatchRenameDlg::OnBnClickedTrackClear()
{
    if (m_manualTrackedSet.empty()) return;
    m_manualTrackedSet.clear();
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

// ========== Tab 1: File batch processing ==========

void CBatchRenameDlg::LoadFiles()
{
    m_entries.clear();
    m_bPreviewDone = false;

    if (m_folderPath.IsEmpty()) return;

    try
    {
        std::wstring wpath = static_cast<LPCWSTR>(m_folderPath);
        for (const auto& entry : fs::directory_iterator(wpath))
        {
            if (entry.is_regular_file())
            {
                RenameEntry re;
                re.oldName = entry.path().filename().c_str();
                re.newName = re.oldName;
                re.fullPath = entry.path();
                re.bMarkedDelete = false;
                // Restore manual ignore/track/unignore state based on path
                if (m_manualIgnoredSet.count(entry.path()) > 0)
                    re.bIgnored = true;
                if (m_manualUnignoredSet.count(entry.path()) > 0)
                    re.bIgnored = false;
                if (m_manualTrackedSet.count(entry.path()) > 0)
                    re.bTracked = true;
                m_entries.push_back(re);
            }
        }
    }
    catch (...) { OutputDebugString(_T("[BatchRename] Failed to enumerate file directory\n")); }

    std::sort(m_entries.begin(), m_entries.end(),
        [](const RenameEntry& a, const RenameEntry& b) {
            return a.oldName.CompareNoCase(b.oldName) < 0;
        });

    GetDlgItem(IDC_BTN_RENAME_PREVIEW)->EnableWindow(!m_entries.empty());
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);

    RefreshFileList();
}

// --- Shared matching helpers ---

bool CBatchRenameDlg::MatchExtension(const CString& fileName, const CString& extList)
{
    CString ext = PathFindExtension(fileName);
    ext.MakeLower();
    CString lowerList = extList;
    lowerList.MakeLower();

    int start = 0;
    while (start < lowerList.GetLength())
    {
        CString token = lowerList.Tokenize(_T(";"), start);
        token.Trim();
        if (!token.IsEmpty())
        {
            if (token[0] != _T('.'))
                token = _T('.') + token;
            if (ext == token)
                return true;
        }
    }
    return false;
}

bool CBatchRenameDlg::MatchPattern(const CString& fileName, const CString& pattern, bool bRegex)
{
    try
    {
        std::wstring wName = static_cast<LPCWSTR>(fileName);
        if (bRegex)
        {
            std::wregex re(static_cast<LPCWSTR>(pattern));
            return std::regex_search(wName, re);
        }
        else
        {
            return wName.find(static_cast<LPCWSTR>(pattern)) != std::wstring::npos;
        }
    }
    catch (const std::regex_error&) { OutputDebugString(_T("[BatchRename] Invalid regex pattern\n")); }
    return false;
}

void CBatchRenameDlg::ApplyIgnoreRules()
{
    BOOL bIgnoreExt = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_EXT))->GetCheck() == BST_CHECKED;
    BOOL bIgnoreMatch = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_MATCH))->GetCheck() == BST_CHECKED;
    BOOL bIgnoreRegex = static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_REGEX))->GetCheck() == BST_CHECKED;

    CString ignoreExtStr, ignorePattern;
    if (bIgnoreExt) GetDlgItemText(IDC_EDIT_IGNORE_EXT, ignoreExtStr);
    if (bIgnoreMatch) GetDlgItemText(IDC_EDIT_IGNORE_PATTERN, ignorePattern);

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // Manual track priority: manually tracked files cannot be ignored
        if (m_manualTrackedSet.count(m_entries[i].fullPath) > 0)
        {
            m_entries[i].bIgnored = false;
            m_entries[i].bTracked = true;
            continue;
        }

        // Manual ignore
        if (m_manualIgnoredSet.count(m_entries[i].fullPath) > 0)
        {
            m_entries[i].bIgnored = true;
            m_entries[i].bTracked = false;
            m_entries[i].bMarkedDelete = false;
            continue;
        }

        // Manual unignore: files explicitly unignored by user are not affected by rules
        if (m_manualUnignoredSet.count(m_entries[i].fullPath) > 0)
        {
            m_entries[i].bIgnored = false;
            continue;
        }

        m_entries[i].bIgnored = false;

        // Ignore by extension
        if (bIgnoreExt && !ignoreExtStr.IsEmpty() && MatchExtension(m_entries[i].oldName, ignoreExtStr))
        {
            m_entries[i].bIgnored = true;
            m_entries[i].bMarkedDelete = false;
        }

        if (m_entries[i].bIgnored) continue;

        // Ignore by match/pattern
        if (bIgnoreMatch && !ignorePattern.IsEmpty() && MatchPattern(m_entries[i].oldName, ignorePattern, bIgnoreRegex))
        {
            m_entries[i].bIgnored = true;
            m_entries[i].bMarkedDelete = false;
        }
    }
}

void CBatchRenameDlg::ApplyTrackRules()
{
    BOOL bTrackExt = static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_EXT))->GetCheck() == BST_CHECKED;
    BOOL bTrackMatch = static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_MATCH))->GetCheck() == BST_CHECKED;
    BOOL bTrackRegex = static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_REGEX))->GetCheck() == BST_CHECKED;

    CString trackExtStr, trackPattern;
    if (bTrackExt) GetDlgItemText(IDC_EDIT_TRACK_EXT, trackExtStr);
    if (bTrackMatch) GetDlgItemText(IDC_EDIT_TRACK_PATTERN, trackPattern);

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // Manual track priority: force clear ignore state
        if (m_manualTrackedSet.count(m_entries[i].fullPath) > 0)
        {
            m_entries[i].bTracked = true;
            m_entries[i].bIgnored = false;
            m_entries[i].bMarkedDelete = false;
            continue;
        }

        // Manually ignored files do not participate in tracking
        if (m_manualIgnoredSet.count(m_entries[i].fullPath) > 0)
        {
            m_entries[i].bTracked = false;
            continue;
        }

        m_entries[i].bTracked = false;

        // Track by extension
        if (bTrackExt && !trackExtStr.IsEmpty() && MatchExtension(m_entries[i].oldName, trackExtStr))
        {
            m_entries[i].bTracked = true;
            m_entries[i].bIgnored = false;
            m_entries[i].bMarkedDelete = false;
        }

        if (m_entries[i].bTracked) continue;

        // Track by match/pattern
        if (bTrackMatch && !trackPattern.IsEmpty() && MatchPattern(m_entries[i].oldName, trackPattern, bTrackRegex))
        {
            m_entries[i].bTracked = true;
            m_entries[i].bIgnored = false;
            m_entries[i].bMarkedDelete = false;
        }
    }
}

void CBatchRenameDlg::ApplyRules()
{
    // Apply ignore rules first, then track rules (track takes priority, can override ignore)
    ApplyIgnoreRules();
    ApplyTrackRules();

    CString prefix, suffix, replaceFrom, replaceTo;
    GetDlgItemText(IDC_EDIT_RENAME_PREFIX, prefix);
    GetDlgItemText(IDC_EDIT_RENAME_SUFFIX, suffix);
    GetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, replaceFrom);
    GetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, replaceTo);

    BOOL bNumbering = static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->GetCheck() == BST_CHECKED;
    BOOL bRegex = static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->GetCheck() == BST_CHECKED;
    BOOL bDeleteMatch = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->GetCheck() == BST_CHECKED;
    BOOL bDeleteInvert = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->GetCheck() == BST_CHECKED;
    BOOL bNumberAfterExt = static_cast<CButton*>(GetDlgItem(IDC_CHECK_NUMBER_AFTER_EXT))->GetCheck() == BST_CHECKED;
    int startNum = GetDlgItemInt(IDC_EDIT_RENAME_START_NUM, nullptr, FALSE);

    CString deletePattern;
    if (bDeleteMatch)
        GetDlgItemText(IDC_EDIT_DELETE_PATTERN, deletePattern);

    // Count non-ignored, non-deleted files for digit padding
    int activeCount = 0;
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].bIgnored && !m_entries[i].bMarkedDelete)
            activeCount++;
    }

    int numDigits = 1;
    if (activeCount > 0)
    {
        int temp = activeCount + startNum - 1;
        while (temp >= 10) { numDigits++; temp /= 10; }
    }

    int numIdx = 0;

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        // Ignored files: skip all operations
        if (m_entries[i].bIgnored)
        {
            m_entries[i].newName = m_entries[i].oldName;
            m_entries[i].bMarkedDelete = false;
            continue;
        }

        // Match delete check
        if (bDeleteMatch && !deletePattern.IsEmpty())
        {
            try
            {
                std::wstring wName = static_cast<LPCWSTR>(m_entries[i].oldName);
                std::wregex re(static_cast<LPCWSTR>(deletePattern));
                bool bMatched = std::regex_search(wName, re);
                // Invert: delete when not matched, keep when matched
                bool bShouldDelete = bDeleteInvert ? !bMatched : bMatched;
                if (bShouldDelete)
                {
                    m_entries[i].bMarkedDelete = true;
                    m_entries[i].newName = _T("被删除");
                    continue;
                }
            }
            catch (const std::regex_error&) { OutputDebugString(_T("[BatchRename] Invalid regex pattern\n")); }
        }

        if (m_entries[i].bMarkedDelete)
        {
            m_entries[i].newName = _T("被删除");
            continue;
        }

        CString name = m_entries[i].oldName;
        int dotPos = name.ReverseFind(_T('.'));
        CString stem, ext;
        if (m_entries[i].bCustomExt)
        {
            // Use user-defined custom extension
            if (dotPos > 0)
                stem = name.Left(dotPos);
            else
                stem = name;
            ext = m_entries[i].customExt;
        }
        else if (dotPos > 0)
        {
            stem = name.Left(dotPos);
            ext = name.Mid(dotPos);
        }
        else
        {
            stem = name;
            ext = _T("");
        }

        if (!replaceFrom.IsEmpty())
        {
            if (bRegex)
            {
                try
                {
                    std::wstring wStem = static_cast<LPCWSTR>(stem);
                    std::wstring wFrom = static_cast<LPCWSTR>(replaceFrom);
                    std::wstring wTo = static_cast<LPCWSTR>(replaceTo);
                    std::wregex re(wFrom);
                    std::wstring result = std::regex_replace(wStem, re, wTo);
                    stem = result.c_str();
                }
                catch (const std::regex_error&) { OutputDebugString(_T("[BatchRename] Invalid regex pattern\n")); }
            }
            else
            {
                stem.Replace(replaceFrom, replaceTo);
            }
        }

        CString numStr;
        if (bNumbering)
        {
            numIdx++;
            numStr.Format(_T("_%0*d"), numDigits, startNum + numIdx - 1);
            if (bNumberAfterExt)
            {
                // Number after extension: prefix + stem + suffix + _num + ext
                m_entries[i].newName = prefix + stem + suffix + numStr + ext;
            }
            else
            {
                // Number before extension (default): prefix + stem + _num + suffix + ext
                m_entries[i].newName = prefix + stem + numStr + suffix + ext;
            }
        }
        else
        {
            m_entries[i].newName = prefix + stem + suffix + ext;
        }
    }
}

void CBatchRenameDlg::RefreshFileList()
{
	CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
	if (!pList) return;

	// Remember the file path at the top of the viewport, so we can put it back after rebuild
	fs::path topPath;
	int nTopIndex = pList->GetTopIndex();
	if (nTopIndex >= 0 && nTopIndex < static_cast<int>(m_entries.size()))
		topPath = m_entries[nTopIndex].fullPath;

	pList->DeleteAllItems();
	int nDisplayIdx = 0;
	for (size_t i = 0; i < m_entries.size(); i++)
	{
		// Skipped entries (ignored or marked-delete) do not consume a serial number
		bool bSkipSerial = m_entries[i].bIgnored || m_entries[i].bMarkedDelete;
		CString strIdx;
		if (bSkipSerial)
			strIdx = _T("-");
		else
			strIdx.Format(_T("%d"), ++nDisplayIdx);

		int idx = pList->InsertItem(static_cast<int>(i), strIdx);
		pList->SetItemText(idx, 1, m_entries[i].oldName);
		CString status;
		if (m_entries[i].bIgnored)
			status = _T("已忽略");
		else if (m_entries[i].bTracked)
			status = _T("【跟踪】") + m_entries[i].newName;
		else
			status = m_entries[i].newName;
		pList->SetItemText(idx, 2, status);
	}

	// Restore scroll position: find the same file and put it back at the top
	if (!topPath.empty())
	{
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			if (m_entries[i].fullPath == topPath)
			{
				// Scroll to bottom first, then to target: this forces the target to the top of viewport
				int nLast = pList->GetItemCount() - 1;
				pList->EnsureVisible(nLast, FALSE);
				pList->EnsureVisible(static_cast<int>(i), FALSE);
				break;
			}
		}
	}
}

void CBatchRenameDlg::OnBnClickedFilePreview()
{
    if (m_entries.empty()) return;

    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnBnClickedFileExecute()
{
    if (!m_bPreviewDone)
    {
        MessageBox(_T("请先点击预览，确认操作结果。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Separate delete and rename operations
    int deleteCount = 0, renameCount = 0, renameFail = 0, deleteFail = 0;

    // Confirm delete
    for (const auto& entry : m_entries)
    {
        if (!entry.bIgnored && entry.bMarkedDelete) deleteCount++;
    }

    if (deleteCount > 0)
    {
        CString msg;
        int renameCountCheck = 0;
        for (const auto& entry : m_entries)
        {
            if (!entry.bIgnored && !entry.bMarkedDelete && entry.oldName != entry.newName)
                renameCountCheck++;
        }
        msg.Format(_T("将删除 %d 个文件，重命名 %d 个文件。确定继续？"), deleteCount, renameCountCheck);
        if (MessageBox(msg, _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }
    else
    {
        // Check for duplicate names
        std::set<CString> newNames;
        for (const auto& entry : m_entries)
        {
            if (entry.bIgnored || entry.bMarkedDelete) continue;
            if (newNames.count(entry.newName))
            {
                MessageBox(_T("存在重名的新文件名，请检查规则。"), _T("错误"), MB_OK | MB_ICONERROR);
                return;
            }
            newNames.insert(entry.newName);
        }

        if (MessageBox(_T("确定要执行重命名吗？"), _T("确认"), MB_YESNO | MB_ICONWARNING) != IDYES)
            return;
    }

    std::error_code ec;

    // Execute delete first (move to recycle bin)
	int actualDeleted = 0;
	for (auto& entry : m_entries)
	{
		if (entry.bIgnored || !entry.bMarkedDelete) continue;
		if (MoveToRecycleBin(entry.fullPath))
			actualDeleted++;
		else
			deleteFail++;
	}

    // Then execute rename
    m_undoList.clear();
    for (auto& entry : m_entries)
    {
        if (entry.bIgnored || entry.bMarkedDelete) continue;
        if (entry.oldName == entry.newName) continue;

        fs::path newPath = entry.fullPath.parent_path() / static_cast<LPCWSTR>(entry.newName);
        fs::path oldPath = entry.fullPath;
        fs::rename(entry.fullPath, newPath, ec);
        if (ec)
            renameFail++;
        else
        {
            renameCount++;
            m_undoList.push_back({newPath, oldPath});
            entry.fullPath = newPath;
            entry.oldName = entry.newName;
        }
    }

    CString msg;
    msg.Format(_T("操作完成：删除 %d 个（失败 %d），重命名 %d 个（失败 %d）。"),
        actualDeleted, deleteFail, renameCount, renameFail);
    MessageBox(msg, _T("结果"), MB_OK | MB_ICONINFORMATION);

    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_RENAME_UNDO)->EnableWindow(!m_undoList.empty());
    LoadFiles();
}

void CBatchRenameDlg::OnBnClickedRenameUndo()
{
    if (m_undoList.empty())
        return;

    if (MessageBox(_T("Undo all renames from last execute?"), _T("Undo"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    int undoCount = 0;
    int undoFail = 0;
    std::error_code ec;

    // Undo in reverse order
    for (auto it = m_undoList.rbegin(); it != m_undoList.rend(); ++it)
    {
        fs::rename(it->first, it->second, ec);
        if (ec)
            undoFail++;
        else
            undoCount++;
    }

    m_undoList.clear();
    GetDlgItem(IDC_BTN_RENAME_UNDO)->EnableWindow(FALSE);

    CString msg;
    msg.Format(_T("Undo complete: %d reverted, %d failed."), undoCount, undoFail);
    MessageBox(msg, _T("Undo"), MB_OK | MB_ICONINFORMATION);

    LoadFiles();
}

void CBatchRenameDlg::OnEnChangeRule()
{
    m_bPreviewDone = false;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(FALSE);
}

void CBatchRenameDlg::OnBnClickedRegexHelp()
{
    if (m_hRegexGuideWnd && ::IsWindow(m_hRegexGuideWnd))
    {
        ::SetForegroundWindow(m_hRegexGuideWnd);
        return;
    }
    auto* pDlg = new CRegexGuideDlg(nullptr);
    pDlg->Create(IDD_REGEX_GUIDE_DLG, nullptr);
    pDlg->ShowWindow(SW_SHOW);
    m_hRegexGuideWnd = pDlg->GetSafeHwnd();
}

// Right-click menu
void CBatchRenameDlg::OnFileListRightClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nSel = pList->GetSelectedCount();
    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    if (nSel > 0)
    {
        menu.AppendMenu(MF_STRING, IDM_FILE_IGNORE, _T("忽略"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNIGNORE, _T("取消忽略"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_TRACK, _T("跟踪"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNTRACK, _T("取消跟踪"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_MARK_DELETE, _T("标记删除"));
        menu.AppendMenu(MF_STRING, IDM_FILE_UNMARK_DELETE, _T("取消标记"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_CHANGE_EXT, _T("修改后缀"));
        menu.AppendMenu(MF_STRING, IDM_FILE_RESTORE_EXT, _T("恢复原后缀名"));
        menu.AppendMenu(MF_SEPARATOR);

        // Submenu: move up
        CMenu menuUp;
        menuUp.CreatePopupMenu();
        menuUp.AppendMenu(MF_STRING, IDM_FILE_MOVE_UP, _T("前移1个"));
        menuUp.AppendMenu(MF_STRING, IDM_FILE_MOVE_TO_UP, _T("前移到..."));
        menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(menuUp.GetSafeHmenu()), _T("前移"));
        menuUp.Detach();

        // Submenu: move down
        CMenu menuDown;
        menuDown.CreatePopupMenu();
        menuDown.AppendMenu(MF_STRING, IDM_FILE_MOVE_DOWN, _T("后移1个"));
        menuDown.AppendMenu(MF_STRING, IDM_FILE_MOVE_TO_DOWN, _T("后移到..."));
        menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(menuDown.GetSafeHmenu()), _T("后移"));
        menuDown.Detach();

        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, IDM_FILE_EXPLORE, _T("在资源管理器中定位"));
    }
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CBatchRenameDlg::OnFileMarkDelete()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_entries[i].bMarkedDelete = true;
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileUnmarkDelete()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_entries[i].bMarkedDelete = false;
            }
        }
    }
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnCheckDeleteInvert()
{
    BOOL bChecked = static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->GetCheck();
    if (bChecked == BST_CHECKED)
    {
        static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_CHECKED);
    }
}

void CBatchRenameDlg::OnFileUnmarkAll()
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].bMarkedDelete = false;
    }

    // Auto execute preview
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileResetAll()
{
    if (MessageBox(_T("确定要清除所有改动吗？\n将重置前缀、后缀、替换、序号、删除标记。"),
        _T("确认"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Clear all edit boxes
    SetDlgItemText(IDC_EDIT_RENAME_PREFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_SUFFIX, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_FROM, _T(""));
    SetDlgItemText(IDC_EDIT_RENAME_REPLACE_TO, _T(""));
    SetDlgItemText(IDC_EDIT_DELETE_PATTERN, _T(""));

    // Uncheck all checkboxes
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_NUMBER))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_RENAME_REGEX))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_MATCH))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_DELETE_INVERT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_NUMBER_AFTER_EXT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_EXT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_MATCH))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_IGNORE_REGEX))->SetCheck(BST_UNCHECKED);
    SetDlgItemText(IDC_EDIT_IGNORE_EXT, _T(""));
    SetDlgItemText(IDC_EDIT_IGNORE_PATTERN, _T(""));
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_EXT))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_MATCH))->SetCheck(BST_UNCHECKED);
    static_cast<CButton*>(GetDlgItem(IDC_CHECK_TRACK_REGEX))->SetCheck(BST_UNCHECKED);
    SetDlgItemText(IDC_EDIT_TRACK_EXT, _T(""));
    SetDlgItemText(IDC_EDIT_TRACK_PATTERN, _T(""));

    // Clear all delete marks, ignore, track, and unignore
    m_manualIgnoredSet.clear();
    m_manualUnignoredSet.clear();
    m_manualTrackedSet.clear();
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].bMarkedDelete = false;
        m_entries[i].bCustomExt = false;
        m_entries[i].customExt.Empty();
    }

    // Auto execute preview
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileChangeExt()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    // Get current extension of selected item
    CString defaultExt = _T("txt");
    for (int i = 0; i < pList->GetItemCount(); i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                int dot = m_entries[i].oldName.ReverseFind(_T('.'));
                if (dot != -1)
                {
                    defaultExt = m_entries[i].oldName.Mid(dot + 1);
                    break;
                }
            }
        }
    }

    // Show input dialog
    CString newExt;
    CInputDialog dlg(_T("请输入新后缀（不含点号）:"), _T("修改后缀"), defaultExt);
    if (dlg.DoModal() != IDOK) return;

    newExt = dlg.GetInput();
    newExt.Trim();
    if (newExt.IsEmpty()) return;

    // Check for illegal characters
    CString invalid = _T("\\/:*?\"<>|.");
    for (int i = 0; i < invalid.GetLength(); i++)
    {
        if (newExt.Find(invalid[i]) != -1)
        {
            MessageBox(_T("后缀包含非法字符。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    // Change extension for all selected items
    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()) && !m_entries[i].bMarkedDelete)
            {
                CString name = m_entries[i].oldName;
                int dot = name.ReverseFind(_T('.'));
                CString stem = (dot > 0) ? name.Left(dot) : name;
                m_entries[i].customExt = _T(".") + newExt;
                m_entries[i].bCustomExt = true;
            }
        }
    }

    // Auto preview to show complete results
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnFileRestoreExt()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_RENAME));
    if (!pList) return;

    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        if (pList->GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
        {
            if (i < static_cast<int>(m_entries.size()))
            {
                m_entries[i].bCustomExt = false;
                m_entries[i].customExt.Empty();
            }
        }
    }

    // Auto preview to restore original extension
    ApplyRules();
    RefreshFileList();
    m_bPreviewDone = true;
    GetDlgItem(IDC_BTN_RENAME_EXECUTE)->EnableWindow(TRUE);
}

void CBatchRenameDlg::OnCancel()
{
    PostMessage(WM_CLOSE);
}

void CBatchRenameDlg::OnClose()
{
    DestroyWindow();
}

void CBatchRenameDlg::PostNcDestroy()
{
    delete this;
}