#include "pch.h"
#include "BatchRenameAIDlg.h"
#include "AIApiClient.h"
#include "json.hpp"
#include "LocalizationManager.h"
#include "Utils.h"
#include <set>
#include <map>
#include <regex>

using json = nlohmann::json;

// Illegal Windows filename characters
static const CString ILLEGAL_CHARS = _T("\\/:*?\"<>|");

CBatchRenameAIDlg::CBatchRenameAIDlg(
    const std::vector<std::pair<CString, std::filesystem::path>>& files,
    CWnd* pParent)
    : CDialogEx(IDD_BATCH_RENAME_AI_DLG, pParent)
    , m_files(files)
{
}

void CBatchRenameAIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBatchRenameAIDlg, CDialogEx)
    ON_BN_CLICKED(IDC_AI_SEND, &CBatchRenameAIDlg::OnBnClickedAiSend)
    ON_BN_CLICKED(IDC_AI_APPLY, &CBatchRenameAIDlg::OnBnClickedAiApply)
    ON_MESSAGE(WM_AI_RESPONSE, &CBatchRenameAIDlg::OnAiResponse)
END_MESSAGE_MAP()

BOOL CBatchRenameAIDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("AiRenameDlg")));

    // Initialize preview list
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_AI_PREVIEW));
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
        CRect rcList;
        pList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pList->InsertColumn(0, loc.GetString(L"BatchRename", L"ColCurrentName"), LVCFMT_LEFT, totalWidth * 45 / 100);
        pList->InsertColumn(1, loc.GetString(L"BatchRename", L"ColAiSuggestion"), LVCFMT_LEFT, totalWidth * 55 / 100);

        // Immediately show current filenames in the first column
        for (size_t i = 0; i < m_files.size(); i++)
        {
            int idx = pList->InsertItem(static_cast<int>(i), m_files[i].first);
            pList->SetItemText(idx, 1, _T(""));  // empty until AI responds
        }
    }

    // Disable Apply button until AI responds
    GetDlgItem(IDC_AI_APPLY)->EnableWindow(FALSE);

    // Show file count in status
    CString status;
    status.Format(loc.GetString(L"BatchRename", L"AiDlgStatus"), (int)m_files.size());
    ShowStatus(status);

    // Translate UI controls
    TranslateUI();

    // Set focus to description edit
    GetDlgItem(IDC_AI_DESC)->SetFocus();

    return FALSE;
}

void CBatchRenameAIDlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();

    // Translate static labels
    SetChildTextByCurrentText(this, _T("描述你的重命名需求（支持自然语言）："), loc.GetString(_T("BatchRename"), _T("AiDlgDesc")));
    SetChildTextByCurrentText(this, _T("AI 建议的新文件名预览："), loc.GetString(_T("BatchRename"), _T("AiDlgPreview")));

    // Translate buttons
    SetDlgItemText(IDC_AI_SEND, loc.GetString(_T("BatchRename"), _T("BtnAiSend")));
    SetDlgItemText(IDC_AI_APPLY, loc.GetString(_T("BatchRename"), _T("BtnApply")));
    SetDlgItemText(IDCANCEL, loc.GetString(_T("BatchRename"), _T("BtnCancel")));
}

void CBatchRenameAIDlg::OnCancel()
{
    m_bDestroying = true;
    CDialogEx::OnCancel();
}

void CBatchRenameAIDlg::ShowStatus(const CString& text, bool bError)
{
    SetDlgItemText(IDC_AI_STATUS, text);
    // Set text color: red for error, default otherwise
    CWnd* pStatus = GetDlgItem(IDC_AI_STATUS);
    if (pStatus)
    {
        // Force redraw
        pStatus->Invalidate();
    }
}

bool CBatchRenameAIDlg::HasIllegalChars(const CString& name)
{
    for (int i = 0; i < ILLEGAL_CHARS.GetLength(); i++)
    {
        if (name.Find(ILLEGAL_CHARS[i]) != -1)
            return true;
    }
    return false;
}

CString CBatchRenameAIDlg::SanitizeFilename(const CString& name)
{
    CString result = name;
    for (int i = 0; i < ILLEGAL_CHARS.GetLength(); i++)
    {
        result.Remove(ILLEGAL_CHARS[i]);
    }
    return result;
}

void CBatchRenameAIDlg::OnBnClickedAiSend()
{
    CString description;
    GetDlgItemText(IDC_AI_DESC, description);
    description.Trim();

    auto& loc = CLocalizationManager::GetInstance();

    if (description.IsEmpty())
    {
        MessageBox(loc.GetString(L"BatchRename", L"AiDlgEnterDesc"), loc.GetString(L"Msg", L"Info"), MB_OK | MB_ICONINFORMATION);
        GetDlgItem(IDC_AI_DESC)->SetFocus();
        return;
    }

    // Read AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        MessageBox(loc.GetString(L"Msg", L"ConfigApiKeyFirst"), loc.GetString(L"Msg", L"Info"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Disable send button, show waiting status
    GetDlgItem(IDC_AI_SEND)->EnableWindow(FALSE);
    ShowStatus(loc.GetString(L"BatchRename", L"AiDlgWaiting"));

    // Build system prompt
    CString systemPrompt = _T("You are a batch file renaming assistant. Given a list of CURRENT filenames and a user's natural language description, generate new filenames for each file.\n\n")
        _T("Important rules:\n")
        _T("- The filenames you receive are the CURRENT state (may already have prefixes/suffixes applied by the user)\n")
        _T("- Only rename files that match the user's request. If a file should keep its current name, do NOT include it in the output.\n")
        _T("- Do NOT change file extensions unless explicitly requested by the user.\n")
        _T("- Avoid illegal Windows filename characters: \\ / : * ? \" < > |\n")
        _T("- Ensure all new filenames are unique (no duplicates).\n")
        _T("- Preserve the original file extension unless the user explicitly asks to change it.\n\n")
        _T("Return ONLY a valid JSON object (no markdown code blocks, no extra text) with the following format:\n")
        _T("{\"mappings\":[{\"old\":\"current_name.ext\",\"new\":\"new_name.ext\"}]}");

    // Build user prompt with file list
    CString userPrompt;
    userPrompt += _T("Current file list:\n");
    for (size_t i = 0; i < m_files.size(); i++)
    {
        CString line;
        line.Format(_T("%d. %s\n"), (int)(i + 1), (LPCTSTR)m_files[i].first);
        userPrompt += line;
    }
    userPrompt += _T("\nUser request: ");
    userPrompt += description;
    userPrompt += _T("\n\nReturn the JSON with the mappings.");

    // Build messages
    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), systemPrompt });
    messages.push_back({ _T("user"), userPrompt });

    // Send async request
    CAIApiClient::SendAsync(messages, vendor, apiKey, model, m_hWnd);
}

LRESULT CBatchRenameAIDlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    if (m_bDestroying)
    {
        if (lParam) delete reinterpret_cast<CString*>(lParam);
        return 0;
    }

    GetDlgItem(IDC_AI_SEND)->EnableWindow(TRUE);

    auto& loc = CLocalizationManager::GetInstance();

    CString* pResult = reinterpret_cast<CString*>(lParam);
    if (!pResult)
    {
        ShowStatus(loc.GetString(L"BatchRename", L"AiDlgEmptyResponse"), true);
        return 0;
    }

    CString response = *pResult;
    delete pResult;

    if (wParam == 0)
    {
        // Error
        CString errMsg = loc.GetString(L"Msg", L"AiAnalysisFailed") + response;
        ShowStatus(errMsg, true);
        MessageBox(errMsg, loc.GetString(L"Msg", L"Error"), MB_OK | MB_ICONERROR);
        return 0;
    }

    // Parse the response
    if (!ParseAIResponse(response))
    {
        // ParseAIResponse already shows error status
        return 0;
    }

    // Refresh preview
    RefreshPreview();

    CString status;
    status.Format(loc.GetString(L"BatchRename", L"AiDlgGenerated"), (int)m_mappings.size());
    ShowStatus(status);

    GetDlgItem(IDC_AI_APPLY)->EnableWindow(!m_mappings.empty());
    m_bAiDone = true;

    return 0;
}

bool CBatchRenameAIDlg::ParseAIResponse(const CString& response)
{
    m_mappings.clear();
    m_reverseMappings.clear();

    auto& loc = CLocalizationManager::GetInstance();

    // Try to extract JSON from the response
    // The AI might wrap JSON in markdown code blocks or add extra text
    std::string jsonStr;

    CStringA responseUtf8 = (LPCSTR)CW2A(response, CP_UTF8);

    // Try to find ```json ... ``` block
    const char* pStart = strstr((LPCSTR)responseUtf8, "```json");
    if (pStart)
    {
        pStart += 7; // skip "```json"
        // Skip optional newline
        if (*pStart == '\r') pStart++;
        if (*pStart == '\n') pStart++;
        const char* pEnd = strstr(pStart, "```");
        if (pEnd)
        {
            jsonStr = std::string(pStart, pEnd - pStart);
        }
    }

    if (jsonStr.empty())
    {
        // Try to find ``` ... ``` block (without language tag)
        pStart = strstr((LPCSTR)responseUtf8, "```");
        if (pStart)
        {
            pStart += 3; // skip "```"
            if (*pStart == '\r') pStart++;
            if (*pStart == '\n') pStart++;
            const char* pEnd = strstr(pStart, "```");
            if (pEnd)
            {
                jsonStr = std::string(pStart, pEnd - pStart);
            }
        }
    }

    if (jsonStr.empty())
    {
        // Try to find { ... } in the response directly
        int braceStart = response.Find(_T('{'));
        int braceEnd = response.ReverseFind(_T('}'));
        if (braceStart >= 0 && braceEnd > braceStart)
        {
            CString jsonPart = response.Mid(braceStart, braceEnd - braceStart + 1);
            jsonStr = (LPCSTR)CW2A(jsonPart, CP_UTF8);
        }
    }

    if (jsonStr.empty())
    {
        ShowStatus(loc.GetString(L"BatchRename", L"AiDlgNoJson"), true);
        MessageBox(loc.GetString(L"BatchRename", L"AiDlgParseError"), loc.GetString(L"BatchRename", L"AiDlgParseErrorTitle"), MB_OK | MB_ICONERROR);
        return false;
    }

    try
    {
        // Trim whitespace
        while (!jsonStr.empty() && (jsonStr.front() == ' ' || jsonStr.front() == '\r' || jsonStr.front() == '\n'))
            jsonStr.erase(0, 1);
        while (!jsonStr.empty() && (jsonStr.back() == ' ' || jsonStr.back() == '\r' || jsonStr.back() == '\n'))
            jsonStr.pop_back();

        json j = json::parse(jsonStr);

        if (!j.contains("mappings") || !j["mappings"].is_array())
        {
            ShowStatus(loc.GetString(L"BatchRename", L"AiDlgInvalidJson"), true);
            return false;
        }

        std::set<CString> newNameSet;
        std::map<CString, std::filesystem::path> nameToPath; // Build a map from current name to fullPath for matching
        for (const auto& f : m_files)
            nameToPath[f.first] = f.second;

        int skippedCount = 0;
        int illegalCount = 0;
        int dupCount = 0;

        for (const auto& item : j["mappings"])
        {
            if (!item.contains("old") || !item.contains("new"))
                continue;

            std::string oldUtf8 = item["old"].get<std::string>();
            std::string newUtf8 = item["new"].get<std::string>();
            CString oldName((LPCWSTR)CA2T(oldUtf8.c_str(), CP_UTF8));
            CString newName((LPCWSTR)CA2T(newUtf8.c_str(), CP_UTF8));

            // Validation 1: old name must exist in the file list (match by current name)
            auto it = nameToPath.find(oldName);
            if (it == nameToPath.end())
            {
                skippedCount++;
                continue;
            }

            // Validation 2: old name must differ from new name
            if (oldName.CompareNoCase(newName) == 0)
            {
                skippedCount++;
                continue;
            }

            // Validation 3: check for illegal characters
            if (HasIllegalChars(newName))
            {
                CString sanitized = SanitizeFilename(newName);
                if (sanitized.IsEmpty())
                {
                    illegalCount++;
                    continue;
                }
                newName = sanitized;
                illegalCount++;
            }

            // Validation 4: check for duplicate new names
            if (newNameSet.find(newName) != newNameSet.end())
            {
                // Add a suffix to make it unique
                CString stem, ext;
                int dotPos = newName.ReverseFind(_T('.'));
                if (dotPos > 0)
                {
                    stem = newName.Left(dotPos);
                    ext = newName.Mid(dotPos);
                }
                else
                {
                    stem = newName;
                    ext = _T("");
                }

                int counter = 1;
                CString uniqueName;
                do
                {
                    uniqueName.Format(_T("%s_%d%s"), (LPCTSTR)stem, counter++, (LPCTSTR)ext);
                } while (newNameSet.find(uniqueName) != newNameSet.end());
                newName = uniqueName;
                dupCount++;
            }

            newNameSet.insert(newName);

            AIRenameMapping mapping;
            mapping.oldName = oldName;
            mapping.newName = newName;
            mapping.fullPath = it->second;  // store fullPath for matching back to entries
            m_mappings.push_back(mapping);

            // Record reverse mapping for undo
            m_reverseMappings.push_back({ newName, oldName });
        }

        if (m_mappings.empty())
        {
            CString msg;
            msg.Format(loc.GetString(L"BatchRename", L"AiDlgNoValidMapping"), skippedCount);
            if (illegalCount > 0)
                msg.AppendFormat(loc.GetString(L"BatchRename", L"AiDlgCleanedIllegal"), illegalCount);
            ShowStatus(msg, true);
            return false;
        }

        // Build warning message if there were issues
        CString warnMsg;
        if (skippedCount > 0 || illegalCount > 0 || dupCount > 0)
        {
            warnMsg = loc.GetString(L"BatchRename", L"AiDlgWarning");
            if (skippedCount > 0)
                warnMsg.AppendFormat(loc.GetString(L"BatchRename", L"AiDlgSkipped"), skippedCount);
            if (illegalCount > 0)
                warnMsg.AppendFormat(loc.GetString(L"BatchRename", L"AiDlgIllegal"), illegalCount);
            if (dupCount > 0)
                warnMsg.AppendFormat(loc.GetString(L"BatchRename", L"AiDlgDupFixed"), dupCount);
            MessageBox(warnMsg, loc.GetString(L"BatchRename", L"AiDlgProcessNote"), MB_OK | MB_ICONINFORMATION);
        }
    }
    catch (const std::exception& e)
    {
        CString errMsg;
        errMsg.Format(loc.GetString(L"BatchRename", L"AiDlgJsonParseFail"), e.what());
        ShowStatus(errMsg, true);
        MessageBox(errMsg, loc.GetString(L"BatchRename", L"AiDlgParseErrorTitle"), MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void CBatchRenameAIDlg::RefreshPreview()
{
    auto& loc = CLocalizationManager::GetInstance();

    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_AI_PREVIEW));
    if (!pList) return;

    // Build a map from current name to AI-generated new name
    std::map<CString, CString> nameToNew;
    for (const auto& m : m_mappings)
        nameToNew[m.oldName] = m.newName;

    // Update column 1 for each item (keep column 0 showing current names)
    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        CString curName = pList->GetItemText(i, 0);
        auto it = nameToNew.find(curName);
        if (it != nameToNew.end())
            pList->SetItemText(i, 1, it->second);
        else
            pList->SetItemText(i, 1, loc.GetString(L"BatchRename", L"AiDlgUnchanged"));
    }
}

void CBatchRenameAIDlg::OnBnClickedAiApply()
{
    auto& loc = CLocalizationManager::GetInstance();

    if (!m_bAiDone || m_mappings.empty())
    {
        MessageBox(loc.GetString(L"BatchRename", L"AiDlgSendFirst"), loc.GetString(L"Msg", L"Info"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Final validation: check for duplicate new names with existing files
    std::set<CString> existingNames;
    for (const auto& f : m_files)
        existingNames.insert(f.first);

    std::set<CString> newNameSet;
    for (const auto& m : m_mappings)
    {
        CString newName = m.newName;
        // Check if the new name conflicts with an existing file that is NOT being renamed
        if (existingNames.find(newName) != existingNames.end())
        {
            // Check if the conflicting file is being renamed to something else
            bool bConflict = true;
            for (const auto& m2 : m_mappings)
            {
                if (m2.oldName == newName)
                {
                    bConflict = false;
                    break;
                }
            }
            if (bConflict)
            {
                CString msg;
                msg.Format(loc.GetString(L"BatchRename", L"AiDlgConflict"), (LPCTSTR)newName);
                MessageBox(msg, loc.GetString(L"BatchRename", L"AiDlgConflictTitle"), MB_OK | MB_ICONWARNING);
                return;
            }
        }
        if (newNameSet.find(newName) != newNameSet.end())
        {
            CString msg;
            msg.Format(loc.GetString(L"BatchRename", L"AiDlgDuplicate"), (LPCTSTR)newName);
            MessageBox(msg, loc.GetString(L"BatchRename", L"AiDlgConflictTitle"), MB_OK | MB_ICONWARNING);
            return;
        }
        newNameSet.insert(newName);
    }

    CDialogEx::OnOK();
}