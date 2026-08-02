// LocalizationManager.h: Singleton localization manager for i18n support
#pragma once
#include <map>
#include <vector>

class CLocalizationManager
{
public:
    static CLocalizationManager& GetInstance();

    // Load language file for the given language identifier (e.g. "zh-CN", "en-US")
    bool LoadLanguage(const CString& langId);

    // Get translated string for a section+key.
    // Fallback chain: current language -> zh-CN (built-in) -> section_key -> key
    CString GetString(LPCTSTR section, LPCTSTR key, LPCTSTR defaultVal = nullptr) const;

    // Get current language identifier
    CString GetCurrentLanguage() const { return m_currentLang; }

    // Get language display name for the current language
    CString GetLanguageDisplayName() const;

    // Get available languages (scans lang\ directory for *.ini files)
    std::vector<std::pair<CString, CString>> GetAvailableLanguages() const;

    // Get the lang directory path
    CString GetLangDir() const;

private:
    CLocalizationManager();
    ~CLocalizationManager() = default;
    CLocalizationManager(const CLocalizationManager&) = delete;
    CLocalizationManager& operator=(const CLocalizationManager&) = delete;

    CString m_currentLang = _T("zh-CN");

    // Built-in Chinese defaults (embedded fallback, no external file needed)
    void LoadBuiltinDefaults();
    std::map<CString, CString> m_defaults;  // key = "section\0key\0" (double-null terminated style)

    // Build a lookup key string from section and key
    static CString MakeKey(LPCTSTR section, LPCTSTR key);
};