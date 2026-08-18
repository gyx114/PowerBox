// WebView2Ctrl.h: minimal WRL-based host for the Edge Chromium WebView2 control.
#pragma once
// Include WebView2.h BEFORE the WRL headers: WRL defines CINTERFACE, which would
// make WebView2's C++ COM interface classes (guard: `!defined(CINTERFACE)`)
// invisible and break the callback handler types below.
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <string>
#include <functional>

// The WebView2 NuGet targets already link WebView2Loader.dll.lib (and copy
// WebView2Loader.dll to the output), so no manual #pragma comment is needed.

// Thin wrapper over ICoreWebView2 (no WIL dependency). Creation is asynchronous:
// call Create(parent), then let OnReady fire from the controller-creation
// completion callback (the only point where the controller is guaranteed valid).
// Do NOT poll IsReady() from a timer and poke the controller — that races with
// the async teardown and can call put_Bounds on a dead controller (access
// violation 0xc0000005 / read of 0xFFFFFFFFFFFFFFFF).
class CWebView2Ctrl
{
public:
    CWebView2Ctrl() = default;
    ~CWebView2Ctrl() { Destroy(); }

    CWebView2Ctrl(const CWebView2Ctrl&) = delete;
    CWebView2Ctrl& operator=(const CWebView2Ctrl&) = delete;

    // Invoked on the UI thread once the controller is created and valid. Use this
    // (not a timer) to do the first Resize/Navigate.
    std::function<void()> OnReady;

    // Invoked on the UI thread after a top-level navigation finishes successfully.
    // Use this to send initial content (web messages) once the target page is live.
    std::function<void()> OnNavigationCompleted;

    // Invoked on the UI thread when the page posts a message to the host via
    // window.chrome.webview.postMessage(...). The argument is the raw JSON string.
    std::function<void(const std::wstring& json)> OnWebMessageReceived;

    // Start environment + controller creation bound to hParent (UI thread).
    bool Create(HWND hParent);
    void Destroy();

    bool IsReady() const { return m_ready; }                // true only after creation completes
    bool Navigate(const std::wstring& url);                 // no-op before IsReady()
    bool NavigateToString(const std::wstring& html);        // no-op before IsReady()
    bool PostWebMessageAsJson(const std::wstring& json);    // no-op before IsReady()
    bool ExecuteScript(const std::wstring& script);         // no-op before IsReady()
    void Resize(LONG x, LONG y, LONG width, LONG height);   // Bounds in parent client coords; no-op before IsReady()

private:
    HRESULT CreateController();

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller>  m_ctrl;
    Microsoft::WRL::ComPtr<ICoreWebView2>            m_webview;
    HWND m_parent = nullptr;
    bool m_ready = false;
    std::wstring m_userDataFolder;  // %TEMP%\PowerBox\WebView2 (keeps profile/cache out of the exe dir)
};