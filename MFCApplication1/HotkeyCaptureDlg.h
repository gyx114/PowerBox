#pragma once
#include "afxdialogex.h"

// Hotkey info: modifier flags + virtual key
struct HotkeyInfo
{
    UINT modifier = 0; // MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN (OR'd)
    UINT vk = 0;       // virtual key code, 0 = empty/unset

    bool IsEmpty() const { return vk == 0; }
    bool operator==(const HotkeyInfo& o) const
    {
        return !IsEmpty() && !o.IsEmpty() && modifier == o.modifier && vk == o.vk;
    }
    bool operator!=(const HotkeyInfo& o) const { return !(*this == o); }

    // Format as display string, e.g. "Ctrl+Alt+Space"
    CString ToDisplay() const;
    // Format as modifier|vk for config storage, e.g. "3|32"
    CString ToConfigString() const;
    // Parse from config string
    static HotkeyInfo FromConfigString(const CString& str);
};

// ============================================================================
// CHotkeyCaptureDlg: modal dialog that captures a key combination
// ============================================================================
class CHotkeyCaptureDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CHotkeyCaptureDlg)

public:
    explicit CHotkeyCaptureDlg(CWnd* pParent, const HotkeyInfo& existing = HotkeyInfo());

    HotkeyInfo m_result; // filled after DoModal returns IDOK

    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnClear();
    DECLARE_MESSAGE_MAP()

private:
    HotkeyInfo m_current;
    void UpdateDisplay();
};