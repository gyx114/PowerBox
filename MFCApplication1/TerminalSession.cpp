// TerminalSession.cpp: ConPTY session lifecycle and I/O
#include "pch.h"
#include "framework.h"
#include "TerminalSession.h"
#include <algorithm>

CTerminalSession::CTerminalSession(HWND hNotify)
    : m_hNotify(hNotify)
{
}

CTerminalSession::~CTerminalSession()
{
    Stop();
}

bool CTerminalSession::Start(const CString& cmdLine, const CString& workDir,
    int cols, int rows)
{
    Stop();

    m_cols = std::max(20, cols);
    m_rows = std::max(3, rows);

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    if (!::CreatePipe(&m_hInputRead, &m_hInputWrite, &sa, 0))
        return false;
    if (!::CreatePipe(&m_hOutputRead, &m_hOutputWrite, &sa, 0))
    {
        Cleanup();
        return false;
    }

    ::SetHandleInformation(m_hInputWrite, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(m_hOutputRead, HANDLE_FLAG_INHERIT, 0);

    COORD size{ static_cast<SHORT>(m_cols), static_cast<SHORT>(m_rows) };
    HRESULT hr = ::CreatePseudoConsole(size, m_hInputRead, m_hOutputWrite, 0, &m_hPC);
    if (FAILED(hr))
    {
        Cleanup();
        return false;
    }

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(siEx);
    SIZE_T attrSize = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    siEx.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        ::HeapAlloc(::GetProcessHeap(), 0, attrSize));
    if (!siEx.lpAttributeList)
    {
        Cleanup();
        return false;
    }

    BOOL attrOk = ::InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrSize);
    if (!attrOk)
    {
        ::HeapFree(::GetProcessHeap(), 0, siEx.lpAttributeList);
        Cleanup();
        return false;
    }

    BOOL setOk = ::UpdateProcThreadAttribute(siEx.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, m_hPC, sizeof(m_hPC), nullptr, nullptr);

    PROCESS_INFORMATION pi{};
    BOOL created = FALSE;
    if (setOk)
    {
        siEx.StartupInfo.dwFlags = EXTENDED_STARTUPINFO_PRESENT;
        CString cmdCopy = cmdLine;
        LPTSTR pCmdLine = cmdCopy.GetBuffer(cmdCopy.GetLength() + 1);
        created = ::CreateProcessW(nullptr, pCmdLine, nullptr, nullptr, TRUE,
            EXTENDED_STARTUPINFO_PRESENT, nullptr,
            workDir.IsEmpty() ? nullptr : workDir.GetString(),
            &siEx.StartupInfo, &pi);
        cmdCopy.ReleaseBuffer();
    }

    ::DeleteProcThreadAttributeList(siEx.lpAttributeList);
    ::HeapFree(::GetProcessHeap(), 0, siEx.lpAttributeList);

    if (!created)
    {
        Cleanup();
        return false;
    }

    m_hProcess = pi.hProcess;
    m_hThreadHandle = pi.hThread;
    m_closing = false;
    m_readerThread = std::thread([this] { ReadLoop(); });
    return true;
}

void CTerminalSession::ReadLoop()
{
    char buf[8192];
    while (!m_closing)
    {
        DWORD wait = ::WaitForSingleObject(m_hProcess, 100);
        if (wait == WAIT_OBJECT_0)
        {
            // Drain remaining output even if a child process still holds the
            // pipe open, otherwise ReadFile would block forever.
            DWORD available = 0;
            while (::PeekNamedPipe(m_hOutputRead, nullptr, 0, nullptr, &available, nullptr) &&
                available > 0)
            {
                DWORD bytesRead = 0;
                DWORD toRead = __min(available, static_cast<DWORD>(sizeof(buf)));
                if (!::ReadFile(m_hOutputRead, buf, toRead, &bytesRead, nullptr) || bytesRead == 0)
                    break;

                auto* p = new std::string(buf, bytesRead);
                if (!::PostMessage(m_hNotify, WM_TERM_OUTPUT,
                    reinterpret_cast<WPARAM>(this), reinterpret_cast<LPARAM>(p)))
                    delete p;
            }
            break;
        }

        DWORD available = 0;
        if (::PeekNamedPipe(m_hOutputRead, nullptr, 0, nullptr, &available, nullptr) &&
            available > 0)
        {
            DWORD bytesRead = 0;
            DWORD toRead = __min(available, static_cast<DWORD>(sizeof(buf)));
            if (::ReadFile(m_hOutputRead, buf, toRead, &bytesRead, nullptr) && bytesRead > 0)
            {
                auto* p = new std::string(buf, bytesRead);
                if (!::PostMessage(m_hNotify, WM_TERM_OUTPUT,
                    reinterpret_cast<WPARAM>(this), reinterpret_cast<LPARAM>(p)))
                    delete p;
            }
        }
    }

    if (m_hProcess)
        ::GetExitCodeProcess(m_hProcess, &m_exitCode);

    if (!m_closing && m_hNotify)
        ::PostMessage(m_hNotify, WM_TERM_EXITED, reinterpret_cast<WPARAM>(this), 0);
}

void CTerminalSession::Stop()
{
    m_closing = true;

    if (m_hProcess)
    {
        ::TerminateProcess(m_hProcess, 1);
        ::WaitForSingleObject(m_hProcess, 3000);
    }

    if (m_readerThread.joinable())
    {
        HANDLE hNative = m_readerThread.native_handle();
        if (hNative)
            ::CancelSynchronousIo(hNative);
        m_readerThread.join();
    }

    Cleanup();
    m_closing = false;
}

void CTerminalSession::Cleanup()
{
    if (m_hPC)
    {
        ::ClosePseudoConsole(m_hPC);
        m_hPC = nullptr;
    }
    if (m_hInputWrite) { ::CloseHandle(m_hInputWrite); m_hInputWrite = nullptr; }
    if (m_hInputRead) { ::CloseHandle(m_hInputRead); m_hInputRead = nullptr; }
    if (m_hOutputWrite) { ::CloseHandle(m_hOutputWrite); m_hOutputWrite = nullptr; }
    if (m_hOutputRead) { ::CloseHandle(m_hOutputRead); m_hOutputRead = nullptr; }
    if (m_hProcess) { ::CloseHandle(m_hProcess); m_hProcess = nullptr; }
    if (m_hThreadHandle) { ::CloseHandle(m_hThreadHandle); m_hThreadHandle = nullptr; }
}

void CTerminalSession::Write(const std::string& utf8)
{
    if (!m_hPC || !m_hInputWrite || utf8.empty())
        return;

    DWORD written = 0;
    ::WriteFile(m_hInputWrite, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

void CTerminalSession::WriteString(const std::wstring& text)
{
    if (!m_hPC || !m_hInputWrite || text.empty())
        return;

    int utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(utf8Len), 0);
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        &utf8[0], utf8Len, nullptr, nullptr);
    Write(utf8);
}

void CTerminalSession::Resize(int cols, int rows)
{
    if (!m_hPC)
        return;

    cols = std::max(20, cols);
    rows = std::max(3, rows);
    if (cols == m_cols && rows == m_rows)
        return;

    m_cols = cols;
    m_rows = rows;
    ::ResizePseudoConsole(m_hPC, COORD{ static_cast<SHORT>(cols), static_cast<SHORT>(rows) });
}
