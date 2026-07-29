#include "pch.h"
#include "BatchRenameAIDlg.h"
#include "AIApiClient.h"
#include "json.hpp"
#include <set>
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

    SetWindowText(_T("AI 批量重命名助手"));

    // Initialize preview list
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_AI_PREVIEW));
    if (pList)
    {
        pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        CRect rcList;
        pList->GetClientRect(&rcList);
        int totalWidth = rcList.Width() - ::GetSystemMetrics(SM_CXVSCROLL) - 4;
        pList->InsertColumn(0, _T("原文件名"), LVCFMT_LEFT, totalWidth * 45 / 100);
        pList->InsertColumn(1, _T("AI 建议新文件名"), LVCFMT_LEFT, totalWidth * 55 / 100);
    }

    // Disable Apply button until AI responds
    GetDlgItem(IDC_AI_APPLY)->EnableWindow(FALSE);

    // Show file count in status
    CString status;
    status.Format(_T("已加载 %d 个文件，请在下方描述你的重命名需求。"), (int)m_files.size());
    ShowStatus(status);

    // Set focus to description edit
    GetDlgItem(IDC_AI_DESC)->SetFocus();

    return FALSE;
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

    if (description.IsEmpty())
    {
        MessageBox(_T("请输入重命名需求的描述。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
        GetDlgItem(IDC_AI_DESC)->SetFocus();
        return;
    }

    // Read AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        MessageBox(_T("请先在设置中配置 AI 密钥。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    // Disable send button, show waiting status
    GetDlgItem(IDC_AI_SEND)->EnableWindow(FALSE);
    ShowStatus(_T("正在等待 AI 响应..."));

    // Build system prompt
    CString systemPrompt = _T("You are a batch file renaming assistant. Given a list of filenames and a user's natural language description, generate new filenames for each file.\n\n")
        _T("Important rules:\n")
        _T("- Only rename files that match the user's request. If a file should keep its original name, do NOT include it in the output.\n")
        _T("- Do NOT change file extensions unless explicitly requested by the user.\n")
        _T("- Avoid illegal Windows filename characters: \\ / : * ? \" < > |\n")
        _T("- Ensure all new filenames are unique (no duplicates).\n")
        _T("- Preserve the original file extension unless the user explicitly asks to change it.\n\n")
        _T("Return ONLY a valid JSON object (no markdown code blocks, no extra text) with the following format:\n")
        _T("{\"mappings\":[{\"old\":\"original_name.ext\",\"new\":\"new_name.ext\"}]}");

    // Build user prompt with file list
    CString userPrompt;
    userPrompt += _T("File list:\n");
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

    CString* pResult = reinterpret_cast<CString*>(lParam);
    if (!pResult)
    {
        ShowStatus(_T("AI 响应为空。"), true);
        return 0;
    }

    CString response = *pResult;
    delete pResult;

    if (wParam == 0)
    {
        // Error
        CString errMsg = _T("AI 请求失败: ") + response;
        ShowStatus(errMsg, true);
        MessageBox(errMsg, _T("错误"), MB_OK | MB_ICONERROR);
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
    status.Format(_T("AI 已生成 %d 个重命名建议，满意则点击「应用」。"), (int)m_mappings.size());
    ShowStatus(status);

    GetDlgItem(IDC_AI_APPLY)->EnableWindow(!m_mappings.empty());
    m_bAiDone = true;

    return 0;
}

bool CBatchRenameAIDlg::ParseAIResponse(const CString& response)
{
    m_mappings.clear();
    m_reverseMappings.clear();

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
        ShowStatus(_T("AI 返回的内容中没有找到有效的 JSON 数据。"), true);
        MessageBox(_T("AI 返回了无法解析的内容。请重试或调整描述。"), _T("解析错误"), MB_OK | MB_ICONERROR);
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
            ShowStatus(_T("AI 返回的 JSON 格式不正确（缺少 mappings 数组）。"), true);
            return false;
        }

        std::set<CString> newNameSet;
        std::set<CString> oldNameSet; // Build a set of original filenames for validation
        for (const auto& f : m_files)
            oldNameSet.insert(f.first);

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

            // Validation 1: old name must exist in the file list
            if (oldNameSet.find(oldName) == oldNameSet.end())
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
            m_mappings.push_back(mapping);

            // Record reverse mapping for undo
            m_reverseMappings.push_back({ newName, oldName });
        }

        if (m_mappings.empty())
        {
            CString msg;
            msg.Format(_T("AI 没有生成有效的重命名映射。\n跳过了 %d 个无效/重复条目。"), skippedCount);
            if (illegalCount > 0)
                msg.AppendFormat(_T("\n清理了 %d 个含非法字符的文件名。"), illegalCount);
            ShowStatus(msg, true);
            return false;
        }

        // Build warning message if there were issues
        CString warnMsg;
        if (skippedCount > 0 || illegalCount > 0 || dupCount > 0)
        {
            warnMsg = _T("注意：");
            if (skippedCount > 0)
                warnMsg.AppendFormat(_T("\n- %d 个条目被跳过（文件不存在或名称未变更）"), skippedCount);
            if (illegalCount > 0)
                warnMsg.AppendFormat(_T("\n- %d 个文件名包含非法字符，已自动清理"), illegalCount);
            if (dupCount > 0)
                warnMsg.AppendFormat(_T("\n- %d 个重名冲突，已自动添加后缀"), dupCount);
            MessageBox(warnMsg, _T("处理说明"), MB_OK | MB_ICONINFORMATION);
        }
    }
    catch (const std::exception& e)
    {
        CString errMsg;
        errMsg.Format(_T("JSON 解析失败: %hs"), e.what());
        ShowStatus(errMsg, true);
        MessageBox(errMsg, _T("解析错误"), MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void CBatchRenameAIDlg::RefreshPreview()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_AI_PREVIEW));
    if (!pList) return;

    pList->DeleteAllItems();

    for (size_t i = 0; i < m_mappings.size(); i++)
    {
        int idx = pList->InsertItem(static_cast<int>(i), m_mappings[i].oldName);
        pList->SetItemText(idx, 1, m_mappings[i].newName);
    }
}

void CBatchRenameAIDlg::OnBnClickedAiApply()
{
    if (!m_bAiDone || m_mappings.empty())
    {
        MessageBox(_T("请先发送需求给 AI 并等待返回结果。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
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
                msg.Format(_T("文件名「%s」与现有文件冲突，且该文件未被重命名。\n请调整描述后重试。"), (LPCTSTR)newName);
                MessageBox(msg, _T("重名冲突"), MB_OK | MB_ICONWARNING);
                return;
            }
        }
        if (newNameSet.find(newName) != newNameSet.end())
        {
            CString msg;
            msg.Format(_T("AI 生成了重复的文件名「%s」。\n请调整描述后重试。"), (LPCTSTR)newName);
            MessageBox(msg, _T("重名冲突"), MB_OK | MB_ICONWARNING);
            return;
        }
        newNameSet.insert(newName);
    }

    CDialogEx::OnOK();
}