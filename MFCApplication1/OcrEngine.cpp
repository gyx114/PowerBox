// OcrEngine.cpp - Isolated in non-MFC compilation unit to avoid C++/WinRT and MFC header conflicts
// This file does not use precompiled headers

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

#include <string>
#include <vector>

using namespace winrt;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Globalization;

// Try to create OCR engine for specified language
static OcrEngine TryCreateEngine(const wchar_t* langTag)
{
    try
    {
        auto lang = Language(langTag);
        return OcrEngine::TryCreateFromLanguage(lang);
    }
    catch (...) { return nullptr; }
}

// Try multiple languages to create OCR engine, return first available
static OcrEngine CreateBestEngine(bool preferChinese)
{
    // Prefer user system language
    auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (engine) return engine;

    // If user language is unavailable, try by priority
    if (preferChinese)
    {
        // Chinese first
        const wchar_t* langs[] = { L"zh-Hans", L"zh-Hant", L"zh-CN", L"zh-TW", L"en", L"ja", L"ko" };
        for (auto lang : langs)
        {
            engine = TryCreateEngine(lang);
            if (engine) return engine;
        }
    }
    else
    {
        const wchar_t* langs[] = { L"en", L"zh-Hans", L"zh-CN", L"ja", L"ko" };
        for (auto lang : langs)
        {
            engine = TryCreateEngine(lang);
            if (engine) return engine;
        }
    }

    return nullptr;
}

bool OcrRecognizeFromFile(const wchar_t* filePath, wchar_t* output, int outputSize, bool preferChinese)
{
    if (!filePath || !output || outputSize <= 0) return false;

    try
    {
        // Create OCR engine (try multiple languages)
        auto engine = CreateBestEngine(preferChinese);
        if (!engine)
        {
            wcsncpy_s(output, outputSize, L"OCR engine creation failed. Please ensure the language pack is installed.", _TRUNCATE);
            return false;
        }

        // Load image via StorageFile
        auto file = StorageFile::GetFileFromPathAsync(filePath).get();
        auto stream = file.OpenAsync(FileAccessMode::Read).get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto frame = decoder.GetFrameAsync(0).get();

        auto swBitmap = frame.GetSoftwareBitmapAsync().get();
        if (!swBitmap)
        {
            wcsncpy_s(output, outputSize, L"Unable to decode screenshot.", _TRUNCATE);
            return false;
        }

        // Convert to Gray8 format (required by OcrEngine)
        if (swBitmap.BitmapPixelFormat() != BitmapPixelFormat::Gray8)
        {
            swBitmap = SoftwareBitmap::Convert(swBitmap, BitmapPixelFormat::Gray8);
        }

        // Execute OCR recognition
        auto ocrResult = engine.RecognizeAsync(swBitmap).get();

        std::wstring text;
        auto lines = ocrResult.Lines();
        for (uint32_t i = 0; i < lines.Size(); i++)
        {
            text += lines.GetAt(i).Text().c_str();
            text += L"\r\n";
        }

        if (text.empty())
            text = L"No text recognized.";

        wcsncpy_s(output, outputSize, text.c_str(), _TRUNCATE);
        return true;
    }
    catch (const winrt::hresult_error& e)
    {
        std::wstring err = L"OCR recognition failed (0x";
        wchar_t hex[16];
        swprintf_s(hex, L"%08X)", static_cast<unsigned int>(e.code()));
        err += hex;
        wcsncpy_s(output, outputSize, err.c_str(), _TRUNCATE);
        return false;
    }
    catch (...)
    {
        wcsncpy_s(output, outputSize, L"OCR failed: unknown error.", _TRUNCATE);
        return false;
    }
}