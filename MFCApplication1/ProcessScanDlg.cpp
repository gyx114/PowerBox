// ProcessScanDlg.cpp: AI process scan result dialog implementation
#include "pch.h"
#include "ProcessScanDlg.h"
#include "resource.h"
#include "AIApiClient.h"
#include <Shellapi.h>
#include <Psapi.h>
#include <algorithm>

using json = nlohmann::json;

IMPLEMENT_DYNAMIC(CProcessScanDlg, CDialogEx)

CProcessScanDlg::CProcessScanDlg(CWnd* pParent)
    : CDialogEx(IDD_PROCESS_SCAN_DLG, pParent)
{
    m_listLeft = m_listTop = 0;
    m_listRightMargin = 0;
    m_btnEndLeft = m_btnLocateLeft = m_btnEndAllLeft = 0;
    m_btnWidth = m_btnHeight = 0;
    m_statusLeft = m_statusTop = 0;
    m_statusWidth = m_statusHeight = 0;
}

CProcessScanDlg::~CProcessScanDlg() = default;

void CProcessScanDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CProcessScanDlg, CDialogEx)
    ON_MESSAGE(WM_AI_RESPONSE, &CProcessScanDlg::OnAiResponse)
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_BTN_SCAN_END, &CProcessScanDlg::OnBnClickedScanEnd)
    ON_BN_CLICKED(IDC_BTN_SCAN_LOCATE, &CProcessScanDlg::OnBnClickedScanLocate)
    ON_BN_CLICKED(IDC_BTN_SCAN_ENDALL, &CProcessScanDlg::OnBnClickedScanEndAll)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_SCAN_RESULTS, &CProcessScanDlg::OnNMRClickList)
    ON_COMMAND(IDM_SCAN_END, &CProcessScanDlg::OnMenuScanEnd)
    ON_COMMAND(IDM_SCAN_LOCATE, &CProcessScanDlg::OnMenuScanLocate)
    ON_COMMAND(IDM_SCAN_COPY_PATH, &CProcessScanDlg::OnMenuScanCopyPath)
END_MESSAGE_MAP()

BOOL CProcessScanDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Initialize list control
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        // Column ratios: 进程名:18, PID:10, 风险等级:12, AI分析结果:60
        m_colRatios[0] = 18;
        m_colRatios[1] = 10;
        m_colRatios[2] = 12;
        m_colRatios[3] = 60;
        pList->InsertColumn(0, _T("进程名"), LVCFMT_LEFT, 0);
        pList->InsertColumn(1, _T("PID"), LVCFMT_LEFT, 0);
        pList->InsertColumn(2, _T("风险等级"), LVCFMT_LEFT, 0);
        pList->InsertColumn(3, _T("AI分析结果"), LVCFMT_LEFT, 0);
    }

    // Save original layout positions from RC file
    CRect rcList, rcBtn, rcDlg;
    GetClientRect(&rcDlg);
    if (pList) { pList->GetWindowRect(&rcList); ScreenToClient(&rcList); }
    CWnd* pBtnEnd = GetDlgItem(IDC_BTN_SCAN_END);
    CWnd* pBtnLocate = GetDlgItem(IDC_BTN_SCAN_LOCATE);
    CWnd* pBtnEndAll = GetDlgItem(IDC_BTN_SCAN_ENDALL);
    CWnd* pStatus = GetDlgItem(IDC_STATIC_SCAN_STATUS);

    m_listLeft = rcList.left;
    m_listTop = rcList.top;
    m_listRightMargin = rcDlg.Width() - rcList.right;

    if (pBtnEnd) { pBtnEnd->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnEndLeft = rcBtn.left; m_btnWidth = rcBtn.Width(); m_btnHeight = rcBtn.Height(); }
    if (pBtnLocate) { pBtnLocate->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnLocateLeft = rcBtn.left; }
    if (pBtnEndAll) { pBtnEndAll->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnEndAllLeft = rcBtn.left; }
    if (pStatus) { pStatus->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_statusLeft = rcBtn.left; m_statusTop = rcBtn.top; m_statusWidth = rcBtn.Width(); m_statusHeight = rcBtn.Height(); }

    // Set initial column widths based on original list size from RC
    CRect rcListClient;
    if (pList)
    {
        pList->GetClientRect(&rcListClient);
        int initListWidth = rcListClient.Width();
        if (initListWidth > 0)
        {
            int totalRatio = m_colRatios[0] + m_colRatios[1] + m_colRatios[2] + m_colRatios[3];
            for (int i = 0; i < 4; i++)
                pList->SetColumnWidth(i, (initListWidth * m_colRatios[i]) / totalRatio);
        }
    }

    UpdateStatus(_T("正在扫描进程..."));

    return TRUE;
}

void CProcessScanDlg::PostNcDestroy()
{
    delete this;
}

void CProcessScanDlg::OnCancel()
{
    DestroyWindow();
}

void CProcessScanDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED) return;
    ResizeControls();
}

void CProcessScanDlg::ResizeControls()
{
    if (!IsWindow(m_hWnd)) return;

    CRect rcClient;
    GetClientRect(&rcClient);
    int cx = rcClient.Width();
    int cy = rcClient.Height();

    int margin = 7;

    // Buttons: anchor to bottom-left, fixed size from RC (only Y changes)
    int btnY = cy - margin - m_btnHeight;

    CWnd* pBtnEnd = GetDlgItem(IDC_BTN_SCAN_END);
    if (pBtnEnd && IsWindow(pBtnEnd->m_hWnd))
        pBtnEnd->SetWindowPos(nullptr, m_btnEndLeft, btnY, m_btnWidth, m_btnHeight, SWP_NOZORDER);

    CWnd* pBtnLocate = GetDlgItem(IDC_BTN_SCAN_LOCATE);
    if (pBtnLocate && IsWindow(pBtnLocate->m_hWnd))
        pBtnLocate->SetWindowPos(nullptr, m_btnLocateLeft, btnY, m_btnWidth, m_btnHeight, SWP_NOZORDER);

    CWnd* pBtnEndAll = GetDlgItem(IDC_BTN_SCAN_ENDALL);
    if (pBtnEndAll && IsWindow(pBtnEndAll->m_hWnd))
        pBtnEndAll->SetWindowPos(nullptr, m_btnEndAllLeft, btnY, m_btnWidth, m_btnHeight, SWP_NOZORDER);

    // Status label: fixed size from RC, only move position
    CWnd* pStatus = GetDlgItem(IDC_STATIC_SCAN_STATUS);
    if (pStatus && IsWindow(pStatus->m_hWnd))
        pStatus->SetWindowPos(nullptr, m_statusLeft, btnY + 2, m_statusWidth, m_statusHeight, SWP_NOZORDER);

    // List control: stretch between top and button row
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (pList && IsWindow(pList->m_hWnd))
    {
        int listWidth = cx - m_listLeft - m_listRightMargin;
        int listHeight = btnY - m_listTop - margin;
        if (listHeight < 50) listHeight = 50;

        pList->SetWindowPos(nullptr, m_listLeft, m_listTop,
            listWidth, listHeight, SWP_NOZORDER);

        // Adjust column widths proportionally
        CRect rcList;
        pList->GetClientRect(&rcList);
        int listClientWidth = rcList.Width();
        if (listClientWidth > 0)
        {
            int totalRatio = m_colRatios[0] + m_colRatios[1] + m_colRatios[2] + m_colRatios[3];
            for (int i = 0; i < 4; i++)
                pList->SetColumnWidth(i, (listClientWidth * m_colRatios[i]) / totalRatio);
        }
    }
}

void CProcessScanDlg::UpdateStatus(const CString& text)
{
    SetDlgItemText(IDC_STATIC_SCAN_STATUS, text);
}

LRESULT CProcessScanDlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    int success = static_cast<int>(wParam);
    CString* pResponse = reinterpret_cast<CString*>(lParam);

    if (success && pResponse)
    {
        ParseAIResponse(*pResponse);
    }
    else
    {
        CString errorMsg = pResponse ? *pResponse : CString(_T("未知错误"));
        UpdateStatus(_T("AI扫描失败: ") + errorMsg);
        MessageBox(_T("AI扫描失败，请检查网络连接和API密钥。\n") + errorMsg,
            _T("AI扫描"), MB_OK | MB_ICONERROR);
    }

    if (pResponse) delete pResponse;
    return 0;
}

void CProcessScanDlg::ParseAIResponse(const CString& jsonStr)
{
    m_entries.clear();

    try
    {
        std::string s = (LPCSTR)CT2A(jsonStr, CP_UTF8);

        // Step 1: Try to strip markdown code block markers
        std::string jsonPart;
        size_t codeStart = s.find("```json");
        if (codeStart != std::string::npos)
        {
            // Extract content between ```json and the next ```
            size_t contentStart = s.find('\n', codeStart);
            if (contentStart == std::string::npos)
                contentStart = codeStart + 7;
            else
                contentStart++;
            size_t codeEnd = s.find("```", contentStart);
            if (codeEnd != std::string::npos)
                jsonPart = s.substr(contentStart, codeEnd - contentStart);
            else
                jsonPart = s.substr(contentStart);
        }
        else
        {
            // Check for generic ``` marker
            codeStart = s.find("```");
            if (codeStart != std::string::npos)
            {
                size_t contentStart = s.find('\n', codeStart);
                if (contentStart != std::string::npos)
                    contentStart++;
                else
                    contentStart = codeStart + 3;
                size_t codeEnd = s.find("```", contentStart);
                if (codeEnd != std::string::npos)
                    jsonPart = s.substr(contentStart, codeEnd - contentStart);
                else
                    jsonPart = s.substr(contentStart);
            }
        }

        // Step 2: If no code block found, use raw string
        if (jsonPart.empty())
            jsonPart = s;

        // Step 3: Find JSON array boundaries
        size_t start = jsonPart.find('[');
        size_t end = jsonPart.rfind(']');
        if (start == std::string::npos || end == std::string::npos || end <= start)
        {
            // Try JSON object as fallback
            start = jsonPart.find('{');
            end = jsonPart.rfind('}');
            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                // Wrap single object in array for uniform processing
                jsonPart = "[" + jsonPart.substr(start, end - start + 1) + "]";
            }
            else
            {
                UpdateStatus(_T("AI返回格式异常，未找到有效JSON"));
                return;
            }
        }
        else
        {
            jsonPart = jsonPart.substr(start, end - start + 1);
        }

        json j = json::parse(jsonPart);

        if (!j.is_array())
        {
            UpdateStatus(_T("AI返回格式异常"));
            return;
        }

        for (const auto& item : j)
        {
            ScanEntry entry;
            try
            {
                if (item.contains("name") && item["name"].is_string())
                    entry.name = CA2T(item["name"].get<std::string>().c_str(), CP_UTF8);
                if (item.contains("pid"))
                {
                    if (item["pid"].is_string())
                        entry.pid = (DWORD)_atoi64(item["pid"].get<std::string>().c_str());
                    else if (item["pid"].is_number())
                        entry.pid = item["pid"].get<DWORD>();
                }
                if (item.contains("risk") && item["risk"].is_string())
                    entry.risk = CA2T(item["risk"].get<std::string>().c_str(), CP_UTF8);
                if (item.contains("reason") && item["reason"].is_string())
                    entry.reason = CA2T(item["reason"].get<std::string>().c_str(), CP_UTF8);

                if (!entry.name.IsEmpty() && entry.pid > 0)
                    m_entries.push_back(entry);
            }
            catch (...) {
                // Skip malformed entries
                OutputDebugStringA("ParseAIResponse: skipped malformed entry\n");
            }
        }

        CString status;
        status.Format(_T("扫描完成，发现 %d 个可疑/无用进程"), (int)m_entries.size());
        UpdateStatus(status);
    }
    catch (const std::exception& e)
    {
        CString errorMsg;
        errorMsg.Format(_T("AI响应解析失败: %hs"), e.what());
        UpdateStatus(errorMsg);
        OutputDebugStringA(("ParseAIResponse error: " + std::string(e.what()) + "\n").c_str());
    }

    RefreshList();
}

void CProcessScanDlg::RefreshList()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    pList->DeleteAllItems();

    for (int i = 0; i < (int)m_entries.size(); i++)
    {
        auto& entry = m_entries[i];
        CString pidStr;
        pidStr.Format(_T("%u"), entry.pid);

        LVITEM li = {0};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = const_cast<LPTSTR>((LPCTSTR)entry.name);
        pList->InsertItem(&li);
        pList->SetItemText(i, 1, pidStr);
        pList->SetItemText(i, 2, entry.risk);
        pList->SetItemText(i, 3, entry.reason);
    }
}

DWORD CProcessScanDlg::GetSelectedPid()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return 0;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)m_entries.size()) return 0;

    return m_entries[idx].pid;
}

CString CProcessScanDlg::GetSelectedPath()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return _T("");

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)m_entries.size()) return _T("");

    DWORD pid = m_entries[idx].pid;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc)
    {
        TCHAR buf[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageName(hProc, 0, buf, &size))
        {
            CloseHandle(hProc);
            return CString(buf);
        }
        CloseHandle(hProc);
    }
    return _T("");
}

void CProcessScanDlg::EndProcess(int index)
{
    if (index < 0 || index >= (int)m_entries.size()) return;

    auto& entry = m_entries[index];
    CString msg;
    msg.Format(_T("确定要结束进程\n%s (PID: %u) 吗？"), entry.name, entry.pid);
    if (MessageBox(msg, _T("确认结束进程"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.pid);
    if (hProc)
    {
        if (TerminateProcess(hProc, 0))
        {
            MessageBox(_T("进程已成功结束。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            // Remove from list
            m_entries.erase(m_entries.begin() + index);
            RefreshList();
        }
        else
        {
            MessageBox(_T("结束进程失败。"), _T("错误"), MB_OK | MB_ICONERROR);
        }
        CloseHandle(hProc);
    }
    else
    {
        MessageBox(_T("无法打开进程，权限不足。"), _T("错误"), MB_OK | MB_ICONERROR);
    }
}

void CProcessScanDlg::LocateProcess(int index)
{
    if (index < 0 || index >= (int)m_entries.size()) return;

    CString path = GetSelectedPath();
    if (path.IsEmpty())
    {
        MessageBox(_T("无法获取进程路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    {
        CString args;
        args.Format(_T("/select,\"%s\""), path);
        ::ShellExecute(NULL, _T("open"), _T("explorer.exe"), args, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("找不到该文件。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

void CProcessScanDlg::OnBnClickedScanEnd()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    EndProcess(idx);
}

void CProcessScanDlg::OnBnClickedScanLocate()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    LocateProcess(idx);
}

void CProcessScanDlg::OnBnClickedScanEndAll()
{
    if (m_entries.empty())
    {
        MessageBox(_T("没有可结束的进程。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    CString msg;
    msg.Format(_T("确定要批量结束列表中所有 %d 个进程吗？"), (int)m_entries.size());
    if (MessageBox(msg, _T("确认批量结束"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    int success = 0, fail = 0;
    for (const auto& entry : m_entries)
    {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.pid);
        if (hProc)
        {
            if (TerminateProcess(hProc, 0))
                success++;
            else
                fail++;
            CloseHandle(hProc);
        }
        else
        {
            fail++;
        }
    }

    CString result;
    result.Format(_T("已结束 %d 个进程，失败 %d 个。"), success, fail);
    MessageBox(result, _T("批量结束完成"), MB_OK | MB_ICONINFORMATION);

    m_entries.clear();
    RefreshList();
    UpdateStatus(_T("批量结束完成"));
}

void CProcessScanDlg::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, IDM_SCAN_END, _T("结束进程"));
    menu.AppendMenu(MF_STRING, IDM_SCAN_LOCATE, _T("定位"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_SCAN_COPY_PATH, _T("复制路径"));

    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CProcessScanDlg::OnMenuScanEnd()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    EndProcess(idx);
}

void CProcessScanDlg::OnMenuScanLocate()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    LocateProcess(idx);
}

void CProcessScanDlg::OnMenuScanCopyPath()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)m_entries.size()) return;

    CString path = GetSelectedPath();
    if (path.IsEmpty())
    {
        MessageBox(_T("无法获取进程路径。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        int len = (path.GetLength() + 1);
        HGLOBAL hGlob = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
        if (hGlob)
        {
            LPWSTR pBuf = (LPWSTR)::GlobalLock(hGlob);
            if (pBuf)
            {
                wcscpy_s(pBuf, len, path);
                ::GlobalUnlock(hGlob);
                ::SetClipboardData(CF_UNICODETEXT, hGlob);
            }
            else
            {
                ::GlobalFree(hGlob);
            }
        }
        ::CloseClipboard();
    }
}