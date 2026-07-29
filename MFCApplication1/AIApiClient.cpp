// AIApiClient.cpp: AI API client implementation
#include "pch.h"
#include "AIApiClient.h"
#include "json.hpp"
#include <winhttp.h>
#include <thread>
#include <algorithm>
#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

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

bool CAIApiClient::ParseUrl(const CString& url, CString& server, int& port, CString& path)
{
    CString s = url;
    bool bSecure = false;

    if (s.Left(8).CompareNoCase(_T("https://")) == 0)
    {
        bSecure = true;
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

    // Check for explicit port
    int colon = server.Find(_T(':'));
    if (colon != -1)
    {
        CString portStr = server.Mid(colon + 1);
        server = server.Left(colon);
        port = _ttoi(portStr);
    }

    return true;
}

CString CAIApiClient::BuildRequestBody(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& model)
{
    json jMessages = json::array();
    for (const auto& msg : messages)
    {
        std::string role = (LPCSTR)CT2A(msg.first, CP_UTF8);
        std::string content = (LPCSTR)CT2A(msg.second, CP_UTF8);
        jMessages.push_back({
            {"role", role},
            {"content", content}
        });
    }

    json jBody;
    std::string modelStr = (LPCSTR)CT2A(model, CP_UTF8);
    jBody["model"] = modelStr;
    jBody["messages"] = jMessages;
    jBody["stream"] = false;

    std::string bodyStr = jBody.dump();
    return CString(CA2T(bodyStr.c_str(), CP_UTF8));
}

CString CAIApiClient::SendHttpRequest(
    const CString& server, int port, const CString& path,
    const CString& apiKey, const CString& body)
{
    bool bSecure = (port == 443);

    HINTERNET hSession = WinHttpOpen(
        L"MFCApplication1/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return _T("");

    HINTERNET hConnect = WinHttpConnect(hSession, server, (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return _T(""); }

    DWORD flags = bSecure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return _T(""); }

    // Set headers
    CString headers;
    headers.Format(L"Content-Type: application/json\r\nAuthorization: Bearer %s\r\n", apiKey.GetString());
    WinHttpAddRequestHeaders(hRequest, headers, -1, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    CStringA bodyUtf8 = (LPCSTR)CW2A(body, CP_UTF8);
    BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)(LPCSTR)bodyUtf8, bodyUtf8.GetLength(), bodyUtf8.GetLength(), 0);

    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return _T(""); }

    bResult = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return _T(""); }

    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
    {
        if (statusCode != 200)
        {
            CString errMsg;
            errMsg.Format(_T("[HTTP Error] Status code: %d"), statusCode);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return errMsg;
        }
    }

    // Read response
    CStringA response;
    DWORD dwSize = 0;
    do
    {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;

        char* pszBuffer = new char[dwSize + 1];
        ZeroMemory(pszBuffer, dwSize + 1);
        DWORD dwDownloaded = 0;
        WinHttpReadData(hRequest, (LPVOID)pszBuffer, dwSize, &dwDownloaded);
        response += pszBuffer;
        delete[] pszBuffer;
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return CString(CA2T(response, CP_UTF8));
}

CString CAIApiClient::ExtractContent(const CString& jsonStr)
{
    try
    {
        std::string s = (LPCSTR)CT2A(jsonStr, CP_UTF8);
        json j = json::parse(s);

        if (j.contains("error"))
        {
            CString errMsg;
            CString errText = (LPCTSTR)CA2T(j["error"]["message"].get<std::string>().c_str(), CP_UTF8);
            errMsg.Format(_T("[API Error] %s"), errText.GetString());
            return errMsg;
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
    catch (const std::exception& e)
    {
        CString err;
        err.Format(_T("[Parse Error] %hs"), e.what());
        return err;
    }
}

void CAIApiClient::SendAsync(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& vendor,
    const CString& apiKey,
    const CString& model,
    HWND hwndNotify)
{
    // Find vendor config
    CString endpoint;
    CString actualModel = model;
    for (const auto& v : GetVendors())
    {
        if (v.name.CompareNoCase(vendor) == 0)
        {
            endpoint = v.endpoint;
            if (actualModel.IsEmpty())
                actualModel = v.defaultModel;
            break;
        }
    }

    if (endpoint.IsEmpty())
    {
        CString* pResult = new CString(_T("[Error] Unknown vendor: ") + vendor);
        ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
        return;
    }

    // Capture data for thread
    CString capturedEndpoint = endpoint;
    CString capturedApiKey = apiKey;
    CString capturedModel = actualModel;

    std::thread([messages, capturedEndpoint, capturedApiKey, capturedModel, hwndNotify]()
    {
        CString server, path;
        int port = 443;

        if (!ParseUrl(capturedEndpoint, server, port, path))
        {
            CString* pResult = new CString(_T("[Error] Invalid endpoint URL"));
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
            return;
        }

        CString body = BuildRequestBody(messages, capturedModel);
        CString response = SendHttpRequest(server, port, path, capturedApiKey, body);

        if (response.IsEmpty())
        {
            CString* pResult = new CString(_T("[Error] Network request failed. Check your network or API key."));
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
            return;
        }

        // If response is already an error message (starts with '['), return it directly
        if (response.GetAt(0) == _T('['))
        {
            CString* pResult = new CString(response);
            ::PostMessage(hwndNotify, WM_AI_RESPONSE, 0, (LPARAM)pResult);
            return;
        }

        CString content = ExtractContent(response);
        CString* pResult = new CString(content);
        ::PostMessage(hwndNotify, WM_AI_RESPONSE, 1, (LPARAM)pResult);
    }).detach();
}

void CAIApiClient::SendAsyncStreaming(
    const std::vector<std::pair<CString, CString>>& messages,
    const CString& vendor,
    const CString& apiKey,
    const CString& model,
    HWND hwndNotify)
{
    // Find vendor config
    CString endpoint;
    CString actualModel = model;
    for (const auto& v : GetVendors())
    {
        if (v.name.CompareNoCase(vendor) == 0)
        {
            endpoint = v.endpoint;
            if (actualModel.IsEmpty())
                actualModel = v.defaultModel;
            break;
        }
    }

    if (endpoint.IsEmpty())
    {
        CString* pResult = new CString(_T("[Error] Unknown vendor: ") + vendor);
        ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult);
        return;
    }

    std::thread([messages, endpoint, apiKey, model = actualModel, hwndNotify]()
    {
        CString server, path;
        int port = 443;

        if (!ParseUrl(endpoint, server, port, path))
        {
            CString* pResult = new CString(_T("[Error] Invalid endpoint URL"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult);
            return;
        }

        // Build streaming request body (stream: true)
        json jMessages = json::array();
        for (const auto& msg : messages)
        {
            std::string role = (LPCSTR)CT2A(msg.first, CP_UTF8);
            std::string content = (LPCSTR)CT2A(msg.second, CP_UTF8);
            jMessages.push_back({ {"role", role}, {"content", content} });
        }
        json jBody;
        std::string modelStr = (LPCSTR)CT2A(model, CP_UTF8);
        jBody["model"] = modelStr;
        jBody["messages"] = jMessages;
        jBody["stream"] = true;
        std::string bodyStr = jBody.dump();
        CString body = CString(CA2T(bodyStr.c_str(), CP_UTF8));

        // Send HTTP request
        bool bSecure = (port == 443);
        HINTERNET hSession = WinHttpOpen(L"MFCApplication1/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession)
        {
            CString* pResult = new CString(_T("[Error] WinHttpOpen failed"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult);
            return;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, server, (INTERNET_PORT)port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession);
            CString* pResult = new CString(_T("[Error] Connection failed"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult); return; }

        DWORD flags = bSecure ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            CString* pResult = new CString(_T("[Error] Request creation failed"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult); return; }

        CString headers;
        headers.Format(L"Content-Type: application/json\r\nAuthorization: Bearer %s\r\n", apiKey.GetString());
        WinHttpAddRequestHeaders(hRequest, headers, -1, WINHTTP_ADDREQ_FLAG_ADD);

        CStringA bodyUtf8 = (LPCSTR)CW2A(body, CP_UTF8);
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)(LPCSTR)bodyUtf8, bodyUtf8.GetLength(), bodyUtf8.GetLength(), 0))
        {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            CString* pResult = new CString(_T("[Error] Send request failed"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult); return;
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr))
        {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            CString* pResult = new CString(_T("[Error] Receive response failed"));
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult); return;
        }

        // Check HTTP status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
        if (statusCode != 200)
        {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            CString errMsg;
            errMsg.Format(_T("[HTTP Error] Status code: %d"), statusCode);
            CString* pResult = new CString(errMsg);
            ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 0, (LPARAM)pResult); return;
        }

        // Read SSE stream incrementally
        CStringA buffer;
        char readBuf[4096];
        DWORD dwSize = 0;
        CStringA accumulatedContent;

        while (true)
        {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0)
                break;

            DWORD toRead = std::min<DWORD>(dwSize, (DWORD)sizeof(readBuf) - 1);
            DWORD dwDownloaded = 0;
            if (!WinHttpReadData(hRequest, readBuf, toRead, &dwDownloaded) || dwDownloaded == 0)
                break;

            readBuf[dwDownloaded] = '\0';
            buffer += readBuf;

            // Parse SSE lines from buffer
            while (true)
            {
                int newline = buffer.Find("\n");
                if (newline < 0) break;

                CStringA line = buffer.Left(newline);
                // Remove trailing \r
                if (line.GetLength() > 0 && line[line.GetLength() - 1] == '\r')
                    line = line.Left(line.GetLength() - 1);

                buffer = buffer.Mid(newline + 1);

                if (line.IsEmpty()) continue; // skip empty lines between chunks

                if (line.Left(6) == "data: ")
                {
                    CStringA data = line.Mid(6);
                    if (data == "[DONE]") continue;

                    try
                    {
                        std::string dataStr(data, data.GetLength());
                        json j = json::parse(dataStr);
                        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty())
                        {
                            auto& choice = j["choices"][0];
                            if (choice.contains("delta") && choice["delta"].contains("content"))
                            {
                                std::string content = choice["delta"]["content"].get<std::string>();
                                accumulatedContent += content.c_str();
                                CString* pChunk = new CString(CA2T(content.c_str(), CP_UTF8));
                                ::PostMessage(hwndNotify, WM_AI_STREAM_CHUNK, 0, (LPARAM)pChunk);
                            }
                        }
                    }
                    catch (...) {}
                }
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Signal completion
        CString* pFinal = new CString(CA2T(accumulatedContent, CP_UTF8));
        ::PostMessage(hwndNotify, WM_AI_STREAM_DONE, 1, (LPARAM)pFinal);
    }).detach();
}