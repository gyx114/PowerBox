// EncodingConverterDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "EncodingConverterDlg.h"
#include "LocalizationManager.h"
#include "afxdialogex.h"
#include <shellapi.h>
#include <string>
#include <filesystem>
#include "Utils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace fs = std::filesystem;

IMPLEMENT_DYNAMIC(CEncodingConverterDlg, CDialogEx)

CEncodingConverterDlg::CEncodingConverterDlg(CWnd* pParent)
	: CDialogEx(IDD_ENCODING_CONVERTER_DLG, pParent)
{
}

CEncodingConverterDlg::~CEncodingConverterDlg()
{
}

void CEncodingConverterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEncodingConverterDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_BTN_ENC_OPEN, &CEncodingConverterDlg::OnBnClickedOpen)
	ON_BN_CLICKED(IDC_BTN_ENC_SAVEAS, &CEncodingConverterDlg::OnBnClickedSaveas)
	ON_BN_CLICKED(IDC_BTN_ENC_OVERWRITE, &CEncodingConverterDlg::OnBnClickedOverwrite)
	ON_CBN_SELCHANGE(IDC_COMBO_ENC_TARGET, &CEncodingConverterDlg::OnCbnSelchangeComboTarget)
END_MESSAGE_MAP()

void CEncodingConverterDlg::PopulateEncodings()
{
	// Supported encodings
	m_encodings[0] = { _T("UTF-8"),        CP_UTF8,  false };
	m_encodings[1] = { _T("UTF-8 BOM"),    CP_UTF8,  true  };
	m_encodings[2] = { _T("UTF-16LE"),     1200,     false };
	m_encodings[3] = { _T("UTF-16LE BOM"), 1200,     true  };
	m_encodings[4] = { _T("UTF-16BE"),     1201,     false };
	m_encodings[5] = { _T("GBK"),          936,      false };
	m_encodings[6] = { _T("Big5"),         950,      false };
	m_encodings[7] = { _T("Shift-JIS"),    932,      false };
	m_encodings[8] = { _T("Latin-1"),      28591,    false };

	CComboBox* pSource = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_SOURCE);
	CComboBox* pTarget = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_TARGET);
	if (pSource && pTarget)
	{
		for (int i = 0; i < ENCODING_COUNT; ++i)
		{
			pSource->AddString(m_encodings[i].name);
			pTarget->AddString(m_encodings[i].name);
		}
		pSource->SetCurSel(0);
		pTarget->SetCurSel(5); // Default target: GBK
	}
}

BOOL CEncodingConverterDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("EncodingDlg")));

	DragAcceptFiles(TRUE);

	// Read layout from RC template
	auto ReadRect = [&](int id) -> CRect {
		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(id);
		if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
		return rc;
	};

	CRect rc;
	rc = ReadRect(IDC_BTN_ENC_OPEN);
	m_btnOpenLeft = rc.left; m_btnOpenTop = rc.top;
	m_btnOpenWidth = rc.Width(); m_btnOpenHeight = rc.Height();

	rc = ReadRect(IDC_STATIC_ENC_PATH);
	m_pathLabelLeft = rc.left; m_pathLabelTop = rc.top; m_pathLabelHeight = rc.Height();

	rc = ReadRect(IDC_COMBO_ENC_SOURCE);
	m_comboSourceLeft = rc.left; m_comboSourceTop = rc.top; m_comboSourceWidth = rc.Width();

	rc = ReadRect(IDC_COMBO_ENC_TARGET);
	m_comboTargetLeft = rc.left; m_comboTargetTop = rc.top; m_comboTargetWidth = rc.Width();

	rc = ReadRect(IDC_EDIT_ENC_SOURCE);
	m_editSourceLeft = rc.left; m_editSourceTop = rc.top;
	m_editWidth = rc.Width(); m_editHeight = rc.Height();
	m_editBottom = rc.bottom;

	rc = ReadRect(IDC_EDIT_ENC_TARGET);
	m_editTargetLeft = rc.left;

	rc = ReadRect(IDC_BTN_ENC_SAVEAS);
	m_btnSaveLeft = rc.left; m_btnSaveTop = rc.top;
	m_btnSaveWidth = rc.Width(); m_btnSaveHeight = rc.Height();

	rc = ReadRect(IDC_BTN_ENC_OVERWRITE);
	m_btnOverwriteLeft = rc.left; m_btnOverwriteTop = rc.top;
	m_btnOverwriteWidth = rc.Width(); m_btnOverwriteHeight = rc.Height();

	PopulateEncodings();

	// Source encoding is auto-detected, disable user modification
	CComboBox* pSource = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_SOURCE);
	if (pSource) pSource->EnableWindow(FALSE);

	TranslateUI();
	return TRUE;
}

BOOL CEncodingConverterDlg::PreTranslateMessage(MSG* pMsg)
{
	if (IsDialogMessage(pMsg))
		return TRUE;
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CEncodingConverterDlg::TranslateUI()
{
	auto& loc = CLocalizationManager::GetInstance();
	SetDlgItemText(IDC_BTN_ENC_OPEN, loc.GetString(_T("Encoding"), _T("BtnOpen")));
	SetDlgItemText(IDC_BTN_ENC_SAVEAS, loc.GetString(_T("Encoding"), _T("BtnSaveAs")));
	SetDlgItemText(IDC_BTN_ENC_OVERWRITE, loc.GetString(_T("Encoding"), _T("BtnOverwrite")));
	SetChildTextByCurrentText(this, _T("源编码:"), loc.GetString(_T("Encoding"), _T("LabelSrcEnc")));
	SetChildTextByCurrentText(this, _T("目标编码:"), loc.GetString(_T("Encoding"), _T("LabelDstEnc")));
}

void CEncodingConverterDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

void CEncodingConverterDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && IsWindow(m_hWnd))
		ResizeControls();
}

void CEncodingConverterDlg::ResizeControls()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	int editTop = m_editSourceTop;
	int editBottom = rcClient.Height() - (rcClient.Height() - m_btnSaveTop) - 2;
	if (editBottom < editTop + 50) editBottom = editTop + 50;

	int halfWidth = (rcClient.Width() - m_editSourceLeft - 5 - (m_editTargetLeft - m_editSourceLeft - m_editWidth)) / 2;
	int gap = m_editTargetLeft - (m_editSourceLeft + m_editWidth);
	int leftWidth = (rcClient.Width() - m_editSourceLeft - 5 - gap) / 2;
	int rightLeft = m_editSourceLeft + leftWidth + gap;
	int rightWidth = rcClient.Width() - rightLeft - 5;

	// Open button: fixed
	CWnd* pBtn = GetDlgItem(IDC_BTN_ENC_OPEN);
	if (pBtn) pBtn->SetWindowPos(nullptr, m_btnOpenLeft, m_btnOpenTop,
		m_btnOpenWidth, m_btnOpenHeight, SWP_NOZORDER);

	// Path label: stretch to right
	CWnd* pPath = GetDlgItem(IDC_STATIC_ENC_PATH);
	if (pPath) pPath->SetWindowPos(nullptr, m_pathLabelLeft, m_pathLabelTop,
		rcClient.Width() - m_pathLabelLeft - 5, m_pathLabelHeight, SWP_NOZORDER);

	// Combos: fixed position
	CWnd* pComboS = GetDlgItem(IDC_COMBO_ENC_SOURCE);
	if (pComboS) pComboS->SetWindowPos(nullptr, m_comboSourceLeft, m_comboSourceTop,
		m_comboSourceWidth, 200, SWP_NOZORDER);

	CWnd* pComboT = GetDlgItem(IDC_COMBO_ENC_TARGET);
	if (pComboT) pComboT->SetWindowPos(nullptr, m_comboTargetLeft, m_comboTargetTop,
		m_comboTargetWidth, 200, SWP_NOZORDER);

	// Source edit: left half
	CWnd* pEditS = GetDlgItem(IDC_EDIT_ENC_SOURCE);
	if (pEditS) pEditS->SetWindowPos(nullptr, m_editSourceLeft, editTop,
		leftWidth, editBottom - editTop, SWP_NOZORDER);

	// Target edit: right half
	CWnd* pEditT = GetDlgItem(IDC_EDIT_ENC_TARGET);
	if (pEditT) pEditT->SetWindowPos(nullptr, rightLeft, editTop,
		rightWidth, editBottom - editTop, SWP_NOZORDER);

	// Save As button: fixed bottom-left
	CWnd* pBtnSave = GetDlgItem(IDC_BTN_ENC_SAVEAS);
	if (pBtnSave) pBtnSave->SetWindowPos(nullptr, m_btnSaveLeft,
		rcClient.Height() - m_btnSaveHeight - 2,
		m_btnSaveWidth, m_btnSaveHeight, SWP_NOZORDER);

	// Overwrite button: next to Save As
	CWnd* pBtnOv = GetDlgItem(IDC_BTN_ENC_OVERWRITE);
	if (pBtnOv) pBtnOv->SetWindowPos(nullptr, m_btnOverwriteLeft,
		rcClient.Height() - m_btnOverwriteHeight - 2,
		m_btnOverwriteWidth, m_btnOverwriteHeight, SWP_NOZORDER);
}

void CEncodingConverterDlg::OnDropFiles(HDROP hDropInfo)
{
	UINT nFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	if (nFiles == 0) return;

	wchar_t szFile[MAX_PATH];
	DragQueryFile(hDropInfo, 0, szFile, MAX_PATH);
	DragFinish(hDropInfo);

	LoadFile(szFile);
}

void CEncodingConverterDlg::OnBnClickedOpen()
{
	CFileDialog dlg(TRUE, nullptr, nullptr,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("Text Files (*.txt;*.md;*.csv;*.log)|*.txt;*.md;*.csv;*.log|All Files (*.*)|*.*||"),
		this);

	if (dlg.DoModal() == IDOK)
		LoadFile(dlg.GetPathName());
}

void CEncodingConverterDlg::LoadFile(const CString& path)
{
	if (path.IsEmpty()) return;

	CFile file;
	if (!file.Open(path, CFile::modeRead))
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("FailedToOpen")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Error")), MB_ICONERROR);
		return;
	}

	ULONGLONG size = file.GetLength();
	if (size == 0 || size > 10 * 1024 * 1024)
	{
		file.Close();
		if (size > 10 * 1024 * 1024)
			MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("FileTooLarge")), 
					  CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Error")), MB_ICONERROR);
		return;
	}

	m_rawBytes.resize((size_t)size);
	file.Read(&m_rawBytes[0], (UINT)size);
	file.Close();

	m_filePath = path;
	SetDlgItemText(IDC_STATIC_ENC_PATH, path);

	// Auto-detect encoding and set source combo
	int detected = DetectEncoding(m_rawBytes);
	CComboBox* pSource = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_SOURCE);
	if (pSource) pSource->SetCurSel(detected);

	DoConvert();
}

int CEncodingConverterDlg::DetectEncoding(const std::string& bytes)
{
	// Check BOMs
	if (bytes.size() >= 3 &&
		(unsigned char)bytes[0] == 0xEF &&
		(unsigned char)bytes[1] == 0xBB &&
		(unsigned char)bytes[2] == 0xBF)
		return 1; // UTF-8 BOM

	if (bytes.size() >= 2 &&
		(unsigned char)bytes[0] == 0xFF &&
		(unsigned char)bytes[1] == 0xFE)
		return 3; // UTF-16LE BOM

	if (bytes.size() >= 2 &&
		(unsigned char)bytes[0] == 0xFE &&
		(unsigned char)bytes[1] == 0xFF)
		return 4; // UTF-16BE (no BOM entry in our list, treat as BE)

	// Heuristic: try UTF-8 decode
	bool isValidUtf8 = true;
	size_t i = 0;
	while (i < bytes.size() && isValidUtf8)
	{
		unsigned char c = (unsigned char)bytes[i];
		if (c < 0x80) { ++i; continue; }
		if (c < 0xC0) { isValidUtf8 = false; break; }
		int extra = 0;
		if (c < 0xE0) extra = 1;
		else if (c < 0xF0) extra = 2;
		else extra = 3;
		if (i + extra >= bytes.size()) { isValidUtf8 = false; break; }
		for (int j = 1; j <= extra; ++j)
		{
			if (((unsigned char)bytes[i + j] & 0xC0) != 0x80)
			{ isValidUtf8 = false; break; }
		}
		i += 1 + extra;
	}

	if (isValidUtf8) return 0; // UTF-8

	// Default to GBK for likely CJK content
	return 5; // GBK
}

std::wstring CEncodingConverterDlg::DecodeBytes(const std::string& bytes, UINT codePage, bool hasBOM)
{
	if (bytes.empty()) return L"";

	const char* data = bytes.data();
	int dataLen = (int)bytes.size();

	// Skip BOM if present
	if (hasBOM)
	{
		if (codePage == CP_UTF8 && dataLen >= 3 &&
			(unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
		{ data += 3; dataLen -= 3; }
		else if (codePage == 1200 && dataLen >= 2 &&
			(unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE)
		{ data += 2; dataLen -= 2; }
	}

	if (codePage == 1200) // UTF-16LE
	{
		int charCount = dataLen / 2;
		std::wstring result((const wchar_t*)data, charCount);
		return result;
	}

	if (codePage == 1201) // UTF-16BE
	{
		int charCount = dataLen / 2;
		std::wstring result;
		result.reserve(charCount);
		for (int i = 0; i + 1 < dataLen; i += 2)
		{
			wchar_t ch = (unsigned char)data[i] << 8 | (unsigned char)data[i + 1];
			result.push_back(ch);
		}
		return result;
	}

	int wlen = MultiByteToWideChar(codePage, 0, data, dataLen, nullptr, 0);
	if (wlen <= 0) return L"";

	std::wstring result(wlen, L'\0');
	MultiByteToWideChar(codePage, 0, data, dataLen, &result[0], wlen);
	return result;
}

std::string CEncodingConverterDlg::EncodeText(const std::wstring& text, UINT codePage, bool writeBOM)
{
	if (text.empty()) return "";

	std::string result;

	if (writeBOM)
	{
		if (codePage == CP_UTF8)
			result += "\xEF\xBB\xBF";
		else if (codePage == 1200)
			result += "\xFF\xFE";
	}

	if (codePage == 1200) // UTF-16LE
	{
		const char* p = (const char*)text.data();
		result.append(p, text.size() * sizeof(wchar_t));
		return result;
	}

	if (codePage == 1201) // UTF-16BE
	{
		for (wchar_t ch : text)
		{
			result += (char)(ch >> 8);
			result += (char)(ch & 0xFF);
		}
		return result;
	}

	int len = WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(),
		nullptr, 0, nullptr, nullptr);
	if (len <= 0) return result;

	std::string buf(len, '\0');
	WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(),
		&buf[0], len, nullptr, nullptr);
	result += buf;
	return result;
}

void CEncodingConverterDlg::DoConvert()
{
	if (m_rawBytes.empty()) return;

	CComboBox* pSource = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_SOURCE);
	CComboBox* pTarget = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_TARGET);
	if (!pSource || !pTarget) return;

	int srcIdx = pSource->GetCurSel();
	int tgtIdx = pTarget->GetCurSel();
	if (srcIdx < 0 || tgtIdx < 0) return;

	// Left: decode raw bytes with source encoding (as if opened with source encoding in Notepad)
	std::wstring srcDecoded = DecodeBytes(m_rawBytes,
		m_encodings[srcIdx].codePage, m_encodings[srcIdx].hasBOM);
	SetDlgItemText(IDC_EDIT_ENC_SOURCE, CString(srcDecoded.c_str(), (int)srcDecoded.size()));

	// Right: decode raw bytes with target encoding (as if opened with target encoding in Notepad)
	std::wstring tgtDecoded = DecodeBytes(m_rawBytes,
		m_encodings[tgtIdx].codePage, m_encodings[tgtIdx].hasBOM);
	SetDlgItemText(IDC_EDIT_ENC_TARGET, CString(tgtDecoded.c_str(), (int)tgtDecoded.size()));
}

void CEncodingConverterDlg::OnCbnSelchangeComboTarget()
{
	DoConvert();
}

bool CEncodingConverterDlg::MoveToRecycleBin(const CString& path)
{
	std::wstring wPath = (LPCWSTR)path;
	std::wstring buffer = wPath + L'\0' + L'\0';
	SHFILEOPSTRUCTW fos = {};
	fos.wFunc = FO_DELETE;
	fos.pFrom = buffer.c_str();
	fos.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOF_SILENT;
	return ::SHFileOperationW(&fos) == 0 && !fos.fAnyOperationsAborted;
}

void CEncodingConverterDlg::SaveFile(const CString& path, int targetEncIdx)
{
	if (targetEncIdx < 0 || targetEncIdx >= ENCODING_COUNT) return;

	// Get current text from source edit (user may have it decoded)
	CString text;
	GetDlgItemText(IDC_EDIT_ENC_SOURCE, text);
	std::wstring wtext((LPCWSTR)text, text.GetLength());

	std::string encoded = EncodeText(wtext,
		m_encodings[targetEncIdx].codePage, m_encodings[targetEncIdx].hasBOM);

	CFile file;
	if (!file.Open(path, CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("FailedToSave")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Error")), MB_ICONERROR);
		return;
	}

	file.Write(encoded.data(), (UINT)encoded.size());
	file.Close();
}

void CEncodingConverterDlg::OnBnClickedSaveas()
{
	if (m_rawBytes.empty())
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("NoFileLoaded")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	CComboBox* pTarget = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_TARGET);
	if (!pTarget) return;
	int tgtIdx = pTarget->GetCurSel();

	CString defaultName = m_filePath;
	if (!defaultName.IsEmpty())
	{
		// Append encoding suffix to filename
		CString ext = PathFindExtension(defaultName);
		CString name = defaultName.Left(defaultName.GetLength() - ext.GetLength());
		defaultName = name + _T("_converted") + ext;
	}

	CFileDialog dlg(FALSE, _T("txt"), defaultName,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		_T("Text Files (*.txt)|*.txt|All Files (*.*)|*.*||"), this);

	if (dlg.DoModal() == IDOK)
	{
		SaveFile(dlg.GetPathName(), tgtIdx);
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("FileSaved")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
	}
}

void CEncodingConverterDlg::OnBnClickedOverwrite()
{
	if (m_filePath.IsEmpty())
	{
		MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("NoOriginalFile")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
		return;
	}

	CComboBox* pTarget = (CComboBox*)GetDlgItem(IDC_COMBO_ENC_TARGET);
	if (!pTarget) return;
	int tgtIdx = pTarget->GetCurSel();

	// Confirmation prompt
	CString msg;
	msg.Format(_T("This will move the original file to Recycle Bin and save with %s encoding.\n\nFile: %s\n\nContinue?"),
		m_encodings[tgtIdx].name, m_filePath);
	if (MessageBox(msg, _T("Confirm Overwrite"), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	// Move original to recycle bin
	if (!MoveToRecycleBin(m_filePath))
	{
		MessageBox(_T("Failed to move original file to Recycle Bin."), _T("Error"), MB_ICONERROR);
		return;
	}

	// Save with target encoding to the same path
	SaveFile(m_filePath, tgtIdx);
	MessageBox(CLocalizationManager::GetInstance().GetString(_T("Encoding"), _T("FileOverwritten")), 
			CLocalizationManager::GetInstance().GetString(_T("Msg"), _T("Info")), MB_ICONINFORMATION);
}
