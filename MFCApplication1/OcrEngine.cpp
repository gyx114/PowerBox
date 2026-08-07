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

// Check if a word is entirely composed of ASCII digits (0-9)
static bool IsAllDigits(const std::wstring& word)
{
    if (word.empty()) return false;
    for (auto c : word)
        if (c < L'0' || c > L'9')
            return false;
    return true;
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
    {
        // Multi-digit numbers (e.g. "81", "26200") are NOT space-joining.
        // OCR frequently splits multi-digit numbers into separate words
        // ("1 81" instead of "181"), and we want them concatenated.
        if (IsAllDigits(word))
            return false;
        return true;
    }
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

// Check if a single character is an ASCII letter (A-Z, a-z)
static bool IsAsciiLetter(wchar_t c)
{
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
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
    std::wstring prevWordText;

    auto words = ocrLine.Words();
    for (uint32_t i = 0; i < words.Size(); i++)
    {
        std::wstring wordText(words.GetAt(i).Text().c_str());
        bool isThisWordSpaceJoining = IsSpaceJoiningWord(wordText);

        // Determine if we should add a space before this word
        bool addSpace = !isFirstWord;

        if (addSpace)
        {
            // Both non-space-joining: no space (e.g. CJK chars)
            if (!isThisWordSpaceJoining && !isPrevWordSpaceJoining)
                addSpace = false;
            // Consecutive single ASCII letters: no space (OCR splits "guany" → "g u a ny")
            else if (wordText.size() == 1 && IsAsciiLetter(wordText[0]) &&
                     prevWordText.size() == 1 && IsAsciiLetter(prevWordText[0]))
                addSpace = false;
            // Single ASCII letter followed by digits: no space (e.g. "M 1010" → "M1010", "x 64" → "x64")
            else if (IsAllDigits(wordText) &&
                     prevWordText.size() == 1 && IsAsciiLetter(prevWordText[0]))
                addSpace = false;
        }

        if (addSpace)
        {
            result += L' ';
        }
        result += wordText;

        isFirstWord = false;
        isPrevWordSpaceJoining = isThisWordSpaceJoining;
        prevWordText = wordText;
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
//   U+FF08 （ → U+0028 (    (full-width left parenthesis)
//   U+FF09 ） → U+0029 )    (full-width right parenthesis)
//   U+201C " → U+0022 "     (left double quote → ASCII quote)
//   U+201D " → U+0022 "     (right double quote → ASCII quote)
//   U+2018 ' → U+0027 '     (left single quote → ASCII apostrophe)
//   U+2019 ' → U+0027 '     (right single quote → ASCII apostrophe)
//   U+2013 – → U+002D -     (en dash → hyphen)
//   U+2014 — → U+002D -     (em dash → hyphen)
//   U+FF0D － → U+002D -    (full-width hyphen → ASCII hyphen)
//   U+2212 − → U+002D -     (minus sign → hyphen)
//   U+FF5E ～ → U+007E ~    (full-width tilde → ASCII tilde)
//   U+30FB ・ → U+00B7 ·    (katakana middle dot)
//   U+3001 、 → U+002C ,    (CJK comma → ASCII comma)
//   U+3002 。 → U+002E .    (CJK period → ASCII period)
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
        case 0xFF08: c = L'('; break;  // （ → (
        case 0xFF09: c = L')'; break;  // ） → )
        case 0xFF0D: c = L'-'; break;  // － → -
        case 0xFF5E: c = L'~'; break;  // ～ → ~
        case 0x2013:
        case 0x2014:
        case 0x2212: c = L'-'; break;  // –/—/− → -
        case 0x201C:
        case 0x201D: c = L'"'; break;  // " " → "
        case 0x2018:
        case 0x2019: c = L'\''; break; // ' ' → '
        case 0x3001: c = L','; break;  // 、 → ,
        case 0x3002: c = L'.'; break;  // 。 → .
        }
    }
}

// Remove spaces around punctuation marks that the OCR engine inserts.
// The OCR engine often outputs spaces before/after punctuation (e.g. "10 . 0"
// instead of "10.0", "2 , 23" instead of "2,23"). This cleanup removes
// spaces before common punctuation and after opening brackets.
static void CleanupPunctuationSpaces(std::wstring& text)
{
    // Remove space BEFORE punctuation: . , : ; ) % ］
    size_t pos = 0;
    while ((pos = text.find_first_of(L".,:;)%]", pos)) != std::wstring::npos)
    {
        if (pos > 0 && text[pos - 1] == L' ')
        {
            text.erase(pos - 1, 1);
            --pos;
        }
        ++pos;
    }

    // Remove space AFTER opening brackets: ( [ ［
    pos = 0;
    while ((pos = text.find_first_of(L"([", pos)) != std::wstring::npos)
    {
        if (pos + 1 < text.size() && text[pos + 1] == L' ')
        {
            text.erase(pos + 1, 1);
        }
        ++pos;
    }
}

// Correct common OCR character confusions in English words.
// The OCR engine (especially the Chinese engine on English text) often confuses
// visually-similar characters:
//   '0' ↔ 'o'  (e.g., "PowerP0int" → "PowerPoint")
//   '1' ↔ 'l'  (e.g., "fi1e" → "file")
//   'I' ↔ 'l'  (e.g., "ExceI" → "Excel")
//
// Rules (conservative — only apply in clear letter contexts):
//   - '0' → 'o' when between two ASCII letters
//   - '1' → 'l' when between two ASCII letters (but NOT in ordinals like "1st")
//   - 'I' → 'l' when preceded by a lowercase ASCII letter
static void CorrectCharConfusions(std::wstring& text)
{
    size_t i = 0;
    while (i < text.size())
    {
        // Find start of a word (non-space, non-newline)
        if (text[i] == L' ' || text[i] == L'\r' || text[i] == L'\n')
        {
            i++;
            continue;
        }

        size_t wordStart = i;
        while (i < text.size() && text[i] != L' ' && text[i] != L'\r' && text[i] != L'\n')
            i++;
        std::wstring word = text.substr(wordStart, i - wordStart);

        // Check if word has both letters and digits (potential 0/o, 1/l confusion)
        bool hasLetter = false, hasDigit = false;
        for (auto c : word)
        {
            if (IsAsciiLetter(c)) hasLetter = true;
            if (c >= L'0' && c <= L'9') hasDigit = true;
        }

        if (hasLetter && hasDigit)
        {
            for (size_t j = 0; j < word.size(); j++)
            {
                // '0' → 'o' when between two ASCII letters
                if (word[j] == L'0' && j > 0 && j + 1 < word.size() &&
                    IsAsciiLetter(word[j - 1]) && IsAsciiLetter(word[j + 1]))
                {
                    word[j] = L'o';
                }
                // '1' → 'l' when between two ASCII letters
                // (skip ordinals like "1st", "1nd", "1rd", "1th" — those start with '1')
                if (word[j] == L'1' && j > 0 && j + 1 < word.size() &&
                    IsAsciiLetter(word[j - 1]) && IsAsciiLetter(word[j + 1]))
                {
                    word[j] = L'l';
                }
            }
        }

        // 'I' → 'l' when preceded by a lowercase ASCII letter
        // (covers "ExceI" → "Excel", "fiIe" → "file"; preserves "AI", "IT", "API")
        for (size_t j = 1; j < word.size(); j++)
        {
            if (word[j] == L'I' && word[j - 1] >= L'a' && word[j - 1] <= L'z')
            {
                word[j] = L'l';
            }
        }

        // Write corrected word back
        text.replace(wordStart, word.size(), word);
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

        // Step 2: Get SoftwareBitmap from frame and ensure BGRA8 format.
        // PowerToys uses GDI+ Bitmap (Format32bppArgb) which is equivalent to BGRA8.
        // Explicitly convert to BGRA8 here so the BMP encoder receives a known format,
        // avoiding potential artifacts from format conversion inside the BMP encoder.
        auto swBitmap = frame.GetSoftwareBitmapAsync().get();
        if (swBitmap.BitmapPixelFormat() != BitmapPixelFormat::Bgra8)
        {
            swBitmap = SoftwareBitmap::Convert(swBitmap, BitmapPixelFormat::Bgra8);
        }

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

        // Step 4: Apply adaptive scaling — try progressively higher factors for small text.
        // Small text (e.g. 8px font) needs more scaling to bring characters above the
        // OCR engine's recognition threshold. Try 4.0x → 3.0x → 2.5x → 2.0x → 1.5x → 1.0x.
        // 4.0x helps disambiguate similar characters (o/0, l/1/I) in small English text.
        // NOTE: No sharpening is applied — Laplacian sharpening emphasizes internal character
        // edges (e.g. 亻/乍 in 作) which causes the OCR engine to split Chinese characters.
        uint32_t maxDim = OcrEngine::MaxImageDimension();
        double scaleCandidates[] = { 4.0, 3.0, 2.5, 2.0, 1.5 };
        double scale = 1.0;

        for (auto s : scaleCandidates)
        {
            if (bmpFrame.PixelWidth() * s <= maxDim &&
                bmpFrame.PixelHeight() * s <= maxDim)
            {
                scale = s;
                break;
            }
        }

        SoftwareBitmap ocrBitmap = nullptr;

        if (scale > 1.0)
        {
            uint32_t newW = static_cast<uint32_t>(bmpFrame.PixelWidth() * scale);
            uint32_t newH = static_cast<uint32_t>(bmpFrame.PixelHeight() * scale);

            BitmapTransform transform;
            transform.ScaledWidth(newW);
            transform.ScaledHeight(newH);
            transform.InterpolationMode(BitmapInterpolationMode::Fant);

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

        // Clean up spaces around punctuation (e.g. "10 . 0" → "10.0", "2 , 23" → "2,23")
        CleanupPunctuationSpaces(text);

        // Correct common OCR character confusions (e.g. "PowerP0int" → "PowerPoint",
        // "ExceI" → "Excel") — especially when Chinese engine processes English text
        CorrectCharConfusions(text);

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