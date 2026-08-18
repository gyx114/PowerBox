// ClipboardHistoryDlg.h: standalone enhanced clipboard-history window
#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <cstdint>
#include "ClipboardManager.h"
#include "resource.h"

// Standalone enhanced clipboard-history window (owner-drawn thumbnails + preview + settings).
class CClipboardHistoryDlg : public CDialogEx
{
public:
    explicit CClipboardHistoryDlg(ClipboardManager* pMgr, CWnd* pParent = nullptr);
    enum { IDD = IDD_CLIPBOARD_HISTORY };

    // Called by the main dialog (same UI thread) when a fresh clipboard entry arrives.
    void Refresh();

protected:
    BOOL OnInitDialog() override;
    void OnOK() override {}          // suppress default close behavior; keep window open
    void OnCancel() override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    afx_msg void OnNMDblclkClipList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedClipCopy();
    afx_msg void OnBnClickedClipDelete();
    afx_msg void OnBnClickedClipClear();
    afx_msg void OnBnClickedClipPin();
    afx_msg void OnBnClickedClipSettings();
    afx_msg void OnBnClickedClipApply();
    afx_msg void OnEnChangeClipSearch();
    afx_msg void OnLvnItemchangedClipList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRclickClipList(NMHDR* pNMHDR, LRESULT* pResult);

private:
    void Relayout();
    void Populate();
    void UpdatePreview();
    void ShowSettings(bool show);
    HICON  IconForKey(const std::wstring& key) const;
    HBITMAP LoadPreviewBitmap(const std::wstring& imgPath);
    CString DescribeTitle(const ClipboardEntry& e) const;
    CString DescribeSub(const ClipboardEntry& e) const;
    uint64_t SelectedId() const;
    std::vector<std::uint64_t> SelectedIds() const;
    std::wstring SelectedImagePath() const;
    void DoReplay(uint64_t id);
    bool QueryMatches(const ClipboardEntry& e) const;
    LRESULT OnRefresh(WPARAM, LPARAM);

    ClipboardManager* m_pMgr = nullptr;

    CListCtrl m_list;
    CEdit     m_search;
    CEdit     m_preview;
    CStatic   m_previewImg;
    HBITMAP   m_previewBmp = nullptr;      // current image preview bitmap (owned here)
    CRect     m_previewBox{ 0, 0, 0, 0 };  // static fit box for the image preview,
                                           // captured once so the scale target never
                                           // changes between entry switches

    // Entries currently displayed, parallel to the list items; rows own their HICON.
    // iconKey caches the per-row thumbnail path / first file path so the icon can be
    // decoded lazily on first paint without taking another full snapshot.
    // title caches the display text so WM_DRAWITEM never has to reenter the list
    // control (sending LVM_GETITEMTEXT while COMCTL32 dispatches WM_DRAWITEM can
    // raise a fatal user-callback exception, 0xc000041d).
    struct Row { std::uint64_t id = 0; HICON icon = nullptr; std::wstring iconKey; CString title; CString typeName; };
    std::vector<Row> m_rows;

    bool m_showSettings = false;
    static constexpr UINT kRefreshMsg = WM_APP + 41;
};
