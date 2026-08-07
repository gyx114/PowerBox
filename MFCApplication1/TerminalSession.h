// TerminalSession.h: reusable ConPTY-backed command session
#pragma once

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>

constexpr UINT WM_TERM_OUTPUT = WM_APP + 30;
constexpr UINT WM_TERM_EXITED = WM_APP + 31;

class CTerminalSession
{
public:
    explicit CTerminalSession(HWND hNotify);
    ~CTerminalSession();

    bool Start(const CString& cmdLine, const CString& workDir, int cols, int rows);
    void Stop();
    void Write(const std::string& utf8);
    void WriteString(const std::wstring& text);
    void Resize(int cols, int rows);
    void SetNotifyWindow(HWND hwnd) { m_hNotify = hwnd; }

    bool IsRunning() const { return m_hPC != nullptr; }
    DWORD ExitCode() const { return m_exitCode; }

private:
    void ReadLoop();
    void Cleanup();

    HWND m_hNotify = nullptr;
    HPCON m_hPC = nullptr;
    HANDLE m_hInputWrite = nullptr;
    HANDLE m_hInputRead = nullptr;
    HANDLE m_hOutputWrite = nullptr;
    HANDLE m_hOutputRead = nullptr;
    HANDLE m_hProcess = nullptr;
    HANDLE m_hThreadHandle = nullptr;
    std::thread m_readerThread;
    std::atomic<bool> m_closing{false};
    DWORD m_exitCode = 0;
    int m_cols = 80;
    int m_rows = 24;
};
