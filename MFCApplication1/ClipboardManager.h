// ClipboardManager.h: Clipboard history core (capture / storage / persistence / replay)
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <thread>

// Display priority of an entry: Image > Files > Text
enum class ClipType { Text, Files, Image, Mixed };

struct ClipboardEntry {
    std::uint64_t id = 0;
    std::uint64_t timestamp = 0;     // seconds since epoch
    ClipType type = ClipType::Text;  // display primary type: Image > Files > Text
    std::wstring text;               // may be empty
    std::vector<std::wstring> files; // CF_HDROP source paths, may be empty
    std::wstring imagePath;          // on-disk PNG absolute path, may be empty
    std::wstring thumbPath;          // on-disk thumbnail absolute path, may be empty
    std::wstring dir;                // per-entry sub-directory
    bool pinned = false;
    std::uint64_t imageFp = 0;       // hash of the DIB image contents (image dedup)
    std::map<std::wstring, std::wstring> snapshotMap; // source path -> archived leaf name in dir
};

// Classify an entry's display type from its actual payload (text / files / image),
// returning Mixed when more than one non-empty kind is present. This is the single
// source of truth used by both the main tab3 list and the enhanced window.
inline ClipType ClassifyClipType(const ClipboardEntry& e)
{
    const int kinds = (!e.text.empty() ? 1 : 0) +
                      (!e.files.empty() ? 1 : 0) +
                      (!e.imagePath.empty() ? 1 : 0);
    if (kinds >= 2) return ClipType::Mixed;
    if (!e.imagePath.empty()) return ClipType::Image;
    if (!e.files.empty())     return ClipType::Files;
    return ClipType::Text;   // text or empty payload
}

class ClipboardManager {
public:
    ClipboardManager();
    ~ClipboardManager();

    void Capture();                 // called on WM_CLIPBOARDUPDATE (async internally)
    void Shutdown();                // stop the capture thread (used during main-window teardown)
    bool Replay(std::uint64_t id);  // re-copy full content to clipboard
    void Remove(std::uint64_t id);
    void Clear();
    void TogglePin(std::uint64_t id);

    const std::deque<ClipboardEntry>& Entries() const;
    std::vector<ClipboardEntry> Snapshot() const;            // thread-safe copy for UI
    std::vector<std::wstring> RecentTextItems(int n) const;  // for tab3

    void SetChangeCallback(std::function<void()> cb);
    void LoadIndex();

    // Config (registry persistence via MFC AfxGetApp profile)
    int  MaxEntries() const;          void SetMaxEntries(int v);
    bool SnapshotEnabled() const;     void SetSnapshotEnabled(bool v);
    int  SnapshotMaxFiles() const;    void SetSnapshotMaxFiles(int v);
    int  SnapshotMaxBytesMB() const;  void SetSnapshotMaxBytesMB(int v);

private:
    void CaptureWorker();
    void CaptureLoop();
    bool BuildEntry(ClipboardEntry& e);
    void NotifyChange();
    static std::wstring ClipboardStoreDir();
    std::wstring MakeEntryDir(std::uint64_t id);
    void SaveIndex();
    void TrimIfNeeded();

    mutable std::mutex m_mutex;
    std::deque<ClipboardEntry> m_entries;
    std::function<void()> m_onChange;
    std::mutex m_callbackMutex;
    std::uint64_t m_nextId = 1;

    // Single serialized capture worker: WM_CLIPBOARDUPDATE can fire repeatedly in
    // quick succession (each set/empty may produce a notification). Using one
    // persistent thread with a pending flag avoids spawning an unbounded number of
    // detached threads that all hammer GDI+ and the system clipboard concurrently.
    std::thread                 m_captureThread;
    std::mutex                  m_captureMutex;
    std::condition_variable     m_captureCv;
    bool                        m_capturePending = false;
    bool                        m_captureStop = false;
};
