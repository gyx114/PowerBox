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
    ON_WM_SIZE()
    ON_WM_DRAWITEM()
    ON_WM_MEASUREITEM()
    ON_MESSAGE(kRefreshMsg, &CClipboardHistoryDlg::OnRefresh)
    ON_NOTIFY(NM_DBLCLK, IDC_CLIP_LIST, &CClipboardHistoryDlg::OnNMDblclkClipList)
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

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_list.InsertColumn(0, _T(""), LVCFMT_LEFT, 460);

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
        m_rows.push_back({ e.id, nullptr, key, title });
        m_list.InsertItem(item, title);
        m_list.SetItemData(item, (DWORD_PTR)(m_rows.size() - 1));
        ++item;
    }
    m_list.SetRedraw(TRUE);
    m_list.Invalidate();

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

CString CClipboardHistoryDlg::DescribeTitle(const ClipboardEntry& e) const
{
    CString t;
    auto& loc = CLocalizationManager::GetInstance();
    const CString pin = e.pinned ? CString(_T("\x2605 ")) : CString(_T(""));

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
    if (e.imagePath.empty() && e.files.empty())        t = loc.GetString(_T("ClipboardHistory"), _T("TabText"));
    else if (!e.files.empty() && e.imagePath.empty()) t = loc.GetString(_T("ClipboardHistory"), _T("TabFiles"));
    else if (!e.imagePath.empty() && e.files.empty())  t = loc.GetString(_T("ClipboardHistory"), _T("TabImage"));
    else                                               t = loc.GetString(_T("ClipboardHistory"), _T("TabMixed"));

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

    // Note: deliberately no icon loading and NO list-control access here.
    // Decoding thumbnails / querying the shell icon / calling back into m_list
    // (e.g. GetItemText) inside the WM_DRAWITEM callback is unsafe — a reentrant
    // send into the list view while COMCTL32 is dispatching WM_DRAWITEM can raise
    // a fatal user callback exception (0xc000041d). Draw from the cached rows.
    if (m_rows.empty() || dis->itemID >= m_rows.size())
    {
        dc.Detach();
        return;
    }
    const CString& title = m_rows[dis->itemID].title;
    CRect textRc(rc.left + 6, rc.top + 3, rc.right - 4, rc.bottom - 3);
    dc.SaveDC();
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(sel ? RGB(255, 255, 255) : RGB(30, 30, 30));
    CFont* f = GetFont();
    if (f) dc.SelectObject(f);
    CRect titleRc = textRc;
    titleRc.bottom = titleRc.top + 22;
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
    if (id && m_pMgr)
    {
        const std::vector<ClipboardEntry> es = m_pMgr->Snapshot();
        for (const auto& e : es)
        {
            if (e.id != id) continue;
            txt += DescribeTitle(e) + _T("\r\n") + DescribeSub(e) + _T("\r\n\r\n");
            if (!e.text.empty()) txt += e.text.c_str();
            else if (!e.files.empty())
            {
                for (const auto& f : e.files) { txt += f.c_str(); txt += _T("\r\n"); }
            }
            else if (!e.imagePath.empty()) txt += e.imagePath.c_str();
            break;
        }
    }
    m_preview.SetWindowTextW(txt);
}

void CClipboardHistoryDlg::OnBnClickedClipCopy()
{
    const uint64_t id = SelectedId();
    if (id) DoReplay(id);
}

void CClipboardHistoryDlg::OnBnClickedClipDelete()
{
    const uint64_t id = SelectedId();
    if (!id || !m_pMgr) return;
    auto& loc = CLocalizationManager::GetInstance();
    if (MessageBox(loc.GetString(_T("ClipboardHistory"), _T("ConfirmDelete")),
                   loc.GetString(_T("ClipboardHistory"), _T("DlgCaption")), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    m_pMgr->Remove(id);
    Populate();
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
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_list.GetSafeHwnd() || cx < 100 || cy < 100) return;

    const int l = 7, t = 7, g = 7;
    const int searchH = 14, btnW = 46;
    const int bottomH = 14;

    // Top row: search box (stretchable) + 设置/清空 buttons.
    m_search.MoveWindow(l, t, static_cast<int>(cx - 3 * btnW - 3 * g), searchH);
    GetDlgItem(IDC_BTN_CLIP_SETTINGS)->MoveWindow(cx - 2 * btnW - 2 * g, t, btnW, searchH);
    GetDlgItem(IDC_BTN_CLIP_CLEAR)->MoveWindow(cx - btnW - g, t, btnW, searchH);

    const int contentTop = t + searchH + g;
    const int contentBottom = cy - bottomH - g;

    // Left: list. Right: preview (or settings panel).
    const int leftW = 200;
    const int rightL = l + leftW + g;
    const int rightW = cx - rightL - g;
    m_list.MoveWindow(l, contentTop, leftW, contentBottom - contentTop);
    if (CWnd* pv = GetDlgItem(IDC_CLIP_PREVIEW)) pv->MoveWindow(rightL, contentTop, rightW, contentBottom - contentTop);
    if (CWnd* pg = GetDlgItem(IDC_CLIP_SET_GROUP)) pg->MoveWindow(rightL, contentTop, rightW, contentBottom - contentTop);

    // Bottom row buttons.
    const int by = cy - bottomH - g;
    GetDlgItem(IDC_BTN_CLIP_COPY)->MoveWindow(l, by, 60, bottomH);
    GetDlgItem(IDC_BTN_CLIP_DELETE)->MoveWindow(l + 67, by, 60, bottomH);
    GetDlgItem(IDC_BTN_CLIP_PIN)->MoveWindow(l + 134, by, 60, bottomH);
    GetDlgItem(IDCANCEL)->MoveWindow(cx - 60 - g, by, 60, bottomH);

    // Reposition the column width to fill the list.
    m_list.SetColumnWidth(0, LVSCW_AUTOSIZE);
    m_list.SetColumnWidth(0, leftW - 8);
}
