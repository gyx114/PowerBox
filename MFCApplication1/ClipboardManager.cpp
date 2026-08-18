// ClipboardManager.cpp: Clipboard history core implementation (skeleton for Task 1)
#include "pch.h"
#include "framework.h"
#include "ClipboardManager.h"
#include <thread>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <shellapi.h>
#include <shlobj.h>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

#include "json.hpp"

static HBITMAP ClipDibToBitmap(HGLOBAL hDib);
static std::pair<std::wstring, std::wstring> ClipSaveBitmapToDisk(HBITMAP hbm, const std::wstring& dir);
static HBITMAP ClipboardLoadPngToBitmap(const std::wstring& path);
static HGLOBAL ClipboardBitmapToDib(HBITMAP hbm);
static void ClipboardDeleteTree(const std::wstring& root);

ClipboardManager::ClipboardManager()
{
    m_captureThread = std::thread([this]() { CaptureLoop(); });
}

ClipboardManager::~ClipboardManager()
{
    Shutdown();
}

void ClipboardManager::Shutdown()
{
    {
        std::lock_guard<std::mutex> lk(m_captureMutex);
        m_captureStop = true;
        m_capturePending = false;
    }
    m_captureCv.notify_all();
    if (m_captureThread.joinable()) m_captureThread.join();
}

const std::deque<ClipboardEntry>& ClipboardManager::Entries() const { return m_entries; }

void ClipboardManager::SetChangeCallback(std::function<void()> cb)
{
    std::lock_guard<std::mutex> lk(m_callbackMutex);
    m_onChange = std::move(cb);
}

void ClipboardManager::NotifyChange()
{
    std::function<void()> cb;
    {
        std::lock_guard<std::mutex> lk(m_callbackMutex);
        cb = m_onChange;
    }
    if (cb) cb();
}

std::vector<ClipboardEntry> ClipboardManager::Snapshot() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<ClipboardEntry> out(m_entries.begin(), m_entries.end());
    // Pinned entries always stay on top, preserving their relative order.
    std::stable_partition(out.begin(), out.end(),
                          [](const ClipboardEntry& e) { return e.pinned; });
    return out;
}

std::vector<std::wstring> ClipboardManager::RecentTextItems(int n) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::wstring> out;
    for (const auto& e : m_entries)
    {
        if (static_cast<int>(out.size()) >= n) break;
        if (!e.text.empty()) out.push_back(e.text);
    }
    return out;
}

void ClipboardManager::Capture()
{
    {
        std::lock_guard<std::mutex> lk(m_captureMutex);
        // Record the arrival time. A short debounce window lets repeated
        // notifications (Snipping Tool posting one screenshot twice, an app
        // clearing-then-filling the clipboard) collapse into a single capture.
        m_lastCapture = std::chrono::steady_clock::now();
        m_capturePending = true;
    }
    m_captureCv.notify_one();
}

void ClipboardManager::CaptureLoop()
{
    constexpr auto kDebounce = std::chrono::milliseconds(600);

    std::unique_lock<std::mutex> lk(m_captureMutex);
    for (;;)
    {
        m_captureCv.wait(lk, [this]() { return m_capturePending || m_captureStop; });
        if (m_captureStop) break;

        // Keep waiting until the debounce window elapses after the most recent
        // notification, so bursts of updates are coalesced into one capture.
        m_captureCv.wait_until(lk, m_lastCapture + kDebounce,
                               [this]() { return m_captureStop; });
        if (m_captureStop) break;

        m_capturePending = false;
        lk.unlock();
        try { CaptureWorker(); }
        catch (...) { /* never let a capture crash the worker loop */ }
        lk.lock();
    }
}

void ClipboardManager::CaptureWorker()
{
    ClipboardEntry e;
    if (!BuildEntry(e)) return;
    SaveIndex();
    NotifyChange();
}

static bool SamePrimary(const ClipboardEntry& a, const ClipboardEntry& b)
{
    if (!a.text.empty() && a.text == b.text) return true;
    if (!a.files.empty() && a.files == b.files) return true;
    return false;
}

// FNV-1a 64-bit hash used to fingerprint an image's DIB bytes so that the same
// screenshot arriving twice on the clipboard (e.g. how Snipping Tool posts the
// image) is collapsed into a single entry.
static std::uint64_t HashBytes(const void* data, size_t len)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i)
    {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

bool ClipboardManager::BuildEntry(ClipboardEntry& e)
{
    if (!::OpenClipboard(nullptr)) return false;

    // Replay writes back into the system clipboard. Without this marker the
    // resulting WM_CLIPBOARDUPDATE would immediately create a duplicate entry.
    const UINT replayFmt = ::RegisterClipboardFormatW(L"MFCApplication1.ReplayMarker");
    if (::IsClipboardFormatAvailable(replayFmt))
    {
        ::CloseClipboard();
        return false;
    }

    const bool hasText  = ::IsClipboardFormatAvailable(CF_UNICODETEXT);
    const bool hasFiles = ::IsClipboardFormatAvailable(CF_HDROP);
    const bool hasImage = ::IsClipboardFormatAvailable(CF_DIB) || ::IsClipboardFormatAvailable(CF_DIBV5);

    // Ignore a momentarily blank clipboard (e.g. an app briefly clearing it while
    // preparing an action) - never record a zero-payload entry.
    if (!hasText && !hasFiles && !hasImage)
    {
        ::CloseClipboard();
        return false;
    }

    // Fingerprint the image bytes while the clipboard is still open. Used below to
    // collapse a screenshot that Snipping Tool posts more than once.
    std::uint64_t imgFp = 0;
    if (hasImage)
    {
        HANDLE hDib = ::GetClipboardData(CF_DIB);
        if (!hDib) hDib = ::GetClipboardData(CF_DIBV5);
        if (hDib)
        {
            if (const void* mem = ::GlobalLock(hDib))
            {
                imgFp = HashBytes(mem, static_cast<size_t>(::GlobalSize(hDib)));
                ::GlobalUnlock(hDib);
            }
        }
    }

    if (hasText)
    {
        HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
        if (h)
        {
            LPCWSTR p = (LPCWSTR)::GlobalLock(h);
            if (p) { e.text = p; ::GlobalUnlock(h); }
        }
    }

    if (hasFiles)
    {
        HANDLE hd = ::GetClipboardData(CF_HDROP);
        if (hd)
        {
            HDROP drop = (HDROP)::GlobalLock(hd);
            if (drop)
            {
                const UINT n = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < n; i++)
                {
                    const int len = ::DragQueryFileW(drop, i, nullptr, 0);
                    std::wstring path(len, L'\0');
                    ::DragQueryFileW(drop, i, &path[0], len + 1);
                    e.files.push_back(std::move(path));
                }
                ::GlobalUnlock(hd);
            }
        }
        // Cut (move) operations are transient; do not record them.
        const UINT uDropEffect = ::RegisterClipboardFormatW(L"Preferred DropEffect");
        const HANDLE hMove = ::GetClipboardData(uDropEffect);
        if (hMove)
        {
            DWORD* eff = (DWORD*)::GlobalLock(hMove);
            if (eff && (*eff & DROPEFFECT_MOVE)) { ::GlobalUnlock(hMove); ::CloseClipboard(); return false; }
            if (eff) ::GlobalUnlock(hMove);
        }
    }
    ::CloseClipboard();

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_entries.empty())
        {
            const ClipboardEntry& f = m_entries.front();
            if (SamePrimary(f, e)) return false;
            // Collapse a duplicate image (identical bytes) posted more than once,
            // e.g. Snipping Tool delivering the same screenshot in two notifications.
            if (hasImage && imgFp)
            {
                const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                             [imgFp](const ClipboardEntry& x)
                                             { return x.type == ClipType::Image && x.imageFp == imgFp; });
                if (it != m_entries.end()) return false;
            }
        }
        e.id = m_nextId++;
    }
    e.imageFp = imgFp;
    e.timestamp = static_cast<std::uint64_t>(std::time(nullptr));

    if (hasImage)      e.type = ClipType::Image;
    else if (hasFiles) e.type = ClipType::Files;
    else               e.type = ClipType::Text;

    if (hasImage)
    {
        HBITMAP hbm = nullptr;
        if (::OpenClipboard(nullptr))
        {
            HANDLE hDib = ::GetClipboardData(CF_DIB);
            if (!hDib) hDib = ::GetClipboardData(CF_DIBV5);
            if (hDib) hbm = ClipDibToBitmap(hDib);
            ::CloseClipboard();
        }
        if (hbm)
        {
            e.dir = MakeEntryDir(e.id);
            ::CreateDirectoryW(e.dir.c_str(), nullptr);
            auto [png, thumb] = ClipSaveBitmapToDisk(hbm, e.dir);
            e.imagePath = png;
            e.thumbPath = thumb;
            ::DeleteObject(hbm);
        }
        if (e.imagePath.empty() && !e.dir.empty())
        {
            ClipboardDeleteTree(e.dir);
            e.dir.clear();
        }
    }

    // Snapshot policy: only when enabled and the file set is within the caps
    // (file count <= SnapshotMaxFiles and total size <= SnapshotMaxBytesMB) do we
    // copy the files into the per-entry archive dir for a durable re-copy.
    if (!e.files.empty() && SnapshotEnabled())
    {
        __int64 total = 0;
        const bool over = e.files.size() > static_cast<size_t>(SnapshotMaxFiles());
        const __int64 mb = static_cast<__int64>(SnapshotMaxBytesMB()) * 1024 * 1024;
        for (const auto& p : e.files)
        {
            WIN32_FILE_ATTRIBUTE_DATA fd{};
            if (::GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fd))
                total += ((__int64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        }
        if (!over && total <= mb)
        {
            if (e.dir.empty()) e.dir = MakeEntryDir(e.id);
            ::CreateDirectoryW(e.dir.c_str(), nullptr);
            int idx = 0;
            for (const auto& p : e.files)
            {
                std::wstring leaf = L"file_" + std::to_wstring(idx++);
                const size_t slash = p.find_last_of(L"\\/");
                if (slash != std::wstring::npos) leaf = p.substr(slash + 1);
                const std::wstring dst = e.dir + L"\\" + leaf;
                if (::CopyFileW(p.c_str(), dst.c_str(), FALSE))
                    e.snapshotMap[p] = leaf;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_entries.push_front(std::move(e));
        TrimIfNeeded();
    }
    return true;
}

void ClipboardManager::TrimIfNeeded()
{
    // Called with m_mutex held by the caller.
    const int cap = MaxEntries();
    const int safe = cap > 0 ? cap : 1;
    while (static_cast<int>(m_entries.size()) > safe)
        m_entries.pop_back();
}

static int ClipGetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0, size = 0;
    if (GetImageEncodersSize(&num, &size) != Ok || size == 0) return -1;
    ImageCodecInfo* pInfo = (ImageCodecInfo*)malloc(size);
    if (!pInfo) return -1;
    GetImageEncoders(num, size, pInfo);
    int found = -1;
    for (UINT i = 0; i < num; i++)
    {
        if (wcscmp(pInfo[i].MimeType, format) == 0) { *pClsid = pInfo[i].Clsid; found = i; break; }
    }
    free(pInfo);
    return found;
}

std::wstring ClipboardManager::ClipboardStoreDir()
{
    wchar_t buf[MAX_PATH]{};
    ::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    std::wstring d = std::wstring(buf) + L"\\PowerBox\\ClipboardHistory";
    ::SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
    return d;
}

std::wstring ClipboardManager::MakeEntryDir(std::uint64_t id)
{
    wchar_t n[32];
    _snwprintf_s(n, 32, _TRUNCATE, L"%llu", id);
    return ClipboardStoreDir() + L"\\" + n;
}

// CF_DIB global block -> HBITMAP (top-down/device-independent bitmap)
static const void* ClipDibBits(const BITMAPINFO* pbi)
{
    const BITMAPINFOHEADER& bi = pbi->bmiHeader;
    UINT colors = bi.biClrUsed;
    if (colors == 0 && bi.biBitCount <= 8)
        colors = 1u << bi.biBitCount;
    if (colors == 0 && bi.biCompression == BI_BITFIELDS)
        colors = 3; // RGB masks immediately follow BITMAPINFOHEADER
    return reinterpret_cast<const BYTE*>(pbi) + bi.biSize + colors * sizeof(RGBQUAD);
}

static HBITMAP ClipDibToBitmap(HGLOBAL hDib)
{
    BITMAPINFO* pbi = (BITMAPINFO*)::GlobalLock(hDib);
    if (!pbi) return nullptr;
    const BITMAPINFOHEADER& bi = pbi->bmiHeader;
    HDC hdc = ::GetDC(nullptr);
    BITMAPINFOHEADER out = bi;
    out.biHeight = (bi.biHeight < 0) ? bi.biHeight : -bi.biHeight; // force top-down
    out.biCompression = BI_RGB;
    out.biBitCount = 32;
    out.biSizeImage = static_cast<DWORD>(abs(out.biHeight)) * static_cast<DWORD>(out.biWidth) * 4;
    out.biClrUsed = 0;
    out.biClrImportant = 0;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(hdc, (BITMAPINFO*)&out, DIB_RGB_COLORS, &bits, nullptr, 0);
    ::ReleaseDC(nullptr, hdc);
    if (!hbm) { ::GlobalUnlock(hDib); return nullptr; }
    HDC hdc2 = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc2, hbm);
    ::StretchDIBits(hdc2, 0, 0, out.biWidth, abs(out.biHeight),
                    0, 0, bi.biWidth, abs(bi.biHeight), ClipDibBits(pbi), pbi, DIB_RGB_COLORS, SRCCOPY);
    ::SelectObject(hdc2, old);
    ::DeleteDC(hdc2);
    ::GlobalUnlock(hDib);
    return hbm;
}

// HBITMAP -> (image.png, thumb.png) in dir
static std::pair<std::wstring, std::wstring> ClipSaveBitmapToDisk(HBITMAP hbm, const std::wstring& dir)
{
    std::wstring png = dir + L"\\image.png";
    std::wstring thumb = dir + L"\\thumb.png";
    Bitmap bmp(hbm, nullptr);
    if (bmp.GetLastStatus() != Ok) return { L"", L"" };
    CLSID clsid;
    if (ClipGetEncoderClsid(L"image/png", &clsid) < 0) return { L"", L"" };
    if (bmp.Save(png.c_str(), &clsid, nullptr) != Ok) return { L"", L"" };
    Bitmap tb(48, 48, PixelFormat32bppARGB);
    Graphics g(&tb);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(&bmp, Rect(0, 0, 48, 48));
    if (tb.Save(thumb.c_str(), &clsid, nullptr) != Ok)
    {
        ::DeleteFileW(png.c_str());
        return { L"", L"" };
    }
    return { png, thumb };
}

// Load a PNG archive file back into an HBITMAP (used by Replay for images).
static HBITMAP ClipboardLoadPngToBitmap(const std::wstring& path)
{
    Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Ok) return nullptr;
    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(Color(255, 255, 255), &hbm); // flatten with white background
    return hbm;
}

// HBITMAP -> erase global memory block in CF_DIB format (top-down 32bpp).
static HGLOBAL ClipboardBitmapToDib(HBITMAP hbm)
{
    BITMAP bm{};
    if (!::GetObject(hbm, sizeof(bm), &bm) || bm.bmHeight <= 0) return nullptr;

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = -bm.bmHeight;      // top-down so pixels match the source rows
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = bm.bmWidth * 4 * bm.bmHeight; // 32bpp rows are already word-aligned

    const SIZE_T total = sizeof(BITMAPINFOHEADER) + bi.biSizeImage;
    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, total);
    if (!h) return nullptr;

    BITMAPINFO* pbi = (BITMAPINFO*)::GlobalLock(h);
    if (!pbi) { ::GlobalFree(h); return nullptr; }
    pbi->bmiHeader = bi;
    void* bits = (char*)pbi + sizeof(BITMAPINFOHEADER);
    HDC hdc = ::CreateCompatibleDC(nullptr);
    const int got = ::GetDIBits(hdc, hbm, 0, bm.bmHeight, bits, pbi, DIB_RGB_COLORS);
    ::DeleteDC(hdc);
    ::GlobalUnlock(h);
    if (got == 0) { ::GlobalFree(h); return nullptr; }
    return h;
}

// Recursively delete a directory (used when removing/clearing entries).
static void ClipboardDeleteTree(const std::wstring& root)
{
    std::wstring pattern = root + L"\\*";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = ::FindFirstFileW(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            const std::wstring child = root + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                ClipboardDeleteTree(child);
            else
                ::DeleteFileW(child.c_str());
        } while (::FindNextFileW(hFind, &fd));
        ::FindClose(hFind);
    }
    ::RemoveDirectoryW(root.c_str());
}

// ---- Task 4: index.json persistence ----
static std::string Utf16ToUtf8(const std::wstring& s)
{
    if (s.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}
static std::wstring Utf8ToUtf16(const std::string& s)
{
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
    return out;
}

void ClipboardManager::SaveIndex()
{
    nlohmann::json arr = nlohmann::json::array();
    std::uint64_t nextId = 0;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (const auto& e : m_entries)
        {
            nlohmann::json o;
            o["id"] = e.id;
            o["ts"] = e.timestamp;
            o["pinned"] = e.pinned;
            o["type"] = static_cast<int>(e.type);
            if (e.imageFp) o["fp"] = e.imageFp;
            if (!e.text.empty())      o["text"] = Utf16ToUtf8(e.text);
            if (!e.files.empty())     { for (const auto& f : e.files) o["files"].push_back(Utf16ToUtf8(f)); }
            if (!e.imagePath.empty()) o["image"] = Utf16ToUtf8(e.imagePath);
            if (!e.thumbPath.empty()) o["thumb"] = Utf16ToUtf8(e.thumbPath);
            if (!e.dir.empty())       o["dir"] = Utf16ToUtf8(e.dir);
            if (!e.snapshotMap.empty())
            {
                o["snapshots"] = nlohmann::json::object();
                for (const auto& kv : e.snapshotMap)
                    o["snapshots"][Utf16ToUtf8(kv.first)] = Utf16ToUtf8(kv.second);
            }
            arr.push_back(std::move(o));
        }
        nextId = m_nextId;
    }
    nlohmann::json root;
    root["nextId"] = nextId;
    root["entries"] = arr;
    const std::wstring ix = ClipboardStoreDir() + L"\\index.json";
    const std::wstring tmp = ix + L".tmp";
    HANDLE h = ::CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        const std::string s = root.dump(2);
        DWORD w = 0;
        ::WriteFile(h, s.data(), static_cast<DWORD>(s.size()), &w, nullptr);
        ::CloseHandle(h);
        ::MoveFileExW(tmp.c_str(), ix.c_str(), MOVEFILE_REPLACE_EXISTING); // atomic replace
    }
}

void ClipboardManager::LoadIndex()
{
    const std::wstring ix = ClipboardStoreDir() + L"\\index.json";
    HANDLE h = ::CreateFileW(ix.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::string s;
    char buf[4096];
    DWORD r = 0;
    while (::ReadFile(h, buf, sizeof(buf), &r, nullptr) && r > 0) s.append(buf, r);
    ::CloseHandle(h);
    try
    {
        const auto root = nlohmann::json::parse(s);
        std::lock_guard<std::mutex> lk(m_mutex);
        m_nextId = root.value("nextId", static_cast<std::uint64_t>(1));
        for (const auto& o : root["entries"])
        {
            ClipboardEntry e;
            e.id = o["id"];
            e.timestamp = o.value("ts", static_cast<std::uint64_t>(0));
            e.pinned = o.value("pinned", false);
            e.type = static_cast<ClipType>(o.value("type", 0));
            e.imageFp = o.value("fp", static_cast<std::uint64_t>(0));
            if (o.contains("text"))  e.text = Utf8ToUtf16(o["text"]);
            if (o.contains("files")) { for (const auto& f : o["files"]) e.files.push_back(Utf8ToUtf16(f)); }
            if (o.contains("image")) e.imagePath = Utf8ToUtf16(o["image"]);
            if (o.contains("thumb")) e.thumbPath = Utf8ToUtf16(o["thumb"]);
            if (o.contains("dir"))   e.dir = Utf8ToUtf16(o["dir"]);
            if (o.contains("snapshots"))
                for (auto it = o["snapshots"].begin(); it != o["snapshots"].end(); ++it)
                    e.snapshotMap[Utf8ToUtf16(it.key())] = Utf8ToUtf16(it.value());
            m_entries.push_back(std::move(e));
        }
    }
    catch (...) { /* corrupted file ignored; will be rebuilt on next save */ }
}

// ---- Task 6: full replay to the system clipboard ----
bool ClipboardManager::Replay(std::uint64_t id)
{
    ClipboardEntry e;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                     [id](const auto& x) { return x.id == id; });
        if (it == m_entries.end()) return false;
        e = *it;
    }

    if (!::OpenClipboard(nullptr)) return false;
    ::EmptyClipboard();
    bool ok = true;

    const UINT replayFmt = ::RegisterClipboardFormatW(L"MFCApplication1.ReplayMarker");
    HGLOBAL marker = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
    if (marker)
    {
        if (::SetClipboardData(replayFmt, marker) == nullptr)
        {
            ::GlobalFree(marker);
            ok = false;
        }
    }
    else ok = false;

    if (!e.text.empty())
    {
        const SIZE_T bytes = (e.text.size() + 1) * sizeof(wchar_t);
        HGLOBAL g = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (g)
        {
            void* p = ::GlobalLock(g);
            if (p)
            {
                memcpy(p, e.text.c_str(), bytes);
                ::GlobalUnlock(g);
                if (::SetClipboardData(CF_UNICODETEXT, g) == nullptr) { ::GlobalFree(g); ok = false; }
            }
            else { ::GlobalFree(g); ok = false; }
        }
        else ok = false;
    }

    if (!e.files.empty())
    {
        std::wstring entryDir = e.dir;
        if (entryDir.empty() && !e.snapshotMap.empty())
            entryDir = MakeEntryDir(e.id);

        // Prefer archived snapshots when available, otherwise fall back to the
        // original path only if it still exists, and never write an empty list.
        std::vector<std::wstring> resolved;
        for (const auto& f : e.files)
        {
            const auto it = e.snapshotMap.find(f);
            if (it != e.snapshotMap.end())
                resolved.push_back(entryDir + L"\\" + it->second);
            else if (::GetFileAttributesW(f.c_str()) != INVALID_FILE_ATTRIBUTES)
                resolved.push_back(f);
        }
        if (!resolved.empty())
        {
            DWORD total = sizeof(DROPFILES) + sizeof(wchar_t); // trailing double NUL
            for (const auto& f : resolved) total += static_cast<DWORD>((f.size() + 1) * sizeof(wchar_t));
            HGLOBAL g = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
            if (g)
            {
                DROPFILES* df = (DROPFILES*)::GlobalLock(g);
                if (df)
                {
                    df->pFiles = sizeof(DROPFILES);
                    df->fWide = TRUE;
                    wchar_t* p = (wchar_t*)((char*)df + sizeof(DROPFILES));
                    for (const auto& f : resolved)
                    {
                        memcpy(p, f.c_str(), (f.size() + 1) * sizeof(wchar_t));
                        p += f.size() + 1;
                    }
                    *p = L'\0'; // second terminating NUL
                    ::GlobalUnlock(g);
                    if (::SetClipboardData(CF_HDROP, g) == nullptr) { ::GlobalFree(g); ok = false; }
                }
                else { ::GlobalFree(g); ok = false; }
            }
            else ok = false;
        }
    }

    if (!e.imagePath.empty())
    {
        HBITMAP hbm = ClipboardLoadPngToBitmap(e.imagePath);
        if (hbm)
        {
            // Build the DIB first (hbm stays alive), then hand hbm to the
            // clipboard as CF_BITMAP — the most widely accepted image format for
            // pasting into Word / Paint / rich edit controls / browsers.
            HGLOBAL g = ClipboardBitmapToDib(hbm);
            if (g)
            {
                if (::SetClipboardData(CF_DIB, g) == nullptr) { ::GlobalFree(g); ok = false; }
            }
            else ok = false;
            if (::SetClipboardData(CF_BITMAP, hbm) == nullptr)
            {
                ::DeleteObject(hbm); // clipboard did not take ownership
                ok = false;
            }
        }
        else ok = false;
    }

    ::CloseClipboard();
    return ok;
}

void ClipboardManager::Remove(std::uint64_t id)
{
    ClipboardEntry victim;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                     [id](const auto& x) { return x.id == id; });
        if (it == m_entries.end()) return;
        victim = *it;
        m_entries.erase(it);
    }
    std::wstring dir = victim.dir;
    if (dir.empty() && (!victim.imagePath.empty() || !victim.snapshotMap.empty()))
        dir = MakeEntryDir(victim.id);
    if (!dir.empty()) ClipboardDeleteTree(dir);
    SaveIndex();
    NotifyChange();
}

void ClipboardManager::Clear()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_entries.clear();
        // All entries are gone, so restart the id sequence for fresh entries.
        m_nextId = 1;
    }
    ClipboardDeleteTree(ClipboardStoreDir()); // clears archived files + index dir contents
    ::CreateDirectoryW(ClipboardStoreDir().c_str(), nullptr);
    SaveIndex();
    NotifyChange();
}

void ClipboardManager::TogglePin(std::uint64_t id)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                     [id](const auto& x) { return x.id == id; });
        if (it == m_entries.end()) return;
        it->pinned = !it->pinned;
    }
    SaveIndex();
    NotifyChange();
}

// ---- Task 5: config + snapshot ----
int  ClipboardManager::MaxEntries() const { return AfxGetApp()->GetProfileInt(_T("Clipboard"), _T("MaxEntries"), 200); }
void ClipboardManager::SetMaxEntries(int v) { AfxGetApp()->WriteProfileInt(_T("Clipboard"), _T("MaxEntries"), v); }
bool ClipboardManager::SnapshotEnabled() const { return AfxGetApp()->GetProfileInt(_T("Clipboard"), _T("Snapshot"), 0) != 0; }
void ClipboardManager::SetSnapshotEnabled(bool b) { AfxGetApp()->WriteProfileInt(_T("Clipboard"), _T("Snapshot"), b ? 1 : 0); }
int  ClipboardManager::SnapshotMaxFiles() const { return AfxGetApp()->GetProfileInt(_T("Clipboard"), _T("SnapFiles"), 5); }
void ClipboardManager::SetSnapshotMaxFiles(int v) { AfxGetApp()->WriteProfileInt(_T("Clipboard"), _T("SnapFiles"), v); }
int  ClipboardManager::SnapshotMaxBytesMB() const { return AfxGetApp()->GetProfileInt(_T("Clipboard"), _T("SnapMB"), 10); }
void ClipboardManager::SetSnapshotMaxBytesMB(int v) { AfxGetApp()->WriteProfileInt(_T("Clipboard"), _T("SnapMB"), v); }
