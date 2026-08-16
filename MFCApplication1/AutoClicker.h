// AutoClicker.h: Auto-clicker functionality encapsulation
#pragma once
#include <stop_token>
#include <thread>
#include <atomic>

// Auto-clicker class: press uppercase A to start clicking, uppercase B to stop
// Uses std::jthread and std::stop_source for cooperative thread cancellation
class CAutoClicker
{
public:
    CAutoClicker() = default;
    ~CAutoClicker();

    // Disallow copy
    CAutoClicker(const CAutoClicker&) = delete;
    CAutoClicker& operator=(const CAutoClicker&) = delete;

    // Start auto-clicker (intervalMs: click interval in ms, hwndOwner: owner window for notification)
    void Start(int intervalMs, HWND hwndOwner);

    // Stop auto-clicker
    void Stop();

    // Pause keyboard monitoring (does not change m_bEnabled state; for OCR screenshot etc.)
    void Pause();

    // Resume keyboard monitoring (only effective when m_bEnabled is true)
    void Resume();

    // Dynamically adjust click interval (ms) without stopping the auto-clicker
    void SetInterval(int intervalMs);

    // Set trigger keys (default: A=start, B=stop)
    void SetKeys(char keyStart, char keyStop);

    // Whether currently running
    [[nodiscard]] bool IsRunning() const { return m_bEnabled.load(); }
    [[nodiscard]] bool IsClicking() const { return m_bClicking.load(); }

    // Get current trigger keys
    [[nodiscard]] char GetKeyStart() const { return m_keyStart.load(); }
    [[nodiscard]] char GetKeyStop() const { return m_keyStop.load(); }

    // Custom message: auto-clicking stopped, wParam and lParam are both 0
    static constexpr UINT WM_STOPPED = WM_APP + 4;

private:
    // Thread functions
    void ClickThreadFunc(std::stop_token stoken);
    void MonitorThreadFunc(std::stop_token stoken);

    // Detect uppercase key press
    [[nodiscard]] static bool IsUppercaseKeyPressed(char ch) noexcept;

    // Prompt for interval via input dialog
    static int PromptForIntervalMs(HWND hwndParent);

    std::atomic<bool> m_bEnabled{false};
    std::atomic<bool> m_bClicking{false};
    std::atomic<bool> m_bMonitorRunning{false};
    std::atomic<bool> m_bPaused{false};
    std::atomic<int> m_intervalMs{100};
    std::atomic<char> m_keyStart{'A'};
    std::atomic<char> m_keyStop{'B'};

    std::jthread m_clickThread;
    std::jthread m_monitorThread;
    std::stop_source m_clickStopSource;
    std::stop_source m_monitorStopSource;

    HWND m_hwndOwner{nullptr};
};