#include "pch.h"
#include "framework.h"
#include "QRCodeGenDlg.h"
#include "qrcodegen.hpp"
#include "resource.h"
#include "LocalizationManager.h"
#include "Utils.h"
#include <gdiplus.h>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

CQRCodeGenDlg::CQRCodeGenDlg(CWnd* pParent) : CDialogEx(IDD_QRCODE_DLG, pParent)
{
}

void CQRCodeGenDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CQRCodeGenDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_QR_GENERATE, &CQRCodeGenDlg::OnBnClickedGenerate)
    ON_BN_CLICKED(IDC_BTN_QR_COPY, &CQRCodeGenDlg::OnBnClickedCopy)
    ON_BN_CLICKED(IDC_BTN_QR_SAVE, &CQRCodeGenDlg::OnBnClickedSave)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CQRCodeGenDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("QRCodeDlg")));

    GetDlgItem(IDC_BTN_QR_COPY)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_QR_SAVE)->EnableWindow(FALSE);

    TranslateUI();
    return TRUE;
}

BOOL CQRCodeGenDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = CWnd::FromHandle(::GetFocus());
        if (pFocus)
        {
            TCHAR className[64] = {0};
            ::GetClassName(pFocus->GetSafeHwnd(), className, 64);
            if (_tcsstr(className, _T("Edit")) || _tcsstr(className, _T("edit")))
            {
                return TRUE;
            }
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CQRCodeGenDlg::OnBnClickedGenerate()
{
    CString text;
    GetDlgItemText(IDC_EDIT_QR_TEXT, text);
    if (text.IsEmpty())
    {
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("EnterText")), CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
        return;
    }
    GenerateQRCode(text);
}

void CQRCodeGenDlg::OnBnClickedCopy()
{
    if (!m_hBitmap) return;

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        ::SetClipboardData(CF_BITMAP, CopyImage(m_hBitmap, IMAGE_BITMAP, 0, 0, 0));
        ::CloseClipboard();
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("QrCopied")), CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
    }
}

void CQRCodeGenDlg::OnBnClickedSave()
{
    if (!m_hBitmap) return;

    CFileDialog dlg(FALSE, _T("png"), _T("qrcode.png"),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("PngFilter")));
    if (dlg.DoModal() != IDOK) return;

    CString filePath = dlg.GetPathName();

    // Use GDI+ to save as PNG
    Gdiplus::Bitmap bmp(m_hBitmap, nullptr);
    CLSID pngClsid;
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);

    CString ext = dlg.GetFileExt();
    ext.MakeLower();
    if (ext == _T("bmp"))
    {
        CLSIDFromString(L"{557CF400-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);
    }

    USES_CONVERSION;
    bmp.Save(T2CW(filePath), &pngClsid, nullptr);
    MessageBox(CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("QrSaved")), CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
}

void CQRCodeGenDlg::GenerateQRCode(const CString& text)
{
    try
    {
        CT2CA utf8Text(text);
        auto qr = qrcodegen::QrCode::encodeText(static_cast<const char*>(utf8Text), qrcodegen::QrCode::Ecc::LOW);

        int moduleSize = 4;
        HBITMAP hNewBitmap = CreateQRBitmap(qr, moduleSize);
        m_bitmapSize = CSize(qr.getSize() * moduleSize, qr.getSize() * moduleSize);

        // Display in static control: SetBitmap returns old bitmap, we are responsible for deleting it
        CStatic* pImage = static_cast<CStatic*>(GetDlgItem(IDC_STATIC_QR_IMAGE));
        if (pImage && hNewBitmap)
        {
            HBITMAP hOld = pImage->SetBitmap(hNewBitmap);
            if (hOld)
                ::DeleteObject(hOld);
            if (m_hBitmap)
                ::DeleteObject(m_hBitmap);
            m_hBitmap = hNewBitmap;
            pImage->Invalidate();
            pImage->UpdateWindow();
        }

        GetDlgItem(IDC_BTN_QR_COPY)->EnableWindow(TRUE);
        GetDlgItem(IDC_BTN_QR_SAVE)->EnableWindow(TRUE);
    }
    catch (const std::exception& e)
    {
        CString err;
        err.Format(CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("GenFail")), e.what());
        MessageBox(err, CLocalizationManager::GetInstance().GetString(_T("QRCode"), _T("Error")), MB_OK | MB_ICONERROR);
    }
}

HBITMAP CQRCodeGenDlg::CreateQRBitmap(const qrcodegen::QrCode& qr, int moduleSize)
{
    int size = qr.getSize();
    int imgSize = (size + 8) * moduleSize; // Leave margin

    HDC hdcScreen = ::GetDC(NULL);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = ::CreateCompatibleBitmap(hdcScreen, imgSize, imgSize);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(::SelectObject(hdcMem, hBitmap));

    // White background
    HBRUSH hWhiteBrush = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    RECT rc = {0, 0, imgSize, imgSize};
    ::FillRect(hdcMem, &rc, hWhiteBrush);

    // Black modules
    HBRUSH hBlackBrush = static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH));
    int margin = 4 * moduleSize;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (qr.getModule(x, y))
            {
                RECT r = {margin + x * moduleSize, margin + y * moduleSize,
                          margin + (x + 1) * moduleSize, margin + (y + 1) * moduleSize};
                ::FillRect(hdcMem, &r, hBlackBrush);
            }
        }
    }

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(NULL, hdcScreen);

    return hBitmap;
}

void CQRCodeGenDlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();
    SetDlgItemText(IDC_BTN_QR_GENERATE, loc.GetString(_T("QRCode"), _T("BtnGenerate")));
    SetDlgItemText(IDC_BTN_QR_COPY, loc.GetString(_T("QRCode"), _T("BtnCopy")));
    SetDlgItemText(IDC_BTN_QR_SAVE, loc.GetString(_T("QRCode"), _T("BtnSave")));
    SetChildTextByCurrentText(this, _T("输入文本(URL/文字):"), loc.GetString(_T("QRCode"), _T("LabelInput")));
}

void CQRCodeGenDlg::OnCancel()
{
    if (m_hBitmap) DeleteObject(m_hBitmap);
    m_hBitmap = nullptr;
    PostMessage(WM_CLOSE);
}

void CQRCodeGenDlg::OnClose()
{
    DestroyWindow();
}

void CQRCodeGenDlg::PostNcDestroy()
{
    delete this;
}