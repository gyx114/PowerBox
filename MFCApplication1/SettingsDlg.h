#pragma once

#include "HotkeyCaptureDlg.h"

class CSettingsDlg : public CDialogEx
{
public:
    CSettingsDlg(CWnd* pParent = nullptr);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
    virtual void OnOK();
    virtual void OnCancel() override;
    virtual void PostNcDestroy() override;
    afx_msg void OnClose();
    afx_msg void OnCbnSelchangeLanguage();
    void OnBrowseScreenshot();
    void OnBrowseStickyDir();
    afx_msg void OnBnClickedAiKeyShow();
    afx_msg void OnCbnSelchangeAiVendor();
    afx_msg void OnBnClickedHotkeyShowHide();
    afx_msg void OnBnClickedHotkeyLocate();

private:
    void BrowseFile(UINT id, LPCTSTR title);
    void BrowseFolder(UINT id, LPCTSTR title);
    void InitAIVendorCombo();
    void LoadVendorKey(const CString& vendor);
    void SaveVendorKey(const CString& vendor);
    CString m_strCurrentVendor;
    CString m_strCurrentLang;
    HotkeyInfo m_hotkeyShowHide;
    HotkeyInfo m_hotkeyLocate;
    DECLARE_MESSAGE_MAP()
};