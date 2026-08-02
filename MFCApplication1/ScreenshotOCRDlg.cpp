#include "pch.h"
#include "framework.h"
#include "ScreenshotOCRDlg.h"
#include "resource.h"
#include "Utils.h"
#include "OcrEngine.h"
#include "MFCApplication1.h"
#include "LocalizationManager.h"

#include <gdiplus.h>
#include <thread>
#include <string>
#include <fstream>
#include <algorithm>
#include <winhttp.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")

using namespace Gdiplus;

// Language pairs: display name to API langpair parameter
const std::pair<const wchar_t*, const wchar_t*> CScreenshotOCRDlg::s_langPairs[] = {
    { L"中文 → 英文", L"zh-CN|en" },
    { L"中文 → 日文", L"zh-CN|ja" },
    { L"中文 → 韩文", L"zh-CN|ko" },
    { L"英文 → 中文", L"en|zh-CN" },
    { L"日文 → 中文", L"ja|zh-CN" },
    { L"韩文 → 中文", L"ko|zh-CN" },
};

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    ImageCodecInfo* pImageCodecInfo = nullptr;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    pImageCodecInfo = static_cast<ImageCodecInfo*>(malloc(size));
    if (!pImageCodecInfo) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// ========== Region selection overlay window procedure ==========
struct RegionCaptureData
{
    HBITMAP hFullScreen;   // Full-screen screenshot (read-only)
    HBITMAP hBackBuffer;   // Off-screen buffer (compose here, then BitBlt to screen in one go)
    HBITMAP hOverlayBmp;   // 32-bit DIB overlay mask (pre-created, reused)
    DWORD*  pOverlayBits;  // Overlay pixel buffer
    POINT   ptStart;       // Drag start point
    POINT   ptEnd;         // Drag end point
    bool    bDragging;
    bool    bDone;
    RECT    rcResult;      // Selected region
    int     screenW, screenH;
    HWND    hOcrWnd;       // OCR dialog to restore on destroy
    HWND    hMainWnd;      // Main window to restore on destroy
};

static LRESULT CALLBACK RegionOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RegionCaptureData* pData = reinterpret_cast<RegionCaptureData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (pData && pData->hFullScreen && pData->hBackBuffer)
        {
            int sw = pData->screenW, sh = pData->screenH;

            // 1. Off-screen buffer: draw full-screen screenshot first
            HDC hdcBuf = CreateCompatibleDC(hdc);
            HBITMAP hOldBuf = static_cast<HBITMAP>(SelectObject(hdcBuf, pData->hBackBuffer));

            HDC hdcSrc = CreateCompatibleDC(hdc);
            HBITMAP hOldSrc = static_cast<HBITMAP>(SelectObject(hdcSrc, pData->hFullScreen));
            BitBlt(hdcBuf, 0, 0, sw, sh, hdcSrc, 0, 0, SRCCOPY);
            SelectObject(hdcSrc, hOldSrc);
            DeleteDC(hdcSrc);

            if (pData->bDragging && pData->hOverlayBmp && pData->pOverlayBits)
            {
                int left = std::min(pData->ptStart.x, pData->ptEnd.x);
                int top = std::min(pData->ptStart.y, pData->ptEnd.y);
                int right = std::max(pData->ptStart.x, pData->ptEnd.x);
                int bottom = std::max(pData->ptStart.y, pData->ptEnd.y);

                if (left < 0) left = 0;
                if (top < 0) top = 0;
                if (right > sw) right = sw;
                if (bottom > sh) bottom = sh;

                // 2. Off-screen buffer: cut out selection + AlphaBlend overlay
                for (int y = top; y < bottom; y++)
                    memset(&pData->pOverlayBits[y * sw + left], 0,
                        (right - left) * sizeof(DWORD));

                HDC hdcOverlay = CreateCompatibleDC(hdc);
                HBITMAP hOldOverlay = static_cast<HBITMAP>(SelectObject(hdcOverlay, pData->hOverlayBmp));
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                AlphaBlend(hdcBuf, 0, 0, sw, sh, hdcOverlay, 0, 0, sw, sh, bf);
                SelectObject(hdcOverlay, hOldOverlay);
                DeleteDC(hdcOverlay);

                // Restore overlay pixels
                for (int y = top; y < bottom; y++)
                {
                    DWORD* row = &pData->pOverlayBits[y * sw + left];
                    for (int x = 0; x < right - left; x++)
                        row[x] = 0x78000000;
                }

                // 3. Off-screen buffer: draw blue selection rectangle
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
                HPEN hOldPen = static_cast<HPEN>(SelectObject(hdcBuf, hPen));
                HBRUSH hNullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdcBuf, hNullBrush));
                Rectangle(hdcBuf, left, top, right, bottom);
                SelectObject(hdcBuf, hOldPen);
                SelectObject(hdcBuf, hOldBrush);
                DeleteObject(hPen);
            }

            // 4. Output to screen in one go (eliminate flicker)
            BitBlt(hdc, 0, 0, sw, sh, hdcBuf, 0, 0, SRCCOPY);

            SelectObject(hdcBuf, hOldBuf);
            DeleteDC(hdcBuf);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;  // Do not erase background

    case WM_LBUTTONDOWN:
        if (pData)
        {
            pData->ptStart.x = GET_X_LPARAM(lParam);
            pData->ptStart.y = GET_Y_LPARAM(lParam);
            pData->ptEnd = pData->ptStart;
            pData->bDragging = true;
            ::SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pData && pData->bDragging)
        {
            pData->ptEnd.x = GET_X_LPARAM(lParam);
            pData->ptEnd.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pData && pData->bDragging)
        {
            ::ReleaseCapture();
            pData->bDragging = false;

            int left = std::min(pData->ptStart.x, pData->ptEnd.x);
            int top = std::min(pData->ptStart.y, pData->ptEnd.y);
            int right = std::max(pData->ptStart.x, pData->ptEnd.x);
            int bottom = std::max(pData->ptStart.y, pData->ptEnd.y);

            int w = right - left;
            int h = bottom - top;

            if (w > 5 && h > 5)
            {
                pData->rcResult = { left, top, right, bottom };
                pData->bDone = true;
                ::DestroyWindow(hwnd);
            }
            else
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (pData)
            {
                pData->bDone = true;
                if (pData->bDragging)
                {
                    ::ReleaseCapture();
                    pData->bDragging = false;
                }
            }
            ::DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        // Release mouse capture and force-release modifier keys to prevent stuck keys
        ::ReleaseCapture();
        ForceReleaseModifierKeys();
        // Restore both windows now that the user has finished selecting (or cancelled)
        if (pData)
        {
            if (pData->hOcrWnd && IsWindow(pData->hOcrWnd))
            {
                ::ShowWindow(pData->hOcrWnd, SW_SHOW);
                ::SetForegroundWindow(pData->hOcrWnd);
            }
            if (pData->hMainWnd && IsWindow(pData->hMainWnd))
                ::ShowWindow(pData->hMainWnd, SW_SHOWNOACTIVATE);
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ========== Region capture screenshot ==========
HBITMAP CScreenshotOCRDlg::CaptureRegion()
{
    int screenW = ::GetSystemMetrics(SM_CXSCREEN);
    int screenH = ::GetSystemMetrics(SM_CYSCREEN);

    // 1. Hide both windows first, so the screenshot shows only the desktop
    ShowWindow(SW_HIDE);
    if (m_pMainWnd && IsWindow(m_pMainWnd->GetSafeHwnd()))
        m_pMainWnd->ShowWindow(SW_HIDE);
    Sleep(200);

    // 2. Full-screen screenshot (windows are hidden, so desktop only)
    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hFullScreen = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, hFullScreen));
    BitBlt(hdcMem, 0, 0, screenW, screenH, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);

    // 3. Pre-create 32-bit DIB overlay and off-screen buffer (double-buffering to eliminate flicker)
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW;
    bmi.bmiHeader.biHeight = -screenH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pOverlayBits = nullptr;
    HBITMAP hOverlayBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void**>(&pOverlayBits), nullptr, 0);
    HBITMAP hBackBuffer = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    ::ReleaseDC(nullptr, hdcScreen);

    if (pOverlayBits)
    {
        DWORD* p = pOverlayBits;
        int total = screenW * screenH;
        for (int i = 0; i < total; i++)
            p[i] = 0x78000000;
    }

    // 4. Register overlay window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = RegionOverlayProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.lpszClassName = _T("OCRRegionOverlay");
    wc.hCursor = ::LoadCursor(nullptr, IDC_CROSS);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.style = CS_HREDRAW | CS_VREDRAW;

    WNDCLASS existing = {};
    if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &existing))
        RegisterClass(&wc);

    // 5. Create full-screen overlay
    RegionCaptureData data = {};
    data.hFullScreen = hFullScreen;
    data.hBackBuffer = hBackBuffer;
    data.hOverlayBmp = hOverlayBmp;
    data.pOverlayBits = pOverlayBits;
    data.screenW = screenW;
    data.screenH = screenH;
    data.hOcrWnd = m_hWnd;
    if (m_pMainWnd && IsWindow(m_pMainWnd->GetSafeHwnd()))
        data.hMainWnd = m_pMainWnd->GetSafeHwnd();

    HWND hOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, _T(""), WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, AfxGetInstanceHandle(), nullptr);

    if (!hOverlay)
    {
        if (hBackBuffer) DeleteObject(hBackBuffer);
        if (hOverlayBmp) DeleteObject(hOverlayBmp);
        DeleteObject(hFullScreen);
        ShowWindow(SW_SHOW);
        if (m_pMainWnd && IsWindow(m_pMainWnd->GetSafeHwnd()))
            m_pMainWnd->ShowWindow(SW_SHOWNOACTIVATE);
        return nullptr;
    }

    ::SetWindowLongPtr(hOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&data));
    ::ShowWindow(hOverlay, SW_SHOW);
    ::SetForegroundWindow(hOverlay);

    // 6. Message loop waiting for user to complete region selection
    MSG msg;
    while (!data.bDone && GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 7. Cleanup (window already destroyed)
    if (hBackBuffer) DeleteObject(hBackBuffer);
    if (hOverlayBmp) DeleteObject(hOverlayBmp);

    // 8. Extract selected region
    HBITMAP hResult = nullptr;
    if (data.bDone && data.rcResult.right > data.rcResult.left)
    {
        int w = data.rcResult.right - data.rcResult.left;
        int h = data.rcResult.bottom - data.rcResult.top;

        HDC hdcScreen2 = ::GetDC(nullptr);
        HDC hdcSrc = CreateCompatibleDC(hdcScreen2);
        HDC hdcDst = CreateCompatibleDC(hdcScreen2);
        hResult = CreateCompatibleBitmap(hdcScreen2, w, h);
        SelectObject(hdcSrc, hFullScreen);
        SelectObject(hdcDst, hResult);
        BitBlt(hdcDst, 0, 0, w, h, hdcSrc,
            data.rcResult.left, data.rcResult.top, SRCCOPY);
        DeleteDC(hdcSrc);
        DeleteDC(hdcDst);
        ::ReleaseDC(nullptr, hdcScreen2);
    }

    DeleteObject(hFullScreen);

    // Windows are already restored by WM_DESTROY in the overlay window procedure
    return hResult;
}

IMPLEMENT_DYNAMIC(CScreenshotOCRDlg, CDialogEx)

CScreenshotOCRDlg::CScreenshotOCRDlg(CWnd* pParent, CAutoClicker* /*pAutoClicker*/)
    : CDialogEx(IDD_SCREENSHOT_OCR_DLG, pParent), m_pMainWnd(pParent)
{
}

CScreenshotOCRDlg::~CScreenshotOCRDlg()
{
}

BEGIN_MESSAGE_MAP(CScreenshotOCRDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OCR_CAPTURE, &CScreenshotOCRDlg::OnBnClickedBtnOcr)
    ON_BN_CLICKED(IDC_BTN_OCR_TRANSLATE, &CScreenshotOCRDlg::OnBnClickedBtnTranslate)
    ON_BN_CLICKED(IDC_BTN_OCR_COPY, &CScreenshotOCRDlg::OnBnClickedBtnCopy)
    ON_MESSAGE(WM_OCR_COMPLETE, &CScreenshotOCRDlg::OnOcrComplete)
    ON_MESSAGE(WM_TRANSLATE_COMPLETE, &CScreenshotOCRDlg::OnTranslateComplete)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CScreenshotOCRDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CScreenshotOCRDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(CLocalizationManager::GetInstance().GetString(_T("DlgCaption"), _T("OCRDlg")));

    // Initialize language selection combo box
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_OCR_LANG));
    if (pCombo)
    {
        for (int i = 0; i < s_langPairCount; i++)
            pCombo->AddString(s_langPairs[i].first);
        pCombo->SetCurSel(0);  // Default Chinese to English
    }

    TranslateUI();
    return TRUE;
}

BOOL CScreenshotOCRDlg::PreTranslateMessage(MSG* pMsg)
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

CString CScreenshotOCRDlg::GetSelectedLangPair() const
{
    CComboBox* pCombo = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_OCR_LANG));
    if (pCombo)
    {
        int sel = pCombo->GetCurSel();
        if (sel >= 0 && sel < s_langPairCount)
            return s_langPairs[sel].second;
    }
    return L"zh-CN|en";  // Default Chinese to English
}

void CScreenshotOCRDlg::TranslateUI()
{
    auto& loc = CLocalizationManager::GetInstance();
    SetDlgItemText(IDC_BTN_OCR_CAPTURE, loc.GetString(_T("OCR"), _T("BtnCapture")));
    SetDlgItemText(IDC_BTN_OCR_TRANSLATE, loc.GetString(_T("OCR"), _T("BtnTranslate")));
    SetDlgItemText(IDC_BTN_OCR_COPY, loc.GetString(_T("OCR"), _T("BtnCopy")));
    SetChildTextByCurrentText(this, _T("识别结果:"), loc.GetString(_T("OCR"), _T("LabelResult")));
    SetChildTextByCurrentText(this, _T("翻译结果:"), loc.GetString(_T("OCR"), _T("LabelTranslated")));
    SetDlgItemText(IDC_STATIC_OCR_STATUS, loc.GetString(_T("OCR"), _T("StatusHint")));
}

void CScreenshotOCRDlg::OnOK() {}
void CScreenshotOCRDlg::OnCancel()
{
    if (m_bBusy)
    {
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("OcrBusy")), CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (m_hScreenshot) DeleteObject(m_hScreenshot);
    m_hScreenshot = nullptr;
    PostMessage(WM_CLOSE);
}

void CScreenshotOCRDlg::OnClose()
{
	// Force-release modifier keys to prevent stuck keys (e.g. Alt causing double-click → properties)
	ForceReleaseModifierKeys();

	DestroyWindow();
}

void CScreenshotOCRDlg::PostNcDestroy()
{
    if (m_pMainWnd && IsWindow(m_pMainWnd->GetSafeHwnd()))
        m_pMainWnd->ShowWindow(SW_SHOW);
    delete this;
}

// ========== Background OCR thread ==========
void CScreenshotOCRDlg::OcrThreadProc(HBITMAP hBitmap, HWND hNotifyWnd)
{
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    int w = bm.bmWidth, h = bm.bmHeight;

    bool bNeedUpscale = (w < 500 && h < 500);
    int newW = bNeedUpscale ? w * 2 : w;
    int newH = bNeedUpscale ? h * 2 : h;

    HDC hdc = ::GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HDC hdcNew = CreateCompatibleDC(hdc);
    HBITMAP hNewBmp = CreateCompatibleBitmap(hdc, newW, newH);
    SelectObject(hdcNew, hNewBmp);
    SelectObject(hdcMem, hBitmap);

    if (bNeedUpscale)
    {
        SetStretchBltMode(hdcNew, HALFTONE);
        SetBrushOrgEx(hdcNew, 0, 0, nullptr);
        StretchBlt(hdcNew, 0, 0, newW, newH, hdcMem, 0, 0, w, h, SRCCOPY);
    }
    else
    {
        BitBlt(hdcNew, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
    }

    DeleteDC(hdcMem);
    DeleteDC(hdcNew);
    ::ReleaseDC(nullptr, hdc);

    WCHAR tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"ocr", 0, tempFile);
    CString pngPath = tempFile;
    pngPath.Replace(_T(".tmp"), _T(".png"));
    DeleteFile(pngPath);

    {
        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
        Bitmap* bmp = Bitmap::FromHBITMAP(hNewBmp, nullptr);
        if (bmp)
        {
            CLSID clsidPng;
            GetEncoderClsid(L"image/png", &clsidPng);
            bmp->Save(pngPath, &clsidPng, nullptr);
            delete bmp;
        }
        GdiplusShutdown(gdiplusToken);
    }
    DeleteObject(hNewBmp);

    WCHAR result[4096] = {0};
    bool bSuccess = OcrRecognizeFromFile(pngPath, result, 4096, true);
    DeleteFile(pngPath);

    size_t len = wcslen(result);
    WCHAR* pResult = new WCHAR[len + 1];
    wcscpy_s(pResult, len + 1, result);
    ::PostMessage(hNotifyWnd, WM_OCR_COMPLETE, bSuccess ? 1 : 0, reinterpret_cast<LPARAM>(pResult));
}

// ========== Start screenshot button ==========
void CScreenshotOCRDlg::OnBnClickedBtnOcr()
{
    if (m_bBusy)
    {
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("OcrBusy")), CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Region capture screenshot
    HBITMAP hCapture = CaptureRegion();
    if (!hCapture)
    {
        // User cancelled
        return;
    }

    if (m_hScreenshot) DeleteObject(m_hScreenshot);
    m_hScreenshot = hCapture;

    BITMAP bm;
    GetObject(m_hScreenshot, sizeof(bm), &bm);
    m_screenshotWidth = bm.bmWidth;
    m_screenshotHeight = bm.bmHeight;

    // Mark busy, disable buttons
    m_bBusy = true;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);
    SetDlgItemText(IDC_EDIT_OCR_RESULT, CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Recognizing")));

    // Copy bitmap for background thread
    HDC hdc = ::GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdc);
    HDC hdcDst = CreateCompatibleDC(hdc);
    HBITMAP hThreadBmp = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
    SelectObject(hdcSrc, m_hScreenshot);
    SelectObject(hdcDst, hThreadBmp);
    BitBlt(hdcDst, 0, 0, bm.bmWidth, bm.bmHeight, hdcSrc, 0, 0, SRCCOPY);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ::ReleaseDC(nullptr, hdc);

    std::thread(OcrThreadProc, hThreadBmp, m_hWnd).detach();
}

// ========== OCR complete ==========
LRESULT CScreenshotOCRDlg::OnOcrComplete(WPARAM wParam, LPARAM lParam)
{
    m_bBusy = false;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(TRUE);

    WCHAR* pResult = reinterpret_cast<WCHAR*>(lParam);
    if (pResult)
    {
        m_ocrText = pResult;
        SetDlgItemText(IDC_EDIT_OCR_RESULT, m_ocrText);
        if (m_hScreenshot) { DeleteObject(m_hScreenshot); m_hScreenshot = nullptr; }
        delete[] pResult;
    }
    return 0;
}

// ========== Background translate thread ==========
void CScreenshotOCRDlg::TranslateThreadProc(const CString& text, const CString& langPair, HWND hNotifyWnd)
{
    CString result = CallTranslateAPI(text, langPair, 10);
    int len = result.GetLength();
    WCHAR* pResult = new WCHAR[len + 1];
    wcscpy_s(pResult, len + 1, result);
    ::PostMessage(hNotifyWnd, WM_TRANSLATE_COMPLETE, 1, reinterpret_cast<LPARAM>(pResult));
}

// ========== Translate API ==========
CString CScreenshotOCRDlg::CallTranslateAPI(const CString& text, const CString& langPair, int timeoutSeconds)
{
    std::string encodedText;
    for (int i = 0; i < text.GetLength(); i++)
    {
        WCHAR ch = text[i];
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_' || ch == L'.' || ch == L'~')
        {
            encodedText += static_cast<char>(ch);
        }
        else if (ch == L' ')
        {
            encodedText += '+';
        }
        else
        {
            std::wstring wch(1, ch);
            int len = WideCharToMultiByte(CP_UTF8, 0, wch.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wch.c_str(), -1, &utf8[0], len, nullptr, nullptr);
            for (int j = 0; j < len - 1; j++)
            {
                char hex[4];
                sprintf_s(hex, "%%%02X", static_cast<unsigned char>(utf8[j]));
                encodedText += hex;
            }
        }
    }

    // Encode language pair parameter
    std::string encodedLangPair;
    int lpLen = WideCharToMultiByte(CP_UTF8, 0, langPair, -1, nullptr, 0, nullptr, nullptr);
    std::string lpUtf8(lpLen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, langPair, -1, &lpUtf8[0], lpLen, nullptr, nullptr);
    for (int j = 0; j < lpLen - 1; j++)
    {
        if (lpUtf8[j] == '|')
            encodedLangPair += "%7C";
        else
            encodedLangPair += lpUtf8[j];
    }

    std::string postData = "q=" + encodedText + "&langpair=" + encodedLangPair;

    HINTERNET hSession = ::WinHttpOpen(L"MFC OCR Tool/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrInitNetwork"));

    DWORD timeout = timeoutSeconds * 1000;
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    ::WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = ::WinHttpConnect(hSession,
        L"api.mymemory.translated.net", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { ::WinHttpCloseHandle(hSession); return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrConnectServer")); }

    HINTERNET hRequest = ::WinHttpOpenRequest(hConnect, L"POST", L"/get",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrCreateRequest")); }

    LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    BOOL bResult = ::WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(wcslen(headers)),
        const_cast<char*>(postData.c_str()), static_cast<DWORD>(postData.size()),
        static_cast<DWORD>(postData.size()), 0);
    if (!bResult) { ::WinHttpCloseHandle(hRequest); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrTimeout")); }

    bResult = ::WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult) { ::WinHttpCloseHandle(hRequest); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateNoResponse")); }

    std::string response;
    DWORD dwSize = 0;
    do
    {
        dwSize = 0;
        if (!::WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize > 0)
        {
            std::string chunk(dwSize + 1, '\0');
            DWORD dwRead = 0;
            if (::WinHttpReadData(hRequest, &chunk[0], dwSize, &dwRead))
            {
                chunk.resize(dwRead);
                response += chunk;
            }
        }
    } while (dwSize > 0);

    ::WinHttpCloseHandle(hRequest);
    ::WinHttpCloseHandle(hConnect);
    ::WinHttpCloseHandle(hSession);

    if (response.empty()) return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateEmptyResponse"));

    std::string marker = "\"translatedText\":\"";
    size_t pos = response.find(marker);
    if (pos == std::string::npos)
    {
        marker = "\"responseDetails\":\"";
        pos = response.find(marker);
        if (pos != std::string::npos)
        {
            size_t end = response.find('"', pos + marker.size());
            if (end != std::string::npos)
            {
                std::string err = response.substr(pos + marker.size(), end - pos - marker.size());
                CString cserr = CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrPrefix"));
                int wlen = MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, nullptr, 0);
                if (wlen > 0)
                {
                    WCHAR* buf = new WCHAR[wlen];
                    MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, buf, wlen);
                    cserr += buf;
                    delete[] buf;
                }
                return cserr;
            }
        }
        return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrParseResponse"));
    }

    size_t start = pos + marker.size();
    size_t end = response.find('"', start);
    if (end == std::string::npos) return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateErrResponseFormat"));

    std::string translated = response.substr(start, end - start);
    std::string unescaped;
    for (size_t i = 0; i < translated.size(); i++)
    {
        if (translated[i] == '\\' && i + 1 < translated.size())
        {
            if (translated[i + 1] == 'n') { unescaped += '\n'; i++; }
            else if (translated[i + 1] == 't') { unescaped += '\t'; i++; }
            else if (translated[i + 1] == 'r') { unescaped += '\r'; i++; }
            else if (translated[i + 1] == '"') { unescaped += '"'; i++; }
            else if (translated[i + 1] == '\\') { unescaped += '\\'; i++; }
            else if (translated[i + 1] == 'u' && i + 5 < translated.size())
            {
                // \uXXXX → UTF-8
                char hex[5] = { translated[i + 2], translated[i + 3], translated[i + 4], translated[i + 5], '\0' };
                wchar_t wc = static_cast<wchar_t>(strtol(hex, nullptr, 16));
                char utf8[8] = {};
                int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, 8, nullptr, nullptr);
                unescaped.append(utf8, len);
                i += 5;
            }
            else unescaped += translated[i];
        }
        else unescaped += translated[i];
    }

    CString result;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, nullptr, 0);
    if (wlen > 0)
    {
        WCHAR* buf = new WCHAR[wlen];
        MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, buf, wlen);
        result = buf;
        delete[] buf;
    }
    if (result.IsEmpty()) return CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("TranslateResultEmpty"));
    return result;
}

// ========== Translate button ==========
void CScreenshotOCRDlg::OnBnClickedBtnTranslate()
{
    if (m_bBusy)
    {
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("OcrBusy")), CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
        return;
    }
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    text.Trim();
    if (text.IsEmpty())
    {
        MessageBox(CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("PleaseCapture")), CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONINFORMATION);
        return;
    }
    auto& loc = CLocalizationManager::GetInstance();
    CString failStr = loc.GetString(_T("OCR"), _T("TranslateFailPrefix"));
    CString errStr = loc.GetString(_T("OCR"), _T("TranslateParseFail"));
    if (text.Find(failStr) >= 0 || text.Find(errStr) >= 0)
    {
        MessageBox(loc.GetString(_T("OCR"), _T("OcrTranslateFail")), loc.GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONWARNING);
        return;
    }

    m_bBusy = true;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(FALSE);
    GetDlgItem(IDC_COMBO_OCR_LANG)->EnableWindow(FALSE);
    SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Translating")));

    CString langPair = GetSelectedLangPair();
    std::thread(TranslateThreadProc, text, langPair, m_hWnd).detach();
}

// ========== Translate complete ==========
LRESULT CScreenshotOCRDlg::OnTranslateComplete(WPARAM /*wParam*/, LPARAM lParam)
{
    m_bBusy = false;
    GetDlgItem(IDC_BTN_OCR_CAPTURE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_OCR_TRANSLATE)->EnableWindow(TRUE);
    GetDlgItem(IDC_COMBO_OCR_LANG)->EnableWindow(TRUE);

    WCHAR* pResult = reinterpret_cast<WCHAR*>(lParam);
    if (pResult)
    {
        m_translatedText = pResult;
        SetDlgItemText(IDC_EDIT_OCR_TRANSLATED, m_translatedText);
        delete[] pResult;
    }
    return 0;
}

// ========== Copy button ==========
void CScreenshotOCRDlg::OnBnClickedBtnCopy()
{
    CString text;
    GetDlgItemText(IDC_EDIT_OCR_TRANSLATED, text);
    if (text.IsEmpty()) GetDlgItemText(IDC_EDIT_OCR_RESULT, text);
    if (text.IsEmpty()) { MessageBox(CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("NoContent")), CLocalizationManager::GetInstance().GetString(_T("OCR"), _T("Tip")), MB_OK | MB_ICONINFORMATION); return; }

    if (::OpenClipboard(m_hWnd))
    {
        ::EmptyClipboard();
        int len = (text.GetLength() + 1) * sizeof(WCHAR);
        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem)
        {
            void* pMem = ::GlobalLock(hMem);
            if (pMem) { memcpy(pMem, static_cast<LPCWSTR>(text), len); ::GlobalUnlock(hMem); }
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        ::CloseClipboard();
    }
}