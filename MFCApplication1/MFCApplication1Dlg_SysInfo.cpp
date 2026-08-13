#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "LocalizationManager.h"
#include "resource.h"
#include <vector>
#include <utility>
#include <winternl.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

// System Information: collect and display system info in a list control

struct SysInfoEntry
{
    CString field;
    CString value;
};

static CString FormatBytes(ULONGLONG bytes)
{
    if (bytes == 0) return _T("0 B");
    const TCHAR* units[] = { _T("B"), _T("KB"), _T("MB"), _T("GB"), _T("TB") };
    int unitIdx = 0;
    double d = static_cast<double>(bytes);
    while (d >= 1024.0 && unitIdx < 4) { d /= 1024.0; ++unitIdx; }
    CString s;
    s.Format(_T("%.1f %s"), d, units[unitIdx]);
    return s;
}

static CString GetOSInfo()
{
    CString os;
    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
    // Use RtlGetVersion to get the real version (not shimmed)
    using RtlGetVersionFn = NTSTATUS(NTAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandle(_T("ntdll.dll"));
    if (hNtdll)
    {
        auto RtlGetVersion = (RtlGetVersionFn)GetProcAddress(hNtdll, "RtlGetVersion");
        if (RtlGetVersion) RtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
    }

    // Determine edition name from registry
    CString edition;
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        TCHAR buf[128] = {0};
        DWORD sz = sizeof(buf);
        if (RegQueryValueEx(hKey, _T("EditionID"), nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
            edition = buf;
        if (edition.IsEmpty())
        {
            sz = sizeof(buf);
            if (RegQueryValueEx(hKey, _T("ProductName"), nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
                edition = buf;
        }
        RegCloseKey(hKey);
    }

    // Determine build info
    CString buildInfo;
    TCHAR buildBuf[64] = {0};
    if (hNtdll && osvi.dwMajorVersion >= 10)
    {
        // Try UBR (Update Build Revision)
        HKEY hKeyUBR = nullptr;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), 0, KEY_READ, &hKeyUBR) == ERROR_SUCCESS)
        {
            DWORD ubr = 0;
            DWORD sz = sizeof(ubr);
            if (RegQueryValueEx(hKeyUBR, _T("UBR"), nullptr, nullptr, (LPBYTE)&ubr, &sz) == ERROR_SUCCESS)
                _stprintf_s(buildBuf, _T("%d.%d.%d.%d"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, ubr);
            RegCloseKey(hKeyUBR);
        }
    }
    if (buildBuf[0] == 0)
        _stprintf_s(buildBuf, _T("%d.%d.%d"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    os.Format(_T("%s (Build %s)"), edition.IsEmpty() ? _T("Windows") : edition.GetString(), buildBuf);
    return os;
}

static CString GetCPUInfo()
{
    CString cpu;
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        TCHAR buf[256] = {0};
        DWORD sz = sizeof(buf);
        if (RegQueryValueEx(hKey, _T("ProcessorNameString"), nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
            cpu = buf;
        RegCloseKey(hKey);
    }
    cpu.Trim();

    // Get core/thread counts
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD cores = 0, threads = 0;
    // Use GetLogicalProcessorInformationEx for more accurate counts
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer = nullptr;
    DWORD bufferSize = 0;
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, buffer, &bufferSize) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(bufferSize);
        if (buffer && GetLogicalProcessorInformationEx(RelationProcessorCore, buffer, &bufferSize))
        {
            BYTE* ptr = (BYTE*)buffer;
            DWORD bytesRemaining = bufferSize;
            while (bytesRemaining >= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX))
            {
                auto entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
                if (entry->Relationship == RelationProcessorCore)
                {
                    cores++;
                    // Count enabled logical processors
                    ULONG_PTR mask = entry->Processor.EfficiencyClass; // not correct
                    // Use group mask instead
                    for (WORD g = 0; g < entry->Processor.GroupCount; ++g)
                    {
                        ULONGLONG m = entry->Processor.GroupMask[g].Mask;
                        while (m) { threads += (m & 1); m >>= 1; }
                    }
                }
                ptr += entry->Size;
                bytesRemaining -= entry->Size;
            }
        }
        free(buffer);
    }
    else
    {
        // Fallback to GetSystemInfo
        threads = si.dwNumberOfProcessors;
    }

    if (threads == 0) threads = si.dwNumberOfProcessors;

    CString suffix;
    suffix.Format(_T("\r\n  %d cores / %d threads"), cores > 0 ? cores : threads, threads);
    return cpu + suffix;
}

static CString GetMemoryInfo()
{
    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);

    CString s;
    s.Format(_T("Total: %s\r\n  Available: %s (%.0f%%)\r\n  Used: %s (%.0f%%)"),
        FormatBytes(mem.ullTotalPhys).GetString(),
        FormatBytes(mem.ullAvailPhys).GetString(),
        mem.dwMemoryLoad,
        FormatBytes(mem.ullTotalPhys - mem.ullAvailPhys).GetString(),
        100.0 - mem.dwMemoryLoad);
    return s;
}

static CString GetDiskInfo()
{
    CString result;
    DWORD drives = GetLogicalDrives();
    TCHAR root[] = _T("A:\\");
    for (int i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            root[0] = _T('A') + i;
            UINT type = GetDriveType(root);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE)
            {
                ULARGE_INTEGER freeBytes, totalBytes;
                if (GetDiskFreeSpaceEx(root, nullptr, &totalBytes, &freeBytes))
                {
                    if (!result.IsEmpty()) result += _T("\r\n");
                    CString driveType = (type == DRIVE_FIXED) ? _T("") : _T(" [Removable]");
                    result.AppendFormat(_T("%s%s: %s / %s Free"),
                        root, driveType,
                        FormatBytes(freeBytes.QuadPart).GetString(),
                        FormatBytes(totalBytes.QuadPart).GetString());
                }
            }
        }
    }
    return result;
}

static CString GetUptime()
{
    ULONGLONG ms = GetTickCount64();
    ULONGLONG seconds = ms / 1000;
    ULONGLONG days = seconds / 86400;
    seconds %= 86400;
    ULONGLONG hours = seconds / 3600;
    seconds %= 3600;
    ULONGLONG minutes = seconds / 60;
    seconds %= 60;

    CString s;
    if (days > 0)
        s.Format(_T("%llu days %02llu:%02llu:%02llu"), days, hours, minutes, seconds);
    else
        s.Format(_T("%02llu:%02llu:%02llu"), hours, minutes, seconds);
    return s;
}

static CString GetComputerNameStr()
{
    TCHAR buf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD sz = _countof(buf);
    if (!::GetComputerName(buf, &sz)) buf[0] = 0;
    return buf;
}

static CString GetUserNameStr()
{
    TCHAR buf[UNLEN + 1] = {0};
    DWORD sz = _countof(buf);
    if (!::GetUserName(buf, &sz)) buf[0] = 0;
    return buf;
}

static CString GetBootTime()
{
    // Use WMI for boot time
    CString result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IWbemLocator* pLoc = nullptr;
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pLoc);
        if (SUCCEEDED(hr) && pLoc)
        {
            IWbemServices* pSvc = nullptr;
            hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr,
                nullptr, 0, nullptr, nullptr, &pSvc);
            if (SUCCEEDED(hr) && pSvc)
            {
                hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                    nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                    nullptr, EOAC_NONE);
                if (SUCCEEDED(hr))
                {
                    IEnumWbemClassObject* pEnum = nullptr;
                    hr = pSvc->ExecQuery(_bstr_t(L"WQL"),
                        _bstr_t(L"SELECT LastBootUpTime FROM Win32_OperatingSystem"),
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                        nullptr, &pEnum);
                    if (SUCCEEDED(hr) && pEnum)
                    {
                        IWbemClassObject* pObj = nullptr;
                        ULONG returned = 0;
                        hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned);
                        if (SUCCEEDED(hr) && returned && pObj)
                        {
                            VARIANT vt;
                            hr = pObj->Get(L"LastBootUpTime", 0, &vt, nullptr, nullptr);
                            if (SUCCEEDED(hr) && vt.vt == VT_BSTR)
                            {
                                CString wmiStr = vt.bstrVal;
                                // Format: YYYYMMDDHHMMSS.******+ZZZ
                                if (wmiStr.GetLength() >= 14)
                                {
                                    result.Format(_T("%c%c%c%c/%c%c/%c%c %c%c:%c%c:%c%c"),
                                        wmiStr[0], wmiStr[1], wmiStr[2], wmiStr[3],  // year
                                        wmiStr[4], wmiStr[5],  // month
                                        wmiStr[6], wmiStr[7],  // day
                                        wmiStr[8], wmiStr[9],  // hour
                                        wmiStr[10], wmiStr[11], // minute
                                        wmiStr[12], wmiStr[13]); // second
                                }
                            }
                            VariantClear(&vt);
                            pObj->Release();
                        }
                        pEnum->Release();
                    }
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
        CoUninitialize();
    }
    return result;
}

void CMFCApplication1Dlg::InitSysInfoTab()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SYSINFO));
    if (!pList) return;

    pList->ModifyStyle(0, LVS_REPORT);
    pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);

    // Distribute the two columns based on the list control's actual client
    // width (the size comes from the .rc resource). Field ~30%, value ~70%.
    CRect rcList;
    pList->GetClientRect(&rcList);
    int totalW = rcList.Width();
    if (totalW <= 0) totalW = 199; // fallback to the .rc width
    int fieldW = (int)(totalW * 0.30);
    int valueW = totalW - fieldW;
    if (valueW < 60) valueW = 60;

    pList->InsertColumn(0, loc.GetString(_T("SysInfoTab"), _T("ColField")), LVCFMT_LEFT, fieldW);
    pList->InsertColumn(1, loc.GetString(_T("SysInfoTab"), _T("ColValue")), LVCFMT_LEFT, valueW);

    // Bring system info controls to the top of the z-order so they are not
    // covered by the tab control background. Must use HWND_TOP (not wndBottom).
    const int sysInfoIds[] = {
        IDC_STATIC_QUICK_SYSINFO_SEP, IDC_STATIC_QUICK_SYSINFO_LABEL,
        IDC_LIST_SYSINFO, IDC_BTN_SYSINFO_REFRESH, IDC_BTN_SYSINFO_COPY
    };
    for (int id : sysInfoIds)
    {
        CWnd* pWnd = GetDlgItem(id);
        if (pWnd && pWnd->m_hWnd)
            pWnd->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    RefreshSysInfo();
}

void CMFCApplication1Dlg::RefreshSysInfo()
{
    auto& loc = CLocalizationManager::GetInstance();
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SYSINFO));
    if (!pList) return;

    pList->DeleteAllItems();
    pList->SetRedraw(FALSE);

    // Collect system info
    std::vector<SysInfoEntry> entries;

    auto addEntry = [&](const CString& fieldKey, const CString& value) {
        SysInfoEntry e;
        e.field = loc.GetString(_T("SysInfoTab"), fieldKey);
        e.value = value;
        entries.push_back(e);
    };

    addEntry(_T("OS"), GetOSInfo());
    addEntry(_T("CPU"), GetCPUInfo());
    addEntry(_T("Memory"), GetMemoryInfo());
    addEntry(_T("Disk"), GetDiskInfo());
    addEntry(_T("Uptime"), GetUptime());

    CString bootTime = GetBootTime();
    if (!bootTime.IsEmpty())
        addEntry(_T("BootTime"), bootTime);

    addEntry(_T("ComputerName"), GetComputerNameStr());
    addEntry(_T("UserName"), GetUserNameStr());

    // Populate list
    for (const auto& e : entries)
    {
        int idx = pList->InsertItem(pList->GetItemCount(), e.field);
        pList->SetItemText(idx, 1, e.value);
    }

    pList->SetRedraw(TRUE);
    pList->RedrawWindow(nullptr, nullptr, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW);
}

void CMFCApplication1Dlg::OnBnClickedSysinfoRefresh()
{
    RefreshSysInfo();
}

void CMFCApplication1Dlg::OnBnClickedSysinfoCopy()
{
    CListCtrl* pList = static_cast<CListCtrl*>(GetDlgItem(IDC_LIST_SYSINFO));
    if (!pList) return;

    CString text;
    int nCount = pList->GetItemCount();
    for (int i = 0; i < nCount; i++)
    {
        CString field = pList->GetItemText(i, 0);
        CString value = pList->GetItemText(i, 1);
        if (!text.IsEmpty()) text += _T("\r\n");
        text += field + _T(": ") + value;
    }

    if (text.IsEmpty()) return;

    if (OpenClipboard())
    {
        EmptyClipboard();
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (text.GetLength() + 1) * sizeof(TCHAR));
        if (hGlobal)
        {
            LPTSTR pStr = static_cast<LPTSTR>(GlobalLock(hGlobal));
            if (pStr)
            {
                _tcscpy_s(pStr, text.GetLength() + 1, text);
                GlobalUnlock(hGlobal);
#ifdef _UNICODE
                SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
                SetClipboardData(CF_TEXT, hGlobal);
#endif
            }
            else
            {
                GlobalFree(hGlobal);
            }
        }
        CloseClipboard();
    }
}