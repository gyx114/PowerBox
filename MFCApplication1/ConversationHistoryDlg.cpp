#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "ConversationHistoryDlg.h"
#include "afxdialogex.h"
#include <shellapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Forward declaration
static BOOL InputBox(CString& value, LPCTSTR title, LPCTSTR prompt);

IMPLEMENT_DYNAMIC(CConversationHistoryDlg, CDialogEx)

CConversationHistoryDlg::CConversationHistoryDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_CONVERSATION_HISTORY_DLG, pParent)
{
}

CConversationHistoryDlg::~CConversationHistoryDlg()
{
}

void CConversationHistoryDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_CONV_HISTORY, m_list);
    DDX_Control(pDX, IDC_STATIC_CONV_PATH, m_staticPath);
}

BEGIN_MESSAGE_MAP(CConversationHistoryDlg, CDialogEx)
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BTN_CONV_LOAD, &CConversationHistoryDlg::OnBnClickedConvLoad)
    ON_BN_CLICKED(IDC_BTN_CONV_RENAME, &CConversationHistoryDlg::OnBnClickedConvRename)
    ON_BN_CLICKED(IDC_BTN_CONV_DELETE, &CConversationHistoryDlg::OnBnClickedConvDelete)
    ON_BN_CLICKED(IDC_BTN_CONV_PATH, &CConversationHistoryDlg::OnBnClickedConvPath)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_CONV_HISTORY, &CConversationHistoryDlg::OnNMDblclkConvHistory)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_CONV_HISTORY, &CConversationHistoryDlg::OnRclickConvHistory)
END_MESSAGE_MAP()

CString CConversationHistoryDlg::GetConversationsFolder()
{
    // Get config.ini path
    TCHAR szExePath[MAX_PATH] = {0};
    GetModuleFileName(nullptr, szExePath, MAX_PATH);
    CString exePath = szExePath;
    int nLastSlash = exePath.ReverseFind(_T('\\'));
    CString configPath = (nLastSlash >= 0) ? exePath.Left(nLastSlash + 1) + _T("config.ini") : CString(_T("config.ini"));

    // Try configured conversation directory
    TCHAR szPath[MAX_PATH] = {0};
    GetPrivateProfileString(_T("Paths"), _T("ConversationDir"), _T(""), szPath, MAX_PATH, configPath);
    CString convDir = szPath;
    convDir.Trim();

    if (convDir.IsEmpty())
    {
        // Fallback to AppData
        TCHAR szAppData[MAX_PATH] = {0};
        SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, szAppData);
        convDir = CString(szAppData) + _T("\\PowerBox\\conversations");
    }

    // Ensure trailing backslash
    if (convDir.Right(1) != _T("\\"))
        convDir += _T("\\");

    CreateDirectory(convDir, nullptr);
    return convDir;
}

void CConversationHistoryDlg::RefreshList()
{
    m_list.DeleteAllItems();
    m_conversations.clear();

    CString folder = GetConversationsFolder();
    m_staticPath.SetWindowText(_T("保存位置: ") + folder);
    CString pattern = folder + _T("*.conv");

    CFileFind finder;
    BOOL bWorking = finder.FindFile(pattern);
    while (bWorking)
    {
        bWorking = finder.FindNextFile();
        if (finder.IsDirectory())
            continue;

        ConversationInfo info;
        info.filePath = finder.GetFilePath();

        // Read file header
        CFile file;
        if (file.Open(info.filePath, CFile::modeRead))
        {
            CArchive ar(&file, CArchive::load);
            try
            {
                int nCount;
                CString title, created, updated;
                ar >> nCount >> title >> created >> updated;

                info.title = title;
                info.createdTime = created;
                info.updatedTime = updated;
                info.messageCount = nCount;
            }
            catch (CArchiveException* e)
            {
                info.title = finder.GetFileName();
                info.messageCount = 0;
                e->Delete();
            }
            ar.Close();
            file.Close();
        }
        else
        {
            info.title = finder.GetFileName();
            info.messageCount = 0;
        }

        if (info.title.IsEmpty())
            info.title = finder.GetFileName();

        m_conversations.push_back(info);
    }
    finder.Close();

    // Populate list control
    for (size_t i = 0; i < m_conversations.size(); i++)
    {
        const auto& conv = m_conversations[i];
        int idx = m_list.InsertItem((int)i, conv.title);
        CString countStr;
        countStr.Format(_T("%d"), conv.messageCount);
        m_list.SetItemText(idx, 1, countStr);
        m_list.SetItemText(idx, 2, conv.updatedTime);
        m_list.SetItemData(idx, (DWORD_PTR)i);
    }
}

void CConversationHistoryDlg::LoadConversation(const ConversationInfo& conv)
{
    // Notify parent via custom message
    CString* pFilePath = new CString(conv.filePath);
    ::PostMessage(GetParent()->GetSafeHwnd(), WM_CONV_LOADED, (WPARAM)pFilePath, 0);
    DestroyWindow();
}

void CConversationHistoryDlg::DeleteConversation(const ConversationInfo& conv)
{
    CString msg;
    msg.Format(_T("确定要删除对话 \"%s\" 吗？"), conv.title.GetString());
    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Move to recycle bin — pFrom requires double-null-terminated string
    CString path = conv.filePath;
    path.AppendChar(_T('\0')); // second null for SHFileOperation
    SHFILEOPSTRUCT fos = {0};
    fos.wFunc = FO_DELETE;
    fos.pFrom = path;
    fos.pTo = nullptr;
    fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    SHFileOperation(&fos);

    RefreshList();
}

void CConversationHistoryDlg::RenameConversation(ConversationInfo& conv)
{
    CString newTitle = conv.title;
    if (!InputBox(newTitle, _T("重命名对话"), _T("请输入新的对话标题:")))
        return;

    if (newTitle.IsEmpty() || newTitle == conv.title)
        return;

    // Update the title in the file header
    CFile file;
    if (!file.Open(conv.filePath, CFile::modeReadWrite))
        return;

    // Read all existing data
    CArchive ar(&file, CArchive::load);
    int nCount;
    CString title, created, updated;
    std::vector<std::pair<CString, CString>> messages;
    try
    {
        ar >> nCount >> title >> created >> updated;
        for (int i = 0; i < nCount; i++)
        {
            CString role, content;
            ar >> role >> content;
            messages.push_back({role, content});
        }
    }
    catch (CArchiveException* e)
    {
        e->Delete();
        ar.Close();
        file.Close();
        return;
    }
    ar.Close();
    file.Close();

    // Write back with new title
    CFile fileOut;
    if (!fileOut.Open(conv.filePath, CFile::modeCreate | CFile::modeWrite))
        return;

    CArchive arOut(&fileOut, CArchive::store);
    arOut << nCount;
    arOut << newTitle;
    arOut << created;
    arOut << updated;
    for (auto& msg : messages)
        arOut << msg.first << msg.second;
    arOut.Close();
    fileOut.Close();

    conv.title = newTitle;
    RefreshList();
}

BOOL CConversationHistoryDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Set list control extended styles
    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

    // Divide columns into 3 equal parts
    CRect rcList;
    m_list.GetClientRect(&rcList);
    int colWidth = rcList.Width() / 3;
    m_list.InsertColumn(0, _T("标题"), LVCFMT_LEFT, colWidth);
    m_list.InsertColumn(1, _T("消息数"), LVCFMT_LEFT, colWidth);
    m_list.InsertColumn(2, _T("更新时间"), LVCFMT_LEFT, colWidth);

    RefreshList();

    return TRUE;
}

void CConversationHistoryDlg::OnDestroy()
{
    CDialogEx::OnDestroy();
}

void CConversationHistoryDlg::OnOK()
{
    DestroyWindow();
}

void CConversationHistoryDlg::OnCancel()
{
    DestroyWindow();
}

void CConversationHistoryDlg::PostNcDestroy()
{
    CDialogEx::PostNcDestroy();
    delete this;
}

void CConversationHistoryDlg::OnBnClickedConvLoad()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(_T("请先选择一个对话"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    DWORD_PTR idx = m_list.GetItemData(sel);
    if (idx < m_conversations.size())
        LoadConversation(m_conversations[idx]);
}

void CConversationHistoryDlg::OnBnClickedConvRename()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(_T("请先选择一个对话"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }
    DWORD_PTR idx = m_list.GetItemData(sel);
    if (idx < m_conversations.size())
        RenameConversation(m_conversations[idx]);
}

void CConversationHistoryDlg::OnBnClickedConvDelete()
{
    // Collect all selected items
    std::vector<int> selectedIndices;
    int sel = -1;
    while ((sel = m_list.GetNextItem(sel, LVNI_SELECTED)) != -1)
    {
        DWORD_PTR idx = m_list.GetItemData(sel);
        if (idx < m_conversations.size())
            selectedIndices.push_back((int)idx);
    }

    if (selectedIndices.empty())
    {
        MessageBox(_T("请先选择要删除的对话"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    CString msg;
    if (selectedIndices.size() == 1)
        msg.Format(_T("确定要删除对话 \"%s\" 吗？"), m_conversations[selectedIndices[0]].title.GetString());
    else
        msg.Format(_T("确定要删除选中的 %d 个对话吗？"), (int)selectedIndices.size());

    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Delete files in reverse order (indices stay valid as we don't modify the vector)
    for (int i = (int)selectedIndices.size() - 1; i >= 0; i--)
    {
        const auto& conv = m_conversations[selectedIndices[i]];
        CString path = conv.filePath;
        path.AppendChar(_T('\0')); // double-null-terminated for SHFileOperation
        SHFILEOPSTRUCT fos = {0};
        fos.wFunc = FO_DELETE;
        fos.pFrom = path;
        fos.pTo = nullptr;
        fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
        SHFileOperation(&fos);
    }

    RefreshList();
}

void CConversationHistoryDlg::OnBnClickedConvPath()
{
    CString currentDir = GetConversationsFolder();
    CFolderPickerDialog dlg(currentDir, OFN_HIDEREADONLY | OFN_NOCHANGEDIR, this);
    if (dlg.DoModal() == IDOK)
    {
        CString newDir = dlg.GetPathName();
        if (newDir.Right(1) == _T("\\"))
            newDir = newDir.Left(newDir.GetLength() - 1);

        // Save to config.ini
        TCHAR szExePath[MAX_PATH] = {0};
        GetModuleFileName(nullptr, szExePath, MAX_PATH);
        CString exePath = szExePath;
        int nLastSlash = exePath.ReverseFind(_T('\\'));
        CString configPath = (nLastSlash >= 0) ? exePath.Left(nLastSlash + 1) + _T("config.ini") : CString(_T("config.ini"));

        WritePrivateProfileString(_T("Paths"), _T("ConversationDir"), newDir, configPath);
        RefreshList();
    }
}

void CConversationHistoryDlg::OnNMDblclkConvHistory(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    // Double-click to load
    OnBnClickedConvLoad();
    *pResult = 0;
}

void CConversationHistoryDlg::OnRclickConvHistory(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        *pResult = 0;
        return;
    }

    // Collect all selected indices
    std::vector<int> selectedIndices;
    int pos = -1;
    while ((pos = m_list.GetNextItem(pos, LVNI_SELECTED)) != -1)
    {
        DWORD_PTR idx = m_list.GetItemData(pos);
        if (idx < m_conversations.size())
            selectedIndices.push_back((int)idx);
    }

    if (selectedIndices.empty())
    {
        *pResult = 0;
        return;
    }

    CPoint pt;
    GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, IDM_CONV_LOAD, _T("加载"));
    menu.AppendMenu(MF_STRING, IDM_CONV_RENAME, _T("重命名"));
    menu.AppendMenu(MF_STRING, IDM_CONV_DELETE, _T("删除"));

    int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, this);
    switch (cmd)
    {
    case IDM_CONV_LOAD:
        if (selectedIndices.size() == 1)
            LoadConversation(m_conversations[selectedIndices[0]]);
        else
            MessageBox(_T("加载对话仅支持单选"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_CONV_RENAME:
        if (selectedIndices.size() == 1)
            RenameConversation(m_conversations[selectedIndices[0]]);
        else
            MessageBox(_T("重命名仅支持单选"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_CONV_DELETE:
    {
        CString msg;
        if (selectedIndices.size() == 1)
            msg.Format(_T("确定要删除对话 \"%s\" 吗？"), m_conversations[selectedIndices[0]].title.GetString());
        else
            msg.Format(_T("确定要删除选中的 %d 个对话吗？"), (int)selectedIndices.size());

        if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            for (int i = (int)selectedIndices.size() - 1; i >= 0; i--)
            {
                const auto& conv = m_conversations[selectedIndices[i]];
                CString path = conv.filePath;
                path.AppendChar(_T('\0')); // double-null-terminated for SHFileOperation
                SHFILEOPSTRUCT fos = {0};
                fos.wFunc = FO_DELETE;
                fos.pFrom = path;
                fos.pTo = nullptr;
                fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
                SHFileOperation(&fos);
            }
            RefreshList();
        }
        break;
    }
    }

    *pResult = 0;
}

// Simple input dialog helper
static BOOL InputBox(CString& value, LPCTSTR title, LPCTSTR prompt)
{
    // Use a simple dialog template
    CDialogEx dlg(IDD_INPUT_DLG);
    dlg.SetWindowText(title);
    dlg.GetDlgItem(IDC_INPUT_PROMPT)->SetWindowText(prompt);
    dlg.GetDlgItem(IDC_INPUT_EDIT)->SetWindowText(value);
    if (dlg.DoModal() == IDOK)
    {
        dlg.GetDlgItem(IDC_INPUT_EDIT)->GetWindowText(value);
        return TRUE;
    }
    return FALSE;
}