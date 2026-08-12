#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "BatchRenameDlg.h"
#include "resource.h"
#include "Utils.h"
#include "LocalizationManager.h"

// File management: handle files dropped onto the dialog
void CMFCApplication1Dlg::OnDropFiles(HDROP hDropInfo)
{
    TCHAR szFilePath[MAX_PATH] = {0};
    if (DragQueryFile(hDropInfo, 0, szFilePath, MAX_PATH) == 0)
    {
        DragFinish(hDropInfo);
        return;
    }

    // Determine current tab to decide which path box receives the drop
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    int nCurTab = pTab ? pTab->GetCurSel() : -1;

    // Git tab (index 5): use independent Git path box
    if (nCurTab == 5)
    {
        DragFinish(hDropInfo);
        CString strPath = szFilePath;
        DWORD attrs = ::GetFileAttributes(szFilePath);
        // If dropped a file, use its parent folder; if folder, use it directly
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            int pos = strPath.ReverseFind(_T('\\'));
            if (pos > 0) strPath = strPath.Left(pos);
        }
        m_strGitWorkDir = strPath;
        SetDlgItemText(IDC_STATIC_GIT_PATH, m_strGitWorkDir);
        UpdateGitRepoInfo();
        return;
    }

    // Check if it is a folder (for file tab)
    DWORD attrs = ::GetFileAttributes(szFilePath);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        // Dropped item is a folder, open folder processing window
        DragFinish(hDropInfo);
        auto* pDlg = new CBatchRenameDlg(nullptr, szFilePath);
        pDlg->Create(IDD_BATCH_RENAME_DLG, nullptr);
        pDlg->ShowWindow(SW_SHOW);
        return;
    }

    // Dropped item is a file, continue with existing tab5 logic
    m_strDroppedFilePath = szFilePath;
    // display path
    CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
    if (pStatic)
        pStatic->SetWindowText(m_strDroppedFilePath);
    // restore edit IDC_EDIT4 to default content whenever a new file is dropped
    SetDlgItemText(IDC_EDIT4, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));

    // Automatically switch to tab 5 (index 4) when a file is dropped
    if (pTab)
    {
        pTab->SetCurSel(4);
        LRESULT res = 0;
        OnTcnSelchangeTab1(NULL, &res);
        // populate rename edits: IDC_EDIT7 (basename without ext), IDC_EDIT8 (extension without dot)
        int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
        int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
        CString base, ext;
        if (nDot != -1 && nDot > nSlash)
        {
            base = m_strDroppedFilePath.Mid(nSlash + 1, nDot - nSlash - 1);
            ext = m_strDroppedFilePath.Mid(nDot + 1); // without dot
        }
        else
        {
            base = m_strDroppedFilePath.Mid(nSlash + 1);
            ext = _T("");
        }
        SetDlgItemText(IDC_EDIT7, base);
        SetDlgItemText(IDC_EDIT8, ext);
    }

    DragFinish(hDropInfo);
    CDialogEx::OnDropFiles(hDropInfo);
}

// Single-instance forwarding: receive a folder path from a second launch
// and open the batch rename dialog with it.
BOOL CMFCApplication1Dlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
    if (pCopyDataStruct && pCopyDataStruct->dwData == 1 && pCopyDataStruct->cbData > 0)
    {
        CString strPath = (LPCTSTR)pCopyDataStruct->lpData;
        strPath.Trim();
        // Strip surrounding quotes if present
        if (!strPath.IsEmpty() && strPath[0] == _T('"'))
        {
            strPath = strPath.Mid(1);
            int nLastQuote = strPath.ReverseFind(_T('"'));
            if (nLastQuote >= 0)
                strPath = strPath.Left(nLastQuote);
        }

        if (!strPath.IsEmpty())
        {
            DWORD attrs = ::GetFileAttributes(strPath);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
            {
                auto* pDlg = new CBatchRenameDlg(nullptr, strPath);
                pDlg->Create(IDD_BATCH_RENAME_DLG, nullptr);
                pDlg->ShowWindow(SW_SHOW);
            }
        }
    }

    // Bring main window to the foreground
    if (IsIconic())
        ShowWindow(SW_RESTORE);
    SetForegroundWindow();

    return CDialogEx::OnCopyData(pWnd, pCopyDataStruct);
}

void CMFCApplication1Dlg::OnBnClickedButton3()
{
    auto& loc = CLocalizationManager::GetInstance();
    // Read current file path and requested new name
    CString src = m_strDroppedFilePath;
    if (src.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newName;
    GetDlgItemText(IDC_EDIT4, newName);
    newName.Trim();
    if (newName.IsEmpty())
    {
        newName = AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T(""));
        if (newName.IsEmpty())
        {
            MessageBox(loc.GetString(_T("Msg"), _T("ConfigDefaultName")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
            SetDlgItemText(IDC_EDIT4, newName);
            return;
        }
        SetDlgItemText(IDC_EDIT4, newName);
    }

    // build paths
    int nSlash = src.ReverseFind(_T('\\'));
    CString dir = (nSlash != -1) ? src.Left(nSlash + 1) : CString(_T(""));
    int nDot = src.ReverseFind(_T('.'));
    CString ext = (nDot != -1 && nDot > nSlash) ? src.Mid(nDot) : CString(_T(""));

    CString candidate = dir + newName + ext;
    CString baseName = newName;
    // avoid infinite loop: limit attempts
    int attempt = 0;
    while (GetFileAttributes(candidate) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        baseName = newName + loc.GetString(_T("Msg"), _T("CopySuffix"));
        candidate = dir + baseName + ext;
        newName = baseName; // next iteration will append again if needed
    }

    if (attempt >= 1000)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotGenUniqueName")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(src, candidate, FALSE))
    {
        CString msg; msg.Format(loc.GetString(_T("Msg"), _T("CopySuccess")), candidate);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Success")), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(loc.GetString(_T("Msg"), _T("CopyFail")), GetLastError());
        MessageBox(err, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}


// Rename handler for IDC_BUTTON23
void CMFCApplication1Dlg::OnBnClickedButton23()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newBase, newExt;
    GetDlgItemText(IDC_EDIT7, newBase);
    GetDlgItemText(IDC_EDIT8, newExt);
    newBase.Trim(); newExt.Trim();
    if (newBase.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNameEmpty")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    // validate characters not allowed in Windows filenames
    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); ++i)
    {
        if (newBase.Find(invalid[i]) != -1 || newExt.Find(invalid[i]) != -1)
        {
            MessageBox(loc.GetString(_T("Msg"), _T("InvalidFileNameChars")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            return;
        }
    }

    int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
    int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
    CString dir = (nSlash != -1) ? m_strDroppedFilePath.Left(nSlash + 1) : CString(_T(""));
    CString srcExt = (nDot != -1 && nDot > nSlash) ? m_strDroppedFilePath.Mid(nDot) : CString(_T(""));

    CString dst;
    if (newExt.IsEmpty())
        dst.Format(_T("%s%s%s"), dir, newBase, srcExt);
    else
        dst.Format(_T("%s%s.%s"), dir, newBase, newExt);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("TargetFileExists")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(loc.GetString(_T("Msg"), _T("RenameSuccess")), dst);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Success")), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
        // do not modify IDC_EDIT4 here; user-managed copy name should remain as-is
    }
    else
    {
        DWORD err = GetLastError();
        CString emsg; emsg.Format(loc.GetString(_T("Msg"), _T("RenameFail")), FormatLastError(err), err);
        MessageBox(emsg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                // Close dialog to allow restarted elevated instance to run
                EndDialog(IDOK);
                return;
            }
        }
    }
}


void CMFCApplication1Dlg::OnStnClickedStaticPath()
{
    // TODO: Add control notification handler code here
}

// Copy selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton25()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("InvalidDestPath")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(loc.GetString(_T("Msg"), _T("DestNotDir")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    // If exists, generate unique name
    CString nameOnly = fileName;
    CString ext = _T("");
    int dot = fileName.ReverseFind('.');
    if (dot != -1) { nameOnly = fileName.Left(dot); ext = fileName.Mid(dot); }
    int attempt = 0;
    while (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        CString newName; newName.Format(_T("%s_copy%d%s"), nameOnly, attempt, ext);
        dst.Format(_T("%s\\%s"), destDir, newName);
    }

    if (attempt >= 1000)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotGenUniqueName")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(m_strDroppedFilePath, dst, FALSE))
    {
        CString msg; msg.Format(loc.GetString(_T("Msg"), _T("CopyFileSuccess")), dst);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(loc.GetString(_T("Msg"), _T("CopyFail")), GetLastError());
        MessageBox(err, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

// Move selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton26()
{
    auto& loc = CLocalizationManager::GetInstance();
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("InvalidDestPath")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(loc.GetString(_T("Msg"), _T("DestNotDir")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileAlreadyExists")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(loc.GetString(_T("Msg"), _T("MoveSuccess")), dst);
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Completed")), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
    }
    else
    {
        DWORD err = GetLastError();
        CString em; em.Format(loc.GetString(_T("Msg"), _T("MoveFail")), err);
        MessageBox(em, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                EndDialog(IDOK);
                return;
            }
        }
    }
}

// Checkbox: set or cancel auto-start (write or delete current user Run registry entry)
void CMFCApplication1Dlg::OnBnClickedCheck1()
{
    auto& loc = CLocalizationManager::GetInstance();
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK1);
    if (!pCheck) return;

    // Get executable file name and path
    TCHAR exePath[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, exePath, MAX_PATH) == 0)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotGetExePath")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    CString csExePath = exePath;
    int pos = csExePath.ReverseFind(_T('\\'));
    CString keyName = (pos != -1) ? csExePath.Mid(pos + 1) : csExePath; // Use executable file name as registry key name

    HKEY hKey = NULL;
    LONG ret = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(loc.GetString(_T("Msg"), _T("CannotOpenRegKey")), FormatLastError(err));
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                return;
        }
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    // Query if auto-start entry already exists in registry to toggle checkbox
    DWORD type = 0;
    TCHAR buf[MAX_PATH] = {0};
    DWORD bufSize = sizeof(buf);
    LONG checkRet = RegQueryValueEx(hKey, keyName, NULL, &type, (LPBYTE)buf, &bufSize);
    BOOL isAlreadyAutostart = (checkRet == ERROR_SUCCESS && type == REG_SZ);

    if (isAlreadyAutostart)
    {
        // Cancel auto-start: delete registry value
        ret = RegDeleteValue(hKey, keyName);
        if (ret == ERROR_SUCCESS || ret == ERROR_FILE_NOT_FOUND)
        {
            MessageBox(loc.GetString(_T("Msg"), _T("AutoStartDisabled")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_UNCHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 0);
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(loc.GetString(_T("Msg"), _T("DeleteStartupFail")), FormatLastError(err));
            if (err == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            pCheck->SetCheck(BST_CHECKED);
        }
    }
    else
    {
        // Set auto-start: write registry, value is full executable path with --elevate flag
        CString runValue;
        // Quote path in case it contains spaces
        runValue.Format(_T("\"%s\" --elevate"), csExePath);
        LONG setRet = RegSetValueEx(hKey, keyName, 0, REG_SZ, (const BYTE*)(LPCTSTR)runValue, (runValue.GetLength() + 1) * sizeof(TCHAR));
        if (setRet == ERROR_SUCCESS)
        {
            MessageBox(loc.GetString(_T("Msg"), _T("AutoStartEnabled")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_CHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 1);
        }
        else
        {
            CString msg;
            msg.Format(loc.GetString(_T("Msg"), _T("AddStartupFail")), FormatLastError(GetLastError()));
            if (GetLastError() == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
            // Restore to unchecked
            pCheck->SetCheck(BST_UNCHECKED);
        }
    }

    RegCloseKey(hKey);
}

void CMFCApplication1Dlg::OnViewMinimizeTray()
{
    // Toggle minimize-to-tray behavior (owner-draw checkbox on menu bar)
    m_bMinimizeOnClose = !m_bMinimizeOnClose;
}

// Strip mnemonic markers from menu text for owner-draw display.
// Handles both "(&X)" (CJK style) and "&X" (Western style) patterns.
static CString StripMnemonic(const CString& text)
{
    CString result = text;
    // Pattern 1: "(&X)" - remove the whole group
    int p = result.Find(_T("(&"));
    if (p >= 0 && p + 3 < result.GetLength() && result[p + 3] == _T(')'))
        result = result.Left(p) + result.Mid(p + 4);
    // Pattern 2: remaining "&X" - remove ampersand only
    result.Remove(_T('&'));
    result.Trim();
    return result;
}

void CMFCApplication1Dlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMIS)
{
    // Owner-draw menu item: nIDCtl == 0 for menus
    if (lpMIS && lpMIS->CtlType == ODT_MENU && lpMIS->itemID == ID_VIEW_MINIMIZE_TRAY)
    {
        // Get menu font for text measurement
        NONCLIENTMETRICS ncm = { sizeof(ncm) };
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        CFont font;
        font.CreateFontIndirect(&ncm.lfMenuFont);

        CClientDC dc(this);
        CFont* pOldFont = dc.SelectObject(&font);

        CString text = StripMnemonic(
            CLocalizationManager::GetInstance().GetString(_T("Menu"), _T("MenuMinimizeToTray")));
        CSize sz(0, 0);
        GetTextExtentPoint32(dc.m_hDC, text, text.GetLength(), &sz);
        dc.SelectObject(pOldFont);

        int checkSize = GetSystemMetrics(SM_CXMENUCHECK);
        int cyMenu = GetSystemMetrics(SM_CYMENU);

        lpMIS->itemWidth = checkSize + 6 + sz.cx + 8;
        lpMIS->itemHeight = cyMenu;
        return;
    }
    CDialogEx::OnMeasureItem(nIDCtl, lpMIS);
}

void CMFCApplication1Dlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
    // Owner-draw menu item: nIDCtl == 0 for menus
    if (lpDIS && lpDIS->CtlType == ODT_MENU && lpDIS->itemID == ID_VIEW_MINIMIZE_TRAY)
    {
        CDC dc;
        dc.Attach(lpDIS->hDC);

        CRect rc = lpDIS->rcItem;
        bool bSelected = (lpDIS->itemState & ODS_SELECTED) != 0;
        bool bDisabled = (lpDIS->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;

        // Background
        dc.FillSolidRect(rc, GetSysColor(bSelected ? COLOR_HIGHLIGHT : COLOR_MENU));

        // Draw checkbox frame
        int checkSize = GetSystemMetrics(SM_CXMENUCHECK);
        int checkY = rc.top + (rc.Height() - checkSize) / 2;
        CRect rcCheck(rc.left + 4, checkY, rc.left + 4 + checkSize, checkY + checkSize);
        UINT dfcs = DFCS_BUTTONCHECK | DFCS_FLAT;
        if (m_bMinimizeOnClose)
            dfcs |= DFCS_CHECKED;
        if (bDisabled)
            dfcs |= DFCS_INACTIVE;
        DrawFrameControl(dc.m_hDC, &rcCheck, DFC_BUTTON, dfcs);

        // Draw text
        CString text = StripMnemonic(
            CLocalizationManager::GetInstance().GetString(_T("Menu"), _T("MenuMinimizeToTray")));
        NONCLIENTMETRICS ncm = { sizeof(ncm) };
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        CFont font;
        font.CreateFontIndirect(&ncm.lfMenuFont);
        CFont* pOldFont = dc.SelectObject(&font);

        dc.SetBkMode(TRANSPARENT);
        if (bDisabled)
            dc.SetTextColor(GetSysColor(COLOR_GRAYTEXT));
        else
            dc.SetTextColor(GetSysColor(bSelected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));

        CRect rcText(rcCheck.right + 6, rc.top, rc.right - 4, rc.bottom);
        dc.DrawText(text, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        dc.SelectObject(pOldFont);
        dc.Detach();
        return;
    }
    CDialogEx::OnDrawItem(nIDCtl, lpDIS);
}

// ============================================================================
// File hash calculator (MD5, SHA-1, SHA-256, SHA-512)
// ============================================================================

// Forward declare WinRT hash helper
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>

namespace winrt
{
    using namespace Windows::Storage::Streams;
    using namespace Windows::Security::Cryptography;
    using namespace Windows::Security::Cryptography::Core;
}

static CString ComputeHashWinRT(const CString& filePath, const CString& algorithm)
{
    CString result;
    try
    {
        // Read file bytes
        HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return CString();

        LARGE_INTEGER fileSize{};
        GetFileSizeEx(hFile, &fileSize);
        if (fileSize.QuadPart > 200 * 1024 * 1024) // limit to 200MB
        {
            CloseHandle(hFile);
            return CString();
        }

        std::vector<BYTE> buffer(static_cast<size_t>(fileSize.QuadPart));
        DWORD bytesRead = 0;
        ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, NULL);
        CloseHandle(hFile);

        if (bytesRead == 0) return CString();

        // Create WinRT buffer from the byte array
        winrt::IBuffer winrtBuffer = winrt::CryptographicBuffer::CreateFromByteArray(
            std::vector<byte>{ buffer.begin(), buffer.begin() + bytesRead });

        // Open the hash algorithm provider
        winrt::HashAlgorithmProvider hasher = winrt::HashAlgorithmProvider::OpenAlgorithm(static_cast<PCWSTR>(algorithm));
        winrt::IBuffer hashed = hasher.HashData(winrtBuffer);
        winrt::hstring hex = winrt::CryptographicBuffer::EncodeToHexString(hashed);

        // Convert to CString
        result = CString(hex.c_str());
    }
    catch (...)
    {
        result.Empty();
    }
    return result;
}

void CMFCApplication1Dlg::OnBnClickedHashCalc()
{
    auto& loc = CLocalizationManager::GetInstance();

    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
        return;
    }

    // Check file exists
    if (GetFileAttributes(m_strDroppedFilePath) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFound")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        return;
    }

    // Determine which algorithm is selected (radio buttons)
    struct { CString name; CString algo; UINT id; } algoList[] = {
        { _T("MD5"),    _T("MD5"),    IDC_CHECK_HASH_MD5 },
        { _T("SHA-1"),  _T("SHA1"),   IDC_CHECK_HASH_SHA1 },
        { _T("SHA-256"),_T("SHA256"), IDC_CHECK_HASH_SHA256 },
        { _T("SHA-512"),_T("SHA512"), IDC_CHECK_HASH_SHA512 },
    };

    int checkedId = GetCheckedRadioButton(IDC_CHECK_HASH_MD5, IDC_CHECK_HASH_SHA512);
    CString result;
    for (const auto& a : algoList)
    {
        if (a.id == static_cast<UINT>(checkedId))
        {
            CString hex = ComputeHashWinRT(m_strDroppedFilePath, a.algo);
            if (!hex.IsEmpty())
            {
                result = a.name + _T(": ") + hex;
            }
            else
            {
                result = a.name + _T(": ") + loc.GetString(_T("Msg"), _T("Error"));
            }
            break;
        }
    }

    if (result.IsEmpty())
    {
        result = loc.GetString(_T("MainCtrl"), _T("HashResult"));
    }

    SetDlgItemText(IDC_EDIT_HASH_RESULT, result);
}

void CMFCApplication1Dlg::OnBnClickedHashCopy()
{
    CString text;
    GetDlgItemText(IDC_EDIT_HASH_RESULT, text);
    if (!text.IsEmpty())
    {
        if (OpenClipboard())
        {
            EmptyClipboard();
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (text.GetLength() + 1) * sizeof(TCHAR));
            if (hGlobal)
            {
                LPTSTR pStr = static_cast<LPTSTR>(GlobalLock(hGlobal));
                if (pStr)
                {
                    _tcscpy_s(pStr, text.GetLength() + 1, text);
                    GlobalUnlock(hGlobal);
#ifdef _UNICODE
                    SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
                    SetClipboardData(CF_TEXT, hGlobal);
#endif
                }
            }
            CloseClipboard();
        }
    }
}