// OcrEngine.cpp - Isolated in non-MFC compilation unit to avoid C++/WinRT and MFC header conflicts
// This file does not use precompiled headers
// Follows PowerToys Text Extractor OCR pipeline approach

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

#include <string>
#include <vector>
#include <algorithm>

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

// Create best available OCR engine — PowerToys-style: prefer single-language engine.
// CRITICAL: Do NOT use TryCreateFromUserProfileLanguages() as the first choice.
// A multi-language engine uses English word-segmentation rules internally, which
// causes Chinese characters to be split with spaces ("月 皮 务 器" instead of "服务器")
// and English letters to be confused with visually-similar CJK characters ('f' → '干').
// A single-language engine (e.g. zh-Hans) uses the correct segmentation for that language.
static OcrEngine CreateBestEngine(bool preferChinese)
{
    auto available = OcrEngine::AvailableRecognizerLanguages();

    // 1. Explicitly try single-language engine for the preferred language first
    const wchar_t* prioLangs[] = {
        preferChinese ? L"zh-Hans" : L"en",
        preferChinese ? L"zh-Hant" : L"zh-Hans",
        L"zh-CN", L"zh-TW",
        L"en", L"ja", L"ko"
    };

    for (auto tag : prioLangs)
    {
        for (uint32_t i = 0; i < available.Size(); i++)
        {
            if (available.GetAt(i).LanguageTag() == tag)
            {
                return OcrEngine::TryCreateFromLanguage(available.GetAt(i));
            }
        }
    }

    // 2. Fallback: multi-language engine (only if no single-language engine works)
    auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (engine) return engine;

    // 3. Last resort: first available
    if (available.Size() > 0)
        return OcrEngine::TryCreateFromLanguage(available.GetAt(0));

    return nullptr;
}

// Check if the language is a space-joining language (like PowerToys' LanguageHelper)
// Chinese (zh) and Japanese (ja) are NOT space-joining; most others are
static bool IsSpaceJoiningLanguage(const Language& lang)
{
    auto tag = lang.LanguageTag();
    if (tag.size() >= 2)
    {
        if ((tag[0] == L'z' && tag[1] == L'h') || // zh-*
            (tag[0] == L'j' && tag[1] == L'a'))    // ja*
        {
            return false;
        }
    }
    return true;
}

// Remove zero-width and control characters, trim whitespace from a line
static std::wstring CleanOcrLine(const std::wstring& line)
{
    // Remove zero-width characters and control characters
    std::wstring cleaned;
    cleaned.reserve(line.size());
    for (auto c : line)
    {
        if (c != 0x200B && c != 0x200C && c != 0x200D &&
            c != 0xFEFF && c != 0x00AD && c != 0x2060 &&
            c != 0x00A0) // non-breaking space
        {
            cleaned += c;
        }
    }

    // Trim leading/trailing whitespace
    auto first = cleaned.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
        return std::wstring();
    auto last = cleaned.find_last_not_of(L" \t\r\n");
    return cleaned.substr(first, last - first + 1);
}

// PowerToys-style word-by-word text reconstruction.
// For CJK (non-space-joining) languages, the OCR engine's OcrLine.Text may have
// incorrect spacing. This method reconstructs the text from individual words,
// adding spaces only between "space-joining" words (English words, numbers)
// but not between CJK characters or punctuation.
// The regex equivalent from PowerToys: @"(^[\p{L}-[\p{Lo}]]|\p{Nd}$)|.{2,}"
// which matches: single non-CJK letter, single digit, or any 2+ char word.
static bool IsSpaceJoiningWord(const std::wstring& word)
{
    if (word.size() >= 2)
        return true;
    if (word.size() == 1)
    {
        wchar_t c = word[0];
        // CJK characters are "other letters" (Lo) in Unicode — NOT space-joining
        // Everything else (ASCII letters, digits, punctuation) is space-joining
        //
        // IMPORTANT: SINGLE DIGIT (0-9) should NOT be space-joined because
        // OCR frequently splits a multi-digit number into separate words,
        // and we want them to be concatenated (1 6 → 16, not 1 6).
        bool isDigit = (c >= L'0' && c <= L'9');
        bool isCJK = (c >= 0x2E80 && c <= 0x2FFF) ||  // CJK Radicals
                     (c >= 0x3000 && c <= 0x303F) ||  // CJK Symbols & Punctuation
                     (c >= 0x3400 && c <= 0x4DBF) ||  // CJK Extension A
                     (c >= 0x4E00 && c <= 0x9FFF);     // CJK Unified Ideographs
        if (isDigit || isCJK)
            return false;
        return true;
    }
    return false;
}

static std::wstring BuildLineTextFromWords(const OcrLine& ocrLine, bool isSpaceJoining)
{
    if (isSpaceJoining)
    {
        // For space-joining languages (English, etc.), OcrLine.Text is correct
        return std::wstring(ocrLine.Text().c_str());
    }

    // For CJK: reconstruct from individual words (PowerToys approach)
    std::wstring result;
    bool isFirstWord = true;
    bool isPrevWordSpaceJoining = false;

    auto words = ocrLine.Words();
    for (uint32_t i = 0; i < words.Size(); i++)
    {
        std::wstring wordText(words.GetAt(i).Text().c_str());
        bool isThisWordSpaceJoining = IsSpaceJoiningWord(wordText);

        if (isFirstWord || (!isThisWordSpaceJoining && !isPrevWordSpaceJoining))
        {
            result += wordText;
        }
        else
        {
            result += L' ';
            result += wordText;
        }

        isFirstWord = false;
        isPrevWordSpaceJoining = isThisWordSpaceJoining;
    }

    return result;
}

// Normalize full-width punctuation to half-width ASCII equivalents.
// The Chinese OCR engine tends to output full-width symbols (．，：)
// even for ASCII punctuation in the original image. Common mappings:
//   U+FF0E ． → U+002E .    (full-width period → ASCII period)
//   U+FF0C ， → U+002C ,    (full-width comma → ASCII comma)
//   U+FF1A ： → U+003A :    (full-width colon → ASCII colon)
//   U+FF1B ； → U+003B ;    (full-width semicolon → ASCII semicolon)
//   U+201C " → U+0022 "     (left double quote → ASCII quote)
//   U+201D " → U+0022 "     (right double quote → ASCII quote)
//   U+2018 ' → U+0027 '     (left single quote → ASCII apostrophe)
//   U+2019 ' → U+0027 '     (right single quote → ASCII apostrophe)
static void NormalizeFullWidthPunctuation(std::wstring& text)
{
    for (auto& c : text)
    {
        switch (c)
        {
        case 0xFF0E: c = L'.'; break;  // ． → .
        case 0xFF0C: c = L','; break;  // ， → ,
        case 0xFF1A: c = L':'; break;  // ： → :
        case 0xFF1B: c = L';'; break;  // ； → ;
        case 0x201C:
        case 0x201D: c = L'"'; break;  // " " → "
        case 0x2018:
        case 0x2019: c = L'\''; break; // ' ' → '
        }
    }
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

        // Step 1: Load image via StorageFile
        auto file = StorageFile::GetFileFromPathAsync(filePath).get();
        auto stream = file.OpenAsync(FileAccessMode::Read).get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        auto frame = decoder.GetFrameAsync(0).get();

        // Step 2: Get SoftwareBitmap from frame (no transform, no format conversion)
        // PowerToys does NOT call SoftwareBitmap::Convert — it lets the BMP encoder/decoder
        // handle format conversion naturally.
        auto swBitmap = frame.GetSoftwareBitmapAsync().get();

        // Step 3: BMP round-trip — exactly like PowerToys:
        //   GDI+ Bitmap (Format32bppArgb) → BMP stream → WinRT decoder → SoftwareBitmap
        // This ensures the image is in the optimal format (BGRA8) for the OCR engine,
        // and strips any color profiles or EXIF metadata.
        auto bmpStream = InMemoryRandomAccessStream();
        {
            auto bmpEncoder = BitmapEncoder::CreateAsync(BitmapEncoder::BmpEncoderId(), bmpStream).get();
            bmpEncoder.SetSoftwareBitmap(swBitmap);
            bmpEncoder.FlushAsync().get();
        }
        bmpStream.Seek(0);

        auto bmpDecoder = BitmapDecoder::CreateAsync(bmpStream).get();
        auto bmpFrame = bmpDecoder.GetFrameAsync(0).get();

        // Step 4: Apply 1.5x scaling (PowerToys uses 1.5x, consistent with their approach)
        // If 1.5x would exceed OCR engine max dimension, skip scaling (like PowerToys does)
        uint32_t maxDim = OcrEngine::MaxImageDimension();
        double scale = 1.5;
        bool shouldScale = (bmpFrame.PixelWidth() * scale <= maxDim);

        SoftwareBitmap ocrBitmap = nullptr;

        if (shouldScale)
        {
            uint32_t newW = static_cast<uint32_t>(bmpFrame.PixelWidth() * scale);
            uint32_t newH = static_cast<uint32_t>(bmpFrame.PixelHeight() * scale);

            BitmapTransform transform;
            transform.ScaledWidth(newW);
            transform.ScaledHeight(newH);
            transform.InterpolationMode(BitmapInterpolationMode::Fant);

            // PowerToys calls GetSoftwareBitmapAsync() with NO parameters —
            // no color management, no explicit pixel format, no EXIF orientation override.
            ocrBitmap = bmpFrame.GetSoftwareBitmapAsync(
                BitmapPixelFormat::Bgra8,
                BitmapAlphaMode::Ignore,
                transform,
                ExifOrientationMode::IgnoreExifOrientation,
                ColorManagementMode::DoNotColorManage
            ).get();
        }
        else
        {
            // Skip scaling — matches PowerToys' behavior when Width*1.5 > MaxImageDimension
            ocrBitmap = bmpFrame.GetSoftwareBitmapAsync().get();
        }

        if (!ocrBitmap)
        {
            // Final fallback: unscaled from original frame
            ocrBitmap = frame.GetSoftwareBitmapAsync().get();
        }

        if (!ocrBitmap)
        {
            wcsncpy_s(output, outputSize, L"Unable to decode screenshot.", _TRUNCATE);
            return false;
        }

        // Step 5: Execute OCR recognition
        auto ocrResult = engine.RecognizeAsync(ocrBitmap).get();

        // Step 6: Post-process text (PowerToys-style word-by-word reconstruction)
        // For CJK languages, the OCR engine's OcrLine.Text may have incorrect spacing
        // (e.g. "月 皮 务 器" instead of "服务器"). We reconstruct text from individual
        // words, properly handling mixed CJK + English text spacing.
        bool isSpaceJoining = IsSpaceJoiningLanguage(engine.RecognizerLanguage());

        std::wstring text;
        auto lines = ocrResult.Lines();
        for (uint32_t i = 0; i < lines.Size(); i++)
        {
            auto ocrLine = lines.GetAt(i);
            std::wstring lineText = BuildLineTextFromWords(ocrLine, isSpaceJoining);
            std::wstring cleanLine = CleanOcrLine(lineText);
            if (!cleanLine.empty())
            {
                text += cleanLine;
                text += L"\r\n";
            }
        }

        if (text.empty())
            text = L"No text recognized.";

        // Normalize full-width punctuation to ASCII (Chinese engine tends to output ．，：)
        NormalizeFullWidthPunctuation(text);

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