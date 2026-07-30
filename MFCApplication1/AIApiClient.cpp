// AIApiClient.cpp: AI API client implementation
// Refactored: RAII handles, SSE parser, unified HTTP logic, exception classification,
// cancel support, thread pool integration
#include "pch.h"
#include "AIApiClient.h"
#include "ThreadPool.h"
#include "json.hpp"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

// ============================================================================
// SSE Parser implementation
// ============================================================================

void SseParser::Reset()
{
    m_buffer.clear();
}

std::vector<std::string> SseParser::Feed(const char* data, size_t len)
{
    m_buffer.append(data, len);
    std::vector<std::string> results;

    while (true)
    {
        size_t pos = m_buffer.find("\n\n");
        if (pos == std::string::npos) break;

        std::string chunk = m_buffer.substr(0, pos);
        m_buffer.erase(0, pos + 2); // skip "\n\n"

        // Parse "data: ..." lines from the chunk
        size_t lineStart = 0;
        while (lineStart < chunk.size())
        {
            // Skip leading \r\n
            while (lineStart < chunk.size() &&
                (chunk[lineStart] == '\r' || chunk[lineStart] == '\n'))
                lineStart++;

            size_t lineEnd = chunk.find('\n', lineStart);
            if (lineEnd == std::string::npos)
                lineEnd = chunk.size();

            std::string line = chunk.substr(lineStart, lineEnd - lineStart);
            // Trim trailing \r
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.size() > 6 && line.substr(0, 6) == "data: ")
            {
                std::string payload = line.substr(6);
                if (payload != "[DONE]")
                    results.push_back(std::move(payload));
            }

            lineStart = lineEnd + 1;
        }
    }

    return results;
}

// ============================================================================
// Static members
// ============================================================================

std::atomic<bool> CAIApiClient::s_bCancel{false};

// ============================================================================
// Thread pool (lazy singleton)
// ============================================================================

static std::unique_ptr<CThreadPool> s_pThreadPool;
static std::mutex s_threadPoolMutex;

CThreadPool& CAIApiClient::GetThreadPool()
{
    if (!s_pThreadPool)
    {
        std::lock_guard<std::mutex> lock(s_threadPoolMutex);
        if (!s_pThreadPool)
            s_pThreadPool = std::make_unique<CThreadPool>(4);
    }
    return *s_pThreadPool;
}

void CAIApiClient::DestroyThreadPool()
{
    std::lock_guard<std::mutex> lock(s_threadPoolMutex);
    if (s_pThreadPool)
    {
        s_pThreadPool->Join();
        s_pThreadPool.reset();
    }
}

// ============================================================================
// Cancel
// ============================================================================

void CAIApiClient::Cancel()
{
    s_bCancel.store(true);
}

// ============================================================================
// Vendors
// ============================================================================

const std::vector<AIVendorConfig>& CAIApiClient::GetVendors()
{
    static const std::vector<AIVendorConfig> vendors = {
        { _T("OpenAI"),    _T("https://api.openai.com/v1/chat/completions"),                   _T("gpt-3.5-turbo") },
        { _T("DeepSeek"),  _T("https://api.deepseek.com/v1/chat/completions"),                 _T("deepseek-v4-flash") },
        { _T("通义千问"),   _T("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"), _T("qwen-turbo") },
        { _T("智谱AI"),    _T("https://open.bigmodel.cn/api/paas/v4/chat/completions"),       _T("glm-4-flash") },
        { _T("Moonshot"),  _T("https://api.moonshot.cn/v1/chat/completions"),                 _T("moonshot-v1-8k") },
        { _T("硅基流动"),   _T("https://api.siliconflow.cn/v1/chat/completions"),               _T("deepseek-ai/DeepSeek-V3") },
    };
    return vendors;
}

// ============================================================================
// Vendor resolution
// ============================================================================

bool CAIApiClient::ResolveVendor(const CString& vendor, const CString& model,
    CString& outEndpoint, CString& outModel)
{
    for (const auto& v : GetVendors())
    {
        if (v.name.CompareNoCase(vendor) == 0)
        {
            outEndpoint = v.endpoint;
            outModel = model.IsEmpty() ? v.defaultModel : model;
            return true;
        }
    }
    return false;
}

// ============================================================================
// URL parsing
// ============================================================================

bool CAIApiClient::ParseUrl(const CString& url, CString& server, int& port, CString& path)
{
    CString s = url;

    if (s.Left(8).CompareNoCase(_T("https://")) == 0)
    {
        port = 443;
        s = s.Mid(8);
    }
    else if (s.Left(7).CompareNoCase(_T("http://")) == 0)
    {
        port = 80;
        s = s.Mid(7);
    }
    else
    {
        return false;
    }

    int slash = s.Find(_T('/'));
    if (slash != -1)
    {
        server = s.Left(slash);
        path = s.Mid(slash);
    }
    else
    {
        server = s;
        path = _T("/");
    }

    int colon = server.Find(_T(':'));
    if (colon != -1)
    {
        CString portStr = server.Mid(colon + 1);
        server = server.Left(colon);
        port = _ttoi(portStr);
    }

    return true;
}

// ============================================================================
// Build request body
// ============================================================================

CString CAIApiClient::BuildRequestBody(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& model,
    bool bStream)
{
    json jMessages = json::array();
    for (const auto& msg : messages)
    {
        std::string role = (LPCSTR)CT2A(msg.first, CP_UTF8);
        std::string content = (LPCSTR)CT2A(msg.second, CP_UTF8);
        jMessages.push_back({ {"role", role}, {"content", content} });
    }

    json jBody;
    jBody["model"] = (LPCSTR)CT2A(model, CP_UTF8);
    jBody["messages"] = jMessages;
    jBody["stream"] = bStream;

    std::string bodyStr = jBody.dump();
    return CString(CA2T(bodyStr.c_str(), CP_UTF8));
}

// ============================================================================
// Unified HTTP request (RAII handles, no manual CloseHandle)
// ============================================================================

std::string CAIApiClient::SendRequestInternal(
    const CString& server, int port, const CString& path,
    const CString& apiKey, const std::string& bodyUtf8)
{
    bool bSecure = (port == 443);

    WinHttpHandle hSession(WinHttpOpen(
        L"MFCApplication1/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession.IsValid())
        throw AiNetworkError("WinHttpOpen failed", GetLastError());

    WinHttpHandle hConnect(WinHttpConnect(hSession, server, (INTERNET_PORT)port, 0));
    if (!hConnect.IsValid())
        throw AiNetworkError("WinHttpConnect failed", GetLastError());

    DWORD flags = bSecure ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect, L"POST", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest.IsValid())
        throw AiNetworkError("WinHttpOpenRequest failed", GetLastError());

    // Set headers
    CString headers;
    headers.Format(L"Content-Type: application/json\r\nAuthorization: Bearer %s\r\n",
        apiKey.GetString());
    if (!WinHttpAddRequestHeaders(hRequest, headers, -1, WINHTTP_ADDREQ_FLAG_ADD))
        throw AiNetworkError("WinHttpAddRequestHeaders failed", GetLastError());

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(), (DWORD)bodyUtf8.size(), 0))
        throw AiNetworkError("WinHttpSendRequest failed", GetLastError());

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr))
        throw AiNetworkError("WinHttpReceiveResponse failed", GetLastError());

    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode == 401 || statusCode == 403)
        throw AiApiKeyError("Invalid or expired API key");
    if (statusCode != 200)
    {
        char errBuf[128];
        sprintf_s(errBuf, "HTTP %d", (int)statusCode);
        throw AiNetworkError(errBuf, statusCode);
    }

    // Read response body
    std::string response;
    DWORD dwSize = 0;
    do
    {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0)
            break;

        std::vector<char> buf(dwSize + 1);
        DWORD dwDownloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded))
            break;

        buf[dwDownloaded] = '\0';
        response.append(buf.data(), dwDownloaded);
    } while (dwSize > 0);

    // All handles auto-closed by RAII on scope exit
    return response;
}

// ============================================================================
// SSE stream reader (uses SseParser, posts WM_AI_STREAM_CHUNK)
// ============================================================================

std::string CAIApiClient::ReadSseStream(HINTERNET hRequest, HWND hwndNotify)
{
    SseParser parser;
    std::string accumulatedContent;

    char readBuf[4096];
    DWORD dwSize = 0;

    while (!s_bCancel.load())
    {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0)
            break;

        DWORD toRead = std::min<DWORD>(dwSize, (DWORD)sizeof(readBuf) - 1);
        DWORD dwDownloaded = 0;
        if (!WinHttpReadData(hRequest, readBuf, toRead, &dwDownloaded) || dwDownloaded == 0)
            break;

        // Feed raw bytes to SSE parser
        auto payloads = parser.Feed(readBuf, dwDownloaded);

        for (const auto& payload : payloads)
        {
            try
            {
                json j = json::parse(payload);
                if (j.contains("choices") && j["choices"].is_array() &&
                    !j["choices"].empty())
                {
                    auto& choice = j["choices"][0];
                    if (choice.contains("delta") && choice["delta"].contains("content"))
                    {
                        std::string content = choice["delta"]["content"].get<std::string>();
                        accumulatedContent += content;
                        CString* pChunk = new CString(CA2T(content.c_str(), CP_UTF8));
                        ::PostMessage(hwndNotify, WM_AI_STREAM_CHUNK, 0, (LPARAM)pChunk);
                    }
                }
            }
            catch (...) {}
        }
    }

    return accumulatedContent;
}

// ============================================================================
// Extract content from JSON response
// ============================================================================

CString CAIApiClient::ExtractContent(const CString& jsonStr)
{
    try
    {
        std::string s = (LPCSTR)CT2A(jsonStr, CP_UTF8);
        json j = json::parse(s);

        if (j.contains("error"))
        {
            std::string errMsg = j["error"]["message"].get<std::string>();
            CString err;
            err.Format(_T("[API Error] %hs"), errMsg.c_str());
            return err;
        }

        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty())
        {
            auto& choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content"))
            {
                std::string content = choice["message"]["content"].get<std::string>();
                return CString(CA2T(content.c_str(), CP_UTF8));
            }
        }

        return _T("[Error] Unexpected response format");
    }
    catch (const json::parse_error& e)
    {
        CString err;
        err.Format(_T("[JSON Parse Error] %hs"), e.what());
        return err;
    }
    catch (const std::exception& e)
    {
        CString err;
        err.Format(_T("[Error] %hs"), e.what());
        return err;
    }
}

// ============================================================================
// SendAsync (non-streaming) — uses thread pool
// ============================================================================

void CAIApiClient::SendAsync(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& vendor,
    const CString& apiKey,
    const CString& model,
    HWND hwndNotify)
{
    // Cancel any pending request and reset flag
    s_bCancel.store(true);
    s_bCancel.store(false);

    CString endpoint, actualModel;
    if (!ResolveVendor(vendor, model, endpoint, actualModel))
    {
        CString* pResult = new CString(_T("[Error] Unknown vendor: ") + vendor);
        ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
        return;
    }

    GetThreadPool().Enqueue([messages, endpoint, apiKey, model = actualModel, hwndNotify]()
    {
        try
        {
            CString server, path;
            int port = 443;
            if (!ParseUrl(endpoint, server, port, path))
            {
                CString* pResult = new CString(_T("[Error] Invalid endpoint URL"));
                ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
                return;
            }

            CString body = BuildRequestBody(messages, model, false);
            std::string bodyUtf8 = (LPCSTR)CW2A(body, CP_UTF8);
            std::string response = SendRequestInternal(server, port, path, apiKey, bodyUtf8);

            if (response.empty())
            {
                CString* pResult = new CString(
                    _T("[Error] Empty response from server. Check your network or API key."));
                ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
                return;
            }

            CString content = ExtractContent(CString(CA2T(response.c_str(), CP_UTF8)));
            bool bSuccess = (content.Find(_T("[Error]")) < 0 &&
                content.Find(_T("[API Error]")) < 0);
            CString* pResult = new CString(content);
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, bSuccess ? 1 : 0, (LPARAM)pResult);
        }
        catch (const AiApiKeyError& e)
        {
            CString err;
            err.Format(_T("[API Key Error] %hs"), e.what());
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)new CString(err));
        }
        catch (const AiNetworkError& e)
        {
            CString err;
            err.Format(_T("[Network Error] %hs (code: %d)"), e.what(), (int)e.errorCode);
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)new CString(err));
        }
        catch (const std::exception& e)
        {
            CString err;
            err.Format(_T("[Error] %hs"), e.what());
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)new CString(err));
        }
    });
}

// ============================================================================
// SendAsyncStreaming — uses thread pool + SSE parser
// ============================================================================

void CAIApiClient::SendAsyncStreaming(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& vendor,
    const CString& apiKey,
    const CString& model,
    HWND hwndNotify)
{
    // Cancel any pending request and reset flag
    s_bCancel.store(true);
    s_bCancel.store(false);

    CString endpoint, actualModel;
    if (!ResolveVendor(vendor, model, endpoint, actualModel))
    {
        CString* pResult = new CString(_T("[Error] Unknown vendor: ") + vendor);
        ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult);
        return;
    }

    GetThreadPool().Enqueue([messages, endpoint, apiKey, model = actualModel, hwndNotify]()
    {
        try
        {
            CString server, path;
            int port = 443;
            if (!ParseUrl(endpoint, server, port, path))
            {
                CString* pResult = new CString(_T("[Error] Invalid endpoint URL"));
                ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult);
                return;
            }

            CString body = BuildRequestBody(messages, model, true);
            std::string bodyUtf8 = (LPCSTR)CW2A(body, CP_UTF8);

            // Use SendRequestInternal for common HTTP setup, then read SSE stream
            bool bSecure = (port == 443);
            WinHttpHandle hSession(WinHttpOpen(L"MFCApplication1/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0));
            if (!hSession.IsValid())
                throw AiNetworkError("WinHttpOpen failed", GetLastError());

            WinHttpHandle hConnect(WinHttpConnect(hSession, server, (INTERNET_PORT)port, 0));
            if (!hConnect.IsValid())
                throw AiNetworkError("WinHttpConnect failed", GetLastError());

            DWORD flags = bSecure ? WINHTTP_FLAG_SECURE : 0;
            WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"POST", path, nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
            if (!hRequest.IsValid())
                throw AiNetworkError("WinHttpOpenRequest failed", GetLastError());

            CString headers;
            headers.Format(
                L"Content-Type: application/json\r\nAuthorization: Bearer %s\r\n",
                apiKey.GetString());
            WinHttpAddRequestHeaders(hRequest, headers, -1, WINHTTP_ADDREQ_FLAG_ADD);

            if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(),
                (DWORD)bodyUtf8.size(), 0))
                throw AiNetworkError("WinHttpSendRequest failed", GetLastError());

            if (!WinHttpReceiveResponse(hRequest, nullptr))
                throw AiNetworkError("WinHttpReceiveResponse failed", GetLastError());

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

            if (statusCode == 401 || statusCode == 403)
                throw AiApiKeyError("Invalid or expired API key");
            if (statusCode != 200)
            {
                char errBuf[128];
                sprintf_s(errBuf, "HTTP %d", (int)statusCode);
                throw AiNetworkError(errBuf, statusCode);
            }

            // Read & parse SSE stream
            std::string accumulatedContent = ReadSseStream(hRequest, hwndNotify);

            if (s_bCancel.load())
            {
                CString* pCancel = new CString(_T(""));
                ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pCancel);
                return;
            }

            CString* pFinal = new CString(CA2T(accumulatedContent.c_str(), CP_UTF8));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 1, (LPARAM)pFinal);
        }
        catch (const AiApiKeyError& e)
        {
            CString err;
            err.Format(_T("[API Key Error] %hs"), e.what());
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)new CString(err));
        }
        catch (const AiNetworkError& e)
        {
            CString err;
            err.Format(_T("[Network Error] %hs (code: %d)"), e.what(), (int)e.errorCode);
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)new CString(err));
        }
        catch (const std::exception& e)
        {
            CString err;
            err.Format(_T("[Error] %hs"), e.what());
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)new CString(err));
        }
    });
}