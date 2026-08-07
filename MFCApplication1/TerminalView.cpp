// TerminalView.cpp: ConPTY-backed terminal emulator rendering
#include "pch.h"
#include "framework.h"
#include "TerminalView.h"
#include "LocalizationManager.h"
#include <imm.h>
#include <algorithm>
#include <cstdlib>

#pragma comment(lib, "imm32.lib")

namespace
{
constexpr int MAX_SCROLLBACK = 3000;
constexpr BYTE TERM_FLAG_WIDE = 0x08;
constexpr BYTE TERM_FLAG_CONT = 0x10;

bool IsWideChar(wchar_t ch)
{
    return (ch >= 0x1100 && (ch <= 0x115F || ch == 0x2329 || ch == 0x232A ||
        (ch >= 0x2E80 && ch <= 0xA4CF && ch != 0x303F) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) ||
        (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE19) ||
        (ch >= 0xFE30 && ch <= 0xFE6F) ||
        (ch >= 0xFF00 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) ||
        (ch >= 0x20000 && ch <= 0x2FFFD) ||
        (ch >= 0x30000 && ch <= 0x3FFFD)));
}

bool FileExists(const CString& path)
{
    return ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

CString ReadRegistryString(HKEY root, const CString& subKey, const CString& valueName)
{
    HKEY hKey = nullptr;
    if (::RegOpenKeyEx(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return {};

    TCHAR buf[MAX_PATH]{};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    CString result;
    if (::RegQueryValueEx(hKey, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS && type == REG_SZ)
    {
        result = buf;
    }
    ::RegCloseKey(hKey);
    return result;
}

CString ResolveGitBashPath()
{
    CString exeDir;
    TCHAR modulePath[MAX_PATH]{};
    if (::GetModuleFileName(nullptr, modulePath, MAX_PATH) > 0)
    {
        CString exePath(modulePath);
        int slash = exePath.ReverseFind(_T('\\'));
        exeDir = (slash >= 0) ? exePath.Left(slash + 1) : CString();
    }

    CString configured;
    if (!exeDir.IsEmpty())
    {
        CString iniPath = exeDir + _T("config.ini");
        TCHAR buf[MAX_PATH]{};
        ::GetPrivateProfileString(_T("Paths"), _T("GitBashPath"), _T(""),
            buf, MAX_PATH, iniPath);
        configured = buf;
    }
    if (configured.IsEmpty() && !exeDir.IsEmpty())
    {
        // Also check the older build output location next to the repository.
        CString legacyIni = exeDir + _T("..\\..\\..\\x64\\Release\\config.ini");
        TCHAR buf[MAX_PATH]{};
        ::GetPrivateProfileString(_T("Paths"), _T("GitBashPath"), _T(""),
            buf, MAX_PATH, legacyIni);
        configured = buf;
    }

    auto normalizeBash = [](CString path) -> CString {
        if (path.Right(12).CompareNoCase(_T("git-bash.exe")) == 0)
        {
            int p = path.ReverseFind(_T('\\'));
            if (p >= 0)
            {
                CString dir = path.Left(p + 1);
                CString candidate = dir + _T("bin\\bash.exe");
                if (FileExists(candidate))
                    return candidate;
                candidate = dir + _T("usr\\bin\\bash.exe");
                if (FileExists(candidate))
                    return candidate;
            }
        }
        return path;
    };

    if (!configured.IsEmpty())
    {
        CString candidate = normalizeBash(configured);
        if (FileExists(candidate))
            return candidate;
    }

    CString installPath = ReadRegistryString(HKEY_CURRENT_USER,
        _T("Software\\GitForWindows"), _T("InstallPath"));
    if (installPath.IsEmpty())
        installPath = ReadRegistryString(HKEY_LOCAL_MACHINE,
            _T("Software\\GitForWindows"), _T("InstallPath"));
    if (installPath.IsEmpty())
        installPath = ReadRegistryString(HKEY_LOCAL_MACHINE,
            _T("Software\\WOW6432Node\\GitForWindows"), _T("InstallPath"));

    if (!installPath.IsEmpty())
    {
        CString candidate = installPath + _T("\\bin\\bash.exe");
        if (FileExists(candidate))
            return candidate;
        candidate = installPath + _T("\\usr\\bin\\bash.exe");
        if (FileExists(candidate))
            return candidate;
    }

    TCHAR pathBuf[MAX_PATH]{};
    if (::SearchPath(nullptr, _T("bash.exe"), nullptr, MAX_PATH, pathBuf, nullptr) > 0)
        return CString(pathBuf);

    return _T("C:\\Program Files\\Git\\bin\\bash.exe");
}

COLORREF TerminalPalette16[16] = {
    RGB(12, 12, 12), RGB(205, 49, 49), RGB(13, 188, 121), RGB(229, 229, 16),
    RGB(36, 114, 200), RGB(188, 63, 188), RGB(17, 168, 205), RGB(204, 204, 204),
    RGB(102, 102, 102), RGB(241, 76, 76), RGB(35, 209, 139), RGB(245, 245, 67),
    RGB(59, 142, 234), RGB(214, 112, 214), RGB(41, 184, 219), RGB(255, 255, 255)
};
}

BEGIN_MESSAGE_MAP(CTerminalView, CWnd)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_KEYDOWN()
    ON_WM_CHAR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_RBUTTONUP()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_TERM_OUTPUT, &CTerminalView::OnTermOutput)
    ON_MESSAGE(WM_TERM_EXITED, &CTerminalView::OnTermExited)
    ON_MESSAGE(WM_IME_COMPOSITION, &CTerminalView::OnImeComposition)
    ON_MESSAGE(WM_IME_CHAR, &CTerminalView::OnImeCharMsg)
    ON_COMMAND(ID_TERMINAL_COPY, &CTerminalView::OnTerminalCopy)
    ON_COMMAND(ID_TERMINAL_PASTE, &CTerminalView::OnTerminalPaste)
    ON_COMMAND(ID_TERMINAL_RESTART, &CTerminalView::OnTerminalRestart)
END_MESSAGE_MAP()

CTerminalView::CTerminalView() = default;

CTerminalView::~CTerminalView()
{
    StopSession();
}

BOOL CTerminalView::CreateTerminal(CWnd* pParent, UINT nID, const CRect& rc)
{
    CString className = AfxRegisterWndClass(
        CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_IBEAM),
        reinterpret_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)),
        nullptr);

    if (!CreateEx(0, className, _T("Terminal"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        rc, pParent, nID))
    {
        return FALSE;
    }

    RebuildFont();
    ResizeGrid();
    ResetScreen();
    return TRUE;
}

BOOL CTerminalView::AttachToPlaceholder(UINT nID, CWnd* pParent)
{
    if (!SubclassDlgItem(nID, pParent))
        return FALSE;

    // The resource template defines this control as a plain static frame.
    // Keep the same HWND, but let the terminal own its painting and input.
    ModifyStyle(SS_TYPEMASK, SS_LEFT);
    ModifyStyle(0, WS_TABSTOP);
    ModifyStyleEx(WS_EX_STATICEDGE, 0);

    RebuildFont();
    ResizeGrid();
    ResetScreen();
    return TRUE;
}

void CTerminalView::RebuildFont()
{
    if (!m_font.GetSafeHandle())
    {
        CClientDC dc(this);
        int height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
        if (height == 0)
            height = -16;

        LOGFONT lf{};
        lf.lfHeight = height;
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        wcscpy_s(lf.lfFaceName, L"Cascadia Mono");

        if (!m_font.CreateFontIndirect(&lf))
        {
            wcscpy_s(lf.lfFaceName, L"Consolas");
            m_font.CreateFontIndirect(&lf);
        }
        if (!m_font.GetSafeHandle())
            m_font.CreateStockObject(DEFAULT_GUI_FONT);
    }

    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_font);
    CSize sz = dc.GetTextExtent(_T("W"));
    dc.SelectObject(pOld);
    m_cellW = std::max(4, static_cast<int>(sz.cx));
    m_cellH = std::max(8, static_cast<int>(sz.cy));
}

void CTerminalView::ResizeGrid()
{
    RebuildFont();

    CRect rc;
    GetClientRect(&rc);
    int cols = std::max(20, static_cast<int>(rc.Width()) / m_cellW);
    int rows = std::max(3, static_cast<int>(rc.Height()) / m_cellH);

    if (cols != m_cols || rows != m_rowsCount)
    {
        m_cols = cols;
        m_rowsCount = rows;

        if (m_session)
            m_session->Resize(cols, rows);

        for (auto& row : m_rows)
        {
            if (static_cast<int>(row.size()) < m_cols)
                row.resize(m_cols);
        }

        if (m_rows.empty())
            m_rows.push_back(NewRow());

        ClampCursorToScreen();
        Invalidate(FALSE);
    }
}

void CTerminalView::ResetScreen()
{
    m_rows.clear();
    m_rows.push_back(NewRow());
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_pendingWrap = false;
    m_scrollOffset = 0;
    m_curFg = 7;
    m_curBg = 0;
    m_curFlags = 0;
    m_escState = EscState::Text;
    m_escParam.clear();
    m_byteBuffer.clear();
    m_selAnchorRow = m_selAnchorCol = m_selStartRow = m_selStartCol = m_selEndRow = m_selEndCol = -1;
    m_bSelecting = false;
    Invalidate(FALSE);
}

std::vector<CTerminalView::TermCell> CTerminalView::NewRow() const
{
    return std::vector<TermCell>(m_cols);
}

size_t CTerminalView::ScreenStart() const
{
    if (m_rows.size() > static_cast<size_t>(m_rowsCount))
        return m_rows.size() - m_rowsCount;
    return 0;
}

int CTerminalView::FirstVisibleRow() const
{
    size_t start = ScreenStart();
    int offset = std::min(m_scrollOffset, static_cast<int>(start));
    return static_cast<int>(start) - offset;
}

void CTerminalView::ClampCursorToScreen()
{
    size_t start = ScreenStart();
    if (m_cursorRow < start)
        m_cursorRow = start;
    if (m_cursorRow >= m_rows.size())
        m_cursorRow = m_rows.size() - 1;
    m_cursorCol = std::max(0, std::min(m_cursorCol, m_cols - 1));
}

void CTerminalView::EnsureRow(size_t row)
{
    while (m_rows.size() <= row)
        m_rows.push_back(NewRow());
}

void CTerminalView::LineFeed()
{
    if (m_cursorRow + 1 >= m_rows.size())
    {
        m_rows.push_back(NewRow());
        if (m_rows.size() > MAX_SCROLLBACK)
        {
            m_rows.pop_front();
            m_cursorRow = m_rows.size() - 1;
        }
        else
        {
            m_cursorRow = m_rows.size() - 1;
        }
    }
    else
    {
        m_cursorRow++;
    }
    m_pendingWrap = false;
    m_scrollOffset = 0;
}

void CTerminalView::ReverseIndex()
{
    size_t start = ScreenStart();
    if (m_cursorRow > start)
        m_cursorRow--;
}

void CTerminalView::PutChar(wchar_t ch)
{
    if (m_pendingWrap)
    {
        m_cursorCol = 0;
        LineFeed();
        m_pendingWrap = false;
    }

    EnsureRow(m_cursorRow);
    auto& row = m_rows[m_cursorRow];
    if (static_cast<int>(row.size()) < m_cols)
        row.resize(m_cols);

    // Replacing a cell should also clear a wide character started in the
    // previous column, otherwise a leftover double-width glyph would overlap.
    if (m_cursorCol > 0 && (row[m_cursorCol - 1].flags & TERM_FLAG_WIDE))
    {
        row[m_cursorCol - 1].ch = L' ';
        row[m_cursorCol - 1].flags &= ~TERM_FLAG_WIDE;
    }

    if (IsWideChar(ch))
    {
        if (m_cursorCol + 2 > m_cols && m_cursorCol > 0)
        {
            m_cursorCol = 0;
            LineFeed();
            EnsureRow(m_cursorRow);
            row = m_rows[m_cursorRow];
            if (static_cast<int>(row.size()) < m_cols)
                row.resize(m_cols);
        }

        row[m_cursorCol] = TermCell{ ch, m_curFg, m_curBg,
            static_cast<BYTE>(m_curFlags | TERM_FLAG_WIDE) };
        if (m_cursorCol + 1 < m_cols)
        {
            row[m_cursorCol + 1] = TermCell{ 0, m_curFg, m_curBg,
                static_cast<BYTE>(m_curFlags | TERM_FLAG_CONT) };
        }

        if (m_cursorCol + 2 >= m_cols)
        {
            m_cursorCol = m_cols - 1;
            m_pendingWrap = true;
        }
        else
        {
            m_cursorCol += 2;
        }
    }
    else
    {
        row[m_cursorCol] = TermCell{ ch, m_curFg, m_curBg, m_curFlags };
        if (m_cursorCol + 1 >= m_cols)
        {
            m_cursorCol = m_cols - 1;
            m_pendingWrap = true;
        }
        else
        {
            m_cursorCol++;
        }
    }
    m_scrollOffset = 0;
}

void CTerminalView::ClearRow(size_t row, int fromCol, int toColExclusive)
{
    if (row >= m_rows.size())
        return;

    auto& r = m_rows[row];
    if (static_cast<int>(r.size()) < m_cols)
        r.resize(m_cols);

    fromCol = std::max(0, fromCol);
    toColExclusive = std::min(m_cols, toColExclusive);
    for (int c = fromCol; c < toColExclusive; c++)
        r[c] = TermCell{};
}

void CTerminalView::ParseParams(const std::wstring& s, std::vector<int>& out)
{
    std::wstring cur;
    for (wchar_t ch : s)
    {
        if (ch == L';')
        {
            out.push_back(cur.empty() ? 0 : _wtoi(cur.c_str()));
            cur.clear();
        }
        else
        {
            cur += ch;
        }
    }

    if (!cur.empty())
        out.push_back(_wtoi(cur.c_str()));
    else if (out.empty())
        out.push_back(0);
}

void CTerminalView::ApplySgr(const std::vector<int>& params)
{
    size_t i = 0;
    while (i < params.size())
    {
        int p = params[i];

        if (p == 0)
        {
            m_curFg = 7;
            m_curBg = 0;
            m_curFlags = 0;
        }
        else if (p == 1)
            m_curFlags |= 1;
        else if (p == 4)
            m_curFlags |= 2;
        else if (p == 7)
            m_curFlags |= 4;
        else if (p == 22)
            m_curFlags &= ~1;
        else if (p == 24)
            m_curFlags &= ~2;
        else if (p == 27)
            m_curFlags &= ~4;
        else if (p >= 30 && p <= 37)
            m_curFg = static_cast<BYTE>(p - 30);
        else if (p >= 40 && p <= 47)
            m_curBg = static_cast<BYTE>(p - 40);
        else if (p >= 90 && p <= 97)
            m_curFg = static_cast<BYTE>(p - 90 + 8);
        else if (p >= 100 && p <= 107)
            m_curBg = static_cast<BYTE>(p - 100 + 8);
        else if ((p == 38 || p == 48) && i + 2 < params.size() && params[i + 1] == 5)
        {
            if (p == 38)
                m_curFg = static_cast<BYTE>(params[i + 2] & 0xFF);
            else
                m_curBg = static_cast<BYTE>(params[i + 2] & 0xFF);
            i += 2;
        }
        else if ((p == 38 || p == 48) && i + 4 < params.size() && params[i + 1] == 2)
        {
            // Truecolor is approximated by the 256-color palette.
            int r = params[i + 2] & 0xFF;
            int g = params[i + 3] & 0xFF;
            int b = params[i + 4] & 0xFF;
            auto cubeIndex = [](int v) {
                return v < 48 ? 0 : (v < 115 ? 1 : ((v - 35) / 40));
            };
            int idx = 16 + cubeIndex(r) * 36 + cubeIndex(g) * 6 + cubeIndex(b);
            if (p == 38)
                m_curFg = static_cast<BYTE>(idx);
            else
                m_curBg = static_cast<BYTE>(idx);
            i += 4;
        }

        i++;
    }
}

void CTerminalView::ExecuteCsi(wchar_t final, const std::wstring& params)
{
    std::vector<int> p;
    ParseParams(params, p);

    auto getParam = [&](size_t index, int def) {
        return index < p.size() ? p[index] : def;
    };

    size_t start = ScreenStart();
    m_pendingWrap = false;

    if (final == L'A')
    {
        int n = std::max(1, getParam(0, 1));
        m_cursorRow = (m_cursorRow >= start + static_cast<size_t>(n))
            ? m_cursorRow - n : start;
    }
    else if (final == L'B')
    {
        int n = std::max(1, getParam(0, 1));
        m_cursorRow = std::min(m_rows.size() - 1, m_cursorRow + n);
    }
    else if (final == L'C')
    {
        int n = std::max(1, getParam(0, 1));
        if (n == 1 && m_cursorCol == m_cols - 1 &&
            m_cursorRow + 1 < m_rows.size())
        {
            m_cursorRow++;
            m_cursorCol = 0;
        }
        else
        {
            m_cursorCol = std::min(m_cols - 1, m_cursorCol + n);
        }
    }
    else if (final == L'D')
    {
        int n = std::max(1, getParam(0, 1));
        if (n == 1 && m_cursorCol == 0 && m_cursorRow > start)
        {
            m_cursorRow--;
            m_cursorCol = m_cols - 1;
        }
        else
        {
            m_cursorCol = std::max(0, m_cursorCol - n);
        }
    }
    else if (final == L'H' || final == L'f')
    {
        int row = std::max(1, getParam(0, 1));
        int col = std::max(1, getParam(1, 1));
        size_t target = start + row - 1;
        if (target >= m_rows.size())
            EnsureRow(target);
        m_cursorRow = std::min(target, m_rows.size() - 1);
        m_cursorCol = std::min(m_cols - 1, col - 1);
    }
    else if (final == L'G')
    {
        m_cursorCol = std::max(0, std::min(m_cols - 1, getParam(0, 1) - 1));
    }
    else if (final == L'd')
    {
        int row = std::max(1, getParam(0, 1));
        size_t target = start + row - 1;
        if (target >= m_rows.size())
            EnsureRow(target);
        m_cursorRow = std::min(target, m_rows.size() - 1);
    }
    else if (final == L'J')
    {
        int mode = getParam(0, 0);
        if (mode == 0)
        {
            for (size_t r = m_cursorRow; r < m_rows.size(); r++)
                ClearRow(r, r == m_cursorRow ? m_cursorCol : 0, m_cols);
        }
        else if (mode == 1)
        {
            for (size_t r = start; r <= m_cursorRow && r < m_rows.size(); r++)
                ClearRow(r, 0, r == m_cursorRow ? m_cursorCol + 1 : m_cols);
        }
        else if (mode == 2)
        {
            for (size_t r = start; r < m_rows.size(); r++)
                ClearRow(r, 0, m_cols);
            m_cursorRow = start;
            m_cursorCol = 0;
        }
    }
    else if (final == L'K')
    {
        int mode = getParam(0, 0);
        int from = (mode == 1) ? 0 : m_cursorCol;
        int to = (mode == 2) ? m_cols : ((mode == 1) ? m_cursorCol + 1 : m_cols);
        ClearRow(m_cursorRow, from, to);
    }
    else if (final == L'X')
    {
        int n = std::max(1, getParam(0, 1));
        ClearRow(m_cursorRow, m_cursorCol, std::min(m_cols, m_cursorCol + n));
    }
    else if (final == L'm')
    {
        ApplySgr(p);
    }
    else if (final == L's')
    {
        m_savedRow = m_cursorRow;
        m_savedCol = m_cursorCol;
    }
    else if (final == L'u')
    {
        m_cursorRow = std::min(m_savedRow, m_rows.size() - 1);
        m_cursorCol = std::min(m_savedCol, m_cols - 1);
    }

    m_scrollOffset = 0;
}

void CTerminalView::ProcessChar(wchar_t ch)
{
    switch (m_escState)
    {
    case EscState::Text:
        if (ch == 0x1B)
        {
            m_escState = EscState::Esc;
            m_escParam.clear();
        }
        else if (ch == L'\r')
        {
            m_cursorCol = 0;
            m_pendingWrap = false;
        }
        else if (ch == L'\n')
        {
            LineFeed();
            m_pendingWrap = false;
        }
        else if (ch == L'\b')
        {
            size_t start = ScreenStart();
            m_pendingWrap = false;
            if (m_cursorCol > 0)
                m_cursorCol--;
            else if (m_cursorRow > start)
            {
                m_cursorRow--;
                m_cursorCol = m_cols - 1;
            }
        }
        else if (ch == L'\t')
        {
            if (m_pendingWrap)
            {
                m_cursorCol = 0;
                LineFeed();
                m_pendingWrap = false;
            }
            m_cursorCol = ((m_cursorCol / 8) + 1) * 8;
            if (m_cursorCol >= m_cols)
            {
                m_cursorCol = 0;
                LineFeed();
                m_pendingWrap = false;
            }
        }
        else if (ch != L'\a')
        {
            PutChar(ch);
        }
        break;

    case EscState::Esc:
        if (ch == L'[')
        {
            m_escState = EscState::Csi;
            m_escParam.clear();
        }
        else if (ch == L']')
        {
            m_escState = EscState::Osc;
            m_escParam.clear();
        }
        else if (ch == L'7')
        {
            m_savedRow = m_cursorRow;
            m_savedCol = m_cursorCol;
            m_escState = EscState::Text;
        }
        else if (ch == L'8')
        {
            m_cursorRow = std::min(m_savedRow, m_rows.size() - 1);
            m_cursorCol = std::min(m_savedCol, m_cols - 1);
            m_escState = EscState::Text;
        }
        else if (ch == L'D')
        {
            LineFeed();
            m_escState = EscState::Text;
        }
        else if (ch == L'M')
        {
            ReverseIndex();
            m_escState = EscState::Text;
        }
        else if (ch == L'c')
        {
            ResetScreen();
            m_escState = EscState::Text;
        }
        else
        {
            m_escState = EscState::Text;
        }
        break;

    case EscState::Csi:
        if (ch >= 0x40 && ch <= 0x7E)
        {
            ExecuteCsi(ch, m_escParam);
            m_escState = EscState::Text;
        }
        else
        {
            m_escParam += ch;
        }
        break;

    case EscState::Osc:
        if (ch == L'\a')
        {
            m_escState = EscState::Text;
        }
        else if (ch == 0x1B)
        {
            m_escState = EscState::Esc;
        }
        else
        {
            m_escParam += ch;
        }
        break;
    }
}

void CTerminalView::FeedText(const std::wstring& text)
{
    for (wchar_t ch : text)
        ProcessChar(ch);
}

void CTerminalView::Feed(const char* data, size_t len)
{
    m_byteBuffer.append(data, len);

    auto decodePrefix = [](const std::string& bytes, int count, UINT codePage,
        bool strict, std::wstring& out) -> bool {
        int wlen = ::MultiByteToWideChar(codePage, strict ? MB_ERR_INVALID_CHARS : 0,
            bytes.data(), count, nullptr, 0);
        if (wlen <= 0)
            return false;
        out.resize(static_cast<size_t>(wlen));
        ::MultiByteToWideChar(codePage, 0, bytes.data(), count, &out[0], wlen);
        return true;
    };

    std::wstring text;
    if (decodePrefix(m_byteBuffer, static_cast<int>(m_byteBuffer.size()), CP_UTF8, true, text))
    {
        m_byteBuffer.clear();
        FeedText(text);
        return;
    }

    // A trailing UTF-8 sequence may be split across read chunks; keep it buffered.
    int size = static_cast<int>(m_byteBuffer.size());
    for (int trim = 1; trim <= 3 && size > trim; trim++)
    {
        text.clear();
        if (decodePrefix(m_byteBuffer, size - trim, CP_UTF8, true, text))
        {
            m_byteBuffer.erase(0, size - trim);
            FeedText(text);
            return;
        }
    }

    // Not valid UTF-8: decode with the system ANSI code page (GBK on zh-CN).
    text.clear();
    if (decodePrefix(m_byteBuffer, size, CP_ACP, false, text))
    {
        m_byteBuffer.clear();
        FeedText(text);
        return;
    }

    m_byteBuffer.erase(0, 1);
}

void CTerminalView::WriteUtf8(const std::string& text)
{
    if (m_session)
        m_session->Write(text);
}

void CTerminalView::WriteString(const std::wstring& text)
{
    if (m_session)
        m_session->WriteString(text);
}

BOOL CTerminalView::StartShell(const CString& shellName)
{
    CString cmdLine;
    if (shellName.CompareNoCase(_T("CMD")) == 0)
    {
        cmdLine = _T("cmd.exe /K chcp 65001 >nul");
    }
    else if (shellName.CompareNoCase(_T("WSL")) == 0)
    {
        cmdLine = _T("wsl.exe");
    }
    else if (shellName.CompareNoCase(_T("Git Bash")) == 0)
    {
        CString bashPath = ResolveGitBashPath();
        cmdLine.Format(_T("\"%s\" --login -i"), bashPath.GetString());
    }
    else
    {
        cmdLine = _T("powershell.exe -NoLogo -NoExit -Command \"$OutputEncoding=[Console]::OutputEncoding=[Text.Encoding]::UTF8; chcp 65001 > $null\"");
    }

    return StartCommandSession(cmdLine, shellName);
}

BOOL CTerminalView::StartCommandSession(const CString& cmdLine, const CString& shellName)
{
    StopSession();
    m_shellName = shellName;

    CString workDir;
    TCHAR envBuf[MAX_PATH]{};
    if (::GetEnvironmentVariable(_T("USERPROFILE"), envBuf, MAX_PATH) > 0)
        workDir = envBuf;

    m_session = std::make_unique<CTerminalSession>(m_hWnd);
    if (!m_session->Start(cmdLine, workDir, m_cols, m_rowsCount))
    {
        m_session.reset();
        ResetScreen();
        FeedText(L"\r\n[Failed to start shell]\r\n");
        Invalidate(FALSE);
        return FALSE;
    }

    ResetScreen();
    FeedText(L"\r\nPowerBox Terminal: " + std::wstring(m_shellName.GetString()) + L"\r\n");
    Invalidate(FALSE);
    return TRUE;
}

void CTerminalView::StopSession()
{
    if (m_session)
    {
        m_session->Stop();
        m_session.reset();
    }
}

bool CTerminalView::AdoptSession(std::unique_ptr<CTerminalSession> session,
    const CString& shellName)
{
    StopSession();
    m_shellName = shellName;
    m_session = std::move(session);
    if (m_session)
        m_session->SetNotifyWindow(m_hWnd);

    ResetScreen();
    FeedText(L"\r\nPowerBox Terminal: " + std::wstring(m_shellName.GetString()) + L"\r\n");
    Invalidate(FALSE);
    return m_session != nullptr;
}

void CTerminalView::ClearScreen()
{
    // Preserve the current prompt line, like cls/clear do in a real terminal.
    std::wstring prompt;
    auto rowHasText = [&](size_t r) {
        if (r >= m_rows.size())
            return false;
        for (const auto& cell : m_rows[r])
        {
            if ((cell.flags & TERM_FLAG_CONT) == 0 &&
                cell.ch != L' ' && cell.ch != L'\0')
            {
                return true;
            }
        }
        return false;
    };

    size_t row = m_rows.empty() ? 0 : m_cursorRow;
    if (row >= m_rows.size())
        row = m_rows.size() - 1;
    while (row > 0 && !rowHasText(row))
        row--;

    if (row < m_rows.size() && rowHasText(row))
    {
        const auto& line = m_rows[row];
        for (const auto& cell : line)
        {
            if (cell.flags & TERM_FLAG_CONT)
                continue;
            prompt.push_back(cell.ch);
        }
        while (!prompt.empty() && (prompt.back() == L' ' || prompt.back() == L'\0'))
            prompt.pop_back();
    }

    ResetScreen();
    if (!prompt.empty())
    {
        FeedText(prompt);
        Invalidate(FALSE);
    }
}

void CTerminalView::RestartShell()
{
    StartShell(m_shellName);
}

COLORREF CTerminalView::PaletteColor(int index, bool bold) const
{
    if (index < 0)
        index = 7;
    if (index < 8 && bold)
        index += 8;
    if (index < 16)
        return TerminalPalette16[index];
    if (index < 232)
    {
        int v = index - 16;
        int r = v / 36;
        int g = (v % 36) / 6;
        int b = v % 6;
        auto cube = [](int c) { return c == 0 ? 0 : 55 + c * 40; };
        return RGB(cube(r), cube(g), cube(b));
    }
    int gray = 8 + (index - 232) * 10;
    return RGB(gray, gray, gray);
}

BOOL CTerminalView::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CTerminalView::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);
    if (rcClient.Width() <= 0 || rcClient.Height() <= 0)
        return;

    COLORREF defaultBg = PaletteColor(0, false);
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(rcClient, defaultBg);

    if (!m_rows.empty())
    {
        CFont fallbackFont;
        CFont* pDrawFont = &m_font;
        if (!m_font.GetSafeHandle())
        {
            fallbackFont.CreateStockObject(DEFAULT_GUI_FONT);
            pDrawFont = &fallbackFont;
        }
        CFont* pOldFont = memDC.SelectObject(pDrawFont);
        memDC.SetBkMode(TRANSPARENT);

        int firstVisible = FirstVisibleRow();
        int rowsToDraw = std::min(m_rowsCount,
            static_cast<int>(rcClient.Height()) / std::max(1, m_cellH));
        rowsToDraw = std::max(0, rowsToDraw);

        int y = 0;
        for (int r = 0; r < rowsToDraw; r++)
        {
            size_t absRow = static_cast<size_t>(firstVisible + r);
            if (absRow >= m_rows.size())
                break;

            const auto& row = m_rows[absRow];
            int x = 0;
            for (int c = 0; c < m_cols; c++)
            {
                const TermCell& cell = (c < static_cast<int>(row.size())) ? row[c] : TermCell{};
                if (cell.flags & TERM_FLAG_CONT)
                {
                    x += m_cellW;
                    continue;
                }

                bool isWide = (cell.flags & TERM_FLAG_WIDE) != 0;
                int drawW = isWide ? m_cellW * 2 : m_cellW;
                BYTE fg = cell.fg;
                BYTE bg = cell.bg;
                if (cell.flags & 4)
                    std::swap(fg, bg);
                if (CellInSelection(static_cast<int>(absRow), c))
                {
                    std::swap(fg, bg);
                    bg = 12; // selection highlight
                }

                COLORREF fgColor = PaletteColor(fg, (cell.flags & 1) != 0);
                COLORREF bgColor = PaletteColor(bg, false);
                CRect cellRect(x, y, x + drawW, y + m_cellH);
                memDC.FillSolidRect(cellRect, bgColor);
                memDC.SetTextColor(fgColor);
                ::TextOutW(memDC.GetSafeHdc(), x, y, &cell.ch, 1);

                if (cell.flags & 2)
                    memDC.FillSolidRect(x, y + m_cellH - 2, drawW, 1, fgColor);

                if (isWide)
                    c++;
                x += drawW;
            }
            y += m_cellH;
        }

        if (m_bFocused && m_cursorRow < m_rows.size() && m_cursorCol < m_cols)
        {
            int visibleRow = static_cast<int>(m_cursorRow) - firstVisible;
            if (visibleRow >= 0 && visibleRow < rowsToDraw)
            {
                int cx = m_cursorCol * m_cellW;
                int cy = visibleRow * m_cellH;
                memDC.FillSolidRect(cx, cy, m_cellW, m_cellH, PaletteColor(7, true));
                memDC.SetTextColor(PaletteColor(0, false));
                const auto& row = m_rows[m_cursorRow];
                wchar_t ch = (m_cursorCol < static_cast<int>(row.size())) ? row[m_cursorCol].ch : L' ';
                ::TextOutW(memDC.GetSafeHdc(), cx, cy, &ch, 1);
            }
        }

        memDC.SelectObject(pOldFont);
    }

    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CTerminalView::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    ResizeGrid();
}

void CTerminalView::OnSetFocus(CWnd* pOldWnd)
{
    CWnd::OnSetFocus(pOldWnd);
    m_bFocused = true;
    Invalidate(FALSE);
}

void CTerminalView::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);
    m_bFocused = false;
    Invalidate(FALSE);
}

void CTerminalView::OnDestroy()
{
    if (m_bAiCapturing)
        FinishAiCapture(m_session ? m_session->ExitCode() : 0);
    StopSession();
    CWnd::OnDestroy();
}

void CTerminalView::SetSelectionFromPoint(CPoint point)
{
    int col = std::max(0, std::min(m_cols - 1, static_cast<int>(point.x) / std::max(1, m_cellW)));
    int visibleRow = static_cast<int>(point.y) / std::max(1, m_cellH);
    int absRow = FirstVisibleRow() + visibleRow;
    absRow = std::max(0, std::min(static_cast<int>(m_rows.size()) - 1, absRow));

    m_selAnchorRow = absRow;
    m_selAnchorCol = col;
    m_selStartRow = m_selEndRow = absRow;
    m_selStartCol = m_selEndCol = col;
    Invalidate(FALSE);
}

void CTerminalView::UpdateSelectionEnd(CPoint point)
{
    int col = std::max(0, std::min(m_cols - 1, static_cast<int>(point.x) / std::max(1, m_cellW)));
    int visibleRow = static_cast<int>(point.y) / std::max(1, m_cellH);
    int absRow = FirstVisibleRow() + visibleRow;
    absRow = std::max(0, std::min(static_cast<int>(m_rows.size()) - 1, absRow));

    m_selEndRow = absRow;
    m_selEndCol = col;

    if (m_selAnchorRow < m_selEndRow ||
        (m_selAnchorRow == m_selEndRow && m_selAnchorCol <= m_selEndCol))
    {
        m_selStartRow = m_selAnchorRow;
        m_selStartCol = m_selAnchorCol;
    }
    else
    {
        m_selStartRow = m_selEndRow;
        m_selStartCol = m_selEndCol;
        m_selEndRow = m_selAnchorRow;
        m_selEndCol = m_selAnchorCol;
    }

    Invalidate(FALSE);
}

void CTerminalView::ClearSelection()
{
    m_selAnchorRow = m_selAnchorCol = -1;
    m_selStartRow = m_selStartCol = -1;
    m_selEndRow = m_selEndCol = -1;
    Invalidate(FALSE);
}

void CTerminalView::StartAiCapture(UINT_PTR id, const CString& startMarker,
    const CString& endMarker, HWND notifyHwnd)
{
    m_aiCaptureId = id;
    m_aiStartMarker = std::string(CT2A(startMarker, CP_UTF8));
    m_aiEndMarker = std::string(CT2A(endMarker, CP_UTF8));
    m_aiNotifyHwnd = notifyHwnd;
    m_aiCaptureBuffer.clear();
    m_bAiCapturing = true;
}

void CTerminalView::StopAiCapture()
{
    m_bAiCapturing = false;
    m_aiCaptureBuffer.clear();
}

void CTerminalView::CheckAiCapture(const std::string& data)
{
    if (!m_bAiCapturing)
        return;

    m_aiCaptureBuffer.append(data);
    DWORD exitCode = m_session ? m_session->ExitCode() : 0;
    size_t startPos = m_aiCaptureBuffer.find(m_aiStartMarker);
    size_t endPos = m_aiCaptureBuffer.find(m_aiEndMarker);
    if (startPos == std::string::npos || endPos == std::string::npos ||
        endPos <= startPos)
    {
        return;
    }

    size_t begin = startPos + m_aiStartMarker.size();
    auto* pResult = new AiCaptureResult;
    pResult->id = m_aiCaptureId;
    pResult->output = m_aiCaptureBuffer.substr(begin, endPos - begin);
    pResult->exitCode = exitCode;
    if (m_aiNotifyHwnd)
        ::PostMessage(m_aiNotifyHwnd, WM_AI_CAPTURE_DONE, 0, reinterpret_cast<LPARAM>(pResult));
    else
        delete pResult;

    m_bAiCapturing = false;
    m_aiCaptureBuffer.clear();
}

void CTerminalView::FinishAiCapture(DWORD exitCode)
{
    if (!m_bAiCapturing)
        return;

    auto* pResult = new AiCaptureResult;
    pResult->id = m_aiCaptureId;
    pResult->output = m_aiCaptureBuffer;
    pResult->exitCode = exitCode;
    if (m_aiNotifyHwnd)
        ::PostMessage(m_aiNotifyHwnd, WM_AI_CAPTURE_DONE, 0, reinterpret_cast<LPARAM>(pResult));
    else
        delete pResult;

    m_bAiCapturing = false;
    m_aiCaptureBuffer.clear();
}

void CTerminalView::OnLButtonDown(UINT, CPoint point)
{
    SetFocus();
    SetCapture();
    m_bSelecting = true;
    SetSelectionFromPoint(point);
}

void CTerminalView::OnLButtonUp(UINT, CPoint)
{
    if (m_bSelecting)
    {
        m_bSelecting = false;
        if (GetCapture() == this)
            ReleaseCapture();
        Invalidate(FALSE);
    }
}

void CTerminalView::OnMouseMove(UINT, CPoint point)
{
    if (m_bSelecting)
        UpdateSelectionEnd(point);
}

void CTerminalView::StartSelectionFromScreen(CPoint screenPt)
{
    CPoint pt = screenPt;
    ScreenToClient(&pt);
    SetFocus();
    SetCapture();
    m_bSelecting = true;
    SetSelectionFromPoint(pt);
}

void CTerminalView::ContinueSelectionFromScreen(CPoint screenPt)
{
    if (!m_bSelecting)
        return;

    CPoint pt = screenPt;
    ScreenToClient(&pt);
    UpdateSelectionEnd(pt);
}

void CTerminalView::FinishSelection()
{
    if (m_bSelecting)
    {
        m_bSelecting = false;
        if (GetCapture() == this)
            ReleaseCapture();
        Invalidate(FALSE);
    }
}

void CTerminalView::ScrollLines(short zDelta)
{
    int maxOffset = static_cast<int>(ScreenStart());
    if (zDelta > 0)
        m_scrollOffset = std::min(maxOffset, m_scrollOffset + 3);
    else
        m_scrollOffset = std::max(0, m_scrollOffset - 3);
    Invalidate(FALSE);
}

BOOL CTerminalView::OnMouseWheel(UINT, short zDelta, CPoint)
{
    ScrollLines(zDelta);
    return TRUE;
}

void CTerminalView::OnRButtonUp(UINT, CPoint point)
{
    CPoint screenPt = point;
    ClientToScreen(&screenPt);
    ShowContextMenu(screenPt);
}

void CTerminalView::ShowContextMenu(CPoint screenPt)
{
    auto& loc = CLocalizationManager::GetInstance();
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_TERMINAL_COPY, loc.GetString(_T("Terminal"), _T("Copy")));
    menu.AppendMenu(MF_STRING, ID_TERMINAL_PASTE, loc.GetString(_T("Terminal"), _T("Paste")));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, ID_TERMINAL_RESTART, loc.GetString(_T("Terminal"), _T("Restart")));

    UINT cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, screenPt.x, screenPt.y, this);
    if (cmd == ID_TERMINAL_COPY)
        OnTerminalCopy();
    else if (cmd == ID_TERMINAL_PASTE)
        OnTerminalPaste();
    else if (cmd == ID_TERMINAL_RESTART)
        OnTerminalRestart();
}

bool CTerminalView::CellInSelection(int row, int col) const
{
    if (m_selStartRow < 0 || m_selEndRow < 0)
        return false;
    if (row < m_selStartRow || row > m_selEndRow)
        return false;
    if (row == m_selStartRow && col < m_selStartCol)
        return false;
    if (row == m_selEndRow && col > m_selEndCol)
        return false;
    return true;
}

std::wstring CTerminalView::GetSelectedText() const
{
    if (m_selStartRow < 0 || m_selEndRow < 0)
        return {};

    std::wstring result;
    for (int r = m_selStartRow; r <= m_selEndRow; r++)
    {
        if (r < 0 || static_cast<size_t>(r) >= m_rows.size())
            continue;

        const auto& row = m_rows[static_cast<size_t>(r)];
        int from = (r == m_selStartRow) ? m_selStartCol : 0;
        int to = (r == m_selEndRow) ? m_selEndCol + 1 : m_cols;
        from = std::max(0, from);
        to = std::min(static_cast<int>(row.size()), to);

        std::wstring line;
        for (int c = from; c < to; c++)
        {
            if (row[c].flags & TERM_FLAG_CONT)
                continue;
            line.push_back(row[c].ch);
        }

        while (!line.empty() && (line.back() == L' ' || line.back() == L'\0'))
            line.pop_back();

        result += line;
        if (r != m_selEndRow)
            result += L"\r\n";
    }
    return result;
}

void CTerminalView::CopySelection()
{
    std::wstring text = GetSelectedText();
    if (text.empty())
        return;

    if (!::OpenClipboard(m_hWnd))
        return;
    ::EmptyClipboard();

    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem)
    {
        void* p = ::GlobalLock(hMem);
        if (p)
        {
            memcpy(p, text.c_str(), bytes);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    ::CloseClipboard();
}

void CTerminalView::PasteClipboard()
{
    if (!::OpenClipboard(m_hWnd))
        return;

    HANDLE hData = ::GetClipboardData(CF_UNICODETEXT);
    if (hData)
    {
        LPCWSTR p = static_cast<LPCWSTR>(::GlobalLock(hData));
        if (p)
        {
            std::wstring text(p);
            std::wstring out;
            out.reserve(text.size());
            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r'))
                    out.push_back(L'\r');
                else if (text[i] != L'\r')
                    out.push_back(text[i]);
            }
            WriteString(out);
            ::GlobalUnlock(hData);
        }
    }
    ::CloseClipboard();
}

void CTerminalView::OnKeyDown(UINT nChar, UINT, UINT)
{
    ClearSelection();

    if (!m_session || !m_session->IsRunning())
        return;

    bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (ctrl && shift && (nChar == 'C' || nChar == 'c'))
    {
        CopySelection();
        return;
    }
    if (ctrl && (nChar == 'C' || nChar == 'c'))
    {
        if (m_selStartRow >= 0)
            CopySelection();
        else
            WriteUtf8("\x03");
        return;
    }
    if (ctrl && (nChar == 'V' || nChar == 'v'))
    {
        PasteClipboard();
        return;
    }
    if (ctrl && nChar >= 'A' && nChar <= 'Z')
    {
        char c = static_cast<char>(nChar - 'A' + 1);
        WriteUtf8(std::string(1, c));
        return;
    }

    if (shift && (nChar == VK_PRIOR || nChar == VK_NEXT))
    {
        ScrollLines(nChar == VK_PRIOR ? 120 : -120);
        return;
    }

    switch (nChar)
    {
    case VK_RETURN: WriteString(L"\r"); break;
    case VK_BACK: WriteString(L"\x7f"); break;
    case VK_TAB: WriteString(L"\t"); break;
    case VK_LEFT: WriteString(L"\x1b[D"); break;
    case VK_RIGHT: WriteString(L"\x1b[C"); break;
    case VK_UP: WriteString(L"\x1b[A"); break;
    case VK_DOWN: WriteString(L"\x1b[B"); break;
    case VK_HOME: WriteString(L"\x1b[H"); break;
    case VK_END: WriteString(L"\x1b[F"); break;
    case VK_DELETE: WriteString(L"\x1b[3~"); break;
    case VK_PRIOR: WriteString(L"\x1b[5~"); break;
    case VK_NEXT: WriteString(L"\x1b[6~"); break;
    case VK_ESCAPE: WriteString(L"\x1b"); break;
    default: break;
    }
}

void CTerminalView::OnChar(UINT nChar, UINT, UINT)
{
    ClearSelection();

    if (!m_session || !m_session->IsRunning())
        return;
    wchar_t ch = static_cast<wchar_t>(nChar);
    if (ch >= 0x20 && ch != 0x7F)
        WriteString(std::wstring(1, ch));
}

LRESULT CTerminalView::OnTermOutput(WPARAM wParam, LPARAM lParam)
{
    auto* p = reinterpret_cast<std::string*>(lParam);
    auto* session = reinterpret_cast<CTerminalSession*>(wParam);
    if (session != m_session.get())
    {
        delete p;
        return 0;
    }
    if (p)
    {
        std::string data = *p;
        Feed(data.data(), data.size());
        CheckAiCapture(data);
        delete p;
    }
    Invalidate(FALSE);
    return 0;
}

LRESULT CTerminalView::OnTermExited(WPARAM wParam, LPARAM)
{
    if (reinterpret_cast<CTerminalSession*>(wParam) != m_session.get())
        return 0;
    if (m_bAiCapturing)
        FinishAiCapture(m_session ? m_session->ExitCode() : 0);
    CString exited = CLocalizationManager::GetInstance().GetString(_T("Terminal"), _T("Exited"));
    std::wstring msg = L"\r\n" + std::wstring(exited.GetString());
    FeedText(msg);
    Invalidate(FALSE);
    return 0;
}

LRESULT CTerminalView::OnImeComposition(WPARAM, LPARAM lParam)
{
    if ((lParam & GCS_RESULTSTR) == 0)
        return 0;

    HIMC hImc = ::ImmGetContext(m_hWnd);
    if (!hImc)
        return 0;

    DWORD size = ::ImmGetCompositionStringW(hImc, GCS_RESULTSTR, nullptr, 0);
    if (size > 0)
    {
        std::wstring text(size / sizeof(wchar_t), 0);
        ::ImmGetCompositionStringW(hImc, GCS_RESULTSTR, &text[0], size);
        WriteString(text);
        ClearSelection();
    }

    ::ImmReleaseContext(m_hWnd, hImc);
    return 1;
}

LRESULT CTerminalView::OnImeCharMsg(WPARAM wParam, LPARAM)
{
    wchar_t ch = static_cast<wchar_t>(wParam);
    if (ch >= 0x20)
    {
        WriteString(std::wstring(1, ch));
        ClearSelection();
    }
    return 1;
}

void CTerminalView::OnTerminalCopy()
{
    CopySelection();
}

void CTerminalView::OnTerminalPaste()
{
    PasteClipboard();
}

void CTerminalView::OnTerminalRestart()
{
    RestartShell();
}
