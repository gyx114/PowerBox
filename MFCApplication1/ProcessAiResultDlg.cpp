// ProcessAiResultDlg.cpp: AI process analysis result dialog implementation
#include "pch.h"
#include "ProcessAiResultDlg.h"
#include "resource.h"
#include "AIApiClient.h"
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

    if (m_procInfos.empty())
    {
        SetResult(_T("未选择任何进程。"));
        return TRUE;
    }

    // Build prompt with all selected processes
    CString prompt;
    prompt = _T("分析以下Windows进程，判断每个进程是否为恶意/可疑/无用进程，并简要解释其用途：\n\n");

    for (int i = 0; i < (int)m_procInfos.size(); i++)
    {
        CString idx;
        idx.Format(_T("%d. "), i + 1);
        prompt += idx + m_procInfos[i] + _T("\n");
    }

    prompt += _T("\n请用以下格式回答（每个进程一段，用---分隔）：\n");
    prompt += _T("【进程名】xxx (PID: 123)\n");
    prompt += _T("【安全等级】安全/可疑/恶意/无用\n");
    prompt += _T("【用途】简要说明\n");
    prompt += _T("【建议】如有必要，给出操作建议\n");
    prompt += _T("---\n");

    // Get AI config
    CString vendor = AfxGetApp()->GetProfileString(_T("AI"), _T("Vendor"), _T("DeepSeek"));
    CString apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey_") + vendor, _T(""));
    if (apiKey.IsEmpty())
        apiKey = AfxGetApp()->GetProfileString(_T("AI"), _T("ApiKey"), _T(""));
    CString model = AfxGetApp()->GetProfileString(_T("AI"), _T("Model"), _T(""));

    if (apiKey.IsEmpty())
    {
        SetResult(_T("请先在设置中配置AI API密钥。"));
        return TRUE;
    }

    // Show "analyzing" status
    CString countStr;
    countStr.Format(_T("正在分析 %d 个进程，请稍候..."), (int)m_procInfos.size());
    SetResult(countStr);

    // Build messages
    std::vector<std::pair<CString, CString>> messages;
    messages.push_back({ _T("system"), _T("你是一个Windows系统安全专家。请用中文回答。") });
    messages.push_back({ _T("user"), prompt });

    CAIApiClient::SendAsync(messages, vendor, apiKey, model, m_hWnd);
    return TRUE;
}

void CProcessAiResultDlg::SetResult(const CString& text)
{
    SetDlgItemText(IDC_RICHEDIT_AI_RESULT, text);
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

    CFileDialog dlg(FALSE, _T("txt"), _T("AI分析结果.txt"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||"), this);
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
        CString errorMsg = pResponse ? *pResponse : CString(_T("未知错误"));
        SetResult(_T("AI分析失败: ") + errorMsg);
    }

    if (pResponse) delete pResponse;
    return 0;
}