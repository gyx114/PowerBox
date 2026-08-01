// AIApiClient.h: AI API client using WinHTTP
// Refactored with RAII handles, SSE parser, exception classification, cancel support
#pragma once
#include <vector>
#include <string>
#include <utility>
#include <atomic>
#include <winhttp.h>

// Custom messages posted to main window
#define WM_AI_RESPONSE        (WM_APP + 7)   // WPARAM: 0/1, LPARAM: CString*
#define WM_AI_STREAM_CHUNK    (WM_APP + 8)   // LPARAM: CString* (delta)
#define WM_AI_STREAM_DONE     (WM_APP + 9)   // WPARAM: 0/1, LPARAM: CString*
#define WM_AI_EXECUTE_COMMAND (WM_APP + 15)  // LPARAM: CString* (JSON command)

// ============================================================================
// RAII WinHTTP handle wrapper
// ============================================================================
class WinHttpHandle
{
public:
    WinHttpHandle() : m_h(nullptr) {}
    explicit WinHttpHandle(HINTERNET h) : m_h(h) {}
    ~WinHttpHandle() { Close(); }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle(WinHttpHandle&& other) noexcept : m_h(other.m_h) { other.m_h = nullptr; }
    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept
    {
        if (this != std::addressof(other)) { Close(); m_h = other.m_h; other.m_h = nullptr; }
        return *this;
    }

    operator HINTERNET() const { return m_h; }
    HINTERNET* operator&() { Close(); return &m_h; }
    bool IsValid() const { return m_h != nullptr; }
    void Close() { if (m_h) { WinHttpCloseHandle(m_h); m_h = nullptr; } }
    HINTERNET Detach() { HINTERNET h = m_h; m_h = nullptr; return h; }

private:
    HINTERNET m_h;
};

// ============================================================================
// Exception types for categorized error handling
// ============================================================================
struct AiNetworkError : std::runtime_error
{
    DWORD errorCode;
    AiNetworkError(const char* msg, DWORD code = 0)
        : std::runtime_error(msg), errorCode(code) {}
};

struct AiApiKeyError : std::runtime_error
{
    AiApiKeyError(const char* msg) : std::runtime_error(msg) {}
};

struct AiJsonError : std::runtime_error
{
    AiJsonError(const char* msg) : std::runtime_error(msg) {}
};

// ============================================================================
// SSE (Server-Sent Events) stream parser
// ============================================================================
class SseParser
{
public:
    // Reset parser state for a new stream
    void Reset();

    // Feed raw bytes; returns list of complete "data: ..." payloads extracted
    // Delimiter: "\n\n" (double newline) between SSE events
    std::vector<std::string> Feed(const char* data, size_t len);

private:
    std::string m_buffer;
};

// ============================================================================
// Vendor configuration
// ============================================================================
struct AIVendorConfig
{
    CString name;
    CString endpoint;
    CString defaultModel;
};

// ============================================================================
// Main AI API client (all static methods)
// ============================================================================
class CAIApiClient
{
public:
    // Global thread pool for AI requests (created on first use, destroyed at exit)
    static class CThreadPool& GetThreadPool();
    static void DestroyThreadPool();

    // Cancel the currently running AI request
    static void Cancel();

    // Get available vendor configurations
    static const std::vector<AIVendorConfig>& GetVendors();

    // Send chat completion request (non-streaming). Posts WM_AI_RESPONSE.
    static void SendAsync(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& vendor,
        const CString& apiKey,
        const CString& model,
        HWND hwndNotify);

    // Send chat completion request with SSE streaming.
    // Posts WM_AI_STREAM_CHUNK for each delta, WM_AI_STREAM_DONE on completion.
    static void SendAsyncStreaming(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& vendor,
        const CString& apiKey,
        const CString& model,
        HWND hwndNotify);

private:
    // Build JSON request body
    static CString BuildRequestBody(
        const std::vector<std::pair<CString, CString>>& messages,
        const CString& model,
        bool bStream);

    // Unified HTTP request logic (used by both streaming/non-streaming)
    // Returns response body on success; throws AiNetworkError/AiApiKeyError on failure
    static std::string SendRequestInternal(
        const CString& server, int port, const CString& path,
        const CString& apiKey, const std::string& bodyUtf8);

    // Read response body as SSE stream, posting WM_AI_STREAM_CHUNK per delta
    static std::string ReadSseStream(HINTERNET hRequest, HWND hwndNotify);

    // Extract content from JSON response; throws AiJsonError on parse failure
    static CString ExtractContent(const CString& json);

    // Parse URL into server, port, path
    static bool ParseUrl(const CString& url, CString& server, int& port, CString& path);

    // Resolve vendor endpoint/model
    static bool ResolveVendor(const CString& vendor, const CString& model,
        CString& outEndpoint, CString& outModel);

    // Cancellation flag shared across all requests
    static std::atomic<bool> s_bCancel;
};