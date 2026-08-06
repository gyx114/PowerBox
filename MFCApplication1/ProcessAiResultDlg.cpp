// ProcessAiResultDlg.cpp: AI process analysis result dialog implementation
#include "pch.h"
#include "ProcessAiResultDlg.h"
#include "resource.h"
#include "AIApiClient.h"
#include "LocalizationManager.h"
#include "Utils.h"
#include <fstream>

IMPLEMENT_DYNAMIC(CProcessAiResultDlg, CDialogEx)

CProcessAiResultDlg::CProcessAiResultDlg(const std::vector<CString>& procInfos, CWnd* pParent)
    : CDialogEx(IDD_PROCESS_AI_RESULT_DLG, pParent)
    , m_procInfos(procInfos)
{
}

CProcessAiResultDlg::~CProcessAiResultDlg() = default;

void CProcessAiResultDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CProcessAiResultDlg, CDialogEx)
    ON_MESSAGE(WM_AI_RESPONSE, &CProcessAiResultDlg::OnAiResponse)
    ON_BN_CLICKED(IDC_BTN_CLOSE_RESULT, &CProcessAiResultDlg::OnBnClickedClose)
    ON_BN_CLICKED(IDC_BTN_COPY_RESULT, &CProcessAiResultDlg::OnBnClickedCopyResult)
    ON_BN_CLICKED(IDC_BTN_SAVE_RESULT, &CProcessAiResultDlg::OnBnClickedSaveResult)
END_MESSAGE_MAP()

BOOL CProcessAiResultDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("ProcessAiResultDlg")));

    TranslateUI();

    if (m_procInfos.empty())
    {
        SetResult(CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("NoProcessSelected")));
        return TRUE;
    }

    // Build prompt with all selected processes
    CString prompt;
    prompt = _T("Analyze the following Windows processes, determine if each process is malicious/suspicious/unnecessary, and briefly explain its purpose:\n\n");

    for (int i = 0; i < (int)m_procInfos.size(); i++)
    {
        CString idx;
        idx.Format(_T("%d. "), i + 1);
        prompt += idx + m_procInfos[i] + _T("\n");
    }

    prompt += _T("\nFor each process, answer in the following format (one paragraph per process, separated by ---):\n");
    prompt += _T("**[Process Name]** xxx (PID: 123)\n");
    prompt += _T("**[Security Level]** Safe/Suspicious/Malicious/Unnecessary\n");
    prompt += _T("**[Purpose]** Brief description\n");
    prompt += _T("**[Suggestion]** If necessary, give suggestions\n");
    prompt += _T("---\n");

    // Add language constraint based on current UI language
    CString langName = CLocalizationManager::GetInstance().GetString(_T("Language"), _T("DisplayName"));
    prompt += _T("\nCRITICAL LANGUAGE REQUIREMENT:\n");
    prompt += _T("- You MUST respond ENTIRELY in ") + langName + _T(".\n");
    prompt += _T("- This includes ALL field labels, security level values, descriptions, and suggestions.\n");
    prompt += _T("- The field labels [Process Name], [Security Level], [Purpose], [Suggestion] MUST be translated to ") + langName + _T(".\n");
    prompt += _T("- The security level values (Safe/Suspicious/Malicious/Unnecessary) MUST be in ") + langName + _T(".\n");
    prompt += _T("- Do NOT include any English translations in parentheses.\n");
    prompt += _T("- Do NOT mix languages. Only exception: process names, file paths, and PIDs stay as-is.\n");
    prompt += _T("- Keep the ** ** bold markers around field labels.\n");

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        SetResult(CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("ConfigApiKeyFirst")));
        return TRUE;
    }

    // Show "analyzing" status
    CString countStr;
    countStr.Format(CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("AnalyzingProcesses")), (int)m_procInfos.size());
    SetResult(countStr);

    // Build messages
    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), _T("You are a Windows system security expert.") });
    messages.push_back({ _T("user"), prompt });

    CAIApiClient::SendAsync(messages, vendor, apiKey, model, m_hWnd);
    return TRUE;
}

void CProcessAiResultDlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();

    // Translate buttons
    SetDlgItemText(IDC_BTN_COPY_RESULT, loc.GetString(_T("ProcessAiResult"), _T("BtnCopy")));
    SetDlgItemText(IDC_BTN_SAVE_RESULT, loc.GetString(_T("ProcessAiResult"), _T("BtnSave")));
    SetDlgItemText(IDC_BTN_CLOSE_RESULT, loc.GetString(_T("ProcessAiResult"), _T("BtnClose")));
}

void CProcessAiResultDlg::SetResult(const CString& text)
{
    CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_RICHEDIT_AI_RESULT);
    if (!pEdit)
    {
        SetDlgItemText(IDC_RICHEDIT_AI_RESULT, text);
        return;
    }

    // Parse **bold** markers: build clean text and record bold ranges
    CString cleanText;
    std::vector<std::pair<int,int>> boldRanges;
    int i = 0;
    int len = text.GetLength();
    while (i < len)
    {
        if (i + 1 < len && text[i] == _T('*') && text[i + 1] == _T('*'))
        {
            // Found opening **
            int boldStart = cleanText.GetLength();
            i += 2;
            while (i + 1 < len && !(text[i] == _T('*') && text[i + 1] == _T('*')))
            {
                cleanText += text[i];
                i++;
            }
            int boldEnd = cleanText.GetLength();
            if (boldEnd > boldStart)
                boldRanges.push_back({boldStart, boldEnd});
            i += 2; // Skip closing **
        }
        else
        {
            cleanText += text[i];
            i++;
        }
    }

    pEdit->SetWindowText(cleanText);

    // Apply bold formatting to recorded ranges
    if (!boldRanges.empty())
    {
        CHARFORMAT2 cf;
        memset(&cf, 0, sizeof(cf));
        cf.cbSize = sizeof(CHARFORMAT2);
        cf.dwMask = CFM_BOLD;
        cf.dwEffects = CFE_BOLD;

        for (const auto& range : boldRanges)
        {
            pEdit->SetSel(range.first, range.second);
            pEdit->SetSelectionCharFormat(cf);
        }
    }

    // Reset selection to top
    pEdit->SetSel(0, 0);
    pEdit->SendMessage(WM_VSCROLL, SB_TOP, 0);
}

void CProcessAiResultDlg::AppendResult(const CString& text)
{
    CString current;
    GetDlgItemText(IDC_RICHEDIT_AI_RESULT, current);
    current += text;
    SetDlgItemText(IDC_RICHEDIT_AI_RESULT, current);
}

void CProcessAiResultDlg::OnBnClickedClose()
{
    DestroyWindow();
}

void CProcessAiResultDlg::PostNcDestroy()
{
    delete this;
}

void CProcessAiResultDlg::OnCancel()
{
    DestroyWindow();
}

void CProcessAiResultDlg::OnBnClickedCopyResult()
{
    CString text;
    GetDlgItemText(IDC_RICHEDIT_AI_RESULT, text);
    if (text.IsEmpty()) return;

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        int len = (text.GetLength() + 1);
        HGLOBAL hGlob = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
        if (hGlob)
        {
            LPWSTR pBuf = (LPWSTR)::GlobalLock(hGlob);
            if (pBuf)
            {
                wcscpy_s(pBuf, len, text);
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

void CProcessAiResultDlg::OnBnClickedSaveResult()
{
    CString text;
    GetDlgItemText(IDC_RICHEDIT_AI_RESULT, text);
    if (text.IsEmpty()) return;

    auto& loc = CLocalizationManager::GetInstance();
    CFileDialog dlg(FALSE, _T("txt"), loc.GetString(_T("ProcessAiResult"), _T("DefaultFileName")),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        loc.GetString(_T("ProcessAiResult"), _T("FileFilter")), this);
    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        // Write UTF-8 BOM
        std::ofstream ofs(CT2A(path), std::ios::binary);
        if (ofs.is_open())
        {
            // Write UTF-8 BOM
            unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            ofs.write((const char*)bom, 3);
            // Convert to UTF-8 and write
            CStringA utf8(text);
            ofs.write(utf8.GetString(), utf8.GetLength());
            ofs.close();
        }
    }
}

LRESULT CProcessAiResultDlg::OnAiResponse(WPARAM wParam, LPARAM lParam)
{
    int success = static_cast<int>(wParam);
    CString* pResponse = reinterpret_cast<CString*>(lParam);

    if (success && pResponse)
    {
        SetResult(*pResponse);
    }
    else
    {
        CString errorMsg = pResponse ? *pResponse : CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("UnknownError"));
        SetResult(CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("AiAnalysisFailed")) + errorMsg);
    }

    if (pResponse) delete pResponse;
    return 0;
}