// Utils.h: Common utility function declarations
#pragma once

// Format Windows error code to readable string
[[nodiscard]] CString FormatLastError(DWORD err);

// Copy text to clipboard
void CopyToClipboard(HWND hwnd, const CString& text);

// Check if current process is running with administrator privileges
[[nodiscard]] bool IsProcessElevated();

// Prompt user to restart with admin privileges, returns true if restart initiated
[[nodiscard]] bool PromptRestartElevated();

// Get saved path from ini, or prompt user to select and save if invalid
[[nodiscard]] CString GetOrAskPath(CWnd* pParent, LPCTSTR pszKey, LPCTSTR pszTitle, bool bFolder = false);

// Parse total seconds from three edit boxes (hours, minutes, seconds)
[[nodiscard]] int ParseShutdownSeconds(CDialogEx* pDlg);

// Allow UIPI messages (for cross-privilege window communication)
void AllowUIPIMessage(HWND hwnd, UINT msg, BOOL allow);

// Launch process as current desktop user (non-admin)
bool LaunchProcessAsShellUser(LPCTSTR exe, LPCTSTR params, CString* pError = nullptr);

// Check if window handle is valid (non-null and IsWindow is true)
[[nodiscard]] inline bool IsValidWindow(HWND hWnd) noexcept
{
    return hWnd != nullptr && ::IsWindow(hWnd);
}

// Force-release stuck modifier keys (Alt/Ctrl/Shift) by injecting a complete
// key-down + key-up cycle. Only releases keys that are actually stuck, to avoid
// triggering side effects (e.g. Alt opening the menu bar).
void ForceReleaseModifierKeys();

// Find a child window by its current text and set new text (for IDC_STATIC labels)
void SetChildTextByCurrentText(CWnd* pParent, LPCTSTR oldText, LPCTSTR newText);