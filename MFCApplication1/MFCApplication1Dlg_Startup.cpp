#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "LocalizationManager.h"
#include <shlobj.h>

// Forward declaration for static helper function
static UINT EnumStartupsThread(LPVOID pParam);

namespace
{
constexpr LPCTSTR kRunKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
constexpr LPCTSTR kRunOnceKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce");
constexpr LPCTSTR kPoliciesRunKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run");
constexpr LPCTSTR kRunServicesKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\RunServices");
constexpr LPCTSTR kRunServicesOnceKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\RunServicesOnce");

CString RegistryValueToText(DWORD type, const BYTE* data, DWORD size)
{
    if (!data || size == 0)
        return CString();

    if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
    {
        const TCHAR* text = reinterpret_cast<const TCHAR*>(data);
        DWORD chars = size / sizeof(TCHAR);
        if (type == REG_MULTI_SZ)
        {
            CString result;
            DWORD pos = 0;
            while (pos + 1 < chars && !(text[pos] == 0 && text[pos + 1] == 0))
            {
                DWORD len = 0;
                while (pos + len < chars && text[pos + len] != 0)
                    len++;
                if (len > 0)
                {
                    if (!result.IsEmpty())
                        result += _T(" | ");
                    result += CString(text + pos, static_cast<int>(len));
                }
                pos += len + 1;
            }
            return result;
        }

        DWORD len = 0;
        while (len < chars && text[len] != 0)
            len++;
        return CString(text, static_cast<int>(len));
    }

    if (type == REG_DWORD && size >= sizeof(DWORD))
    {
        DWORD value = 0;
        memcpy(&value, data, sizeof(value));
        CString result;
        result.Format(_T("0x%08X (%u)"), value, value);
        return result;
    }

    CString hex;
    for (DWORD i = 0; i < size; i++)
        hex.AppendFormat(_T("%02X"), data[i]);
    return hex;
}

void AddStartupRegistryEntries(HKEY root, LPCTSTR subKey, DWORD view,
    LPCTSTR location, std::vector<CMFCApplication1Dlg::StartupInfo>& results)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(root, subKey, 0, KEY_READ | view, &hKey) != ERROR_SUCCESS)
        return;

    TCHAR valueName[512]{};
    DWORD index = 0;
    while (true)
    {
        DWORD nameSize = _countof(valueName);
        DWORD dataSize = 0;
        DWORD type = 0;
        LONG ret = RegEnumValue(hKey, index, valueName, &nameSize,
            nullptr, &type, nullptr, &dataSize);
        if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA)
            break;

        if (dataSize > 1024 * 1024)
            dataSize = 1024 * 1024;
        std::vector<BYTE> data(dataSize > 0 ? dataSize : 2);
        DWORD readSize = static_cast<DWORD>(data.size());
        nameSize = _countof(valueName);
        ret = RegEnumValue(hKey, index, valueName, &nameSize,
            nullptr, &type, data.data(), &readSize);
        if (ret == ERROR_SUCCESS)
        {
            CMFCApplication1Dlg::StartupInfo si;
            si.name = valueName;
            si.cmd = RegistryValueToText(type, data.data(), readSize);
            si.location = location;
            si.root = root;
            si.subKey = subKey;
            si.view = view;
            results.push_back(si);
        }

        index++;
    }

    RegCloseKey(hKey);
}

void AddStartupFolderEntries(LPCTSTR folder, LPCTSTR location,
    std::vector<CMFCApplication1Dlg::StartupInfo>& results)
{
    CString pattern(folder);
    if (pattern.IsEmpty())
        return;
    if (pattern.Right(1) != _T("\\"))
        pattern += _T("\\");
    pattern += _T("*");

    CFileFind finder;
    BOOL found = finder.FindFile(pattern);
    while (found)
    {
        found = finder.FindNextFile();
        if (finder.IsDots() || finder.IsDirectory())
            continue;

        CMFCApplication1Dlg::StartupInfo si;
        si.name = finder.GetFileName();
        si.cmd = finder.GetFilePath();
        si.location = location;
        si.folderPath = finder.GetFilePath();
        si.isFolder = true;
        results.push_back(si);
    }
    finder.Close();
}

void AddRegistryStartupLocations(HKEY root, LPCTSTR rootName, bool separateWowViews,
    std::vector<CMFCApplication1Dlg::StartupInfo>& results)
{
    CString base(rootName);
    if (separateWowViews)
    {
        AddStartupRegistryEntries(root, kRunKey, KEY_WOW64_64KEY,
            base + _T(" Run (64-bit)"), results);
        AddStartupRegistryEntries(root, kRunKey, KEY_WOW64_32KEY,
            base + _T(" Run (32-bit)"), results);
        AddStartupRegistryEntries(root, kRunOnceKey, KEY_WOW64_64KEY,
            base + _T(" RunOnce (64-bit)"), results);
        AddStartupRegistryEntries(root, kRunOnceKey, KEY_WOW64_32KEY,
            base + _T(" RunOnce (32-bit)"), results);
        AddStartupRegistryEntries(root, kPoliciesRunKey, KEY_WOW64_64KEY,
            base + _T(" Policies Run (64-bit)"), results);
        AddStartupRegistryEntries(root, kPoliciesRunKey, KEY_WOW64_32KEY,
            base + _T(" Policies Run (32-bit)"), results);
        AddStartupRegistryEntries(root, kRunServicesKey, KEY_WOW64_64KEY,
            base + _T(" RunServices (64-bit)"), results);
        AddStartupRegistryEntries(root, kRunServicesKey, KEY_WOW64_32KEY,
            base + _T(" RunServices (32-bit)"), results);
        AddStartupRegistryEntries(root, kRunServicesOnceKey, KEY_WOW64_64KEY,
            base + _T(" RunServicesOnce (64-bit)"), results);
        AddStartupRegistryEntries(root, kRunServicesOnceKey, KEY_WOW64_32KEY,
            base + _T(" RunServicesOnce (32-bit)"), results);
    }
    else
    {
        // HKCU\Software is shared by 32-bit and 64-bit processes; there is no
        // redirected Wow6432Node view for the current user's startup keys.
        AddStartupRegistryEntries(root, kRunKey, 0,
            base + _T(" Run"), results);
        AddStartupRegistryEntries(root, kRunOnceKey, 0,
            base + _T(" RunOnce"), results);
        AddStartupRegistryEntries(root, kPoliciesRunKey, 0,
            base + _T(" Policies Run"), results);
        AddStartupRegistryEntries(root, kRunServicesKey, 0,
            base + _T(" RunServices"), results);
        AddStartupRegistryEntries(root, kRunServicesOnceKey, 0,
            base + _T(" RunServicesOnce"), results);
    }
}

void CollectStartupEntries(std::vector<CMFCApplication1Dlg::StartupInfo>& results)
{
    AddRegistryStartupLocations(HKEY_CURRENT_USER, _T("HKCU"), false, results);
    AddRegistryStartupLocations(HKEY_LOCAL_MACHINE, _T("HKLM"), true, results);

    TCHAR userStartup[MAX_PATH] = {};
    TCHAR machineStartup[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_STARTUP, nullptr, 0, userStartup)))
        AddStartupFolderEntries(userStartup, _T("Startup Folder (User)"), results);
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_COMMON_STARTUP, nullptr, 0, machineStartup)))
        AddStartupFolderEntries(machineStartup, _T("Startup Folder (All Users)"), results);
}

bool RemoveStartupRegistryValue(HKEY root, LPCTSTR subKey, LPCTSTR name, DWORD view)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(root, subKey, 0, KEY_SET_VALUE | view, &hKey) != ERROR_SUCCESS)
        return false;
    LONG ret = RegDeleteValue(hKey, name);
    RegCloseKey(hKey);
    return ret == ERROR_SUCCESS;
}

bool RemoveStartupFolderEntry(LPCTSTR path, HWND hwnd)
{
    CString pathDouble(path);
    pathDouble += _T('\0');
    pathDouble += _T('\0');

    SHFILEOPSTRUCT sh = {};
    sh.hwnd = hwnd;
    sh.wFunc = FO_DELETE;
    sh.pFrom = pathDouble;
    sh.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    return SHFileOperation(&sh) == 0 && !sh.fAnyOperationsAborted;
}
}

// Double-click on CListCtrl: copy command
void CMFCApplication1Dlg::OnNMDblclkList4(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Use the item activation info to determine which row was double-clicked
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    int idx = (pItem) ? pItem->iItem : -1;
    if (idx >= 0)
    {
        CWnd* pWnd = GetDlgItem(IDC_LIST4);
        if (pWnd && IsValidWindow(pWnd->GetSafeHwnd()))
        {
            TCHAR cls[64] = {0};
            GetClassName(pWnd->GetSafeHwnd(), cls, _countof(cls));
            if (CString(cls).CompareNoCase(_T("SysListView32")) == 0)
            {
                CListCtrl* pListCtrl = (CListCtrl*)pWnd;
                // Prefer the command column (subitem 1). If empty, try to parse from
                // the first column (description) using common separators.
                CString cmd = pListCtrl->GetItemText(idx, 1);
                if (cmd.IsEmpty())
                {
                    CString combined = pListCtrl->GetItemText(idx, 0);
                    int sep = combined.Find(_T('|'));
                    if (sep != -1) cmd = combined.Mid(sep + 1);
                    else
                    {
                        sep = combined.Find(_T('\t'));
                        if (sep != -1) cmd = combined.Mid(sep + 1);
                        else cmd = combined; // fallback: copy whole text
                    }
                }
                if (!cmd.IsEmpty()) CopyToClipboard(m_hWnd, cmd);
            }
        }
    }
    *pResult = 0;
}

// Delete selected dropped file (move to Recycle Bin)
void CMFCApplication1Dlg::OnBnClickedButton24()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Delete (move to Recycle Bin) the currently selected dropped file
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("DeleteConfirm")), m_strDroppedFilePath);
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmDelete")), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Prepare SHFILEOPSTRUCT for moving to recycle bin
    // SHFileOperation expects double-null-terminated strings
    CString pathDouble = m_strDroppedFilePath;
    pathDouble += _T('\0');
    pathDouble += _T('\0');

    SHFILEOPSTRUCT sh = {0};
    sh.hwnd = m_hWnd;
    sh.wFunc = FO_DELETE;
    sh.pFrom = pathDouble;
    sh.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION; // already confirmed by us

    int ret = SHFileOperation(&sh);
    if (ret == 0 && !sh.fAnyOperationsAborted)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("DeleteSuccess")), loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath.Empty();
        SetDlgItemText(IDC_STATIC_PATH, loc.GetString(_T("FileTab"), _T("DropHint")));
        // clear rename edits
        SetDlgItemText(IDC_EDIT7, _T(""));
        SetDlgItemText(IDC_EDIT8, _T(""));
    }
    else
    {
        CString err;
        err.Format(loc.GetString(_T("Msg"), _T("DeleteFail")), ret);
        MessageBox(err, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

// Refresh startup list (Tab2)
void CMFCApplication1Dlg::RefreshStartupList()
{
    // Start background thread to enumerate startup entries
    AfxBeginThread(EnumStartupsThread, this);
}

// Add startup: select executable via file dialog, use filename as entry name
void CMFCApplication1Dlg::OnAddStartup()
{
    auto& loc = CLocalizationManager::GetInstance();
    CFileDialog dlg(TRUE, _T("exe"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Executable Files (*.exe)|*.exe|All Files (*.*)|*.*||"));
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        int pos = path.ReverseFind('\\');
        CString name = (pos != -1) ? path.Mid(pos + 1) : path;
        CString command = path;
        if (path.Find(_T(' ')) >= 0 && path[0] != _T('"'))
            command.Format(_T("\"%s\""), path.GetString());

        // Try to write to registry
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            LONG ret = RegSetValueEx(hKey, name, 0, REG_SZ,
                (const BYTE*)(LPCTSTR)command, (command.GetLength() + 1) * sizeof(TCHAR));
            RegCloseKey(hKey);
            if (ret == ERROR_SUCCESS)
            {
                MessageBox(loc.GetString(_T("Msg"), _T("StartupAdded")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
                RefreshStartupList();
                return;
            }
        }

        MessageBox(loc.GetString(_T("Msg"), _T("StartupAddFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

// Add startup entry for all users (HKLM Run)
void CMFCApplication1Dlg::OnAddMachineStartup()
{
    auto& loc = CLocalizationManager::GetInstance();
    CFileDialog dlg(TRUE, _T("exe"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Executable Files (*.exe)|*.exe|All Files (*.*)|*.*||"));
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        int pos = path.ReverseFind('\\');
        CString name = (pos != -1) ? path.Mid(pos + 1) : path;
        CString command = path;
        if (path.Find(_T(' ')) >= 0 && path[0] != _T('"'))
            command.Format(_T("\"%s\""), path.GetString());

        HKEY hKey = NULL;
        LONG openRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, kRunKey, 0,
            KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey);
        if (openRet != ERROR_SUCCESS)
            openRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, kRunKey, 0,
                KEY_SET_VALUE, &hKey);
        if (openRet == ERROR_SUCCESS)
        {
            LONG ret = RegSetValueEx(hKey, name, 0, REG_SZ,
                (const BYTE*)(LPCTSTR)command, (command.GetLength() + 1) * sizeof(TCHAR));
            RegCloseKey(hKey);
            if (ret == ERROR_SUCCESS)
            {
                MessageBox(loc.GetString(_T("Msg"), _T("StartupAdded")),
                    loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
                RefreshStartupList();
                return;
            }
        }

        MessageBox(loc.GetString(_T("Msg"), _T("StartupAddFail")),
            loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

// Delete selected startup item
void CMFCApplication1Dlg::OnRemoveStartup()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST2);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    DWORD_PTR itemData = pList->GetItemData(idx);
    if (itemData >= m_startupInfos.size())
        return;
    const StartupInfo& si = m_startupInfos[static_cast<size_t>(itemData)];

    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("ConfirmRemoveStartup")), si.name);
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmDelete")), MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    bool removed = si.isFolder
        ? RemoveStartupFolderEntry(si.folderPath, m_hWnd)
        : RemoveStartupRegistryValue(si.root, si.subKey, si.name, si.view);
    if (removed)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("StartupRemoved")),
            loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
        RefreshStartupList();
        return;
    }

    MessageBox(loc.GetString(_T("Msg"), _T("StartupRemoveFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
}

static UINT EnumStartupsThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::StartupInfo>* results = new std::vector<CMFCApplication1Dlg::StartupInfo>();
    CollectStartupEntries(*results);

    HWND hwnd = dlg->GetSafeHwnd();
    if (hwnd != NULL && IsValidWindow(hwnd))
    {
        if (!::PostMessage(hwnd, CMFCApplication1Dlg::WM_REFRESH_STARTUPS_DONE, (WPARAM)results, 0))
        {
            delete results;
        }
    }
    else
    {
        delete results;
    }
    return 0;
}
