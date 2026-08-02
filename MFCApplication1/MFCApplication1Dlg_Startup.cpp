#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include "LocalizationManager.h"

// Forward declaration for static helper function
static UINT EnumStartupsThread(LPVOID pParam);

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
    CFileDialog dlg(TRUE, _T("exe"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, _T("Executable Files (*.exe)|*.exe||"));
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        int pos = path.ReverseFind('\\');
        CString name = (pos != -1) ? path.Mid(pos + 1) : path;

        // Try to write to registry
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            LONG ret = RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE*)(LPCTSTR)path, (path.GetLength() + 1) * sizeof(TCHAR));
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

// Delete selected startup item
void CMFCApplication1Dlg::OnRemoveStartup()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST2);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString name = pList->GetItemText(idx, 0);

    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("ConfirmRemoveStartup")), name);
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmDelete")), MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        LONG ret = RegDeleteValue(hKey, name);
        RegCloseKey(hKey);
        if (ret == ERROR_SUCCESS)
        {
            MessageBox(loc.GetString(_T("Msg"), _T("StartupRemoved")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
            RefreshStartupList();
            return;
        }
    }

    MessageBox(loc.GetString(_T("Msg"), _T("StartupRemoveFail")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
}

static UINT EnumStartupsThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::StartupInfo>* results = new std::vector<CMFCApplication1Dlg::StartupInfo>();

    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        TCHAR valueName[256];
        TCHAR data[1024];
        DWORD valueNameSize = 0;
        DWORD dataSize = 0;
        DWORD type = 0;
        DWORD index = 0;

        while (TRUE)
        {
            valueNameSize = _countof(valueName);
            dataSize = sizeof(data);
            LONG ret = RegEnumValue(hKey, index, valueName, &valueNameSize, NULL, &type, (LPBYTE)data, &dataSize);
            if (ret != ERROR_SUCCESS) break;

            CMFCApplication1Dlg::StartupInfo si;
            si.name = valueName;
            si.cmd = data;
            results->push_back(si);

            index++;
        }

        RegCloseKey(hKey);
    }

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