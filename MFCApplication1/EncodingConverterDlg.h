// EncodingConverterDlg.h: header file
//

#pragma once
#include "afxdialogex.h"

class CEncodingConverterDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CEncodingConverterDlg)

public:
	CEncodingConverterDlg(CWnd* pParent = nullptr);
	virtual ~CEncodingConverterDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ENCODING_CONVERTER_DLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();
	void TranslateUI();

	DECLARE_MESSAGE_MAP()

private:
	// Raw file bytes and path
	std::string m_rawBytes;
	CString m_filePath;

	// Encoding info
	struct EncodingInfo
	{
		CString name;
		UINT codePage;
		bool hasBOM;
	};

	static constexpr int ENCODING_COUNT = 9;
	EncodingInfo m_encodings[ENCODING_COUNT];

	// Layout values read from RC
	int m_btnOpenLeft, m_btnOpenTop, m_btnOpenWidth, m_btnOpenHeight;
	int m_pathLabelLeft, m_pathLabelTop, m_pathLabelHeight;
	int m_comboSourceLeft, m_comboSourceTop, m_comboSourceWidth;
	int m_comboTargetLeft, m_comboTargetTop, m_comboTargetWidth;
	int m_editSourceLeft, m_editSourceTop;
	int m_editTargetLeft, m_editTargetTop;
	int m_editWidth, m_editHeight;
	int m_editBottom;
	int m_btnSaveLeft, m_btnSaveTop, m_btnSaveWidth, m_btnSaveHeight;
	int m_btnOverwriteLeft, m_btnOverwriteTop, m_btnOverwriteWidth, m_btnOverwriteHeight;

	void ResizeControls();
	void PopulateEncodings();
	void LoadFile(const CString& path);
	void DoConvert();
	void SaveFile(const CString& path, int targetEncIdx);
	static bool MoveToRecycleBin(const CString& path);
	static std::wstring DecodeBytes(const std::string& bytes, UINT codePage, bool hasBOM);
	static std::string EncodeText(const std::wstring& text, UINT codePage, bool writeBOM);
	static int DetectEncoding(const std::string& bytes);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedOpen();
	afx_msg void OnBnClickedSaveas();
	afx_msg void OnBnClickedOverwrite();
	afx_msg void OnCbnSelchangeComboTarget();
};
