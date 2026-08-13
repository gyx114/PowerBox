// ProcessScanDlg.cpp: AI process scan result dialog implementation
#include "pch.h"
#include "ProcessScanDlg.h"
#include "resource.h"
#include "LocalizationManager.h"
#include "Utils.h"
#include "AIApiClient.h"
#include "MFCApplication1Dlg.h"
#include <Shellapi.h>
#include <Psapi.h>
#include <algorithm>

using json = nlohmann::json;

IMPLEMENT_DYNAMIC(CProcessScanDlg, CDialogEx)

CProcessScanDlg::CProcessScanDlg(CWnd* pParent)
    : CDialogEx(IDD_PROCESS_SCAN_DLG, pParent)
{
    m_listLeft = m_listTop = 0;
    m_listHeight = 0;
    m_listRightMargin = 0;
    m_btnEndLeft = m_btnLocateLeft = m_btnEndAllLeft = m_btnStartLeft = 0;
    m_btnWidth = m_btnHeight = 0;
    m_labelLevelLeft = m_labelLevelTop = 0;
    m_labelLevelWidth = m_labelLevelHeight = 0;
    m_cmbLevelLeft = m_cmbLevelTop = 0;
    m_cmbLevelWidth = m_cmbLevelHeight = 0;
    m_statusLeft = m_statusTop = 0;
    m_statusWidth = m_statusHeight = 0;
    m_checkPrefilterLeft = m_checkPrefilterTop = 0;
}

CProcessScanDlg::~CProcessScanDlg() = default;

void CProcessScanDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CProcessScanDlg, CDialogEx)
    ON_MESSAGE(WM_AI_RESPONSE, &CProcessScanDlg::OnAiResponse)
    ON_MESSAGE(WM_AI_STREAM_CHUNK, &CProcessScanDlg::OnAiStreamChunk)
    ON_MESSAGE(WM_AI_STREAM_DONE, &CProcessScanDlg::OnAiStreamDone)
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_BTN_SCAN_END, &CProcessScanDlg::OnBnClickedScanEnd)
    ON_BN_CLICKED(IDC_BTN_SCAN_LOCATE, &CProcessScanDlg::OnBnClickedScanLocate)
    ON_BN_CLICKED(IDC_BTN_SCAN_ENDALL, &CProcessScanDlg::OnBnClickedScanEndAll)
    ON_BN_CLICKED(IDC_BTN_SCAN_START, &CProcessScanDlg::OnBnClickedScanStart)
    ON_BN_CLICKED(IDC_BTN_SCAN_CLEAR_CACHE, &CProcessScanDlg::OnBnClickedScanClearCache)
    ON_NOTIFY(NM_RCLICK, IDC_LIST_SCAN_RESULTS, &CProcessScanDlg::OnNMRClickList)
    ON_COMMAND(IDM_SCAN_END, &CProcessScanDlg::OnMenuScanEnd)
    ON_COMMAND(IDM_SCAN_LOCATE, &CProcessScanDlg::OnMenuScanLocate)
    ON_COMMAND(IDM_SCAN_COPY_PATH, &CProcessScanDlg::OnMenuScanCopyPath)
END_MESSAGE_MAP()

BOOL CProcessScanDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("ProcessScanDlg")));

    auto& loc = CLocalizationManager::GetInstance();

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
        pList->InsertColumn(0, loc.GetString(_T("ProcessScan"), _T("ColProcessName")), LVCFMT_LEFT, 0);
        pList->InsertColumn(1, loc.GetString(_T("ProcessScan"), _T("ColPID")), LVCFMT_LEFT, 0);
        pList->InsertColumn(2, loc.GetString(_T("ProcessScan"), _T("ColSecurityLevel")), LVCFMT_LEFT, 0);
        pList->InsertColumn(3, loc.GetString(_T("ProcessScan"), _T("ColDescription")), LVCFMT_LEFT, 0);
    }

    // Save original layout positions from RC file
    CRect rcList, rcBtn, rcDlg;
    GetClientRect(&rcDlg);
    if (pList) { pList->GetWindowRect(&rcList); ScreenToClient(&rcList); }
    CWnd* pBtnEnd = GetDlgItem(IDC_BTN_SCAN_END);
    CWnd* pBtnLocate = GetDlgItem(IDC_BTN_SCAN_LOCATE);
    CWnd* pBtnEndAll = GetDlgItem(IDC_BTN_SCAN_ENDALL);
    CWnd* pBtnStart = GetDlgItem(IDC_BTN_SCAN_START);
    CWnd* pLabelLevel = GetDlgItem(IDC_STATIC_SCAN_LEVEL_LABEL);
    CWnd* pCmbLevelWnd = GetDlgItem(IDC_COMBO_SCAN_LEVEL);
    CWnd* pStatus = GetDlgItem(IDC_STATIC_SCAN_STATUS);

    m_listLeft = rcList.left;
    m_listTop = rcList.top;
    m_listHeight = rcList.Height();  // fixed height from RC, only width adjusts on resize
    m_listRightMargin = rcDlg.Width() - rcList.right;

    if (pBtnEnd) { pBtnEnd->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnEndLeft = rcBtn.left; m_btnWidth = rcBtn.Width(); m_btnHeight = rcBtn.Height(); }
    if (pBtnLocate) { pBtnLocate->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnLocateLeft = rcBtn.left; }
    if (pBtnEndAll) { pBtnEndAll->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnEndAllLeft = rcBtn.left; }
    if (pBtnStart) { pBtnStart->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_btnStartLeft = rcBtn.left; }
    if (pLabelLevel) { pLabelLevel->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_labelLevelLeft = rcBtn.left; m_labelLevelTop = rcBtn.top; m_labelLevelWidth = rcBtn.Width(); m_labelLevelHeight = rcBtn.Height(); }
    if (pCmbLevelWnd) { pCmbLevelWnd->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_cmbLevelLeft = rcBtn.left; m_cmbLevelTop = rcBtn.top; m_cmbLevelWidth = rcBtn.Width(); m_cmbLevelHeight = rcBtn.Height(); }
    if (pStatus) { pStatus->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_statusLeft = rcBtn.left; m_statusTop = rcBtn.top; m_statusWidth = rcBtn.Width(); m_statusHeight = rcBtn.Height(); }
    CWnd* pCheckPrefilter = GetDlgItem(IDC_CHECK_SCAN_PREFILTER);
    if (pCheckPrefilter) { pCheckPrefilter->GetWindowRect(&rcBtn); ScreenToClient(&rcBtn); m_checkPrefilterLeft = rcBtn.left; m_checkPrefilterTop = rcBtn.top; }

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

    // Initialize scan level combo box
    CComboBox* pCmbLevel = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_SCAN_LEVEL));
    if (pCmbLevel)
    {
        pCmbLevel->AddString(loc.GetString(_T("ProcessScan"), _T("LevelConservative")));
        pCmbLevel->AddString(loc.GetString(_T("ProcessScan"), _T("LevelStandard")));
        pCmbLevel->AddString(loc.GetString(_T("ProcessScan"), _T("LevelAggressive")));
        pCmbLevel->SetCurSel(1); // Default to "标准"
    }

    // Pre-filter checkbox defaults to checked (local pre-filtering is the
    // recommended performance optimization; user can uncheck to force full AI scan)
    CWnd* pCheck = GetDlgItem(IDC_CHECK_SCAN_PREFILTER);
    if (pCheck)
        pCheck->SendMessage(BM_SETCHECK, BST_CHECKED, 0);

    CString statusReady;
    statusReady.Format(loc.GetString(_T("ProcessScan"), _T("StatusReadyFormat")), loc.GetString(_T("ProcessScan"), _T("BtnStartScan")));
    UpdateStatus(statusReady);

    TranslateUI();

    return TRUE;
}

void CProcessScanDlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();

    // Translate buttons
    SetDlgItemText(IDC_BTN_SCAN_END, loc.GetString(_T("ProcessScan"), _T("BtnEnd")));
    SetDlgItemText(IDC_BTN_SCAN_LOCATE, loc.GetString(_T("ProcessScan"), _T("BtnLocate")));
    SetDlgItemText(IDC_BTN_SCAN_ENDALL, loc.GetString(_T("ProcessScan"), _T("BtnEndAll")));
    SetDlgItemText(IDC_BTN_SCAN_START, loc.GetString(_T("ProcessScan"), _T("BtnStartScan")));
    SetDlgItemText(IDC_BTN_SCAN_CLEAR_CACHE, loc.GetString(_T("ProcessScan"), _T("BtnClearCache")));
    SetDlgItemText(IDC_CHECK_SCAN_PREFILTER, loc.GetString(_T("ProcessScan"), _T("PrefilterLabel")));

    // Translate static label
    SetChildTextByCurrentText(this, _T("审查级别:"), loc.GetString(_T("ProcessScan"), _T("LevelLabel")));
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

    // All controls except the list stay at their original RC positions.
    // Only the list control width adjusts when window is widened horizontally.
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (pList && IsWindow(pList->m_hWnd))
    {
        int listWidth = cx - m_listLeft - m_listRightMargin;

        pList->SetWindowPos(nullptr, m_listLeft, m_listTop,
            listWidth, m_listHeight, SWP_NOZORDER);

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

int CProcessScanDlg::GetScanLevel() const
{
    CComboBox* pCmb = static_cast<CComboBox*>(const_cast<CProcessScanDlg*>(this)->GetDlgItem(IDC_COMBO_SCAN_LEVEL));
    if (pCmb && IsWindow(pCmb->m_hWnd))
        return pCmb->GetCurSel();
    return 1; // Default to "标准"
}

LRESULT CProcessScanDlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    auto& loc = CLocalizationManager::GetInstance();
    int success = static_cast<int>(wParam);
    CString* pResponse = reinterpret_cast<CString*>(lParam);

    if (success && pResponse)
    {
        ParseAIResponse(*pResponse);
    }
    else
    {
        CString errorMsg = pResponse ? *pResponse : loc.GetString(_T("Msg"), _T("UnknownError"));
        CString status;
        status.Format(loc.GetString(_T("Msg"), _T("AiScanFailedStatus")), errorMsg.GetString());
        UpdateStatus(status);
        CString msg;
        msg.Format(loc.GetString(_T("Msg"), _T("AiScanFailedMsg")), errorMsg.GetString());
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }

    if (pResponse) delete pResponse;
    return 0;
}

LRESULT CProcessScanDlg::OnAiStreamChunk(WPARAM wParam, LPARAM lParam)
{
    CString* pChunk = reinterpret_cast<CString*>(lParam);
    if (pChunk)
    {
        m_streamBuffer += *pChunk;
        delete pChunk;
    }
    return 0;
}

LRESULT CProcessScanDlg::OnAiStreamDone(WPARAM wParam, LPARAM lParam)
{
    auto& loc = CLocalizationManager::GetInstance();
    int success = static_cast<int>(wParam);
    CString* pFinal = reinterpret_cast<CString*>(lParam);

    if (success)
    {
        CString full = m_streamBuffer;
        if (pFinal && !pFinal->IsEmpty() && full.IsEmpty())
            full = *pFinal;
        if (!full.IsEmpty())
        {
            ParseAIResponse(full);
        }
        else
        {
            UpdateStatus(loc.GetString(_T("Msg"), _T("AiScanNoContent")));
        }
    }
    else
    {
        CString errorMsg = pFinal ? *pFinal : loc.GetString(_T("Msg"), _T("UnknownError"));
        CString status;
        status.Format(loc.GetString(_T("Msg"), _T("AiScanFailedStatus")), errorMsg.GetString());
        UpdateStatus(status);
        CString msg;
        msg.Format(loc.GetString(_T("Msg"), _T("AiScanFailedMsg")), errorMsg.GetString());
        MessageBox(msg, loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }

    m_streamBuffer.Empty();
    if (pFinal) delete pFinal;
    return 0;
}

void CProcessScanDlg::ParseAIResponse(const CString& jsonStr)
{
    auto& loc = CLocalizationManager::GetInstance();
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
                UpdateStatus(loc.GetString(_T("Msg"), _T("AiParseError")));
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
            UpdateStatus(loc.GetString(_T("Msg"), _T("AiParseError")));
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
        status.Format(loc.GetString(_T("ProcessScan"), _T("ScanCompleteMsg")), (int)m_entries.size());
        UpdateStatus(status);
    }
    catch (const std::exception& e)
    {
        CString errorMsg;
        errorMsg.Format(loc.GetString(_T("Msg"), _T("AiParseFailMsg")), e.what());
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
    auto& loc = CLocalizationManager::GetInstance();
    if (index < 0 || index >= (int)m_entries.size()) return;

    auto& entry = m_entries[index];
    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("ConfirmEndProcess")), entry.name.GetString(), entry.pid);
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmEndProcessTitle")), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.pid);
    if (hProc)
    {
        if (TerminateProcess(hProc, 0))
        {
            MessageBox(loc.GetString(_T("Msg"), _T("ProcessEnded")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
            // Remove from list
            m_entries.erase(m_entries.begin() + index);
            RefreshList();
        }
        else
        {
            MessageBox(loc.GetString(_T("Msg"), _T("ProcessEndFailSimple")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
        }
        CloseHandle(hProc);
    }
    else
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotOpenProcessSimple")), loc.GetString(_T("Msg"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

void CProcessScanDlg::LocateProcess(int index)
{
    auto& loc = CLocalizationManager::GetInstance();
    if (index < 0 || index >= (int)m_entries.size()) return;

    CString path = GetSelectedPath();
    if (path.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotGetPath")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
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
        MessageBox(loc.GetString(_T("Msg"), _T("FileNotFoundMsg")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
    }
}

void CProcessScanDlg::OnBnClickedScanEnd()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    // Collect all selected indices
    std::vector<int> selected;
    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    while (idx != -1)
    {
        selected.push_back(idx);
        idx = pList->GetNextItem(idx, LVNI_SELECTED);
    }
    if (selected.empty()) return;

    if ((int)selected.size() == 1)
    {
        EndProcess(selected[0]);
        return;
    }

    // Multi-select: confirm first, then batch-terminate
    CString msg;
    msg.Format(loc.GetString(_T("Msg"), _T("ConfirmBatchEndMsg")), (int)selected.size());
    if (MessageBox(msg, loc.GetString(_T("Msg"), _T("ConfirmBatchEnd")), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    int success = 0, fail = 0;
    // Sort indices descending so erasing larger ones doesn't invalidate smaller ones
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    for (int i : selected)
    {
        if (i < 0 || i >= (int)m_entries.size()) { fail++; continue; }
        auto& entry = m_entries[i];
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.pid);
        if (hProc)
        {
            if (TerminateProcess(hProc, 0))
                success++;
            else
                fail++;
            CloseHandle(hProc);
            m_entries.erase(m_entries.begin() + i);
        }
        else
        {
            fail++;
        }
    }
    RefreshList();

    CString result;
    result.Format(loc.GetString(_T("Msg"), _T("BatchEndResult")), success, fail);
    MessageBox(result, loc.GetString(_T("Msg"), _T("BatchEndResultTitle")), MB_OK | MB_ICONINFORMATION);
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
    auto& loc = CLocalizationManager::GetInstance();
    if (m_entries.empty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("NoProcessToEnd")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONINFORMATION);
        return;
    }

    int nTotal = (int)m_entries.size();

    // Build a preview of the first several entries to be terminated
    CString preview;
    int previewCount = (nTotal < 8) ? nTotal : 8;
    for (int i = 0; i < previewCount; i++)
    {
        CString line;
        line.Format(_T("  %s (PID: %u)\n"), m_entries[i].name.GetString(), m_entries[i].pid);
        preview += line;
    }
    if (nTotal > previewCount)
    {
        CString more;
        more.Format(loc.GetString(_T("Msg"), _T("EndAllPreviewMore")), nTotal - previewCount);
        preview += more;
    }

    // First confirmation (critical warning, default button = NO)
    CString step1;
    step1.Format(loc.GetString(_T("Msg"), _T("EndAllStep1")),
        nTotal, preview.GetString());
    if (MessageBox(step1, loc.GetString(_T("Msg"), _T("EndAllStep1Title")),
        MB_YESNO | MB_ICONSTOP | MB_DEFBUTTON2) != IDYES)
        return;

    // Second confirmation (double-check)
    CString step2;
    step2.Format(loc.GetString(_T("Msg"), _T("EndAllStep2")), nTotal);
    if (MessageBox(step2, loc.GetString(_T("Msg"), _T("EndAllStep2Title")),
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

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
    result.Format(loc.GetString(_T("Msg"), _T("EndAllResult")), success, fail);
    MessageBox(result, loc.GetString(_T("Msg"), _T("EndAllResultTitle")), MB_OK | MB_ICONINFORMATION);

    m_entries.clear();
    RefreshList();
    UpdateStatus(loc.GetString(_T("Msg"), _T("EndAllDone")));
}

void CProcessScanDlg::OnBnClickedScanStart()
{
    auto& loc = CLocalizationManager::GetInstance();
    int level = GetScanLevel();
    UpdateStatus(loc.GetString(_T("Msg"), _T("Scanning")));

    // Pack pre-filter checkbox state into the high bit of wParam (0x10000)
    WPARAM wParam = (WPARAM)level;
    CWnd* pCheckPrefilter = GetDlgItem(IDC_CHECK_SCAN_PREFILTER);
    if (pCheckPrefilter &&
        pCheckPrefilter->SendMessage(BM_GETCHECK, 0, 0) == BST_CHECKED)
        wParam |= 0x10000;

    // Notify parent (main dialog) to start the AI scan
    CWnd* pParent = GetParent();
    if (pParent && IsWindow(pParent->m_hWnd))
    {
        pParent->PostMessage(WM_PROCESS_SCAN_START, wParam, (LPARAM)m_hWnd);
    }
}

void CProcessScanDlg::OnBnClickedScanClearCache()
{
    auto& loc = CLocalizationManager::GetInstance();

    // Confirm before clearing
    CString prompt = loc.GetString(_T("ProcessScan"), _T("ClearCacheConfirm"));
    if (MessageBox(prompt, loc.GetString(_T("ProcessScan"), _T("BtnClearCache")),
        MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // Clear the version info cache in the main dialog
    CMFCApplication1Dlg::ClearVersionInfoCache();

    CString status = loc.GetString(_T("ProcessScan"), _T("CacheCleared"));
    UpdateStatus(status);
}

void CProcessScanDlg::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CPoint pt;
    ::GetCursorPos(&pt);

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, IDM_SCAN_END, loc.GetString(_T("ProcessScan"), _T("RClickEnd")));
    menu.AppendMenu(MF_STRING, IDM_SCAN_LOCATE, loc.GetString(_T("ProcessScan"), _T("RClickLocate")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_SCAN_COPY_PATH, loc.GetString(_T("ProcessScan"), _T("RClickCopyPath")));

    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    *pResult = 0;
}

void CProcessScanDlg::OnMenuScanEnd()
{
    // Reuse button logic: supports multi-select
    OnBnClickedScanEnd();
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
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SCAN_RESULTS));
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx < 0 || idx >= (int)m_entries.size()) return;

    CString path = GetSelectedPath();
    if (path.IsEmpty())
    {
        MessageBox(loc.GetString(_T("Msg"), _T("CannotGetPath")), loc.GetString(_T("Msg"), _T("Info")), MB_OK | MB_ICONWARNING);
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