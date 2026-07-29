// AIApiClient.h: AI API client using WinHTTP
#pragma once
#include <vector>
#include <string>
#include <utility>

// Custom message posted to main window when AI response arrives
// WPARAM: 0=failure, 1=success
// LPARAM: pointer to CString (must be deleted by receiver)
#define WM_AI_RESPONSE (WM_APP + 7)

// Streaming messages: content chunks during streaming
// WM_AI_STREAM_CHUNK: LPARAM = CString* (delta content, must be deleted by receiver)
#define WM_AI_STREAM_CHUNK (WM_APP + 8)
// WM_AI_STREAM_DONE: WPARAM = 1=success 0=error, LPARAM = CString* (error message or nullptr, deleted by receiver)
#define WM_AI_STREAM_DONE  (WM_APP + 9)

struct AIVendorConfig
{
    CString name;
    CString endpoint;
    CString defaultModel;
};

class CAIApiClient
{
public:
    // Get available vendor configurations
    static const std::vector<AIVendorConfig>& GetVendors();

    // Send chat completion request asynchronously (non-streaming)
    static void SendAsync(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& vendor,
        const CString& apiKey,
        const CString& model,
        HWND hwndNotify);

    // Send chat completion request with SSE streaming
    // Posts WM_AI_STREAM_CHUNK for each content delta, WM_AI_STREAM_DONE when complete
    static void SendAsyncStreaming(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& vendor,
        const CString& apiKey,
        const CString& model,
        HWND hwndNotify);

private:
    static CString BuildRequestBody(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& model);
    static CString SendHttpRequest(
        const CString& server, int port, const CString& path,
        const CString& apiKey, const CString& body);
    static CString ExtractContent(const CString& json);
    static bool ParseUrl(const CString& url, CString& server, int& port, CString& path);
};