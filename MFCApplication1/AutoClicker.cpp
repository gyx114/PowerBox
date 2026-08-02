// AutoClicker.cpp: Auto-clicker functionality implementation
#include "pch.h"
#include "framework.h"
#include "AutoClicker.h"
#include "LocalizationManager.h"

CAutoClicker::~CAutoClicker()
{
    Stop();
}

void CAutoClicker::Start(int intervalMs, HWND hwndOwner)
{
    if (intervalMs <= 0 || intervalMs > 10000) intervalMs = 100;
    m_intervalMs = intervalMs;
    m_hwndOwner = hwndOwner;

    if (m_bMonitorRunning.load()) return; // Already running

    m_bEnabled = true;
    m_monitorStopSource.request_stop();
    if (m_monitorThread.joinable()) m_monitorThread.join();
    m_monitorStopSource = std::stop_source{};
    m_monitorThread = std::jthread(&CAutoClicker::MonitorThreadFunc, this, m_monitorStopSource.get_token());
}

void CAutoClicker::Stop()
{
    m_bEnabled = false;
    m_bPaused = false;
    m_bClicking = false;
    m_monitorStopSource.request_stop();
    m_clickStopSource.request_stop();
    if (m_clickThread.joinable()) m_clickThread.join();
    if (m_monitorThread.joinable()) m_monitorThread.join();
    m_monitorStopSource = std::stop_source{};
    m_clickStopSource = std::stop_source{};
}

void CAutoClicker::Pause()
{
    if (m_bPaused.load()) return;
    m_bPaused = true;
    m_bClicking = false;
    m_monitorStopSource.request_stop();
    m_clickStopSource.request_stop();
    if (m_clickThread.joinable()) m_clickThread.join();
    if (m_monitorThread.joinable()) m_monitorThread.join();
    m_monitorStopSource = std::stop_source{};
    m_clickStopSource = std::stop_source{};
}

void CAutoClicker::Resume()
{
    if (!m_bPaused.load()) return;
    m_bPaused = false;
    if (!m_bEnabled.load()) return;
    m_monitorStopSource.request_stop();
    if (m_monitorThread.joinable()) m_monitorThread.join();
    m_monitorStopSource = std::stop_source{};
    m_monitorThread = std::jthread(&CAutoClicker::MonitorThreadFunc, this, m_monitorStopSource.get_token());
}

void CAutoClicker::SetInterval(int intervalMs)
{
    if (intervalMs < 10) intervalMs = 10;
    if (intervalMs > 10000) intervalMs = 10000;
    m_intervalMs = intervalMs;

    // If currently clicking, restart click thread to apply new interval
    if (m_bClicking.load())
    {
        m_clickStopSource.request_stop();
        if (m_clickThread.joinable()) m_clickThread.join();
        m_clickStopSource = std::stop_source{};
        m_clickThread = std::jthread(ClickThreadFunc, m_clickStopSource.get_token(), intervalMs);
    }
}

void CAutoClicker::SetKeys(char keyStart, char keyStop)
{
    char ks = static_cast<char>(toupper(static_cast<unsigned char>(keyStart)));
    char kp = static_cast<char>(toupper(static_cast<unsigned char>(keyStop)));
    // Guard: empty char or same key, keep default A/B
    if (ks == '\0') ks = 'A';
    if (kp == '\0') kp = 'B';
    if (ks == kp) return;
    m_keyStart = ks;
    m_keyStop = kp;
}

void CAutoClicker::ClickThreadFunc(std::stop_token stoken, int intervalMs)
{
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    while (!stoken.stop_requested())
    {
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(intervalMs);
    }
}

void CAutoClicker::MonitorThreadFunc(std::stop_token stoken)
{
    m_bMonitorRunning = true;
    while (!stoken.stop_requested())
    {
        if (!m_bClicking.load() && IsUppercaseKeyPressed(m_keyStart.load()))
        {
            m_bClicking = true;
            m_clickStopSource.request_stop();
            if (m_clickThread.joinable()) m_clickThread.join();
            m_clickStopSource = std::stop_source{};
            m_clickThread = std::jthread(ClickThreadFunc, m_clickStopSource.get_token(), m_intervalMs.load());
        }

        if (m_bClicking.load() && IsUppercaseKeyPressed(m_keyStop.load()))
        {
            m_bClicking = false;
            m_clickStopSource.request_stop();
            if (m_clickThread.joinable()) m_clickThread.join();
            if (m_hwndOwner)
                ::PostMessage(m_hwndOwner, WM_STOPPED, 0, 0);
            m_bEnabled = false;
            break;
        }

        Sleep(60);
    }

    m_bClicking = false;
    m_clickStopSource.request_stop();
    if (m_clickThread.joinable()) m_clickThread.join();
    m_bMonitorRunning = false;
}

[[nodiscard]] bool CAutoClicker::IsUppercaseKeyPressed(char ch) noexcept
{
    SHORT state = GetAsyncKeyState(toupper(ch));
    if (!(state & 0x8000)) return false;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    return (caps && !shift) || (!caps && shift);
}

int CAutoClicker::PromptForIntervalMs(HWND hwndParent)
{
    TCHAR tmpPath[MAX_PATH];
    if (!GetTempPath(MAX_PATH, tmpPath)) wcscpy_s(tmpPath, MAX_PATH, L"C:\\Windows\\Temp\\");
    CString tmpFile;
    tmpFile.Format(_T("%sautoclick_interval_%u.txt"), tmpPath, GetTickCount());

    CString tmpVbs;
    tmpVbs.Format(_T("%sautoclick_input_%u.vbs"), tmpPath, GetTickCount());

    CString vbs;
    vbs += _T("On Error Resume Next\r\n");
    {
        auto& loc = CLocalizationManager::GetInstance();
        CString prompt = loc.GetString(_T("AutoClicker"), _T("VbsPrompt"));
        CString title = loc.GetString(_T("AutoClicker"), _T("VbsTitle"));
        vbs += _T("s = InputBox(\"");
        vbs += prompt;
        vbs += _T("\", \"");
        vbs += title;
        vbs += _T("\", \"100\")\r\n");
    }
    vbs += _T("If Not IsNull(s) And s <> \"\" Then\r\n");
    {
        CString part;
        part.Format(_T("  Set f = CreateObject(\"Scripting.FileSystemObject\").OpenTextFile(\"%s\", 2, True)\r\n"), tmpFile);
        vbs += part;
    }
    vbs += _T("  f.WriteLine s\r\n");
    vbs += _T("  f.Close\r\n");
    vbs += _T("End If\r\n");

    {
        std::wstring wv = vbs.GetString();
        HANDLE h = CreateFileW(tmpVbs, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            BYTE bom[2] = { 0xFF, 0xFE };
            WriteFile(h, bom, 2, &written, nullptr);
            WriteFile(h, wv.c_str(), static_cast<DWORD>(wv.size() * sizeof(wchar_t)), &written, nullptr);
            CloseHandle(h);
        }
    }

    SHELLEXECUTEINFO sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = _T("open");
    sei.lpFile = _T("wscript.exe");
    sei.lpParameters = tmpVbs;
    sei.nShow = SW_HIDE;
    bool ok = (ShellExecuteEx(&sei) != FALSE);

    if (ok && sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, 30000);
        CloseHandle(sei.hProcess);
    }

    int val = 100;
    {
        HANDLE hFile = CreateFileW(tmpFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            char buf[64]{};
            DWORD read = 0;
            if (ReadFile(hFile, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
            {
                val = atoi(buf);
            }
            CloseHandle(hFile);
        }
    }

    DeleteFileW(tmpVbs);
    DeleteFileW(tmpFile);
    return val;
}