#pragma once
#include "afxdialogex.h"
#include "resource.h"
#include "qrcodegen.hpp"

class CQRCodeGenDlg : public CDialogEx
{
public:
    CQRCodeGenDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_QRCODE_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
    virtual void OnCancel() override;
    virtual void PostNcDestroy() override;
    void TranslateUI();
    afx_msg void OnClose();

    DECLARE_MESSAGE_MAP()

private:
    afx_msg void OnBnClickedGenerate();
    afx_msg void OnBnClickedCopy();
    afx_msg void OnBnClickedSave();
    void GenerateQRCode(const CString& text);
    HBITMAP CreateQRBitmap(const class qrcodegen::QrCode& qr, int moduleSize);

    HBITMAP m_hBitmap{nullptr};
    CSize m_bitmapSize{0, 0};
};