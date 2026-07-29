// AIApiClient.h: AI API client using WinHTTP
#pragma once
#include <vector>
#include <string>
#include <utility>

// Custom message posted to main window when AI response arrives
// WPARAM: 0=failure, 1=success
// LPARAM: pointer to CString (must be deleted by receiver)
#define WM_AI_RESPONSE (WM_APP + 7)

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

    // Send chat completion request asynchronously
    // messages: pairs of (role, content), e.g. ("system", "..."), ("user", "..."), ("assistant", "...")
    // vendor: vendor name matching AIVendorConfig::name
    // apiKey: API key
    // model: model name (uses vendor default if empty)
    // hwndNotify: HWND to receive WM_AI_RESPONSE on completion
    static void SendAsync(
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