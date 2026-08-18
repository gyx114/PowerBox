// WebView2Ctrl.cpp: implementation of the minimal WRL WebView2 host.
#include "pch.h"
#include "WebView2Ctrl.h"
#include <windows.h>

using namespace Microsoft::WRL;

bool CWebView2Ctrl::Create(HWND hParent)
{
    Destroy();
    m_parent = hParent;

    // Put WebView2's runtime profile/cache under %TEMP% so it doesn't create a
    // big "<exe>.WebView2" folder next to the exe (portable-friendly). Passing
    // nullptr for userDataFolder would make WebView2 create it right beside the exe.
    std::wstring base;
    wchar_t tmpDir[MAX_PATH] = {};
    DWORD len = ::GetTempPathW(MAX_PATH, tmpDir);
    if (len > 0 && len < MAX_PATH)
        base.assign(tmpDir, len);
    else
        base = L"C:\\Temp\\";   // fallback if %TEMP% is unavailable
    m_userDataFolder = base + L"PowerBox\\WebView2";
    ::CreateDirectoryW(m_userDataFolder.c_str(), nullptr);  // no-op if already exists

    HRESULT hr = ::CreateCoreWebView2EnvironmentWithOptions(
        nullptr, m_userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result) || !env) return result;
                // AddRef via ComPtr::operator= — Attach() would NOT take a strong
                // reference, and WebView2 releases its reference after this
                // callback returns, leaving m_env dangling.
                m_env = env;
                return CreateController();
            }).Get());
    return SUCCEEDED(hr);
}

HRESULT CWebView2Ctrl::CreateController()
{
    return m_env->CreateCoreWebView2Controller(
        m_parent,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT
            {
                if (FAILED(result) || !ctrl) return result;
                // AddRef via ComPtr::operator= (NOT Attach): WebView2 releases its
                // reference once this callback returns, so we must take our own
                // strong reference or the controller is freed and later put_Bounds
                // crashes (virtual call on a dead object → read of 0xFFFFFFFFFFFFFFFF).
                m_ctrl = ctrl;
                if (FAILED(m_ctrl->get_CoreWebView2(&m_webview)))
                    return result;
                // Notify on top-level navigation completion (e.g. to push the
                // initial document content via web message once the page is live).
                EventRegistrationToken navToken;
                m_webview->add_NavigationCompleted(
                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
                        {
                            BOOL ok = FALSE;
                            if (args && SUCCEEDED(args->get_IsSuccess(&ok)) && ok && OnNavigationCompleted)
                                OnNavigationCompleted();
                            return S_OK;
                        }).Get(), &navToken);

                // Receive messages the page posts via window.chrome.webview.postMessage.
                EventRegistrationToken msgToken;
                m_webview->add_WebMessageReceived(
                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                        {
                            LPWSTR json = nullptr;
                            if (args && SUCCEEDED(args->get_WebMessageAsJson(&json)) && json)
                            {
                                if (OnWebMessageReceived)
                                    OnWebMessageReceived(json);
                                ::CoTaskMemFree(json);
                            }
                            return S_OK;
                        }).Get(), &msgToken);
                m_ctrl->put_IsVisible(TRUE);
                m_ready = true;
                if (OnReady) OnReady();   // UI thread; controller is valid here
                return S_OK;
            }).Get());
}

void CWebView2Ctrl::Destroy()
{
    m_ready = false;
    if (m_webview) m_webview.Reset();
    if (m_ctrl) { m_ctrl->Close(); m_ctrl.Reset(); }
    m_env.Reset();
    m_parent = nullptr;
}

bool CWebView2Ctrl::Navigate(const std::wstring& url)
{
    if (!IsReady()) return false;
    return SUCCEEDED(m_webview->Navigate(url.c_str()));
}

bool CWebView2Ctrl::NavigateToString(const std::wstring& html)
{
    if (!IsReady()) return false;
    return SUCCEEDED(m_webview->NavigateToString(html.c_str()));
}

bool CWebView2Ctrl::PostWebMessageAsJson(const std::wstring& json)
{
    if (!IsReady() || !m_webview) return false;
    return SUCCEEDED(m_webview->PostWebMessageAsJson(json.c_str()));
}

bool CWebView2Ctrl::ExecuteScript(const std::wstring& script)
{
    if (!IsReady() || !m_webview) return false;
    return SUCCEEDED(m_webview->ExecuteScript(script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get()));
}

void CWebView2Ctrl::Resize(LONG x, LONG y, LONG width, LONG height)
{
    if (!IsReady() || !m_ctrl) return;
    m_ctrl->put_Bounds(RECT{ x, y, width, height });
}