#pragma once

#include "afxwin.h"
#include "resource.h"

// Custom message: OCR recognition complete, wParam=0 failure/1 success, lParam=PWSTR result text
#define WM_OCR_COMPLETE       (WM_USER + 100)
#define WM_TRANSLATE_COMPLETE (WM_USER + 101)

class CScreenshotOCRDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CScreenshotOCRDlg)

public:
    CScreenshotOCRDlg(CWnd* pParent = nullptr, class CAutoClicker* pAutoClicker = nullptr);
    virtual ~CScreenshotOCRDlg();

    enum { IDD = IDD_SCREENSHOT_OCR_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
    virtual void OnOK() override;
    virtual void OnCancel() override;
    virtual void PostNcDestroy() override;
    void TranslateUI();
    afx_msg void OnClose();

    CWnd* m_pMainWnd{nullptr};

    afx_msg void OnBnClickedBtnOcr();
    afx_msg void OnBnClickedBtnTranslate();
    afx_msg void OnBnClickedBtnCopy();

    afx_msg LRESULT OnOcrComplete(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTranslateComplete(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    // Region capture: hide dialog → fullscreen screenshot → drag-select → return selected bitmap
    HBITMAP CaptureRegion();

    // Background thread: OCR recognition
    static void OcrThreadProc(HBITMAP hBitmap, HWND hNotifyWnd);

    // Background thread: translation (with language pair)
    static void TranslateThreadProc(const CString& text, const CString& langPair, HWND hNotifyWnd);

    // Translation API call (with timeout and language pair)
    static CString CallTranslateAPI(const CString& text, const CString& langPair, int timeoutSeconds = 10);

    // Get currently selected language pair (e.g. "zh-CN|en")
    CString GetSelectedLangPair() const;

    bool m_bBusy{ false };

    HBITMAP m_hScreenshot{ nullptr };
    int m_screenshotWidth{ 0 };
    int m_screenshotHeight{ 0 };

    CString m_ocrText;
    CString m_translatedText;

    // Language pair mapping: display name → API parameter
    static const std::pair<const wchar_t*, const wchar_t*> s_langPairs[];
    static constexpr int s_langPairCount = 6;
};