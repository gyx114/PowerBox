// MarkdownDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MarkdownDlg.h"
#include "LocalizationManager.h"
#include "afxdialogex.h"
#include <MsHTML.h>
#include <ExDisp.h>
#include "json.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMarkdownDlg, CDialogEx)

CMarkdownDlg::CMarkdownDlg(CWnd* pParent)
	: CDialogEx(IDD_MARKDOWN_DLG, pParent)
	, m_splitPos(280)
	, m_bDragging(false)
	, m_dragOffset(0)
	, m_btnLeft(6)
	, m_btnTop(5)
	, m_btnWidth(57)
	, m_btnHeight(16)
	, m_contentTop(27)
	, m_pathLabelLeft(79)
	, m_pathLabelTop(7)
	, m_pathLabelHeight(12)
{
}

CMarkdownDlg::~CMarkdownDlg()
{
}

void CMarkdownDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MARKDOWN, m_markdownText);
}

BEGIN_MESSAGE_MAP(CMarkdownDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_DROPFILES()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_EN_CHANGE(IDC_EDIT_MARKDOWN, &CMarkdownDlg::OnEnChangeEdit)
	ON_BN_CLICKED(IDC_BTN_OPEN, &CMarkdownDlg::OnBnClickedOpen)
END_MESSAGE_MAP()

BOOL CMarkdownDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("MarkdownDlg")));

	DragAcceptFiles(TRUE);

	// Read button dimensions from RC template
	CWnd* pBtn = GetDlgItem(IDC_BTN_OPEN);
	if (pBtn)
	{
		CRect rcBtn;
		pBtn->GetWindowRect(&rcBtn);
		ScreenToClient(&rcBtn);
		m_btnLeft = rcBtn.left;
		m_btnTop = rcBtn.top;
		m_btnWidth = rcBtn.Width();
		m_btnHeight = rcBtn.Height();
	}

	// Read path label position from RC template
	CWnd* pPathLabel = GetDlgItem(IDC_STATIC_MARKDOWN_PATH);
	if (pPathLabel)
	{
		CRect rcLabel;
		pPathLabel->GetWindowRect(&rcLabel);
		ScreenToClient(&rcLabel);
		m_pathLabelLeft = rcLabel.left;
		m_pathLabelTop = rcLabel.top;
		m_pathLabelHeight = rcLabel.Height();
	}

	// Read content top from edit control's position in RC
	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit)
	{
		CRect rcEdit;
		pEdit->GetWindowRect(&rcEdit);
		ScreenToClient(&rcEdit);
		m_contentTop = rcEdit.top;

		m_fontEdit.CreatePointFont(100, _T("Consolas"));
		pEdit->SetFont(&m_fontEdit);
	}

	// Compute initial split position from the preview placeholder in RC
	CWnd* pPlaceholder = GetDlgItem(IDC_MARKDOWN_PREVIEW);
	if (pPlaceholder)
	{
		CRect rcPreview;
		pPlaceholder->GetWindowRect(&rcPreview);
		ScreenToClient(&rcPreview);
		m_splitPos = rcPreview.left;
		pPlaceholder->DestroyWindow();
	}

	// Create WebBrowser ActiveX control; position will be set in ResizeControls
	if (m_browser.CreateControl(CLSID_WebBrowser, nullptr,
		WS_VISIBLE | WS_CHILD, CRect(0, 0, 0, 0), this, IDC_MARKDOWN_PREVIEW))
	{
		LPUNKNOWN pUnk = m_browser.GetControlUnknown();
		if (pUnk)
		{
			IWebBrowser2* pWeb2 = nullptr;
			if (SUCCEEDED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
			{
				BSTR bstrBlank = SysAllocString(L"about:blank");
				pWeb2->Navigate(bstrBlank, nullptr, nullptr, nullptr, nullptr);
				SysFreeString(bstrBlank);
				pWeb2->Release();
			}
		}
	}

	// Give the browser its real size immediately — a zero-size control
	// will never render even if content is written to its document
	ResizeControls();

	static const wchar_t* sample = L"# Markdown Preview\r\n"
		L"\r\n"
		L"Welcome to the **Markdown Editor**!\r\n"
		L"\r\n"
		L"## Features\r\n"
		L"\r\n"
		L"- **Bold** and *italic* text\r\n"
		L"- `inline code`\r\n"
		L"- [Links](https://example.com)\r\n"
		L"- > Blockquotes\r\n"
		L"- ~~Strikethrough~~\r\n"
		L"\r\n"
		L"## Code Block\r\n"
		L"\r\n"
		L"```cpp\r\n"
		L"#include <iostream>\r\n"
		L"int main() {\r\n"
		L"    std::cout << \"Hello!\";\r\n"
		L"    return 0;\r\n"
		L"}\r\n"
		L"```\r\n"
		L"\r\n"
		L"---\r\n"
		L"\r\n"
		L"### Table\r\n"
		L"\r\n"
		L"| Feature | Status |\r\n"
		L"|---------|--------|\r\n"
		L"| Syntax Highlighting | Done |\r\n"
		L"| Tables | Done |\r\n"
		L"\r\n"
		L"*Start typing to preview!*";

	m_markdownText = sample;
	UpdateData(FALSE);

	// Navigate("about:blank") is async — the document won't be ready yet.
	// Use a retry timer until SetBrowserHtml succeeds.
	SetTimer(1, 100, nullptr);

	return TRUE;
}

void CMarkdownDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
}

void CMarkdownDlg::OnPaint()
{
	CDialogEx::OnPaint();

	// Draw the splitter bar
	CRect rcClient;
	GetClientRect(&rcClient);
	const int splitterHalf = 2;

	CRect rcSplitter(m_splitPos - splitterHalf, m_contentTop,
		m_splitPos + splitterHalf, rcClient.Height());

	CClientDC dc(this);
	dc.FillSolidRect(&rcSplitter, RGB(200, 200, 200));
}

void CMarkdownDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		// Retry until the WebBrowser document is ready (about:blank is async)
		if (SetBrowserHtml(MarkdownToHtml(m_markdownText)))
			KillTimer(1);
	}
	CDialogEx::OnTimer(nIDEvent);
}

BOOL CMarkdownDlg::PreTranslateMessage(MSG* pMsg)
{
	if (IsDialogMessage(pMsg))
		return TRUE;
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CMarkdownDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

void CMarkdownDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED)
		ResizeControls();
}

void CMarkdownDlg::OnEnChangeEdit()
{
	UpdateData(TRUE);
	RefreshPreview();
}

void CMarkdownDlg::OnDropFiles(HDROP hDropInfo)
{
	UINT nFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	if (nFiles == 0)
		return;

	wchar_t szFile[MAX_PATH];
	DragQueryFile(hDropInfo, 0, szFile, MAX_PATH);
	DragFinish(hDropInfo);

	LoadFile(szFile);
}

void CMarkdownDlg::OnBnClickedOpen()
{
	CFileDialog dlg(TRUE, _T("*.md"), nullptr,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("Markdown Files (*.md)|*.md|Text Files (*.txt)|*.txt|All Files (*.*)|*.*||"),
		this);

	if (dlg.DoModal() == IDOK)
		LoadFile(dlg.GetPathName());
}

void CMarkdownDlg::LoadFile(const CString& path)
{
	if (path.IsEmpty())
		return;

	CFile file;
	if (!file.Open(path, CFile::modeRead))
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Markdown"), _T("FailedToOpen")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Error")), MB_ICONERROR);
		return;
	}

	ULONGLONG size = file.GetLength();
	if (size == 0 || size > 10 * 1024 * 1024)
	{
		file.Close();
		if (size > 10 * 1024 * 1024)
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("Markdown"), _T("FileTooLarge")), 
					  CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Error")), MB_ICONERROR);
		return;
	}

	std::string raw((size_t)size, '\0');
	file.Read(&raw[0], (UINT)size);
	file.Close();

	if (raw.size() >= 3 &&
		(unsigned char)raw[0] == 0xEF &&
		(unsigned char)raw[1] == 0xBB &&
		(unsigned char)raw[2] == 0xBF)
	{
		raw = raw.substr(3);
	}

	int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.c_str(), -1, nullptr, 0);
	UINT cp = CP_UTF8;
	if (wlen == 0)
	{
		wlen = MultiByteToWideChar(CP_ACP, 0, raw.c_str(), -1, nullptr, 0);
		cp = CP_ACP;
	}

	if (wlen > 0)
	{
		wchar_t* wbuf = m_markdownText.GetBuffer(wlen);
		MultiByteToWideChar(cp, 0, raw.c_str(), -1, wbuf, wlen);
		m_markdownText.ReleaseBuffer(wlen - 1);
	}

	UpdateData(FALSE);
	RefreshPreview();

	CString title;
	title.Format(CLocalizationManager::GetInstance().GetString(_T("Markdown"), _T("MarkdownTitle")), PathFindFileName(path));
	SetWindowText(title);

	// Show file path in the label
	SetDlgItemText(IDC_STATIC_MARKDOWN_PATH, path);
}

void CMarkdownDlg::ResizeControls()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	const int splitterHalf = 2;

	// Clamp split position to valid range
	int minSplit = 100;
	int maxSplit = rcClient.Width() - 100;
	if (m_splitPos < minSplit) m_splitPos = minSplit;
	if (m_splitPos > maxSplit) m_splitPos = maxSplit;

	// Open button: stays at RC-defined position (fixed distance from top-left)
	CWnd* pBtn = GetDlgItem(IDC_BTN_OPEN);
	if (pBtn && ::IsWindow(pBtn->m_hWnd))
	{
		pBtn->SetWindowPos(nullptr, m_btnLeft, m_btnTop,
			m_btnWidth, m_btnHeight, SWP_NOZORDER);
	}

	// Path label: fills from RC-defined left edge to window right edge
	CWnd* pPathLabel = GetDlgItem(IDC_STATIC_MARKDOWN_PATH);
	if (pPathLabel && ::IsWindow(pPathLabel->m_hWnd))
	{
		pPathLabel->SetWindowPos(nullptr,
			m_pathLabelLeft, m_pathLabelTop,
			rcClient.Width() - m_pathLabelLeft - 5, m_pathLabelHeight,
			SWP_NOZORDER);
	}

	// Edit control: fills from left edge to splitter
	CWnd* pEdit = GetDlgItem(IDC_EDIT_MARKDOWN);
	if (pEdit && ::IsWindow(pEdit->m_hWnd))
	{
		pEdit->SetWindowPos(nullptr,
			5, m_contentTop,
			m_splitPos - splitterHalf - 5,
			rcClient.Height() - m_contentTop - 5,
			SWP_NOZORDER);
	}

	// WebBrowser: fills from splitter to right edge
	if (m_browser.m_hWnd && ::IsWindow(m_browser.m_hWnd))
	{
		int previewLeft = m_splitPos + splitterHalf;
		m_browser.SetWindowPos(nullptr,
			previewLeft, m_contentTop,
			rcClient.Width() - previewLeft - 5,
			rcClient.Height() - m_contentTop - 5,
			SWP_NOZORDER);
	}

	// Invalidate the splitter area to ensure it repaints
	CRect rcSplitter(m_splitPos - splitterHalf, m_contentTop,
		m_splitPos + splitterHalf, rcClient.Height());
	InvalidateRect(&rcSplitter, FALSE);
}

BOOL CMarkdownDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (pWnd == this && nHitTest == HTCLIENT)
	{
		CPoint pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);

		CRect rcClient;
		GetClientRect(&rcClient);

		// Check if cursor is over the splitter area (4px hot zone)
		if (pt.y >= m_contentTop && pt.x >= m_splitPos - 4 && pt.x <= m_splitPos + 4)
		{
			::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
			return TRUE;
		}
	}
	return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}

void CMarkdownDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (point.y >= m_contentTop && point.x >= m_splitPos - 4 && point.x <= m_splitPos + 4)
	{
		m_bDragging = true;
		m_dragOffset = point.x - m_splitPos;
		SetCapture();
	}
	else
	{
		CDialogEx::OnLButtonDown(nFlags, point);
	}
}

void CMarkdownDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		m_bDragging = false;
		ReleaseCapture();
	}
	else
	{
		CDialogEx::OnLButtonUp(nFlags, point);
	}
}

void CMarkdownDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		int newSplit = point.x - m_dragOffset;
		// Clamp: minimum 100px for each side
		CRect rcClient;
		GetClientRect(&rcClient);
		if (newSplit < 100) newSplit = 100;
		if (newSplit > rcClient.Width() - 100) newSplit = rcClient.Width() - 100;

		if (newSplit != m_splitPos)
		{
			m_splitPos = newSplit;
			ResizeControls();
		}
	}
	else
	{
		CDialogEx::OnMouseMove(nFlags, point);
	}
}

void CMarkdownDlg::RefreshPreview()
{
	if (!m_browser.m_hWnd)
		return;
	SetBrowserHtml(MarkdownToHtml(m_markdownText));
}

bool CMarkdownDlg::SetBrowserHtml(const CString& html)
{
	LPUNKNOWN pUnk = m_browser.GetControlUnknown();
	if (!pUnk)
		return false;

	IWebBrowser2* pWeb2 = nullptr;
	if (FAILED(pUnk->QueryInterface(IID_IWebBrowser2, (void**)&pWeb2)))
		return false;

	IDispatch* pDocDisp = nullptr;
	if (FAILED(pWeb2->get_Document(&pDocDisp)) || !pDocDisp)
	{
		// Document not ready yet (Navigate is async)
		pWeb2->Release();
		return false;
	}

	IHTMLDocument2* pDoc = nullptr;
	if (FAILED(pDocDisp->QueryInterface(IID_IHTMLDocument2, (void**)&pDoc)))
	{
		pDocDisp->Release();
		pWeb2->Release();
		return false;
	}

	BSTR bstrHtml = html.AllocSysString();
	SAFEARRAY* psa = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	if (psa)
	{
		VARIANT* pv = nullptr;
		if (SUCCEEDED(SafeArrayAccessData(psa, (void**)&pv)))
		{
			pv->vt = VT_BSTR;
			pv->bstrVal = bstrHtml;
			SafeArrayUnaccessData(psa);
			pDoc->close();
			pDoc->write(psa);
			pDoc->close();
		}
		SafeArrayDestroy(psa);
	}
	else
	{
		SysFreeString(bstrHtml);
	}

	pDoc->Release();
	pDocDisp->Release();
	pWeb2->Release();
	return true;
}

CString CMarkdownDlg::EscapeHtml(const CString& text)
{
	CString out;
	out.Preallocate(text.GetLength() * 2);
	for (int i = 0; i < text.GetLength(); ++i)
	{
		TCHAR ch = text[i];
		switch (ch)
		{
		case L'&': out += _T("&amp;"); break;
		case L'<': out += _T("&lt;"); break;
		case L'>': out += _T("&gt;"); break;
		case L'\"': out += _T("&quot;"); break;
		default: out += ch; break;
		}
	}
	return out;
}

CString CMarkdownDlg::FormatInline(const CString& text)
{
	CString out;
	int i = 0;
	int n = text.GetLength();
	while (i < n)
	{
		// Link [text](url)
		if (text[i] == _T('['))
		{
			int j = text.Find(_T(']'), i + 1);
			if (j > i + 1 && j + 1 < n && text[j + 1] == _T('('))
			{
				int k = text.Find(_T(')'), j + 2);
				if (k > j + 2)
				{
					CString linkText = text.Mid(i + 1, j - i - 1);
					CString url = text.Mid(j + 2, k - j - 2);
					out += _T("<a href=\"") + EscapeHtml(url) + _T("\">") + FormatInline(linkText) + _T("</a>");
					i = k + 1;
					continue;
				}
			}
		}

		// Code `...`
		if (text[i] == _T('`'))
		{
			int j = text.Find(_T('`'), i + 1);
			if (j > i + 1)
			{
				CString code = text.Mid(i + 1, j - i - 1);
				out += _T("<code>") + EscapeHtml(code) + _T("</code>");
				i = j + 1;
				continue;
			}
		}

		// Bold **...**
		if (i + 1 < n && text[i] == _T('*') && text[i + 1] == _T('*'))
		{
			int j = i + 2;
			while (j + 1 < n && !(text[j] == _T('*') && text[j + 1] == _T('*'))) ++j;
			if (j + 1 < n)
			{
				CString bold = text.Mid(i + 2, j - i - 2);
				out += _T("<strong>") + FormatInline(bold) + _T("</strong>");
				i = j + 2;
				continue;
			}
		}

		// Strikethrough ~~...~~
		if (i + 1 < n && text[i] == _T('~') && text[i + 1] == _T('~'))
		{
			int j = text.Find(_T("~~"), i + 2);
			if (j > i + 2)
			{
				CString strike = text.Mid(i + 2, j - i - 2);
				out += _T("<del>") + FormatInline(strike) + _T("</del>");
				i = j + 2;
				continue;
			}
		}

		// Italic *...* (single)
		if (text[i] == _T('*'))
		{
			int j = text.Find(_T('*'), i + 1);
			if (j > i + 1)
			{
				CString italic = text.Mid(i + 1, j - i - 1);
				out += _T("<em>") + FormatInline(italic) + _T("</em>");
				i = j + 1;
				continue;
			}
		}

		out += EscapeHtml(text.Mid(i, 1));
		++i;
	}
	return out;
}

CString CMarkdownDlg::MarkdownToHtml(const CString& markdown)
{
	CString html;
	html += _T("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">");
	html += _T("<style>");
	html += _T("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;");
	html += _T("font-size:14px;line-height:1.6;color:#24292f;background:#ffffff;padding:16px;margin:0;}");
	html += _T("h1,h2,h3,h4,h5,h6{font-weight:600;line-height:1.25;margin-top:24px;margin-bottom:16px;}");
	html += _T("h1{font-size:2em;border-bottom:1px solid #d0d7de;padding-bottom:.3em;}");
	html += _T("h2{font-size:1.5em;border-bottom:1px solid #d0d7de;padding-bottom:.3em;}");
	html += _T("h3{font-size:1.25em;}");
	html += _T("p{margin-top:0;margin-bottom:16px;}");
	html += _T("a{color:#0969da;text-decoration:none;}a:hover{text-decoration:underline;}");
	html += _T("code{font-family:ui-monospace,SFMono-Regular,SF Mono,Menlo,Consolas,monospace;");
	html += _T("font-size:85%;background-color:rgba(175,184,193,0.2);padding:.2em .4em;border-radius:6px;}");
	html += _T("pre{background-color:#f6f8fa;border-radius:6px;padding:16px;overflow:auto;margin-bottom:16px;}");
	html += _T("pre code{background:transparent;padding:0;font-size:100%;}");
	html += _T("blockquote{color:#57606a;border-left:.25em solid #d0d7de;padding:0 1em;margin:0 0 16px 0;}");
	html += _T("blockquote p{margin-bottom:0;}");
	html += _T("ul,ol{padding-left:2em;margin-bottom:16px;}");
	html += _T("li{margin-bottom:.25em;}");
	html += _T("hr{height:.25em;padding:0;margin:24px 0;background:#d0d7de;border:0;}");
	html += _T("table{border-collapse:collapse;border-spacing:0;margin-bottom:16px;width:auto;}");
	html += _T("th,td{border:1px solid #d0d7de;padding:6px 13px;}");
	html += _T("th{background:#f6f8fa;font-weight:600;}");
	html += _T("tr:nth-child(2n){background:#f6f8fa;}");
	html += _T("del{color:#57606a;text-decoration:line-through;}");
	html += _T(".action-card{border:2px solid #d0d7de;border-radius:8px;padding:12px 16px;margin:12px 0;background:#f6f8fa;font-family:Consolas,monospace;}");
	html += _T(".action-card.action-level-low{border-color:#2da44e;}");
	html += _T(".action-card.action-level-medium{border-color:#d4a72c;}");
	html += _T(".action-card.action-level-high{border-color:#cf222e;}");
	html += _T(".action-purpose{font-size:13px;color:#24292f;margin-bottom:4px;}");
	html += _T(".action-risk{font-size:11px;font-weight:600;margin-bottom:8px;}");
	html += _T(".action-risk.level-low{color:#2da44e;}");
	html += _T(".action-risk.level-medium{color:#d4a72c;}");
	html += _T(".action-risk.level-high{color:#cf222e;}");
	html += _T(".action-btn{background:#2da44e;color:#fff;border:none;border-radius:6px;padding:4px 12px;font-size:13px;cursor:pointer;margin-bottom:6px;}");
	html += _T(".action-btn:hover{background:#218838;}");
	html += _T(".action-card.action-level-high .action-btn{background:#cf222e;}");
	html += _T(".action-card.action-level-high .action-btn:hover{background:#a11c26;}");
	html += _T(".action-card.action-level-medium .action-btn{background:#d4a72c;}");
	html += _T(".action-card.action-level-medium .action-btn:hover{background:#b88a1f;}");
	html += _T(".action-command{font-size:12px;color:#57606a;background:#eaeef2;padding:4px 8px;border-radius:4px;word-break:break-all;}");
	html += _T("</style></head><body>");

	CString body;
	bool inCodeBlock = false;
	bool inActionBlock = false;
	CString actionJson;
	bool inList = false;
	bool inQuote = false;
	bool inTable = false;
	bool inTHead = false;
	bool inParagraph = false;

	CStringArray lines;
	{
		int start = 0;
		while (start < markdown.GetLength())
		{
			int end = markdown.Find(_T("\n"), start);
			if (end < 0)
			{
				lines.Add(markdown.Mid(start));
				break;
			}
			CString line = markdown.Mid(start, end - start);
			line.Remove(_T('\r'));
			lines.Add(line);
			start = end + 1;
		}
	}

	auto CloseBlocks = [&](bool keepParagraph)
	{
		if (inCodeBlock) { body += _T("</code></pre>"); inCodeBlock = false; }
		if (inActionBlock) { inActionBlock = false; }
		if (inList) { body += _T("</ul>"); inList = false; }
		if (inQuote) { body += _T("</blockquote>"); inQuote = false; }
		if (inTable) { body += _T("</tbody></table>"); inTable = false; inTHead = false; }
		if (inParagraph && !keepParagraph) { body += _T("</p>"); inParagraph = false; }
	};

	auto RenderActionCard = [&](const CString& jsonStr)
	{
		CString command, purpose, risk;
		try
		{
			std::string s = (LPCSTR)CT2A(jsonStr, CP_UTF8);
			nlohmann::json j = nlohmann::json::parse(s);
			command = CString(CA2T(j["command"].get<std::string>().c_str(), CP_UTF8));
			purpose = CString(CA2T(j["purpose"].get<std::string>().c_str(), CP_UTF8));
			risk = CString(CA2T(j["risk"].get<std::string>().c_str(), CP_UTF8));
		}
		catch (const nlohmann::json::parse_error&) { }
		risk.MakeLower();
		if (risk.IsEmpty()) risk = _T("medium");

		// Risk level escalation for dangerous keywords
		CString cmdLower = command;
		cmdLower.MakeLower();
		static const CString dangerous[] = {
			_T("del "), _T("rd /s"), _T("rmdir /s"),
			_T("format "),
			_T("reg delete"), _T("reg add"),
			_T("net user"), _T("net localgroup"),
			_T("takeown"), _T("icacls"),
			_T("schtasks"),
			_T("bcdedit")
		};
		for (const auto& kw : dangerous)
		{
			if (cmdLower.Find(kw) >= 0)
			{
				risk = _T("high");
				break;
			}
		}

		auto& loc = CLocalizationManager::GetInstance();
		CString riskText, riskLevel;
		if (risk == _T("high"))
		{
			riskText = loc.GetString(_T("Markdown"), _T("RiskHigh"));
			riskLevel = _T("level-high");
		}
		else if (risk == _T("low"))
		{
			riskText = loc.GetString(_T("Markdown"), _T("RiskLow"));
			riskLevel = _T("level-low");
		}
		else
		{
			riskText = loc.GetString(_T("Markdown"), _T("RiskMedium"));
			riskLevel = _T("level-medium");
		}

		// Build JSON string for data-cmd attribute using nlohmann::json
		CString jsonAttr;
		{
			nlohmann::json j;
			j["command"] = (LPCSTR)CT2A(command, CP_UTF8);
			j["purpose"] = (LPCSTR)CT2A(purpose, CP_UTF8);
			j["risk"] = (LPCSTR)CT2A(risk, CP_UTF8);
			jsonAttr = CString(CA2T(j.dump().c_str(), CP_UTF8));
		}

		CString escPurpose = EscapeHtml(purpose);
		CString escCmd = EscapeHtml(command);

		body += _T("<div class=\"action-card action-") + riskLevel + _T("\">");
		body += _T("<div class=\"action-purpose\">") + loc.GetString(_T("Markdown"), _T("PurposeLabel")) + escPurpose + _T("</div>");
		body += _T("<div class=\"action-risk ") + riskLevel + _T("\">") + loc.GetString(_T("Markdown"), _T("RiskLevelLabel")) + riskText + _T("</div>");
		body += _T("<button class=\"action-btn\" data-cmd=\"") + EscapeHtml(jsonAttr) + _T("\" onclick=\"execCmd(this)\">") + loc.GetString(_T("Markdown"), _T("ExecuteBtn")) + _T("</button>");
		body += _T("<div class=\"action-command\">$ ") + escCmd + _T("</div>");
		body += _T("</div>");
	};

	for (int idx = 0; idx < lines.GetSize(); ++idx)
	{
		CString line = lines[idx];

		if (line.GetLength() >= 3 && line.Left(3) == _T("```"))
		{
			if (inActionBlock)
			{
				// Closing ``` of action block — parse and render card
				RenderActionCard(actionJson);
				inActionBlock = false;
				inCodeBlock = false;
				actionJson.Empty();
			}
			else if (inCodeBlock)
			{
				body += _T("</code></pre>");
				inCodeBlock = false;
			}
			else
			{
				CloseBlocks(false);
				CString lang = line.Mid(3).Trim();
				if (lang == _T("action"))
				{
					inActionBlock = true;
					inCodeBlock = true;
					actionJson.Empty();
				}
				else
				{
					body += _T("<pre><code");
					if (!lang.IsEmpty())
						body += _T(" class=\"language-") + EscapeHtml(lang) + _T("\"");
					body += _T(">");
					inCodeBlock = true;
				}
			}
			continue;
		}

		if (inActionBlock)
		{
			actionJson += line;
			actionJson += _T("\n");
			continue;
		}

		if (inCodeBlock)
		{
			body += EscapeHtml(line);
			body += _T("\n");
			continue;
		}

		if (line.IsEmpty())
		{
			CloseBlocks(false);
			continue;
		}

		if (line == _T("---") || line == _T("***") || line == _T("___"))
			CloseBlocks(false);

		bool isList = false;
		bool isQuote = false;
		bool isTable = false;
		CString listContent;
		CString quoteContent;

		if ((line[0] == _T('-') || line[0] == _T('*') || line[0] == _T('+')) &&
			line.GetLength() > 1 && line[1] == _T(' '))
		{
			isList = true;
			listContent = line.Mid(2);
		}

		if (line[0] == _T('>'))
		{
			isQuote = true;
			if (line.GetLength() > 1 && line[1] == _T(' '))
				quoteContent = line.Mid(2);
			else
				quoteContent = line.Mid(1);
		}

		// Table detection: distinguish real Markdown tables from command lines with pipes.
		// A line starts a table only if:
		//   1. It has | AND starts/ends with | (e.g. "| a | b |"), OR
		//   2. It has | AND the next line is a separator (contains --- and |)
		// Once inside a table, any line with | continues the table.
		int pipeCount = 0;
		for (int i = 0; i < line.GetLength(); i++)
			if (line[i] == _T('|')) pipeCount++;

		if (pipeCount > 0)
		{
			if (inTable)
			{
				isTable = true;
			}
			else
			{
				CString trimmedLine = line;
				trimmedLine.Trim();
				bool startsOrEndsWithPipe = (!trimmedLine.IsEmpty() &&
					(trimmedLine[0] == _T('|') || trimmedLine[trimmedLine.GetLength() - 1] == _T('|')));

				bool nextIsSeparator = false;
				if (idx + 1 < lines.GetSize())
				{
					CString nextLine = lines[idx + 1];
					nextLine.Remove(_T('\r'));
					if (nextLine.Find(_T('|')) >= 0 && nextLine.Find(_T("---")) >= 0)
						nextIsSeparator = true;
				}

				if (startsOrEndsWithPipe || nextIsSeparator)
					isTable = true;
			}
		}
		else if (inTable)
		{
			// Previous lines formed a table, but this line has no pipe.
			inTable = false;
			inTHead = false;
			body += _T("</tbody></table>");
		}

		if (isTable)
		{
			CString trimmed = line.Trim(_T("|"));
			if (trimmed.Find(_T("---")) >= 0)
			{
				inTHead = false;
				continue;
			}

			if (!inTable)
			{
				CloseBlocks(false);
				body += _T("<table><thead>");
				inTable = true;
				inTHead = true;
			}

			CStringArray cells;
			{
				int pos = 0;
				while (pos < line.GetLength())
				{
					if (line[pos] == _T('|')) { ++pos; continue; }
					int next = line.Find(_T('|'), pos);
					CString cell = next < 0 ? line.Mid(pos) : line.Mid(pos, next - pos);
					cell.Trim();
					cells.Add(cell);
					pos = next < 0 ? line.GetLength() : next + 1;
				}
			}

			if (inTHead)
			{
				body += _T("<tr>");
				for (int i = 0; i < cells.GetSize(); ++i)
					body += _T("<th>") + FormatInline(cells[i]) + _T("</th>");
				body += _T("</tr></thead><tbody>");
				inTHead = false;
			}
			else
			{
				body += _T("<tr>");
				for (int i = 0; i < cells.GetSize(); ++i)
					body += _T("<td>") + FormatInline(cells[i]) + _T("</td>");
				body += _T("</tr>");
			}
			continue;
		}

		if (inTable) { body += _T("</tbody></table>"); inTable = false; inTHead = false; }

		if (isQuote)
		{
			if (!inQuote)
			{
				CloseBlocks(false);
				body += _T("<blockquote>");
				inQuote = true;
			}
			body += _T("<p>") + FormatInline(quoteContent) + _T("</p>");
			continue;
		}

		if (inQuote) { body += _T("</blockquote>"); inQuote = false; }

		if (isList)
		{
			if (!inList)
			{
				CloseBlocks(false);
				body += _T("<ul>");
				inList = true;
			}
			body += _T("<li>") + FormatInline(listContent) + _T("</li>");
			continue;
		}

		if (inList) { body += _T("</ul>"); inList = false; }

		if (line == _T("---") || line == _T("***") || line == _T("___"))
		{
			body += _T("<hr>");
			continue;
		}

		if (line[0] == _T('#'))
		{
			CloseBlocks(false);
			int level = 0;
			while (level < line.GetLength() && level < 6 && line[level] == _T('#'))
				++level;
			if (level > 0 && level < line.GetLength() && line[level] == _T(' '))
			{
				CString title = line.Mid(level + 1);
				CString tag;
				tag.Format(_T("<h%d>%s</h%d>"), level, FormatInline(title), level);
				body += tag;
				continue;
			}
		}

		if (!inParagraph)
		{
			body += _T("<p>");
			inParagraph = true;
		}
		else
		{
			body += _T(" ");
		}
		body += FormatInline(line);
	}

	CloseBlocks(false);

	html += body;
	html += _T("</body></html>");
	return html;
}

CString CMarkdownDlg::MarkdownToBody(const CString& markdown)
{
    // Extract just the body content from MarkdownToHtml output
    // so it can be embedded in a custom page (e.g. dark-themed AI chat)
    CString fullHtml = MarkdownToHtml(markdown);
    int bodyStart = fullHtml.Find(_T("<body>"));
    int bodyEnd = fullHtml.Find(_T("</body>"));
    if (bodyStart >= 0 && bodyEnd > bodyStart)
        return fullHtml.Mid(bodyStart + 6, bodyEnd - bodyStart - 6);
    return EscapeHtml(markdown); // fallback: just escape the raw text
}
