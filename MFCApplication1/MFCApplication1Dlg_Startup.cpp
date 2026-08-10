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
constexpr LPCTSTR kApprovedRun = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run");
constexpr LPCTSTR kApprovedRun32 = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run32");
constexpr LPCTSTR kApprovedStartupFolder = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder");
constexpr LPCTSTR kApprovedStartupFolder32 = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder32");

CString EscapeRegValueName(LPCTSTR value)
{
    CString result;
    for (int i = 0; value[i] != 0; i++)
    {
        if (value[i] == _T('"'))
            result += _T("\\\"");
        else if (value[i] == _T('\\'))
            result += _T("\\\\");
        else
            result += value[i];
    }
    return result;
}

bool IsStartupApprovedDisabled(HKEY root, LPCTSTR approvedSubKey, LPCTSTR name, DWORD view)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(root, approvedSubKey, 0, KEY_QUERY_VALUE | view, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = 0;
    LONG ret = RegQueryValueEx(hKey, name, nullptr, &type, nullptr, &size);
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return false;
    }

    bool disabled = false;
    if (type == REG_BINARY && size >= sizeof(DWORD))
    {
        DWORD flag = 0;
        std::vector<BYTE> buffer(size > 0 ? size : sizeof(flag));
        DWORD readSize = static_cast<DWORD>(buffer.size());
        if (RegQueryValueEx(hKey, name, nullptr, nullptr,
            buffer.data(), &readSize) == ERROR_SUCCESS &&
            readSize >= sizeof(flag))
        {
            memcpy(&flag, buffer.data(), sizeof(flag));
            disabled = (flag == 3);
        }
    }

    RegCloseKey(hKey);
    return disabled;
}

bool WriteStartupApprovedState(HKEY root, LPCTSTR approvedSubKey,
    LPCTSTR name, DWORD view, bool enabled)
{
    HKEY hKey = nullptr;
    LONG ret = RegCreateKeyEx(root, approvedSubKey, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | view, nullptr, &hKey, nullptr);
    if (ret != ERROR_SUCCESS)
        return false;

    // Task Manager stores a 12-byte StartupApproved value: 2 = enabled, 3 = disabled.
    BYTE data[12] = {};
    data[0] = enabled ? 2 : 3;
    ret = RegSetValueEx(hKey, name, 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(hKey);
    return ret == ERROR_SUCCESS;
}

CString AlternateStartupApprovedKey(HKEY root, LPCTSTR approvedSubKey)
{
    if (root == HKEY_CURRENT_USER &&
        (_tcsicmp(approvedSubKey, kApprovedRun) == 0 ||
         _tcsicmp(approvedSubKey, kApprovedStartupFolder) == 0))
    {
        return (_tcsicmp(approvedSubKey, kApprovedRun) == 0)
            ? kApprovedRun32 : kApprovedStartupFolder32;
    }
    if (root == HKEY_LOCAL_MACHINE &&
        _tcsicmp(approvedSubKey, kApprovedStartupFolder) == 0)
    {
        return kApprovedStartupFolder32;
    }
    return CString();
}

bool IsStartupEntryApprovedDisabled(const CMFCApplication1Dlg::StartupInfo& si)
{
    if (IsStartupApprovedDisabled(si.approvedRoot, si.approvedSubKey,
        si.name, si.approvedView))
    {
        return true;
    }

    CString alternate = AlternateStartupApprovedKey(si.approvedRoot, si.approvedSubKey);
    return !alternate.IsEmpty() &&
        IsStartupApprovedDisabled(si.approvedRoot, alternate, si.name, si.approvedView);
}

bool WriteStartupEntryApprovedState(const CMFCApplication1Dlg::StartupInfo& si, bool enabled)
{
    bool ok = WriteStartupApprovedState(si.approvedRoot, si.approvedSubKey,
        si.name, si.approvedView, enabled);

    CString alternate = AlternateStartupApprovedKey(si.approvedRoot, si.approvedSubKey);
    if (!alternate.IsEmpty())
        ok = WriteStartupApprovedState(si.approvedRoot, alternate,
            si.name, si.approvedView, enabled) && ok;
    return ok;
}

bool QueryRegistryValueData(HKEY root, LPCTSTR subKey, LPCTSTR name,
    DWORD view, DWORD& type, std::vector<BYTE>& data)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(root, subKey, 0, KEY_QUERY_VALUE | view, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD size = 0;
    LONG ret = RegQueryValueEx(hKey, name, nullptr, &type, nullptr, &size);
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return false;
    }

    data.resize(size > 0 ? size : 1);
    DWORD readSize = static_cast<DWORD>(data.size());
    ret = RegQueryValueEx(hKey, name, nullptr, &type, data.data(), &readSize);
    RegCloseKey(hKey);
    return ret == ERROR_SUCCESS;
}

bool WriteRegBackupFile(const CString& path, const std::wstring& content)
{
    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    const BYTE bom[] = { 0xFF, 0xFE };
    DWORD written = 0;
    bool ok = WriteFile(hFile, bom, sizeof(bom), &written, nullptr) &&
        written == sizeof(bom);
    if (ok && !content.empty())
    {
        DWORD bytes = static_cast<DWORD>(content.size() * sizeof(wchar_t));
        ok = WriteFile(hFile, content.c_str(), bytes, &written, nullptr) &&
            written == bytes;
    }

    CloseHandle(hFile);
    return ok;
}

bool BuildRegBackupText(const CMFCApplication1Dlg::StartupInfo& si, CString& text)
{
    DWORD type = 0;
    std::vector<BYTE> data;
    if (!QueryRegistryValueData(si.root, si.subKey, si.name, si.view, type, data))
        return false;

    CString rootName = (si.root == HKEY_CURRENT_USER)
        ? _T("HKEY_CURRENT_USER") : _T("HKEY_LOCAL_MACHINE");
    CString regSubKey = si.subKey;
    if (si.root == HKEY_LOCAL_MACHINE && (si.view & KEY_WOW64_32KEY))
    {
        const CString prefix = _T("Software\\");
        if (regSubKey.Left(prefix.GetLength()).CompareNoCase(prefix) == 0)
            regSubKey = _T("Software\\Wow6432Node\\") + regSubKey.Mid(prefix.GetLength());
    }
    text = _T("Windows Registry Editor Version 5.00\r\n\r\n");
    text.AppendFormat(_T("[%s\\%s]\r\n"), rootName.GetString(), regSubKey.GetString());
    if (si.name.IsEmpty())
        text += _T("@=");
    else
        text.AppendFormat(_T("\"%s\"="), EscapeRegValueName(si.name).GetString());

    if (type == REG_DWORD && data.size() >= sizeof(DWORD))
    {
        DWORD value = 0;
        memcpy(&value, data.data(), sizeof(value));
        CString valueText;
        valueText.Format(_T("dword:%08x\r\n"), value);
        text += valueText;
        return true;
    }

    CString prefix = _T("hex:");
    if (type == REG_EXPAND_SZ)
        prefix = _T("hex(2):");
    else if (type == REG_MULTI_SZ)
        prefix = _T("hex(7):");
    else if (type == REG_SZ)
        prefix = _T("hex(1):");
    else if (type == REG_QWORD && data.size() >= sizeof(DWORD64))
        prefix = _T("hex(b):");
    text += prefix;

    if (data.empty() && (type == REG_SZ || type == REG_EXPAND_SZ))
    {
        text += _T("00,00\r\n");
        return true;
    }

    for (size_t i = 0; i < data.size(); i++)
    {
        if (i > 0)
            text += _T(",");
        text.AppendFormat(_T("%02X"), data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < data.size())
            text += _T(",\\\r\n ");
    }
    text += _T("\r\n");
    return true;
}

CString SanitizeFileName(const CString& name)
{
    CString safe;
    for (int i = 0; i < name.GetLength(); i++)
    {
        TCHAR c = name[i];
        if (c == _T('<') || c == _T('>') || c == _T(':') ||
            c == _T('"') || c == _T('/') || c == _T('\\') ||
            c == _T('|') || c == _T('?') || c == _T('*'))
            c = _T('_');
        safe += c;
    }
    if (safe.IsEmpty())
        safe = _T("startup_entry");
    return safe;
}

CString GetStartupBackupDir()
{
    TCHAR appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        GetTempPath(MAX_PATH, appData);

    CString dir(appData);
    if (!dir.IsEmpty() && dir.Right(1) != _T("\\"))
        dir += _T("\\");
    dir += _T("PowerBox\\StartupBackup");
    SHCreateDirectoryEx(nullptr, dir, nullptr);
    return dir;
}

bool BackupStartupEntry(const CMFCApplication1Dlg::StartupInfo& si, CString& backupPath)
{
    backupPath.Empty();
    if (si.isFolder)
        return true;

    CString regText;
    if (!BuildRegBackupText(si, regText))
        return false;

    CString dir = GetStartupBackupDir();
    if (dir.Right(1) != _T("\\"))
        dir += _T("\\");

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    CString file;
    file.Format(_T("%sPowerBox_Startup_%04u%02u%02u_%02u%02u%02u_%llu_%s.reg"),
        dir.GetString(), st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, GetTickCount64(),
        SanitizeFileName(si.name).GetString());

    std::wstring content(regText.GetString());
    if (!WriteRegBackupFile(file, content))
        return false;

    backupPath = file;
    return true;
}

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
            if (_tcsicmp(subKey, kRunKey) == 0)
            {
                si.canToggle = true;
                si.approvedRoot = root;
                if (root == HKEY_LOCAL_MACHINE)
                {
                    si.approvedSubKey = (view == KEY_WOW64_32KEY)
                        ? kApprovedRun32 : kApprovedRun;
                    si.approvedView = KEY_WOW64_64KEY;
                }
                else
                {
                    si.approvedSubKey = kApprovedRun;
                    si.approvedView = 0;
                }
                si.enabled = !IsStartupEntryApprovedDisabled(si);
            }
            results.push_back(si);
        }

        index++;
    }

    RegCloseKey(hKey);
}

void AddStartupFolderEntries(LPCTSTR folder, LPCTSTR location,
    HKEY approvedRoot, LPCTSTR approvedSubKey, DWORD approvedView,
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
        si.canToggle = true;
        si.approvedRoot = approvedRoot;
        si.approvedSubKey = approvedSubKey;
        si.approvedView = approvedView;
        si.enabled = !IsStartupEntryApprovedDisabled(si);
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
        AddStartupFolderEntries(userStartup, _T("Startup Folder (User)"),
            HKEY_CURRENT_USER, kApprovedStartupFolder, 0, results);
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_COMMON_STARTUP, nullptr, 0, machineStartup)))
        AddStartupFolderEntries(machineStartup, _T("Startup Folder (All Users)"),
            HKEY_LOCAL_MACHINE, kApprovedStartupFolder, KEY_WOW64_64KEY, results);
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

void CMFCApplication1Dlg::SetSelectedStartupEnabled(bool enabled)
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST2));
    if (!pList)
        return;

    int nSel = pList->GetNextItem(-1, LVNI_SELECTED);
    if (nSel == -1)
        return;

    DWORD_PTR itemData = pList->GetItemData(nSel);
    if (itemData >= m_startupInfos.size())
        return;
    const StartupInfo& si = m_startupInfos[static_cast<size_t>(itemData)];
    if (!si.canToggle || si.approvedSubKey.IsEmpty())
        return;

    if (!WriteStartupEntryApprovedState(si, enabled))
    {
        MessageBox(loc.GetString(_T("Msg"), _T("StartupToggleFail")),
            loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    RefreshStartupList();
}

void CMFCApplication1Dlg::OnEnableStartup()
{
    SetSelectedStartupEnabled(true);
}

void CMFCApplication1Dlg::OnDisableStartup()
{
    SetSelectedStartupEnabled(false);
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
    msg.Format(loc.GetString(_T("Msg"), _T("ConfirmRemoveStartupDanger")),
        si.name, si.location);
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmDelete")),
        MB_YESNO | MB_ICONWARNING) != IDYES) return;

    CString backupPath;
    if (!BackupStartupEntry(si, backupPath))
    {
        MessageBox(loc.GetString(_T("Msg"), _T("StartupBackupFailed")),
            loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    bool removed = si.isFolder
        ? RemoveStartupFolderEntry(si.folderPath, m_hWnd)
        : RemoveStartupRegistryValue(si.root, si.subKey, si.name, si.view);
    if (removed)
    {
        if (!backupPath.IsEmpty())
        {
            CString removedMsg;
            removedMsg.Format(loc.GetString(_T("Msg"), _T("StartupRemovedWithBackup")),
                backupPath);
            MessageBox(removedMsg, loc.GetString(_T("Msg"), _T("Info")),
                MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            MessageBox(loc.GetString(_T("Msg"), _T("StartupRemoved")),
                loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
        }
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
