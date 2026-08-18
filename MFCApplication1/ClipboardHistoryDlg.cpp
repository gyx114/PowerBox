// ClipboardHistoryDlg.cpp: standalone enhanced clipboard-history window
#include "pch.h"
#include "framework.h"
#include "ClipboardHistoryDlg.h"
#include "LocalizationManager.h"
#include "resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <ctime>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
using namespace Gdiplus;

CClipboardHistoryDlg::CClipboardHistoryDlg(ClipboardManager* pMgr, CWnd* pParent)
    : CDialogEx(IDD_CLIPBOARD_HISTORY, pParent)
    , m_pMgr(pMgr)
{
}

BEGIN_MESSAGE_MAP(CClipboardHistoryDlg, CDialogEx)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_DRAWITEM()
    ON_WM_MEASUREITEM()
    ON_MESSAGE(kRefreshMsg, &CClipboardHistoryDlg::OnRefresh)
    ON_NOTIFY(NM_DBLCLK, IDC_CLIP_LIST, &CClipboardHistoryDlg::OnNMDblclkClipList)
    ON_NOTIFY(NM_RCLICK, IDC_CLIP_LIST, &CClipboardHistoryDlg::OnRclickClipList)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_CLIP_LIST, &CClipboardHistoryDlg::OnLvnItemchangedClipList)
    ON_EN_CHANGE(IDC_CLIP_SEARCH_EDIT, &CClipboardHistoryDlg::OnEnChangeClipSearch)
    ON_COMMAND(IDC_BTN_CLIP_COPY, &CClipboardHistoryDlg::OnBnClickedClipCopy)
    ON_COMMAND(IDC_BTN_CLIP_DELETE, &CClipboardHistoryDlg::OnBnClickedClipDelete)
    ON_COMMAND(IDC_BTN_CLIP_CLEAR, &CClipboardHistoryDlg::OnBnClickedClipClear)
    ON_COMMAND(IDC_BTN_CLIP_PIN, &CClipboardHistoryDlg::OnBnClickedClipPin)
    ON_COMMAND(IDC_BTN_CLIP_SETTINGS, &CClipboardHistoryDlg::OnBnClickedClipSettings)
    ON_COMMAND(IDC_BTN_CLIP_APPLY, &CClipboardHistoryDlg::OnBnClickedClipApply)
END_MESSAGE_MAP()

BOOL CClipboardHistoryDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_list.SubclassDlgItem(IDC_CLIP_LIST, this);
    m_search.SubclassDlgItem(IDC_CLIP_SEARCH_EDIT, this);
    m_preview.SubclassDlgItem(IDC_CLIP_PREVIEW, this);
    m_previewImg.SubclassDlgItem(IDC_CLIP_PREVIEW_IMG, this);
    // The image box is statically sized in the .rc resource; capture its client
    // area once so every preview scales to the very same fixed target. Reading
    // the live control size on each switch can drift if the control ever resizes.
    if (m_previewImg.GetSafeHwnd()) m_previewImg.GetClientRect(&m_previewBox);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_list.InsertColumn(0, _T(""), LVCFMT_LEFT, 90);
    m_list.InsertColumn(1, _T(""), LVCFMT_LEFT, 370);

    auto& loc = CLocalizationManager::GetInstance();
    SetWindowText(loc.GetString(_T("ClipboardHistory"), _T("DlgCaption")));
    SetDlgItemText(IDC_BTN_CLIP_COPY, loc.GetString(_T("ClipboardHistory"), _T("BtnCopy")));
    SetDlgItemText(IDC_BTN_CLIP_DELETE, loc.GetString(_T("ClipboardHistory"), _T("BtnDelete")));
    SetDlgItemText(IDC_BTN_CLIP_CLEAR, loc.GetString(_T("ClipboardHistory"), _T("BtnClear")));
    SetDlgItemText(IDC_BTN_CLIP_PIN, loc.GetString(_T("ClipboardHistory"), _T("MenuPin")));
    SetDlgItemText(IDC_BTN_CLIP_SETTINGS, loc.GetString(_T("ClipboardHistory"), _T("BtnSettings")));
    SetDlgItemText(IDC_CLIP_SET_GROUP, loc.GetString(_T("ClipboardHistory"), _T("BtnSettings")));
    SetDlgItemText(IDC_STATIC_CLIP_MAX, loc.GetString(_T("ClipboardHistory"), _T("LabelMaxEntries")));
    SetDlgItemText(IDC_CLIP_SET_SNAP, loc.GetString(_T("ClipboardHistory"), _T("LabelSnapshot")));
    SetDlgItemText(IDC_STATIC_CLIP_SNAPFILES, loc.GetString(_T("ClipboardHistory"), _T("LabelSnapFiles")));
    SetDlgItemText(IDC_STATIC_CLIP_SNAPMP, loc.GetString(_T("ClipboardHistory"), _T("LabelSnapMB")));
    SetDlgItemText(IDC_BTN_CLIP_APPLY, loc.GetString(_T("ClipboardHistory"), _T("BtnApply")));
    SetDlgItemText(IDC_STATIC_CLIP_SET_TIP, loc.GetString(_T("ClipboardHistory"), _T("SettingsTip")));

    if (m_pMgr)
    {
        CString tmp;
        tmp.Format(_T("%d"), m_pMgr->MaxEntries());
        SetDlgItemText(IDC_CLIP_SET_MAX, tmp);
        ((CButton*)GetDlgItem(IDC_CLIP_SET_SNAP))->SetCheck(m_pMgr->SnapshotEnabled() ? BST_CHECKED : BST_UNCHECKED);
        tmp.Format(_T("%d"), m_pMgr->SnapshotMaxFiles());
        SetDlgItemText(IDC_CLIP_SET_SNAPFILES, tmp);
        tmp.Format(_T("%d"), m_pMgr->SnapshotMaxBytesMB());
        SetDlgItemText(IDC_CLIP_SET_SNAPMP, tmp);
    }

    m_showSettings = false;
    ShowSettings(false);

    Populate();
    return TRUE;
}

void CClipboardHistoryDlg::OnCancel() { DestroyWindow(); }
void CClipboardHistoryDlg::OnClose() { DestroyWindow(); }

void CClipboardHistoryDlg::OnDestroy()
{
    if (m_previewBmp) { ::DeleteObject(m_previewBmp); m_previewBmp = nullptr; }
    CDialogEx::OnDestroy();
}

LRESULT CClipboardHistoryDlg::OnRefresh(WPARAM, LPARAM)
{
    Populate();
    return 0;
}

// ---- grouping helpers ----
void CClipboardHistoryDlg::ShowSettings(bool show)
{
    const int ids[] = { IDC_CLIP_SET_GROUP, IDC_STATIC_CLIP_MAX, IDC_CLIP_SET_MAX,
                        IDC_CLIP_SET_SNAP, IDC_STATIC_CLIP_SNAPFILES, IDC_CLIP_SET_SNAPFILES,
                        IDC_STATIC_CLIP_SNAPMP, IDC_CLIP_SET_SNAPMP, IDC_BTN_CLIP_APPLY,
                        IDC_STATIC_CLIP_SET_TIP };
    for (int id : ids)
        if (CWnd* w = GetDlgItem(id)) w->ShowWindow(show ? SW_SHOW : SW_HIDE);
    if (CWnd* w = GetDlgItem(IDC_CLIP_PREVIEW)) w->ShowWindow(show ? SW_HIDE : SW_SHOW);
    if (m_previewImg.GetSafeHwnd()) m_previewImg.ShowWindow(SW_HIDE);
    if (!show) UpdatePreview(); // restore the stacked (text + image) preview layout
}

void CClipboardHistoryDlg::OnBnClickedClipSettings()
{
    m_showSettings = !m_showSettings;
    ShowSettings(m_showSettings);
}

void CClipboardHistoryDlg::OnBnClickedClipApply()
{
    if (!m_pMgr) return;
    UINT v = 0;
    CString s;
    GetDlgItemText(IDC_CLIP_SET_MAX, s); if (_stscanf_s(s, _T("%u"), &v) == 1 && v >= 1) m_pMgr->SetMaxEntries((int)v);
    GetDlgItemText(IDC_CLIP_SET_SNAPFILES, s); if (_stscanf_s(s, _T("%u"), &v) == 1 && v >= 1) m_pMgr->SetSnapshotMaxFiles((int)v);
    GetDlgItemText(IDC_CLIP_SET_SNAPMP, s); if (_stscanf_s(s, _T("%u"), &v) == 1 && v >= 1) m_pMgr->SetSnapshotMaxBytesMB((int)v);
    m_pMgr->SetSnapshotEnabled(((CButton*)GetDlgItem(IDC_CLIP_SET_SNAP))->GetCheck() == BST_CHECKED);

    m_showSettings = false;
    ShowSettings(false);
    Populate();
}

// ---- filtering ----
bool CClipboardHistoryDlg::QueryMatches(const ClipboardEntry& e) const
{
    CString q;
    m_search.GetWindowTextW(q);
    q.Trim();
    if (q.IsEmpty()) return true;
    const std::wstring needle((LPCWSTR)q);
    std::wstring all = e.text;
    for (const auto& f : e.files) all += L" " + f;
    std::wstring nl = needle;
    for (wchar_t& c : nl) c = (wchar_t)towlower(c);
    for (wchar_t& c : all) c = (wchar_t)towlower(c);
    return all.find(nl) != std::wstring::npos;
}

// ---- population ----
void CClipboardHistoryDlg::Refresh() { Populate(); }

void CClipboardHistoryDlg::Populate()
{
    if (!m_pMgr) return;
    const std::vector<ClipboardEntry> entries = m_pMgr->Snapshot();
    auto& loc = CLocalizationManager::GetInstance();

    for (auto& r : m_rows) if (r.icon) { ::DestroyIcon(r.icon); r.icon = nullptr; }
    m_rows.clear();

    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();

    int item = 0;
    for (const auto& e : entries)
    {
        if (!QueryMatches(e)) continue;
        // Icons are loaded lazily on first paint (see OnDrawItem) so that opening
        // the window does not decode every thumbnail / probe every file up front.
        const std::wstring key = !e.thumbPath.empty() ? e.thumbPath
                               : !e.files.empty()   ? e.files[0]
                               : std::wstring();
        CString title = DescribeTitle(e);
        CString typeName;
        switch (ClassifyClipType(e))
        {
        case ClipType::Files: typeName = loc.GetString(_T("ClipboardHistory"), _T("TabFiles")); break;
        case ClipType::Image: typeName = loc.GetString(_T("ClipboardHistory"), _T("TabImage")); break;
        case ClipType::Mixed: typeName = loc.GetString(_T("ClipboardHistory"), _T("TabMixed")); break;
        default:              typeName = loc.GetString(_T("ClipboardHistory"), _T("TabText")); break;
        }
        // For multi-file entries the type shows its count too, e.g. "文件(4)".
        if (!e.files.empty() && e.files.size() > 1)
        {
            CString n; n.Format(_T("(%d)"), (int)e.files.size());
            typeName += n;
        }
        m_rows.push_back({ e.id, nullptr, key, title, typeName });
        m_list.InsertItem(item, typeName);
        m_list.SetItemText(item, 1, title);
        m_list.SetItemData(item, (DWORD_PTR)(m_rows.size() - 1));
        ++item;
    }
    m_list.SetRedraw(TRUE);
    m_list.Invalidate();

    // Widths: type column gets a fixed comfortable width, content fills the rest.
    CRect rc;
    m_list.GetClientRect(&rc);
    const int scrollW = ::GetSystemMetrics(SM_CXVSCROLL);
    const int total = rc.Width() - scrollW;
    const int wType = 90;
    const int wContent = (total - wType) > 0 ? (total - wType) : 120;
    m_list.SetColumnWidth(0, wType);
    m_list.SetColumnWidth(1, wContent);

    UpdatePreview();
}

// Load a lazy icon from a cached key path (image thumbnail or first file path).
HICON CClipboardHistoryDlg::IconForKey(const std::wstring& key) const
{
    HICON h = nullptr;
    const bool isImage = !key.empty();
    if (isImage)
    {
        // Treat the key as an archived PNG thumbnail.
        Bitmap bmp(key.c_str());
        if (bmp.GetLastStatus() == Ok)
        {
            bmp.GetHICON(&h);
            if (h) return h;
        }
    }
    if (!key.empty())
    {
        SHFILEINFO sfi{};
        if (::SHGetFileInfoW(key.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON))
            if (sfi.hIcon) return sfi.hIcon;
    }
    SHFILEINFO sfi{};
    if (::SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES))
        return sfi.hIcon;
    return nullptr;
}

// Load an image file (PNG from the per-entry archive) as an HBITMAP, scaled to fit
// the preview box so large screenshots fit without cropping. The caller owns the
// returned bitmap.
HBITMAP CClipboardHistoryDlg::LoadPreviewBitmap(const std::wstring& imgPath)
{
    if (imgPath.empty()) return nullptr;
    Gdiplus::Bitmap src(imgPath.c_str());
    if (src.GetLastStatus() != Gdiplus::Ok) return nullptr;

    // Scale to the fixed-fit box captured at init so the target never drifts
    // between entry switches; fall back to the live control size only if unset.
    CRect rc = m_previewBox;
    if (rc.Width() <= 0 || rc.Height() <= 0)
        m_previewImg.GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return nullptr;

    const int availW = rc.Width(), availH = rc.Height();
    const int sw = src.GetWidth(), sh = src.GetHeight();
    if (sw <= 0 || sh <= 0) return nullptr;

    // Aspect-preserving fit inside the preview box.
    double scale = 1.0;
    if (sw > availW || sh > availH)
    {
        const double s1 = (double)availW / sw;
        const double s2 = (double)availH / sh;
        scale = (s1 < s2) ? s1 : s2;
    }
    const int dw = (sw * scale >= 1.0) ? (int)(sw * scale) : 1;
    const int dh = (sh * scale >= 1.0) ? (int)(sh * scale) : 1;

    Gdiplus::Bitmap scaled(dw, dh, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(&src, 0, 0, dw, dh);
    }
    Gdiplus::Bitmap* pBmp = &scaled;
    HBITMAP hb = nullptr;
    pBmp->GetHBITMAP(Gdiplus::Color(255, 255, 255), &hb);
    return hb;
}

CString CClipboardHistoryDlg::DescribeTitle(const ClipboardEntry& e) const
{
    CString t;
    auto& loc = CLocalizationManager::GetInstance();
    const CString pin = e.pinned ? CString(L"\u2605 ") : CString();

    if (!e.text.empty() && e.imagePath.empty() && e.files.empty())
    {
        std::wstring s = e.text;
        for (wchar_t& c : s) if (c == L'\r' || c == L'\n') c = L' ';
        if (s.size() > 48) s = s.substr(0, 48) + L"…";
        t = pin + s.c_str();
    }
    else if (!e.files.empty())
    {
        std::wstring leafs;
        for (size_t i = 0; i < e.files.size() && i < 3; ++i)
        {
            const std::wstring& p = e.files[i];
            const size_t s = p.find_last_of(L"\\/");
            std::wstring name = (s != std::wstring::npos) ? p.substr(s + 1) : p;
            leafs += (i ? L"、" : L"") + name;
        }
        if (e.files.size() > 3) leafs += L"…";
        t = pin + leafs.c_str();
    }
    else if (!e.imagePath.empty())
    {
        t = pin + loc.GetString(_T("ClipboardHistory"), _T("TabImage"));
        if (!e.text.empty())
        {
            std::wstring s = e.text; for (wchar_t& c : s) if (c == L'\r' || c == L'\n') c = L' ';
            if (s.size() > 30) s = s.substr(0, 30) + L"…";
            t += _T("  ");
            t += s.c_str();
        }
    }
    else
    {
        t = pin + loc.GetString(_T("ClipboardHistory"), _T("TabEmpty"));
    }
    return t;
}

CString CClipboardHistoryDlg::DescribeSub(const ClipboardEntry& e) const
{
    CString t;
    auto& loc = CLocalizationManager::GetInstance();
    // Use the same payload-based classification as the main tab so that combined
    // entries (e.g. text + files, files + image) are labelled correctly, not Files.
    switch (ClassifyClipType(e))
    {
    case ClipType::Files: t = loc.GetString(_T("ClipboardHistory"), _T("TabFiles")); break;
    case ClipType::Image: t = loc.GetString(_T("ClipboardHistory"), _T("TabImage")); break;
    case ClipType::Mixed: t = loc.GetString(_T("ClipboardHistory"), _T("TabMixed")); break;
    default:              t = loc.GetString(_T("ClipboardHistory"), _T("TabText")); break;
    }

    if (e.timestamp)
    {
        CTime tm(e.timestamp);
        CString ts = tm.Format(_T("  %Y-%m-%d %H:%M"));
        t += ts;
    }
    if (!e.files.empty())
    {
        CString n; n.Format(_T("  (%d)"), (int)e.files.size());
        t += n;
    }
    return t;
}

// ---- owner draw ----
void CClipboardHistoryDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT dis)
{
    if (nIDCtl != IDC_CLIP_LIST) { CDialogEx::OnDrawItem(nIDCtl, dis); return; }
    CDC dc;
    dc.Attach(dis->hDC);
    CRect rc = dis->rcItem;

    const bool sel = (dis->itemState & ODS_SELECTED) != 0;
    const COLORREF bg = sel ? RGB(51, 153, 255) : RGB(255, 255, 255);
    dc.FillSolidRect(&rc, bg);

    if (m_rows.empty() || dis->itemID >= m_rows.size())
    {
        dc.Detach();
        return;
    }
    Row& row = m_rows[dis->itemID];

    // Lazy-load the row thumbnail once and cache it. IconForKey reads only the
    // on-disk thumb.png / shell icon from the cached key — no list-control access
    // — so it is safe here (avoids the 0xc000041d reentrancy crash).
    if (!row.icon && !row.iconKey.empty())
        row.icon = IconForKey(row.iconKey);

    const CString& title = row.title;
    const CString& typeName = row.typeName;

    // Thumbnail strip on the left (48x48, centered vertically in the 48px row).
    if (row.icon)
        ::DrawIconEx(dc.m_hDC, rc.left + 2, rc.top + ((rc.Height() - 48) / 2),
                     row.icon, 48, 48, 0, nullptr, DI_NORMAL);

    const int thumbW = 48 + 6; // icon + gap before the text columns
    CRect textRc(rc.left + thumbW, rc.top + 3, rc.right - 4, rc.bottom - 3);
    dc.SaveDC();
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(sel ? RGB(255, 255, 255) : RGB(30, 30, 30));
    CFont* f = GetFont();
    if (f) dc.SelectObject(f);

    // Column 0 = type (fixed width), column 1 = content (title).
    const int typeW = 90;
    CRect typeRc = textRc;
    typeRc.right = textRc.left + typeW - 6;
    dc.DrawText(typeName, &typeRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    CRect titleRc = textRc;
    titleRc.left = textRc.left + typeW;
    dc.DrawText(title, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    dc.RestoreDC(-1);
    dc.Detach();
}

void CClipboardHistoryDlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT mis)
{
    if (nIDCtl == IDC_CLIP_LIST && mis)
        mis->itemHeight = 48;
    else
        CDialogEx::OnMeasureItem(nIDCtl, mis);
}

// ---- interactions ----
uint64_t CClipboardHistoryDlg::SelectedId() const
{
    const int n = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (n < 0) return 0;
    const DWORD_PTR row = m_list.GetItemData(n);
    if (row < m_rows.size()) return m_rows[row].id;
    return 0;
}

// All currently selected entry ids (for multi-select delete). m_rows is kept in
// parallel with the list items, so each selected row maps to a stable entry id.
std::vector<uint64_t> CClipboardHistoryDlg::SelectedIds() const
{
    std::vector<uint64_t> ids;
    for (int n = m_list.GetNextItem(-1, LVNI_SELECTED); n != -1; n = m_list.GetNextItem(n, LVNI_SELECTED))
    {
        const DWORD_PTR row = m_list.GetItemData(n);
        if (row < m_rows.size()) ids.push_back(m_rows[row].id);
    }
    return ids;
}

// Return the archived PNG path of the currently selected entry, if it is an image.
std::wstring CClipboardHistoryDlg::SelectedImagePath() const
{
    const uint64_t id = SelectedId();
    if (!id || !m_pMgr) return std::wstring();
    const std::vector<ClipboardEntry> es = m_pMgr->Snapshot();
    for (const auto& e : es)
        if (e.id == id && !e.imagePath.empty()) return e.imagePath;
    return std::wstring();
}

void CClipboardHistoryDlg::DoReplay(uint64_t id)
{
    if (m_pMgr) m_pMgr->Replay(id);
}

void CClipboardHistoryDlg::OnNMDblclkClipList(NMHDR*, LRESULT* pResult)
{
    const uint64_t id = SelectedId();
    if (id) DoReplay(id);
    *pResult = 0;
}

void CClipboardHistoryDlg::OnLvnItemchangedClipList(NMHDR*, LRESULT* pResult)
{
    UpdatePreview();
    *pResult = 0;
}

void CClipboardHistoryDlg::OnEnChangeClipSearch()
{
    Populate();
}

void CClipboardHistoryDlg::UpdatePreview()
{
    const uint64_t id = SelectedId();
    CString txt;
    bool wantImg = false;
    if (id && m_pMgr)
    {
        const std::vector<ClipboardEntry> es = m_pMgr->Snapshot();
        for (const auto& e : es)
        {
            if (e.id != id) continue;
            // Path / summary line, always shown even for pure images.
            txt += DescribeSub(e) + _T("\r\n") + _T("----------------------------") + _T("\r\n\r\n");
            if (!e.text.empty()) txt += e.text.c_str();
            else if (!e.files.empty())
            {
                for (const auto& f : e.files) { txt += f.c_str(); txt += _T("\r\n"); }
            }
            else if (!e.imagePath.empty())
            {
                // Pure image: keep the path visible and flag that a picture
                // preview should be rendered below the text.
                txt += e.imagePath.c_str();
                if (e.text.empty()) wantImg = true;
            }
            break;
        }
    }
    m_preview.SetWindowTextW(txt);

    // Image preview: for a pure image entry show the picture inside the statically
    // defined image box (stacked below the text in the .rc layout). Everything else
    // just shows text. No runtime MoveWindow is used — control positions are static.
    if (m_previewImg.GetSafeHwnd())
    {
        m_preview.ShowWindow(SW_SHOW);
        if (wantImg)
        {
            if (HBITMAP hb = LoadPreviewBitmap(SelectedImagePath()))
            {
                if (m_previewBmp) { ::DeleteObject(m_previewBmp); m_previewBmp = nullptr; }
                m_previewBmp = hb;
                m_previewImg.SetBitmap(m_previewBmp);
                m_previewImg.ShowWindow(SW_SHOW);
                m_previewImg.Invalidate(TRUE);
            }
            else
            {
                m_previewImg.ShowWindow(SW_HIDE);
            }
        }
        else
        {
            m_previewImg.ShowWindow(SW_HIDE);
        }
    }
}

void CClipboardHistoryDlg::OnBnClickedClipCopy()
{
    const uint64_t id = SelectedId();
    if (id) DoReplay(id);
}

void CClipboardHistoryDlg::OnBnClickedClipDelete()
{
    if (!m_pMgr) return;
    const std::vector<uint64_t> ids = SelectedIds();
    if (ids.empty()) return;
    auto& loc = CLocalizationManager::GetInstance();
    if (MessageBox(loc.GetString(_T("ClipboardHistory"), _T("ConfirmDelete")),
                   loc.GetString(_T("ClipboardHistory"), _T("DlgCaption")), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    for (uint64_t id : ids) m_pMgr->Remove(id);
    Populate();
}

// Right-click context menu on the clipboard list: copy / pin / delete / clear.
// Copy and pin act on the primary (first) selection; delete removes every selected
// entry, so a multi-select deletes them all at once.
void CClipboardHistoryDlg::OnRclickClipList(NMHDR*, LRESULT* pResult)
{
    auto& loc = CLocalizationManager::GetInstance();
    const bool hasSel = (m_list.GetNextItem(-1, LVNI_SELECTED) != -1);
    const uint64_t primary = SelectedId();
    bool pinned = false;
    if (primary && m_pMgr)
        for (const auto& e : m_pMgr->Snapshot())
            if (e.id == primary) { pinned = e.pinned; break; }

    enum { kCopy = 1, kPin = 2, kDelete = 3, kClear = 4 };
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, kCopy, loc.GetString(_T("ClipboardHistory"), _T("BtnCopy")));
    if (hasSel)
    {
        menu.AppendMenu(MF_STRING, kPin,
                        pinned ? loc.GetString(_T("ClipboardHistory"), _T("MenuUnpin"))
                               : loc.GetString(_T("ClipboardHistory"), _T("MenuPin")));
        menu.AppendMenu(MF_STRING, kDelete, loc.GetString(_T("ClipboardHistory"), _T("BtnDelete")));
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, kClear, loc.GetString(_T("ClipboardHistory"), _T("BtnClear")));

    CPoint pt;
    ::GetCursorPos(&pt);
    const UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, this);
    switch (cmd)
    {
    case kCopy: { const uint64_t id = SelectedId(); if (id) DoReplay(id); break; }
    case kPin:  OnBnClickedClipPin(); break;
    case kDelete: OnBnClickedClipDelete(); break;
    case kClear: OnBnClickedClipClear(); break;
    }
    *pResult = 0;
}

void CClipboardHistoryDlg::OnBnClickedClipClear()
{
    if (!m_pMgr) return;
    auto& loc = CLocalizationManager::GetInstance();
    if (MessageBox(loc.GetString(_T("ClipboardHistory"), _T("ConfirmClear")),
                   loc.GetString(_T("ClipboardHistory"), _T("DlgCaption")), MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    m_pMgr->Clear();
    Populate();
}

void CClipboardHistoryDlg::OnBnClickedClipPin()
{
    const uint64_t id = SelectedId();
    if (!id || !m_pMgr) return;
    m_pMgr->TogglePin(id);
    Populate();
}

void CClipboardHistoryDlg::OnSize(UINT nType, int cx, int cy)
{
    // The dialog is fixed-size (no resize border): every control keeps its
    // statically defined position from the .rc resource. No MoveWindow here.
    CDialogEx::OnSize(nType, cx, cy);
}
