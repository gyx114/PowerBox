// ContextMenuDlg.cpp: implementation file
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "ContextMenuDlg.h"
#include "afxdialogex.h"
#include <algorithm>
#include <set>
#include "GuidInfosDic.h"

#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <aclapi.h>
#include <winver.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "ole32.lib")

// Static member initialization
std::map<CString, CContextMenuDlg::DictEntry> CContextMenuDlg::s_guidDict;
bool CContextMenuDlg::s_bDictLoaded = false;
CString CContextMenuDlg::s_dictPath;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Menu command IDs for right-click context menu in the list
#define ID_MENU_CM_DELETE   50020
#define ID_MENU_CM_LOCATE   50021
#define ID_MENU_CM_TOGGLE   50022

IMPLEMENT_DYNAMIC(CContextMenuDlg, CDialogEx)

CContextMenuDlg::CContextMenuDlg(CWnd* pParent)
	: CDialogEx(IDD_CONTEXT_MENU_DLG, pParent)
	, m_listLeft(0), m_listTop(0), m_statusTop(0)
{
}

CContextMenuDlg::~CContextMenuDlg()
{
}

void CContextMenuDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CContextMenuDlg, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BTN_CM_REFRESH, &CContextMenuDlg::OnBnClickedRefresh)
	ON_CBN_SELCHANGE(IDC_COMBO_CM_LOCATION, &CContextMenuDlg::OnCbnSelchangeLocation)
	ON_BN_CLICKED(IDC_BTN_CM_DELETE, &CContextMenuDlg::OnBnClickedDelete)
	ON_BN_CLICKED(IDC_BTN_CM_LOCATE, &CContextMenuDlg::OnBnClickedLocate)
	ON_BN_CLICKED(IDC_CHECK_CM_FOLDER, &CContextMenuDlg::OnBnClickedCheckFolder)
	ON_BN_CLICKED(IDC_CHECK_CM_WIN11_CLASSIC, &CContextMenuDlg::OnBnClickedCheckWin11Classic)
	ON_BN_CLICKED(IDC_BTN_CM_REBUILD, &CContextMenuDlg::OnBnClickedRebuild)
	ON_BN_CLICKED(IDC_BTN_CM_DICTPATH, &CContextMenuDlg::OnBnClickedDictPath)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_CM_ENTRIES, &CContextMenuDlg::OnNMRClickList)
	ON_COMMAND(ID_MENU_CM_DELETE, &CContextMenuDlg::OnMenuDelete)
	ON_COMMAND(ID_MENU_CM_LOCATE, &CContextMenuDlg::OnMenuLocate)
	ON_COMMAND(ID_MENU_CM_TOGGLE, &CContextMenuDlg::OnMenuToggle)
END_MESSAGE_MAP()

BOOL CContextMenuDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_F5)
	{
		OnBnClickedRefresh();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

// ============================================================================
// Scene initialization — matches ContextMenuManager's MENUPATH_* constants
// ============================================================================

void CContextMenuDlg::InitLocations()
{
	m_scenes.clear();
	// Scene-based architecture matching ContextMenuManager's ShellList.Scenes enum.
	// Each scene maps to a specific HKCR-relative registry base path.
	// ScanEntries scans both <basePath>\shell (static verbs) and
	// <basePath>\shellex (COM handlers, both ContextMenuHandlers and -ContextMenuHandlers).
	// Some scenes have additional paths scanned in ScanEntries().
	m_scenes.push_back({ _T("全部"),                                 _T("") });
	m_scenes.push_back({ _T("文件 (*)"),                             _T("*") });
	m_scenes.push_back({ _T("文件夹 (Directory)"),                   _T("Directory") });
	m_scenes.push_back({ _T("文件夹 (Folder)"),                      _T("Folder") });
	m_scenes.push_back({ _T("目录背景"),                             _T("Directory\\Background") });
	m_scenes.push_back({ _T("桌面背景"),                             _T("DesktopBackground") });
	m_scenes.push_back({ _T("驱动器"),                               _T("Drive") });
	m_scenes.push_back({ _T("所有文件"),                             _T("AllFilesystemObjects") });
	m_scenes.push_back({ _T("此电脑"),                               _T("CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}") });
	m_scenes.push_back({ _T("回收站"),                               _T("CLSID\\{645FF040-5081-101B-9F08-00AA002F954E}") });
	m_scenes.push_back({ _T("库文件夹"),                             _T("LibraryFolder") });
	m_scenes.push_back({ _T("UWP快捷方式"),                          _T("Launcher.ImmersiveApplication") });
	m_scenes.push_back({ _T("exe文件"),                              _T("SystemFileAssociations\\.exe") });
	m_scenes.push_back({ _T("未知文件"),                             _T("Unknown") });
	m_scenes.push_back({ _T("快捷方式"),                             _T("lnkfile") });

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_CM_LOCATION);
	if (pCombo)
	{
		pCombo->ResetContent();
		for (size_t i = 0; i < m_scenes.size(); ++i)
			pCombo->AddString(m_scenes[i].name);
		pCombo->SetCurSel(0);
	}
}

// ============================================================================
// Name / string resolution helpers
// ============================================================================

// ============================================================================
// GuidInfo dictionary (from GuidInfosDic.ini + external files, matching ContextMenuManager)
// ============================================================================

// Helper: parse INI content into the dictionary map (overwrites existing entries for same key)
static void ParseDictIni(const CString& iniContent, std::map<CString, CContextMenuDlg::DictEntry>& dict)
{
	int pos = 0;
	CString currentSection;

	while (pos < iniContent.GetLength())
	{
		int end = iniContent.Find(_T('\n'), pos);
		CString line;
		if (end == -1)
		{
			line = iniContent.Mid(pos);
			pos = iniContent.GetLength();
		}
		else
		{
			line = iniContent.Mid(pos, end - pos);
			pos = end + 1;
		}

		line.Trim();
		// Also trim \r for Windows line endings
		line.TrimRight(_T('\r'));
		if (line.IsEmpty()) continue;
		if (line[0] == _T(';') || line[0] == _T('#')) continue;

		// Section line: [guid]
		if (line[0] == _T('['))
		{
			int close = line.Find(_T(']'));
			if (close > 1)
			{
				currentSection = line.Mid(1, close - 1);
				currentSection.MakeLower();
				// Ensure section exists in map
				if (dict.find(currentSection) == dict.end())
					dict[currentSection] = CContextMenuDlg::DictEntry();
			}
			continue;
		}

		// Key=Value line
		int eq = line.Find(_T('='));
		if (eq > 0 && !currentSection.IsEmpty())
		{
			CString key = line.Left(eq);
			key.Trim();
			CString value = line.Mid(eq + 1);
			value.Trim();

			auto it = dict.find(currentSection);
			if (it != dict.end())
			{
				if (key.CompareNoCase(_T("ResText")) == 0)
					it->second.resText = value;
				else if (key.CompareNoCase(_T("zh-CN-Text")) == 0)
					it->second.zhText = value;
				else if (key.CompareNoCase(_T("Text")) == 0)
					it->second.text = value;
			}
		}
	}
}

// Helper: read entire file content into a CString
static CString ReadFileContent(const CString& filePath)
{
	CString content;
	try
	{
		CStdioFile file;
		if (file.Open(filePath, CFile::modeRead | CFile::shareDenyWrite))
		{
			CString line;
			while (file.ReadString(line))
			{
				content += line + _T("\n");
			}
			file.Close();
		}
	}
	catch (CFileException*)
	{
		// File not found or access denied — return empty
	}
	return content;
}

// Helper: write content to a file
static bool WriteFileContent(const CString& filePath, const CString& content)
{
	try
	{
		CStdioFile file;
		if (file.Open(filePath, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite))
		{
			// Write as UTF-8 with BOM for compatibility
			// Convert CString (UTF-16LE) to UTF-8
			int nLen = content.GetLength();
			int nUtf8Len = WideCharToMultiByte(CP_UTF8, 0, content, nLen, nullptr, 0, nullptr, nullptr);
			if (nUtf8Len > 0)
			{
				std::vector<char> utf8Buf(nUtf8Len + 1);
				WideCharToMultiByte(CP_UTF8, 0, content, nLen, utf8Buf.data(), nUtf8Len, nullptr, nullptr);
				utf8Buf[nUtf8Len] = 0;
				file.Write(utf8Buf.data(), (UINT)nUtf8Len);
			}
			file.Close();
			return true;
		}
	}
	catch (CFileException*)
	{
	}
	return false;
}

void CContextMenuDlg::LoadGuidDictionary()
{
	if (s_bDictLoaded) return;
	s_bDictLoaded = true;
	s_guidDict.clear();

	// Priority (lowest to highest): embedded hardcoded → user config → cache file
	// Later calls overwrite earlier entries for the same CLSID.

	// Layer 1: Load embedded hardcoded dictionary (lowest priority)
	ParseDictIni(g_guidInfosDic, s_guidDict);

	// Layer 2: Load user-configured external dictionary
	// Read path from app directory's config.ini
	if (s_dictPath.IsEmpty())
	{
		CString configPath = GetConfigPath();
		TCHAR szPath[MAX_PATH] = { 0 };
		GetPrivateProfileString(_T("Dictionary"), _T("Path"), _T(""), szPath, MAX_PATH, configPath);
		CString readPath = szPath;
		readPath.Trim();
		if (!readPath.IsEmpty() && PathFileExists(readPath))
		{
			s_dictPath = readPath;
		}
	}

	// If still no path configured, use default: %AppData%/MFCApplication1/
	if (s_dictPath.IsEmpty())
	{
		TCHAR szAppData[MAX_PATH] = { 0 };
		if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, szAppData)))
		{
			s_dictPath = CString(szAppData) + _T("\\MFCApplication1");
			CreateDirectory(s_dictPath, nullptr);
		}
	}

	if (!s_dictPath.IsEmpty() && PathFileExists(s_dictPath))
	{
		// Load all .ini files from the dictionary folder
		CFileFind finder;
		CString filter = s_dictPath + _T("\\*.ini");
		BOOL bFound = finder.FindFile(filter);
		while (bFound)
		{
			bFound = finder.FindNextFile();
			if (finder.IsDots() || finder.IsDirectory()) continue;
			CString content = ReadFileContent(finder.GetFilePath());
			if (!content.IsEmpty())
				ParseDictIni(content, s_guidDict);
		}
		finder.Close();
	}

	// Layer 3: Load cache file (highest priority, overwrites all previous entries)
	CString cachePath = GetCachePath();
	if (!cachePath.IsEmpty() && PathFileExists(cachePath))
	{
		CString content = ReadFileContent(cachePath);
		if (!content.IsEmpty())
			ParseDictIni(content, s_guidDict);
	}
}

CString CContextMenuDlg::LookupGuidDict(const CString& clsid)
{
	if (!s_bDictLoaded)
		LoadGuidDictionary();

	// Normalize: lowercase, no braces
	CString key = clsid;
	key.MakeLower();
	if (!key.IsEmpty() && key[0] == _T('{'))
		key = key.Mid(1, key.GetLength() - 2);

	auto it = s_guidDict.find(key);
	if (it == s_guidDict.end())
		return CString();

	// Priority matches ContextMenuManager GuidInfo.GetText:
	//   ResText > [culture]-Text > Text
	if (!it->second.resText.IsEmpty())
	{
		// ContextMenuManager calls GetAbsStr before GetDirectString.
		// GetAbsStr resolves relative paths in ResText:
		//   - Plain filename like "shell32.dll" → search system paths
		//   - ".\xxx.dll" → relative to GUID file path
		//   - "..\xxx.dll" → parent of GUID file path
		//   - "*xxx.dll" → the GUID file itself
		// For system DLLs (shell32.dll, imageres.dll etc.), SHLoadIndirectString
		// already handles bare filenames, so we pass through directly.
		CString resolved = ResolveMUIString(it->second.resText);
		if (!resolved.IsEmpty()) return resolved;
	}
	if (!it->second.zhText.IsEmpty())
		return it->second.zhText;
	if (!it->second.text.IsEmpty())
	{
		// Text may be a MUI string or plain text
		CString resolved = ResolveMUIString(it->second.text);
		if (!resolved.IsEmpty()) return resolved;
		// If MUI resolution failed, return as-is (it's plain text)
		return it->second.text;
	}

	return CString();
}

CString CContextMenuDlg::ResolveClsidName(const CString& clsid)
{
	// Matches ContextMenuManager's GuidInfo.GetText:
	// Strategy order (highest priority first):
	//   1. Dictionary ResText → SHLoadIndirectString
	//   2. Dictionary zh-CN-Text
	//   3. Dictionary Text → SHLoadIndirectString
	//   4. Registry LocalizedString / InfoTip / default (3 CLSID paths)
	//   5. ProgID → ProgID friendly name
	//   6. InprocServer32/LocalServer32 → FileVersionInfo → FileName
	//   7. AuxUserType\2

	// Step 1: Dictionary lookup (highest priority, matches ContextMenuManager)
	CString dictName = LookupGuidDict(clsid);
	if (!dictName.IsEmpty())
		return dictName;

	// Ensure CLSID has braces for registry lookup
	CString clsidKey = clsid;
	if (!clsidKey.IsEmpty() && clsidKey[0] != _T('{'))
	{
		CString tmp;
		tmp.Format(_T("{%s}"), clsidKey);
		clsidKey = tmp;
	}

	static const struct { HKEY hRoot; const TCHAR* fmt; } clsidPaths[] = {
		{ HKEY_CLASSES_ROOT,   _T("CLSID\\%s") },
		{ HKEY_CLASSES_ROOT,   _T("WOW6432Node\\CLSID\\%s") },
		{ HKEY_LOCAL_MACHINE,  _T("SOFTWARE\\WOW6432Node\\Classes\\CLSID\\%s") },
	};

	CString friendly;
	HKEY hFirstKey = nullptr;  // Save first valid key for ProgID/InprocServer32 lookup
	bool bFound = false;

	for (const auto& cp : clsidPaths)
	{
		CString fullPath;
		fullPath.Format(cp.fmt, clsidKey);
		HKEY hKey = nullptr;
		if (RegOpenKeyEx(cp.hRoot, fullPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
			continue;

		if (!hFirstKey)
			hFirstKey = hKey;  // Save for later strategies; will be closed later

		TCHAR szVal[MAX_PATH * 2] = { 0 };
		DWORD cbVal = sizeof(szVal);
		DWORD dwType = 0;

		// Strategy 1: LocalizedString
		cbVal = sizeof(szVal); szVal[0] = 0;
		if (RegQueryValueEx(hKey, _T("LocalizedString"), nullptr, &dwType,
			(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
		{
			friendly = ResolveMUIString(szVal);
		}

		// Strategy 2: InfoTip
		if (friendly.IsEmpty())
		{
			cbVal = sizeof(szVal); szVal[0] = 0;
			if (RegQueryValueEx(hKey, _T("InfoTip"), nullptr, &dwType,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				friendly = ResolveMUIString(szVal);
			}
		}

		// Strategy 3: Default value
		if (friendly.IsEmpty())
		{
			cbVal = sizeof(szVal); szVal[0] = 0;
			if (RegQueryValueEx(hKey, nullptr, nullptr, &dwType,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				friendly = ResolveMUIString(szVal);
			}
		}

		if (!friendly.IsEmpty())
		{
			bFound = true;
			// Close current key; hFirstKey will be closed at end
			if (hKey != hFirstKey)
				RegCloseKey(hKey);
			break;
		}
		if (hKey != hFirstKey)
			RegCloseKey(hKey);
	}

	// If we found a name from the direct lookup, close hFirstKey and return
	if (bFound && hFirstKey)
	{
		RegCloseKey(hFirstKey);
		return friendly;
	}

	// Strategy 4: ProgID → friendly name (use first valid CLSID key)
	if (friendly.IsEmpty() && hFirstKey)
	{
		TCHAR szVal[MAX_PATH * 2] = { 0 };
		DWORD cbVal = sizeof(szVal);
		DWORD dwType = 0;
		HKEY hProgID = nullptr;
		if (RegOpenKeyEx(hFirstKey, _T("ProgID"), 0, KEY_READ, &hProgID) == ERROR_SUCCESS)
		{
			cbVal = sizeof(szVal); szVal[0] = 0;
			if (RegQueryValueEx(hProgID, nullptr, nullptr, &dwType,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				CString progID = szVal;
				HKEY hProg = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, progID, 0, KEY_READ, &hProg) == ERROR_SUCCESS)
				{
					cbVal = sizeof(szVal); szVal[0] = 0;
					if (RegQueryValueEx(hProg, nullptr, nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
						friendly = ResolveMUIString(szVal);
					RegCloseKey(hProg);
				}
				// Also check if ProgID has its own LocalizedString
				if (friendly.IsEmpty())
				{
					cbVal = sizeof(szVal); szVal[0] = 0;
					if (RegQueryValueEx(hProgID, _T("LocalizedString"), nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
						friendly = ResolveMUIString(szVal);
				}
				if (friendly.IsEmpty())
				{
					cbVal = sizeof(szVal); szVal[0] = 0;
					if (RegQueryValueEx(hProgID, _T("InfoTip"), nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
						friendly = ResolveMUIString(szVal);
				}
			}
			RegCloseKey(hProgID);
		}

		// Strategy 5: InprocServer32/LocalServer32 → FileVersionInfo.FileDescription → FileName
		if (friendly.IsEmpty())
		{
			for (const TCHAR* subKeyName : { _T("InprocServer32"), _T("LocalServer32") })
			{
				HKEY hSub = nullptr;
				if (RegOpenKeyEx(hFirstKey, subKeyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS)
				{
					cbVal = sizeof(szVal); szVal[0] = 0;
					if (RegQueryValueEx(hSub, nullptr, nullptr, &dwType,
						(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
					{
						CString dllPath = szVal;
						int nComma = dllPath.Find(_T(','));
						if (nComma > 0) dllPath = dllPath.Left(nComma);
						dllPath.Trim();

						if (PathFileExists(dllPath))
						{
							DWORD dwHandle = 0;
							DWORD dwSize = GetFileVersionInfoSize(dllPath, &dwHandle);
							if (dwSize > 0)
							{
								std::vector<BYTE> verData(dwSize);
								if (GetFileVersionInfo(dllPath, 0, dwSize, verData.data()))
								{
									struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } *lpTranslate = nullptr;
									UINT cbTranslate = 0;
									if (VerQueryValue(verData.data(), _T("\\VarFileInfo\\Translation"),
										(LPVOID*)&lpTranslate, &cbTranslate) && cbTranslate >= sizeof(LANGANDCODEPAGE))
									{
										TCHAR szBlock[128];
										_stprintf_s(szBlock, _T("\\StringFileInfo\\%04x%04x\\FileDescription"),
											lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
										TCHAR* pDesc = nullptr;
										UINT cbDesc = 0;
										if (VerQueryValue(verData.data(), szBlock, (LPVOID*)&pDesc, &cbDesc) && pDesc && pDesc[0])
											friendly = pDesc;
									}
								}
							}
							if (friendly.IsEmpty())
								friendly = PathFindFileName(dllPath);
						}
					}
					RegCloseKey(hSub);
					if (!friendly.IsEmpty()) break;
				}
			}
		}

		// Strategy 6: AuxUserType\2 (short display name)
		if (friendly.IsEmpty())
		{
			HKEY hAux = nullptr;
			if (RegOpenKeyEx(hFirstKey, _T("AuxUserType\\2"), 0, KEY_READ, &hAux) == ERROR_SUCCESS)
			{
				cbVal = sizeof(szVal); szVal[0] = 0;
				if (RegQueryValueEx(hAux, nullptr, nullptr, &dwType,
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
					friendly = szVal;
				RegCloseKey(hAux);
			}
		}

		RegCloseKey(hFirstKey);
	}
	else
	{
		// No CLSID path found at all
		if (hFirstKey)
			RegCloseKey(hFirstKey);
	}

	return friendly;
}

CString CContextMenuDlg::ResolveMUIString(const CString& raw)
{
	// Matches ContextMenuManager's ResourceString.GetDirectString exactly:
	//   SHLoadIndirectString on every value; non-@ strings pass through;
	//   @ strings are resolved; on failure returns empty.
	if (raw.IsEmpty())
		return raw;
	if (raw[0] != _T('@'))
		return raw;

	// C# GetDirectString: just calls SHLoadIndirectString, returns result.
	// Does NOT check return value; empty buffer = empty string on failure.
	TCHAR szResult[1024] = { 0 };
	HRESULT hr = SHLoadIndirectString(raw, szResult, 1024, nullptr);
	if (SUCCEEDED(hr) && szResult[0])
		return CString(szResult);

	// Resolution failed — return empty (matches GetDirectString behavior)
	return CString();
}

// ============================================================================
// Registry ownership takeover — matches ContextMenuManager's RegTrustedInstaller
// ============================================================================

static bool TakeRegKeyOwnership(HKEY hRoot, const CString& subKey)
{
	// First, try to open with write access — if this works, we already have access
	HKEY hKey = nullptr;
	if (RegOpenKeyEx(hRoot, subKey, 0, KEY_WRITE | KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return true;
	}

	// We can read but not write. Try to take ownership.
	// Enable SeTakeOwnershipPrivilege and SeRestorePrivilege
	HANDLE hToken = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	auto EnablePrivilege = [&](LPCTSTR privName) -> bool {
		LUID luid;
		if (!LookupPrivilegeValue(nullptr, privName, &luid))
			return false;
		TOKEN_PRIVILEGES tp = {};
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
		return (GetLastError() == ERROR_SUCCESS);
	};

	bool bOk = EnablePrivilege(SE_TAKE_OWNERSHIP_NAME) && EnablePrivilege(SE_RESTORE_NAME);
	CloseHandle(hToken);
	if (!bOk) return false;

	// Open with WRITE_OWNER access
	if (RegOpenKeyEx(hRoot, subKey, 0, WRITE_OWNER | READ_CONTROL | WRITE_DAC, &hKey) != ERROR_SUCCESS)
		return false;

	// Get current user SID
	DWORD dwUserSize = 256;
	TCHAR szUser[256];
	if (!GetUserName(szUser, &dwUserSize))
	{
		RegCloseKey(hKey);
		return false;
	}

	DWORD dwSidSize = 0;
	DWORD dwDomainSize = 0;
	SID_NAME_USE snu;
	LookupAccountName(nullptr, szUser, nullptr, &dwSidSize, nullptr, &dwDomainSize, &snu);

	std::vector<BYTE> sidBuf(dwSidSize);
	std::vector<TCHAR> domainBuf(dwDomainSize);
	PSID pUserSid = sidBuf.data();

	if (!LookupAccountName(nullptr, szUser, pUserSid, &dwSidSize, domainBuf.data(), &dwDomainSize, &snu))
	{
		RegCloseKey(hKey);
		return false;
	}

	// Build security descriptor with new owner and full control
	SECURITY_DESCRIPTOR sd = {};
	if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
	{
		RegCloseKey(hKey);
		return false;
	}

	if (!SetSecurityDescriptorOwner(&sd, pUserSid, FALSE))
	{
		RegCloseKey(hKey);
		return false;
	}

	EXPLICIT_ACCESS ea = {};
	ea.grfAccessPermissions = KEY_ALL_ACCESS;
	ea.grfAccessMode = SET_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea.Trustee.ptstrName = (LPTSTR)pUserSid;

	PACL pNewDacl = nullptr;
	DWORD dwErr = SetEntriesInAcl(1, &ea, nullptr, &pNewDacl);
	if (dwErr != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return false;
	}

	if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewDacl, FALSE))
	{
		LocalFree(pNewDacl);
		RegCloseKey(hKey);
		return false;
	}

	dwErr = RegSetKeySecurity(hKey, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &sd);
	LocalFree(pNewDacl);
	RegCloseKey(hKey);

	return (dwErr == ERROR_SUCCESS);
}

// ============================================================================
// Default verb name resolution — matches ContextMenuManager's DefaultNameIndexs
// ============================================================================

static CString ResolveDefaultVerbName(const CString& verb)
{
	struct VerbName { const TCHAR* verb; int id; };
	static const VerbName verbs[] = {
		{ _T("open"), 8496 },
		{ _T("edit"), 8516 },
		{ _T("print"), 8497 },
		{ _T("find"), 8503 },
		{ _T("play"), 8498 },
		{ _T("runas"), 8505 },
		{ _T("explore"), 8502 },
		{ _T("preview"), 8499 },
	};

	CString verbLower = verb;
	verbLower.MakeLower();
	for (const auto& vn : verbs)
	{
		if (verbLower == vn.verb)
		{
			CString raw;
			raw.Format(_T("@windows.storage.dll,-%d"), vn.id);
			// SHLoadIndirectString handles the MUI resolution
			TCHAR szResult[1024] = { 0 };
			HRESULT hr = SHLoadIndirectString(raw, szResult, 1024, nullptr);
			if (SUCCEEDED(hr) && szResult[0])
				return CString(szResult);
			break;
		}
	}
	return verb;
}

// ============================================================================
// Core scanning logic — matches ContextMenuManager's ShellList.LoadItems
// ============================================================================

void CContextMenuDlg::ScanScene(const CString& basePath, const CString& sceneName, std::set<CString>& seen)
{
	// Scan static verbs: <basePath>\shell
	// ContextMenuManager: LoadShellItems(GetShellPath(scenePath))
	CString shellPath = basePath + _T("\\shell");
	ScanShellVerbs(shellPath, sceneName, m_entries, seen);

	// Scan ShellEx handlers: <basePath>\shellex (both ContextMenuHandlers and -ContextMenuHandlers)
	// ContextMenuManager: LoadShellExItems(GetShellExPath(scenePath))
	CString shellexBase = basePath + _T("\\shellex");
	ScanShellExHandlers(shellexBase, sceneName, m_entries, seen);
}

void CContextMenuDlg::ScanShellVerbs(const CString& shellPath, const CString& sceneName,
	std::vector<MenuEntry>& entries, std::set<CString>& seen)
{
	// Matches ContextMenuManager's ShellList.LoadShellItems:
	//   RegTrustedInstaller.TakeRegTreeOwnerShip(shellKey.Name);
	//   foreach (string keyName in shellKey.GetSubKeyNames())
	//       this.AddItem(new ShellItem($@"{shellPath}\{keyName}"));
	//
	// Try KEY_READ first; if that fails, take ownership and retry
	HKEY hShell = nullptr;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, shellPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
	{
		TakeRegKeyOwnership(HKEY_CLASSES_ROOT, shellPath);
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, shellPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
			return;
	}

	DWORD dwIndex = 0;
	TCHAR szVerb[MAX_PATH];
	DWORD cbVerb = MAX_PATH;
	while (RegEnumKeyEx(hShell, dwIndex, szVerb, &cbVerb, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		CString verbPath = shellPath + _T("\\") + szVerb;
		CString dedupKey = shellPath + _T("|") + szVerb;
		if (seen.find(dedupKey) != seen.end())
		{
			dwIndex++;
			cbVerb = MAX_PATH;
			continue;
		}
		seen.insert(dedupKey);

		// Open the verb key for reading all properties
		HKEY hVerb = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, verbPath, 0, KEY_READ, &hVerb) != ERROR_SUCCESS)
		{
			TakeRegKeyOwnership(HKEY_CLASSES_ROOT, verbPath);
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, verbPath, 0, KEY_READ, &hVerb) != ERROR_SUCCESS)
			{
				dwIndex++;
				cbVerb = MAX_PATH;
				continue;
			}
		}

		// --- ItemVisible check (matches ContextMenuManager ShellItem.ItemVisible.get) ---
		// Checks: HideBasedOnVelocityId == 0x639bc8 (Win10 1703+),
		//         LegacyDisable, ProgrammaticAccessOnly, CommandFlags % 16 >= 8
		BOOL bDisabled = FALSE;
		{
			DWORD dwHide = 0;
			DWORD cbHide = sizeof(dwHide);
			if (RegQueryValueEx(hVerb, _T("HideBasedOnVelocityId"), nullptr, nullptr,
				(LPBYTE)&dwHide, &cbHide) == ERROR_SUCCESS && dwHide == 0x639bc8)
				bDisabled = TRUE;

			if (!bDisabled)
			{
				TCHAR szTest[4] = { 0 };
				DWORD cbTest = sizeof(szTest);
				if (RegQueryValueEx(hVerb, _T("LegacyDisable"), nullptr, nullptr,
					(LPBYTE)szTest, &cbTest) == ERROR_SUCCESS)
					bDisabled = TRUE;
			}

			if (!bDisabled)
			{
				TCHAR szTest[4] = { 0 };
				DWORD cbTest = sizeof(szTest);
				if (RegQueryValueEx(hVerb, _T("ProgrammaticAccessOnly"), nullptr, nullptr,
					(LPBYTE)szTest, &cbTest) == ERROR_SUCCESS)
					bDisabled = TRUE;
			}

			if (!bDisabled)
			{
				DWORD dwCmdFlags = 0;
				DWORD cbCmdFlags = sizeof(dwCmdFlags);
				if (RegQueryValueEx(hVerb, _T("CommandFlags"), nullptr, nullptr,
					(LPBYTE)&dwCmdFlags, &cbCmdFlags) == ERROR_SUCCESS && (dwCmdFlags % 16) >= 8)
					bDisabled = TRUE;
			}
		}

		// --- Extended check (Shift+right-click only) ---
		BOOL bExtended = FALSE;
		{
			TCHAR szTest[4] = { 0 };
			DWORD cbTest = sizeof(szTest);
			if (RegQueryValueEx(hVerb, _T("Extended"), nullptr, nullptr,
				(LPBYTE)szTest, &cbTest) == ERROR_SUCCESS)
				bExtended = TRUE;
		}

		// --- Resolve display name (matches ContextMenuManager ShellItem.ItemText exactly) ---
		// Priority: MUIVerb > default value > DefaultNameIndexs > KeyName
		// Each value passes through GetDirectString (SHLoadIndirectString).
		// Multi-level menus (SubCommands/ExtendedSubCommandsKey) skip default value.
		CString displayName;
		{
			// Check if this is a multi-level menu (has SubCommands or ExtendedSubCommandsKey)
			BOOL bIsMultiItem = FALSE;
			TCHAR szVal[MAX_PATH] = { 0 };
			DWORD cbVal = sizeof(szVal);
			if (RegQueryValueEx(hVerb, _T("SubCommands"), nullptr, nullptr,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				bIsMultiItem = TRUE;
			cbVal = sizeof(szVal); szVal[0] = 0;
			if (RegQueryValueEx(hVerb, _T("ExtendedSubCommandsKey"), nullptr, nullptr,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				bIsMultiItem = TRUE;

			// Try MUIVerb first
			cbVal = sizeof(szVal); szVal[0] = 0;
			if (RegQueryValueEx(hVerb, _T("MUIVerb"), nullptr, nullptr,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				displayName = ResolveMUIString(szVal);
			}

			// Fall back to default value (Skip for multi-level menus —
			// ContextMenuManager: "多级母菜单不支持使用默认值作为名称")
			if (displayName.IsEmpty() && !bIsMultiItem)
			{
				cbVal = sizeof(szVal); szVal[0] = 0;
				if (RegQueryValueEx(hVerb, nullptr, nullptr, nullptr,
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				{
					displayName = ResolveMUIString(szVal);
				}
			}

			// Fall back to DefaultNameIndexs (open→Open, edit→Edit, etc.)
			if (displayName.IsEmpty())
				displayName = ResolveDefaultVerbName(szVerb);

			// Final fallback: use the verb name itself
			if (displayName.IsEmpty())
				displayName = szVerb;
		}

		// Read command from "command" subkey default value
		// ContextMenuManager: ItemCommand = Registry.GetValue(CommandPath, "", null)?.ToString()
		CString command;
		{
			CString cmdPath = verbPath + _T("\\command");
			HKEY hCmd = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, cmdPath, 0, KEY_READ, &hCmd) == ERROR_SUCCESS)
			{
				TCHAR szCmd[MAX_PATH * 2] = { 0 };
				DWORD cbCmd = sizeof(szCmd);
				DWORD dwType = 0;
				if (RegQueryValueEx(hCmd, nullptr, nullptr, &dwType,
					(LPBYTE)szCmd, &cbCmd) == ERROR_SUCCESS)
					command = szCmd;
				RegCloseKey(hCmd);
			}
		}

		RegCloseKey(hVerb);

		entries.push_back({
			sceneName,                  // location
			szVerb,                     // keyName
			displayName,                // displayName
			command,                    // command
			shellPath,                  // regPath (parent path)
			false,                      // bIsShellEx
			bExtended != FALSE,         // bExtended
			bDisabled != FALSE,         // bDisabled
			!bDisabled                  // bEnabled
		});

		dwIndex++;
		cbVerb = MAX_PATH;
	}
	RegCloseKey(hShell);
}

// ============================================================================
// ShellEx handler key display name resolution
// Tries multiple sources in order:
//   1. ShellEx handler key's own LocalizedString → SHLoadIndirectString
//   2. ShellEx handler key's own InfoTip → SHLoadIndirectString
//   3. ShellEx handler key's default value → SHLoadIndirectString
//   4. CLSID's LocalizedString / InfoTip / default value
//   5. ProgID chain
//   6. File version info from InprocServer32
// ============================================================================

CString CContextMenuDlg::ResolveShellExKeyName(HKEY hHandlerKey, const CString& clsid)
{
	// Strategy 1: Read display name values directly from the ShellEx handler key
	TCHAR szVal[MAX_PATH * 2] = { 0 };
	DWORD cbVal = sizeof(szVal);
	DWORD dwType = 0;

	// 1a: LocalizedString on the handler key itself
	cbVal = sizeof(szVal); szVal[0] = 0;
	if (RegQueryValueEx(hHandlerKey, _T("LocalizedString"), nullptr, &dwType,
		(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
	{
		CString name = CContextMenuDlg::ResolveMUIString(szVal);
		if (!name.IsEmpty()) return name;
	}

	// 1b: InfoTip on the handler key itself
	cbVal = sizeof(szVal); szVal[0] = 0;
	if (RegQueryValueEx(hHandlerKey, _T("InfoTip"), nullptr, &dwType,
		(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
	{
		CString name = CContextMenuDlg::ResolveMUIString(szVal);
		if (!name.IsEmpty()) return name;
	}

	// Strategy 2: Resolve via CLSID (if available)
	if (!clsid.IsEmpty())
	{
		CString name = CContextMenuDlg::ResolveClsidName(clsid);
		if (!name.IsEmpty()) return name;
	}

	return CString();
}

void CContextMenuDlg::ScanShellExHandlers(const CString& shellexBase, const CString& sceneName,
	std::vector<MenuEntry>& entries, std::set<CString>& seen)
{
	static const struct { const TCHAR* folder; bool bEnabled; } folders[] = {
		{ _T("ContextMenuHandlers"), true },
		{ _T("-ContextMenuHandlers"), false },
	};

	for (const auto& fi : folders)
	{
		CString folderPath = shellexBase + _T("\\") + fi.folder;
		HKEY hShell = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, folderPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
		{
			TakeRegKeyOwnership(HKEY_CLASSES_ROOT, folderPath);
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, folderPath, 0, KEY_READ, &hShell) != ERROR_SUCCESS)
				continue;
		}

		DWORD dwIndex = 0;
		TCHAR szSubKey[MAX_PATH];
		DWORD cbSubKey = MAX_PATH;
		while (RegEnumKeyEx(hShell, dwIndex, szSubKey, &cbSubKey,
			nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			CString subKeyPath = folderPath + _T("\\") + szSubKey;
			CString dedupKey = folderPath + _T("|") + szSubKey;
			if (seen.find(dedupKey) != seen.end())
			{
				dwIndex++;
				cbSubKey = MAX_PATH;
				continue;
			}
			seen.insert(dedupKey);

			// Open the handler key
			HKEY hHandler = nullptr;
			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyPath, 0, KEY_READ, &hHandler) != ERROR_SUCCESS)
			{
				TakeRegKeyOwnership(HKEY_CLASSES_ROOT, subKeyPath);
				RegOpenKeyEx(HKEY_CLASSES_ROOT, subKeyPath, 0, KEY_READ, &hHandler);
			}

			// Read CLSID: default value first, then key name if it looks like a GUID
			CString clsid;
			if (hHandler)
			{
				TCHAR szVal[MAX_PATH] = { 0 };
				DWORD cbVal = sizeof(szVal);
				if (RegQueryValueEx(hHandler, nullptr, nullptr, nullptr,
					(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
				{
					clsid = szVal;
				}
			}
			if (clsid.IsEmpty() && szSubKey[0] == _T('{'))
				clsid = szSubKey;
			// Normalize CLSID for registry lookup: ensure braces are present
			// Registry CLSID keys use format {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
			CString clsidForLookup = clsid;
			if (!clsidForLookup.IsEmpty() && clsidForLookup[0] != _T('{'))
			{
				// Add braces if not present
				CString tmp;
				tmp.Format(_T("{%s}"), clsidForLookup);
				clsidForLookup = tmp;
			}
			// Also keep a copy without braces for comparison purposes
			CString clsidNoBraces = clsid;
			if (!clsidNoBraces.IsEmpty() && clsidNoBraces[0] == _T('{'))
				clsidNoBraces = clsidNoBraces.Mid(1, clsidNoBraces.GetLength() - 2);

			// Resolve display name using multiple sources
			CString displayName;
			if (hHandler)
			{
				displayName = ResolveShellExKeyName(hHandler, clsidForLookup);
			}
			else if (!clsidForLookup.IsEmpty())
			{
				displayName = ResolveClsidName(clsidForLookup);
			}

			// Fall back: use key name as-is, but try to resolve if it's a ProgID
			if (displayName.IsEmpty())
			{
				displayName = szSubKey;
				if (szSubKey[0] != _T('{'))
				{
					// Key name is not a GUID — try as ProgID
					HKEY hProg = nullptr;
					if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szSubKey, 0, KEY_READ, &hProg) == ERROR_SUCCESS)
					{
						TCHAR szVal[MAX_PATH] = { 0 };
						DWORD cbVal = sizeof(szVal);
						if (RegQueryValueEx(hProg, nullptr, nullptr, nullptr,
							(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
						{
							displayName = ResolveMUIString(szVal);
						}
						RegCloseKey(hProg);
					}
				}
			}

			// Read DLL path from CLSID's InprocServer32 as the "command"
			CString command;
			if (!clsidForLookup.IsEmpty())
			{
				CString inprocKey;
				inprocKey.Format(_T("CLSID\\%s\\InprocServer32"), clsidForLookup);
				HKEY hInproc = nullptr;
				if (RegOpenKeyEx(HKEY_CLASSES_ROOT, inprocKey, 0, KEY_READ, &hInproc) == ERROR_SUCCESS)
				{
					TCHAR szDll[MAX_PATH * 2] = { 0 };
					DWORD cbDll = sizeof(szDll);
					if (RegQueryValueEx(hInproc, nullptr, nullptr, nullptr,
						(LPBYTE)szDll, &cbDll) == ERROR_SUCCESS && szDll[0])
						command = szDll;
					RegCloseKey(hInproc);
				}
			}

			if (hHandler)
				RegCloseKey(hHandler);

			entries.push_back({
				sceneName,       // location
				szSubKey,        // keyName
				displayName,     // displayName
				command,         // command
				folderPath,      // regPath (parent path)
				true,            // bIsShellEx
				false,           // bExtended
				false,           // bDisabled
				fi.bEnabled      // bEnabled
			});

			dwIndex++;
			cbSubKey = MAX_PATH;
		}
		RegCloseKey(hShell);
	}
}

void CContextMenuDlg::ScanEntries(const CString& filter)
{
	m_entries.clear();
	std::set<CString> seen;

	// In "全部" mode (filter empty or "全部"), scan all scenes.
	// In filtered mode, scan only the matching scene.
	// ContextMenuManager: each scene is independent, no cross-scene dedup needed.
	// We use cross-scene dedup for "全部" mode to avoid showing the same item twice.
	bool bScanAll = filter.IsEmpty() || filter == _T("全部");

	for (const auto& scene : m_scenes)
	{
		if (scene.basePath.IsEmpty()) continue; // skip "全部"

		if (!bScanAll && scene.name != filter)
			continue;

		ScanScene(scene.basePath, scene.name, seen);

		// Additional paths for specific scenes (matching ContextMenuManager's LoadItems):
		//   Library: also scans LibraryFolder\Background and UserLibraryFolder
		//   ExeFile:  also scans exefile (GetOpenModePath(".exe"))
		if (scene.name == _T("库文件夹"))
		{
			ScanScene(_T("LibraryFolder\\Background"), scene.name, seen);
			ScanScene(_T("UserLibraryFolder"), scene.name, seen);
		}
		else if (scene.name == _T("exe文件"))
		{
			ScanScene(_T("exefile"), scene.name, seen);
		}
	}
}

// ============================================================================
// List display
// ============================================================================

void CContextMenuDlg::RefreshList()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		int idx = pList->InsertItem((int)i, m_entries[i].location);
		pList->SetItemText(idx, 1, m_entries[i].displayName);

		// Column 2: type
		pList->SetItemText(idx, 2, m_entries[i].bIsShellEx ? _T("ShellEx") : _T("静态"));

		// Column 3: visibility
		CString strVis;
		if (!m_entries[i].bEnabled)
			strVis = _T("已禁用");
		else if (m_entries[i].bDisabled)
			strVis = _T("系统禁用");
		else if (m_entries[i].bExtended)
			strVis = _T("Shift显示");
		else
			strVis = _T("正常");
		pList->SetItemText(idx, 3, strVis);

		// Column 4: key name
		pList->SetItemText(idx, 4, m_entries[i].keyName);

		// Column 5: command
		pList->SetItemText(idx, 5, m_entries[i].command);

		// Gray out disabled items
		if (!m_entries[i].bEnabled)
			pList->SetItemState(idx, LVIS_CUT, LVIS_CUT);
	}
	pList->SetRedraw(TRUE);

	CString status;
	status.Format(_T("共找到 %d 个右键菜单项"), (int)m_entries.size());
	// Append dictionary path info
	if (!s_dictPath.IsEmpty())
	{
		CString tmp;
		tmp.Format(_T("  |  字典：%s"), s_dictPath);
		status += tmp;
	}
	else
	{
		CString cachePath = GetCachePath();
		if (!cachePath.IsEmpty() && PathFileExists(cachePath))
		{
			CString tmp;
			tmp.Format(_T("  |  字典：内嵌+缓存 (%s)"), cachePath);
			status += tmp;
		}
		else
		{
			status += _T("  |  字典：内嵌硬编码");
		}
	}
	UpdateStatus(status);
}

void CContextMenuDlg::UpdateStatus(const CString& text)
{
	SetDlgItemText(IDC_STATIC_CM_STATUS, text);
}

// ============================================================================
// Dialog initialization
// ============================================================================

BOOL CContextMenuDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Read layout positions
	auto ReadRect = [&](int id) -> CRect {
		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(id);
		if (pWnd) { pWnd->GetWindowRect(&rc); ScreenToClient(&rc); }
		return rc;
	};

	CRect rc = ReadRect(IDC_LIST_CM_ENTRIES);
	m_listLeft = rc.left; m_listTop = rc.top;

	rc = ReadRect(IDC_STATIC_CM_STATUS);
	m_statusTop = rc.top;

	// Initialize list columns
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (pList)
	{
		pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		pList->InsertColumn(0, _T("位置"),     LVCFMT_LEFT, 50);
		pList->InsertColumn(1, _T("显示名称"), LVCFMT_LEFT, 150);
		pList->InsertColumn(2, _T("类型"),     LVCFMT_LEFT, 60);
		pList->InsertColumn(3, _T("可见性"),   LVCFMT_LEFT, 70);
		pList->InsertColumn(4, _T("键名"),     LVCFMT_LEFT, 70);
		pList->InsertColumn(5, _T("命令"),     LVCFMT_LEFT, 130);
	}

	InitLocations();
	LoadGuidDictionary();

	// Show current dictionary path on status bar
	{
		CString dictInfo;
		CString cachePath = GetCachePath();
		if (!s_dictPath.IsEmpty())
			dictInfo.Format(_T("当前字典：%s  |  缓存：%s"), s_dictPath, cachePath);
		else if (!cachePath.IsEmpty() && PathFileExists(cachePath))
			dictInfo.Format(_T("当前字典：内嵌 + 缓存  |  缓存：%s"), cachePath);
		else
			dictInfo.Format(_T("当前字典：内嵌硬编码  |  缓存：%s"), cachePath);
		UpdateStatus(dictInfo);
	}

	// Scan all scenes (filter empty = "全部" mode)
	ScanEntries(_T(""));
	RefreshList();

	AdjustColumnWidths();

	LoadSelfContextMenuState();
	LoadWin11ClassicState();

	return TRUE;
}

void CContextMenuDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	delete this;
}

void CContextMenuDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED && IsWindow(m_hWnd))
	{
		CRect rcClient;
		GetClientRect(&rcClient);

		CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
		if (pList)
		{
			pList->SetWindowPos(nullptr, m_listLeft, m_listTop,
				rcClient.Width() - m_listLeft - 15,
				m_statusTop - m_listTop - 5, SWP_NOZORDER);
			AdjustColumnWidths();
		}

		CWnd* pStatus = GetDlgItem(IDC_STATIC_CM_STATUS);
		if (pStatus)
			pStatus->SetWindowPos(nullptr, m_listLeft, m_statusTop,
				0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
}

void CContextMenuDlg::AdjustColumnWidths()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList || !pList->GetSafeHwnd()) return;

	CRect rcList;
	pList->GetClientRect(&rcList);
	int nWidth = rcList.Width();
	if (nWidth <= 0) return;

	// Set a minimum total width so columns are wider than the list,
	// which triggers the horizontal scrollbar for long content.
	int nMinTotal = (nWidth + 1 > 1200) ? (nWidth + 1) : 1200;
	pList->SetColumnWidth(0, nMinTotal * 10 / 100 + 10);
	pList->SetColumnWidth(1, nMinTotal * 20 / 100);
	pList->SetColumnWidth(2, nMinTotal * 8 / 100);
	pList->SetColumnWidth(3, nMinTotal * 10 / 100);
	pList->SetColumnWidth(4, nMinTotal * 12 / 100);
	pList->SetColumnWidth(5, nMinTotal * 40 / 100);
}

// ============================================================================
// Refresh / filter
// ============================================================================

void CContextMenuDlg::OnBnClickedRefresh()
{
	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_CM_LOCATION);
	CString filter;
	if (pCombo)
	{
		int sel = pCombo->GetCurSel();
		if (sel >= 0 && sel < (int)m_scenes.size())
			filter = m_scenes[sel].name;
	}
	ScanEntries(filter);
	RefreshList();
}

void CContextMenuDlg::OnCbnSelchangeLocation()
{
	OnBnClickedRefresh();
}

// ============================================================================
// Registry utility helpers
// ============================================================================

bool CContextMenuDlg::DeleteRegistryKeyRecursive(HKEY hParent, const CString& subKey)
{
	return (SHDeleteKey(hParent, subKey) == ERROR_SUCCESS);
}

// Forward declaration
static bool ResolveWritableHive(const CString& hkcrPath, HKEY& outRoot, CString& outSubPath);

void CContextMenuDlg::OpenRegEditToPath(HKEY hRoot, const CString& path)
{
	CString fullRegPath;
	if (hRoot == HKEY_CLASSES_ROOT)
	{
		HKEY realRoot = nullptr;
		CString realSubPath;
		if (ResolveWritableHive(path, realRoot, realSubPath))
		{
			if (realRoot == HKEY_CURRENT_USER)
				fullRegPath = _T("HKEY_CURRENT_USER\\") + realSubPath;
			else if (realRoot == HKEY_LOCAL_MACHINE)
				fullRegPath = _T("HKEY_LOCAL_MACHINE\\") + realSubPath;
		}
		if (fullRegPath.IsEmpty())
			fullRegPath = _T("HKEY_CLASSES_ROOT\\") + path;
	}
	else
		fullRegPath = path;

	HKEY hKey = nullptr;
	CString regeditKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit");
	if (RegOpenKeyEx(HKEY_CURRENT_USER, regeditKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
	{
		RegSetValueEx(hKey, _T("LastKey"), 0, REG_SZ,
			(LPBYTE)fullRegPath.GetString(),
			(fullRegPath.GetLength() + 1) * sizeof(TCHAR));
		RegCloseKey(hKey);
	}

	ShellExecute(nullptr, _T("open"), _T("regedit.exe"), nullptr, nullptr, SW_SHOWNORMAL);
}

static bool ResolveWritableHive(const CString& hkcrPath, HKEY& outRoot, CString& outSubPath)
{
	CString subPath = _T("Software\\Classes\\") + hkcrPath;
	HKEY hTest = nullptr;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, subPath, 0, KEY_READ, &hTest) == ERROR_SUCCESS)
	{
		RegCloseKey(hTest);
		outRoot = HKEY_CURRENT_USER;
		outSubPath = subPath;
		return true;
	}

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subPath, 0, KEY_READ, &hTest) == ERROR_SUCCESS)
	{
		RegCloseKey(hTest);
		outRoot = HKEY_LOCAL_MACHINE;
		outSubPath = subPath;
		return true;
	}

	return false;
}

static bool CopyRegistryKey(HKEY hSrc, HKEY hDstParent, const CString& srcSubKey, const CString& dstSubKey)
{
	// Recursively copy a registry key and all its subkeys/values.
	// Used by ShellEx move operation (ContextMenuManager's RegistryEx.CopyTo).
	HKEY hSrcKey = nullptr;
	if (RegOpenKeyEx(hSrc, srcSubKey, 0, KEY_READ, &hSrcKey) != ERROR_SUCCESS)
		return false;

	HKEY hDstKey = nullptr;
	if (RegCreateKeyEx(hDstParent, dstSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
		KEY_WRITE | KEY_READ, nullptr, &hDstKey, nullptr) != ERROR_SUCCESS)
	{
		RegCloseKey(hSrcKey);
		return false;
	}

	// Copy all values
	DWORD dwIndex = 0;
	TCHAR szName[MAX_PATH];
	DWORD cbName = MAX_PATH;
	BYTE szValue[8192];
	DWORD cbValue = sizeof(szValue);
	DWORD dwType = 0;
	while (RegEnumValue(hSrcKey, dwIndex, szName, &cbName, nullptr, &dwType, szValue, &cbValue) == ERROR_SUCCESS)
	{
		RegSetValueEx(hDstKey, szName, 0, dwType, szValue, cbValue);
		dwIndex++;
		cbName = MAX_PATH;
		cbValue = sizeof(szValue);
	}

	// Recursively copy subkeys
	dwIndex = 0;
	TCHAR szSubKey[MAX_PATH];
	DWORD cbSubKey = MAX_PATH;
	while (RegEnumKeyEx(hSrcKey, dwIndex, szSubKey, &cbSubKey, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		CopyRegistryKey(hSrcKey, hDstKey, szSubKey, szSubKey);
		dwIndex++;
		cbSubKey = MAX_PATH;
	}

	RegCloseKey(hSrcKey);
	RegCloseKey(hDstKey);
	return true;
}

bool CContextMenuDlg::IsRunningAsAdmin()
{
	BOOL bIsAdmin = FALSE;
	PSID pAdminGroup = nullptr;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&NtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &pAdminGroup))
	{
		CheckTokenMembership(nullptr, pAdminGroup, &bIsAdmin);
		FreeSid(pAdminGroup);
	}
	return bIsAdmin != FALSE;
}

// ============================================================================
// Self context menu state (add/remove our own folder context menu)
// ============================================================================

void CContextMenuDlg::LoadSelfContextMenuState()
{
	HKEY hKey = nullptr;
	CString path = _T("Directory\\shell\\MFCApplication1");
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		CheckDlgButton(IDC_CHECK_CM_FOLDER, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(IDC_CHECK_CM_FOLDER, BST_UNCHECKED);
	}
}

void CContextMenuDlg::SaveSelfContextMenuState(bool bEnable)
{
	TCHAR szExe[MAX_PATH];
	GetModuleFileName(nullptr, szExe, MAX_PATH);
	CString exePath = szExe;

	CString baseKey = _T("Directory\\shell\\MFCApplication1");

	if (bEnable)
	{
		HKEY hKey = nullptr;
		DWORD dwDisp = 0;
		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, baseKey, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)_T("用 MFC工具箱打开"),
				(DWORD)((_tcslen(_T("用 MFC工具箱打开")) + 1) * sizeof(TCHAR)));
			RegSetValueEx(hKey, _T("Icon"), 0, REG_SZ,
				(LPBYTE)exePath.GetString(),
				(exePath.GetLength() + 1) * sizeof(TCHAR));
			RegCloseKey(hKey);
		}

		CString cmdKey = baseKey + _T("\\command");
		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, cmdKey, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			CString cmd;
			cmd.Format(_T("\"%s\" \"%s\""), exePath, _T("%1"));
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)cmd.GetString(),
				(cmd.GetLength() + 1) * sizeof(TCHAR));
			RegCloseKey(hKey);
		}
	}
	else
	{
		DeleteRegistryKeyRecursive(HKEY_CLASSES_ROOT, baseKey);
	}
}

void CContextMenuDlg::OnBnClickedCheckFolder()
{
	BOOL bChecked = IsDlgButtonChecked(IDC_CHECK_CM_FOLDER);
	SaveSelfContextMenuState(bChecked == BST_CHECKED);

	CString msg = bChecked ? _T("已添加文件夹右键菜单项") : _T("已移除文件夹右键菜单项");
	UpdateStatus(msg);
	OnBnClickedRefresh();
}

// ============================================================================
// Win11 classic context menu
// ============================================================================

void CContextMenuDlg::LoadWin11ClassicState()
{
	const CString strKey = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32");

	HKEY hKey = nullptr;
	BOOL bEnabled = FALSE;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, strKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		bEnabled = TRUE;
		RegCloseKey(hKey);
	}
	CheckDlgButton(IDC_CHECK_CM_WIN11_CLASSIC, bEnabled ? BST_CHECKED : BST_UNCHECKED);
}

void CContextMenuDlg::SaveWin11ClassicState(bool bEnable)
{
	const CString strBase = _T("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}");
	const CString strInproc = strBase + _T("\\InprocServer32");

	if (bEnable)
	{
		HKEY hKey = nullptr;
		DWORD dwDisp = 0;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, strInproc, 0, nullptr, 0,
			KEY_WRITE, nullptr, &hKey, &dwDisp) == ERROR_SUCCESS)
		{
			const TCHAR* szEmpty = _T("");
			RegSetValueEx(hKey, nullptr, 0, REG_SZ,
				(LPBYTE)szEmpty, sizeof(TCHAR));
			RegCloseKey(hKey);
		}
	}
	else
	{
		DeleteRegistryKeyRecursive(HKEY_CURRENT_USER, strBase);
	}

	AfxGetApp()->WriteProfileInt(_T("ContextMenu"), _T("Win11Classic"), bEnable ? 1 : 0);
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	RestartExplorer();
}

void CContextMenuDlg::OnBnClickedCheckWin11Classic()
{
	BOOL bCheck = IsDlgButtonChecked(IDC_CHECK_CM_WIN11_CLASSIC);
	SaveWin11ClassicState(bCheck == BST_CHECKED);

	CString msg;
	if (bCheck)
		msg = _T("已启用Win11经典菜单，正在重启资源管理器...");
	else
		msg = _T("已恢复Win11新菜单，正在重启资源管理器...");
	UpdateStatus(msg);
}

void CContextMenuDlg::RestartExplorer()
{
	HWND hwndShell = ::FindWindow(_T("Progman"), nullptr);
	if (hwndShell)
	{
		::PostMessage(hwndShell, WM_QUIT, 0, 0);
		Sleep(500);
	}
	system("taskkill /f /im explorer.exe >nul 2>&1");
	Sleep(1000);
	system("start explorer.exe");
}

// ============================================================================
// Toggle enable/disable — matches ContextMenuManager logic exactly
// ============================================================================

void CContextMenuDlg::ToggleEntry(int index)
{
	if (index < 0 || index >= (int)m_entries.size()) return;

	auto& entry = m_entries[index];

	// Build the HKCR-relative full path: regPath\keyName (e.g. "*\shell\edit")
	CString hkcrFullPath = entry.regPath + _T("\\") + entry.keyName;

	// Clean up any previous HKCU override from earlier attempts
	// (ensures HKLM writes are not masked by stale HKCU entries in merged view)
	CString hkcuCleanup = _T("Software\\Classes\\") + hkcrFullPath;
	SHDeleteKey(HKEY_CURRENT_USER, hkcuCleanup);

	if (entry.bIsShellEx)
	{
		// --- ShellEx handler: move between ContextMenuHandlers and -ContextMenuHandlers ---
		// Matches C# ShellExItem.ItemVisible.set:
		//   RegistryEx.MoveTo(RegPath, BackupPath); RegPath = BackupPath;
		// Writes DIRECTLY to HKLM\SOFTWARE\Classes (via HKCR), not HKCU.

		int lastSlash = hkcrFullPath.ReverseFind(_T('\\'));
		if (lastSlash < 0)
		{
			MessageBox(_T("无法解析ShellEx注册表路径。"), _T("操作失败"), MB_ICONERROR);
			return;
		}
		CString keyName = hkcrFullPath.Mid(lastSlash + 1);
		CString parentPath = hkcrFullPath.Left(lastSlash);

		int secondSlash = parentPath.ReverseFind(_T('\\'));
		if (secondSlash < 0)
		{
			MessageBox(_T("无法解析ShellEx注册表路径。"), _T("操作失败"), MB_ICONERROR);
			return;
		}
		CString shellexBase = hkcrFullPath.Left(secondSlash);  // e.g. "*\shellex"
		CString curFolder = parentPath.Mid(secondSlash + 1);    // "ContextMenuHandlers" or "-ContextMenuHandlers"

		CString newFolder;
		if (curFolder == _T("ContextMenuHandlers"))
			newFolder = _T("-ContextMenuHandlers");
		else if (curFolder == _T("-ContextMenuHandlers"))
			newFolder = _T("ContextMenuHandlers");
		else
		{
			CString errMsg;
			errMsg.Format(_T("无法识别的ShellEx文件夹：%s"), curFolder);
			MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
			return;
		}

		// Build full HKCR paths
		CString srcParentPath = shellexBase + _T("\\") + curFolder;
		CString dstParentPath = shellexBase + _T("\\") + newFolder;

		// Take ownership of source key (matches C# TakeRegTreeOwnerShip)
		CString srcKeyPath = srcParentPath + _T("\\") + keyName;
		TakeRegKeyOwnership(HKEY_CLASSES_ROOT, srcKeyPath);

		// Open source parent in HKCR for reading
		HKEY hSrcParent = nullptr;
		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, srcParentPath, 0, KEY_READ, &hSrcParent) != ERROR_SUCCESS)
		{
			MessageBox(_T("无法打开源注册表项。"), _T("操作失败"), MB_ICONERROR);
			return;
		}

		// Take ownership of destination parent and create it
		TakeRegKeyOwnership(HKEY_CLASSES_ROOT, dstParentPath);
		HKEY hDstParent = nullptr;
		DWORD dwDisp = 0;
		if (RegCreateKeyEx(HKEY_CLASSES_ROOT, dstParentPath, 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, nullptr, &hDstParent, &dwDisp) != ERROR_SUCCESS)
		{
			RegCloseKey(hSrcParent);
			MessageBox(_T("无法创建目标注册表项。"), _T("操作失败"), MB_ICONERROR);
			return;
		}

		// Copy key from source to destination (matches C# RegistryEx.CopyTo)
		if (!CopyRegistryKey(hSrcParent, hDstParent, keyName, keyName))
		{
			RegCloseKey(hSrcParent);
			RegCloseKey(hDstParent);
			MessageBox(_T("复制注册表项失败。"), _T("操作失败"), MB_ICONERROR);
			return;
		}
		RegCloseKey(hDstParent);

		// Delete source key (matches C# RegistryEx.DeleteKeyTree)
		// Writing to HKCR deletes from HKLM\SOFTWARE\Classes
		LONG delResult = SHDeleteKey(hSrcParent, keyName);
		RegCloseKey(hSrcParent);

		if (delResult != ERROR_SUCCESS)
		{
			MessageBox(_T("删除旧注册表项失败，但新项已成功创建。\n请手动检查注册表。"),
				_T("部分成功"), MB_ICONWARNING);
		}

		// Update entry state (matches C#: RegPath = BackupPath)
		entry.regPath = shellexBase + _T("\\") + newFolder;
		entry.bEnabled = !entry.bEnabled;
	}
	else
	{
		// --- Static verb: set/remove LegacyDisable + ProgrammaticAccessOnly + HideBasedOnVelocityId ---
		// Matches C# ShellItem.ItemVisible.set exactly:
		//   Disable: Registry.SetValue(RegPath, "HideBasedOnVelocityId", 0x639bc8);
		//            Registry.SetValue(RegPath, "LegacyDisable", "");
		//            Registry.SetValue(RegPath, "ProgrammaticAccessOnly", "");
		//   Enable:  RegistryEx.DeleteValue(RegPath, "HideBasedOnVelocityId");
		//            RegistryEx.DeleteValue(RegPath, "LegacyDisable");
		//            RegistryEx.DeleteValue(RegPath, "ProgrammaticAccessOnly");
		//            if (CommandFlags % 16 >= 8) RegistryEx.DeleteValue(RegPath, "CommandFlags");
		// Writes DIRECTLY to HKLM\SOFTWARE\Classes (via HKCR), not HKCU.

		// Take ownership of the key (matches C# TakeRegTreeOwnerShip)
		TakeRegKeyOwnership(HKEY_CLASSES_ROOT, hkcrFullPath);

		// Open/create the key in HKCR (writes to HKLM\SOFTWARE\Classes)
		HKEY hVerb = nullptr;
		DWORD dwDisp = 0;
		LONG lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, hkcrFullPath, 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, nullptr, &hVerb, &dwDisp);
		if (lResult != ERROR_SUCCESS)
		{
			CString errMsg;
			errMsg.Format(_T("无法打开注册表项：%s\n错误代码：%d"), hkcrFullPath, lResult);
			MessageBox(errMsg, _T("操作失败"), MB_ICONERROR);
			return;
		}

		if (entry.bEnabled)
		{
			// Disable: set values (matches C# Registry.SetValue)
			DWORD dwHideBased = 0x639bc8;
			RegSetValueEx(hVerb, _T("HideBasedOnVelocityId"), 0, REG_DWORD,
				(BYTE*)&dwHideBased, sizeof(DWORD));
			const TCHAR* szEmpty = _T("");
			RegSetValueEx(hVerb, _T("LegacyDisable"), 0, REG_SZ,
				(LPBYTE)szEmpty, sizeof(TCHAR));
			RegSetValueEx(hVerb, _T("ProgrammaticAccessOnly"), 0, REG_SZ,
				(LPBYTE)szEmpty, sizeof(TCHAR));
			entry.bEnabled = false;
			entry.bDisabled = true;
		}
		else
		{
			// Enable: delete values (matches C# RegistryEx.DeleteValue)
			RegDeleteValue(hVerb, _T("LegacyDisable"));
			RegDeleteValue(hVerb, _T("ProgrammaticAccessOnly"));
			RegDeleteValue(hVerb, _T("HideBasedOnVelocityId"));

			// Also delete CommandFlags if it was hiding the item
			DWORD dwCmdFlags = 0;
			DWORD cbCmdFlags = sizeof(dwCmdFlags);
			if (RegQueryValueEx(hVerb, _T("CommandFlags"), nullptr, nullptr,
				(LPBYTE)&dwCmdFlags, &cbCmdFlags) == ERROR_SUCCESS && (dwCmdFlags % 16) >= 8)
			{
				RegDeleteValue(hVerb, _T("CommandFlags"));
			}
			entry.bEnabled = true;
			entry.bDisabled = false;
		}
		RegCloseKey(hVerb);
	}

	// Notify shell of changes
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

	// Refresh the list to show updated state
	RefreshList();

	CString msg;
	msg.Format(_T("已%s: %s"), entry.bEnabled ? _T("启用") : _T("禁用"), entry.displayName);
	UpdateStatus(msg);
}

// ============================================================================
// Delete — matches ContextMenuManager's DeleteMe
// ============================================================================

void CContextMenuDlg::OnBnClickedDelete()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	std::vector<int> selected;
	POSITION pos = pList->GetFirstSelectedItemPosition();
	while (pos)
		selected.push_back(pList->GetNextSelectedItem(pos));

	if (selected.empty())
	{
		MessageBox(_T("请先选择要删除的项。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	CString msg;
	msg.Format(_T("确定要删除选中的 %d 个右键菜单项吗？\n此操作将修改注册表，不可撤销。"),
		(int)selected.size());
	if (MessageBox(msg, _T("确认删除"), MB_ICONWARNING | MB_YESNO) != IDYES)
		return;

	// ContextMenuManager:
	//   ShellItem.DeleteMe():  RegistryEx.DeleteKeyTree(this.RegPath, true)
	//   ShellExItem.DeleteMe(): RegistryEx.DeleteKeyTree(this.RegPath, true);
	//                           RegistryEx.DeleteKeyTree(this.BackupPath);
	// Writes DIRECTLY to HKLM\SOFTWARE\Classes (via HKCR), not HKCU.

	int deleted = 0;
	std::sort(selected.rbegin(), selected.rend());
	for (int idx : selected)
	{
		auto& entry = m_entries[idx];
		CString hkcrRelPath = entry.regPath + _T("\\") + entry.keyName;

		// Clean up any previous HKCU override
		CString hkcuPath = _T("Software\\Classes\\") + hkcrRelPath;
		SHDeleteKey(HKEY_CURRENT_USER, hkcuPath);

		// Take ownership and delete from HKCR (= HKLM\SOFTWARE\Classes)
		TakeRegKeyOwnership(HKEY_CLASSES_ROOT, hkcrRelPath);
		bool bDeleted = (SHDeleteKey(HKEY_CLASSES_ROOT, hkcrRelPath) == ERROR_SUCCESS);

		// For ShellEx items, also delete from the opposite folder (backup path)
		// ContextMenuManager: RegistryEx.DeleteKeyTree(this.BackupPath)
		if (entry.bIsShellEx)
		{
			int lastSlash = hkcrRelPath.ReverseFind(_T('\\'));
			if (lastSlash >= 0)
			{
				CString keyName = hkcrRelPath.Mid(lastSlash + 1);
				CString parentPath = hkcrRelPath.Left(lastSlash);
				int secondSlash = parentPath.ReverseFind(_T('\\'));
				if (secondSlash >= 0)
				{
					CString shellexBase = hkcrRelPath.Left(secondSlash);
					CString curFolder = parentPath.Mid(secondSlash + 1);
					CString otherFolder = (curFolder == _T("ContextMenuHandlers"))
						? _T("-ContextMenuHandlers") : _T("ContextMenuHandlers");
					CString backupPath = shellexBase + _T("\\") + otherFolder + _T("\\") + keyName;
					TakeRegKeyOwnership(HKEY_CLASSES_ROOT, backupPath);
					SHDeleteKey(HKEY_CLASSES_ROOT, backupPath);
					// Also clean HKCU backup
					CString hkcuBackup = _T("Software\\Classes\\") + backupPath;
					SHDeleteKey(HKEY_CURRENT_USER, hkcuBackup);
				}
			}
		}

		if (bDeleted)
		{
			pList->DeleteItem(idx);
			m_entries.erase(m_entries.begin() + idx);
			deleted++;
		}
		else
		{
			CString errMsg;
			errMsg.Format(_T("无法删除：%s\n\n可能原因：系统保护、权限不足、或该项正在被使用。"),
				entry.displayName);
			MessageBox(errMsg, _T("删除失败"), MB_ICONERROR);
		}
	}

	CString status;
	status.Format(_T("已删除 %d 项"), deleted);
	UpdateStatus(status);

	if (deleted > 0)
	{
		OnBnClickedRefresh();
		LoadSelfContextMenuState();
	}
}

void CContextMenuDlg::OnBnClickedLocate()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos)
	{
		MessageBox(_T("请先选择要定位的项。"), _T("提示"), MB_ICONINFORMATION);
		return;
	}

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	const auto& entry = m_entries[idx];
	CString fullRegPath = entry.regPath + _T("\\") + entry.keyName;
	OpenRegEditToPath(HKEY_CLASSES_ROOT, fullRegPath);
}

// ============================================================================
// Right-click context menu on list items
// ============================================================================

void CContextMenuDlg::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos) return;

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	CMenu menu;
	menu.CreatePopupMenu();

	if (m_entries[idx].bEnabled)
		menu.AppendMenu(MF_STRING, ID_MENU_CM_TOGGLE, _T("禁用"));
	else
		menu.AppendMenu(MF_STRING, ID_MENU_CM_TOGGLE, _T("启用"));

	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, ID_MENU_CM_DELETE, _T("删除选中"));
	menu.AppendMenu(MF_STRING, ID_MENU_CM_LOCATE, _T("定位(注册表)"));

	CPoint pt;
	GetCursorPos(&pt);
	menu.TrackPopupMenu(TPM_RIGHTBUTTON, pt.x, pt.y, this);
}

void CContextMenuDlg::OnMenuDelete()
{
	OnBnClickedDelete();
}

void CContextMenuDlg::OnMenuLocate()
{
	OnBnClickedLocate();
}

void CContextMenuDlg::OnMenuToggle()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_CM_ENTRIES);
	if (!pList) return;

	POSITION pos = pList->GetFirstSelectedItemPosition();
	if (!pos) return;

	int idx = pList->GetNextSelectedItem(pos);
	if (idx < 0 || idx >= (int)m_entries.size()) return;

	ToggleEntry(idx);
}

// ============================================================================
// Dictionary path helpers
// ============================================================================

CString CContextMenuDlg::GetCachePath()
{
	// Cache file is always in the dictionary folder.
	// If user configured a folder, use it; otherwise use default %AppData%.
	if (!s_dictPath.IsEmpty())
	{
		return s_dictPath + _T("\\GuidInfosDic.cache.ini");
	}

	// Default: %AppData%\MFCApplication1\GuidInfosDic.cache.ini
	TCHAR szPath[MAX_PATH] = { 0 };
	if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, szPath)))
	{
		CString dir = CString(szPath) + _T("\\MFCApplication1");
		CreateDirectory(dir, nullptr);
		return dir + _T("\\GuidInfosDic.cache.ini");
	}
	return CString();
}

CString CContextMenuDlg::GetConfigPath()
{
	TCHAR szExePath[MAX_PATH] = { 0 };
	GetModuleFileName(nullptr, szExePath, MAX_PATH);
	CString exePath = szExePath;
	int nLastSlash = exePath.ReverseFind(_T('\\'));
	if (nLastSlash >= 0)
		return exePath.Left(nLastSlash + 1) + _T("config.ini");
	return _T("config.ini");
}

// ============================================================================
// OnBnClickedDictPath — user selects a folder for dictionary INI files
// ============================================================================

void CContextMenuDlg::OnBnClickedDictPath()
{
	CFolderPickerDialog dlg(
		s_dictPath.IsEmpty() ? nullptr : s_dictPath,
		0, this);
	dlg.m_ofn.lpstrTitle = _T("选择字典文件夹");

	if (dlg.DoModal() == IDOK)
	{
		CString newPath = dlg.GetPathName();
		if (newPath == s_dictPath)
			return; // no change

		// Migrate old dictionary files to new location
		if (!s_dictPath.IsEmpty() && PathFileExists(s_dictPath))
		{
			MigrateDictionaryFiles(s_dictPath, newPath);
		}

		s_dictPath = newPath;

		// Save the path to app directory's config.ini
		CString configPath = GetConfigPath();
		WritePrivateProfileString(_T("Dictionary"), _T("Path"), s_dictPath, configPath);

		// Reload dictionary with the new folder
		s_bDictLoaded = false;
		LoadGuidDictionary();

		// Refresh the list to show updated names
		OnBnClickedRefresh();

		CString msg;
		msg.Format(_T("字典文件夹已更新：%s"), s_dictPath);
		UpdateStatus(msg);
	}
}

// ============================================================================
// MigrateDictionaryFiles — copy .ini files from old folder to new folder
// ============================================================================

bool CContextMenuDlg::MigrateDictionaryFiles(const CString& oldPath, const CString& newPath)
{
	if (oldPath == newPath)
		return false;

	// Ensure the new folder exists
	if (!PathFileExists(newPath))
	{
		if (!CreateDirectory(newPath, nullptr))
			return false;
	}

	if (!PathFileExists(oldPath) || !PathIsDirectory(oldPath))
		return false;

	bool bSuccess = true;
	CFileFind finder;
	CString filter = oldPath + _T("\\*.ini");
	BOOL bFound = finder.FindFile(filter);
	while (bFound)
	{
		bFound = finder.FindNextFile();
		if (finder.IsDots() || finder.IsDirectory()) continue;

		CString srcFile = finder.GetFilePath();
		CString dstFile = newPath + _T("\\") + finder.GetFileName();
		if (!CopyFile(srcFile, dstFile, FALSE))
			bSuccess = false;
	}
	finder.Close();
	return bSuccess;
}

// ============================================================================
// COM-based dictionary rebuild — queries actual context menu names via COM
// ============================================================================

// Helper: get display name for a ShellEx CLSID by creating the COM object
// and querying its IContextMenu interface.
static CString GetComDisplayName(const CString& clsidWithBraces, IDataObject* pDataObj)
{
	// Parse CLSID from string
	CLSID clsid = { 0 };
	if (FAILED(CLSIDFromString(clsidWithBraces, &clsid)))
		return CString();

	IUnknown* pUnk = nullptr;
	HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
		IID_IUnknown, (void**)&pUnk);
	if (FAILED(hr) || !pUnk)
		return CString();

	CString result;

	// Try IShellExtInit first
	IShellExtInit* pInit = nullptr;
	if (SUCCEEDED(pUnk->QueryInterface(IID_IShellExtInit, (void**)&pInit)) && pInit)
	{
		pInit->Initialize(nullptr, pDataObj, nullptr);
		pInit->Release();
	}

	// Query IContextMenu
	IContextMenu* pCtxMenu = nullptr;
	if (SUCCEEDED(pUnk->QueryInterface(IID_IContextMenu, (void**)&pCtxMenu)) && pCtxMenu)
	{
		HMENU hMenu = CreatePopupMenu();
		if (hMenu)
		{
			hr = pCtxMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);
			if (SUCCEEDED(hr))
			{
				int nCount = GetMenuItemCount(hMenu);
				if (nCount > 0)
				{
					TCHAR szText[256] = { 0 };
					MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
					mii.fMask = MIIM_STRING;
					mii.dwTypeData = szText;
					mii.cch = 256;
					if (GetMenuItemInfo(hMenu, 0, TRUE, &mii) && szText[0])
					{
						result = szText;
					}
				}
			}
			DestroyMenu(hMenu);
		}
		pCtxMenu->Release();
	}

	pUnk->Release();
	return result;
}

// Helper: create an IDataObject for a file path
static IDataObject* CreateDataObjectForFile(const CString& filePath)
{
	// Get PIDL for the file path
	LPITEMIDLIST pidl = ILCreateFromPath(filePath);
	if (!pidl)
		return nullptr;

	IShellFolder* pParentFolder = nullptr;
	LPCITEMIDLIST pidlChild = nullptr;
	HRESULT hr = SHBindToParent(pidl, IID_IShellFolder, (void**)&pParentFolder, &pidlChild);

	IDataObject* pDataObj = nullptr;
	if (SUCCEEDED(hr) && pParentFolder && pidlChild)
	{
		pParentFolder->GetUIObjectOf(nullptr, 1, &pidlChild, IID_IDataObject, nullptr, (void**)&pDataObj);
		pParentFolder->Release();
	}

	ILFree(pidl);
	return pDataObj;
}

// Helper: create an IDataObject for a folder path (directory)
static IDataObject* CreateDataObjectForFolder(const CString& folderPath)
{
	return CreateDataObjectForFile(folderPath);
}

// Helper: get the InprocServer32 DLL filename for a CLSID as fallback display name
static CString GetClsidDllName(const CString& clsidWithBraces)
{
	static const struct { HKEY hRoot; const TCHAR* fmt; } clsidPaths[] = {
		{ HKEY_CLASSES_ROOT,   _T("CLSID\\%s\\InprocServer32") },
		{ HKEY_CLASSES_ROOT,   _T("WOW6432Node\\CLSID\\%s\\InprocServer32") },
		{ HKEY_LOCAL_MACHINE,  _T("SOFTWARE\\WOW6432Node\\Classes\\CLSID\\%s\\InprocServer32") },
	};
	for (const auto& cp : clsidPaths)
	{
		CString fullPath;
		fullPath.Format(cp.fmt, clsidWithBraces);
		HKEY hKey = nullptr;
		if (RegOpenKeyEx(cp.hRoot, fullPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			TCHAR szVal[MAX_PATH * 2] = { 0 };
			DWORD cbVal = sizeof(szVal);
			DWORD dwType = 0;
			if (RegQueryValueEx(hKey, nullptr, nullptr, &dwType,
				(LPBYTE)szVal, &cbVal) == ERROR_SUCCESS && szVal[0])
			{
				RegCloseKey(hKey);
				CString dllPath = szVal;
				int nComma = dllPath.Find(_T(','));
				if (nComma > 0) dllPath = dllPath.Left(nComma);
				dllPath.Trim();
				// Extract just the filename
				int nSlash = dllPath.ReverseFind(_T('\\'));
				if (nSlash >= 0)
					return dllPath.Mid(nSlash + 1);
				return dllPath;
			}
			RegCloseKey(hKey);
		}
	}
	return CString();
}

void CContextMenuDlg::RebuildDictionary()
{
	// Collect all ShellEx CLSIDs from all scenes
	std::set<CString> allClsids;
	{
		std::vector<MenuEntry> tempEntries;
		std::set<CString> seen;
		for (const auto& scene : m_scenes)
		{
			if (scene.basePath.IsEmpty()) continue;
			ScanShellExHandlers(scene.basePath + _T("\\shellex"), scene.name, tempEntries, seen);
		}
		for (const auto& entry : tempEntries)
		{
			if (!entry.keyName.IsEmpty() && entry.keyName[0] == _T('{'))
			{
				allClsids.insert(entry.keyName);
			}
		}
	}

	if (allClsids.empty())
	{
		UpdateStatus(_T("未找到任何 ShellEx CLSID，无需重建。"));
		return;
	}

	CString statusMsg;
	statusMsg.Format(_T("正在重建字典，共发现 %d 个 CLSID，请稍候..."), (int)allClsids.size());
	UpdateStatus(statusMsg);

	// Ensure COM is initialized
	HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	bool bComInitialized = SUCCEEDED(hrCom);

	// Create IDataObject for common file types
	// Create a temp .txt file
	TCHAR szTempPath[MAX_PATH] = { 0 };
	TCHAR szTempFile[MAX_PATH] = { 0 };
	GetTempPath(MAX_PATH, szTempPath);
	GetTempFileName(szTempPath, _T("ctx"), 0, szTempFile);
	// Ensure the file exists
	HANDLE hTempFile = CreateFile(szTempFile, GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hTempFile != INVALID_HANDLE_VALUE)
		CloseHandle(hTempFile);

	IDataObject* pFileDataObj = CreateDataObjectForFile(szTempFile);

	// Create a temp folder for folder context menu
	TCHAR szTempDir[MAX_PATH] = { 0 };
	GetTempFileName(szTempPath, _T("ctx"), 0, szTempDir);
	DeleteFile(szTempDir); // Remove the temp file, we'll create a directory
	CreateDirectory(szTempDir, nullptr);
	IDataObject* pFolderDataObj = CreateDataObjectForFolder(szTempDir);

	// For each CLSID, try to get the display name via multiple strategies
	int nResolved = 0;
	int nComResolved = 0;
	int nFallback = 0;
	int nTotal = (int)allClsids.size();

	// Build cache INI content
	CString cacheContent;
	cacheContent += _T("; Auto-generated dictionary cache\n");
	cacheContent += _T("; Generated by MFCApplication1 context menu manager\n");
	cacheContent += _T("; This file is overwritten on each rebuild.\n\n");

	for (const auto& clsid : allClsids)
	{
		// Normalize: remove braces for dictionary key
		CString clsidNoBraces = clsid;
		if (!clsidNoBraces.IsEmpty() && clsidNoBraces[0] == _T('{'))
			clsidNoBraces = clsidNoBraces.Mid(1, clsidNoBraces.GetLength() - 2);

		// Strategy 1: Try registry-based resolution (includes embedded dictionary lookup)
		CString name = ResolveClsidName(clsid);
		bool bFromCom = false;
		bool bFromFallback = false;

		// Strategy 2: If name is empty or looks like a raw GUID/DLL, try COM
		bool bLooksUnresolved = name.IsEmpty() ||
			(name.Find(_T('{')) >= 0 && name.Find(_T('}')) >= 0) ||
			(name.Find(_T(".dll")) >= 0 && name.Find(_T(' ')) < 0);

		if (bLooksUnresolved)
		{
			// Try COM with file data object first
			if (pFileDataObj)
			{
				CString comName = GetComDisplayName(clsid, pFileDataObj);
				if (!comName.IsEmpty())
				{
					name = comName;
					bFromCom = true;
				}
			}
			// Try COM with folder data object
			if (bLooksUnresolved && name.IsEmpty() && pFolderDataObj)
			{
				CString comName = GetComDisplayName(clsid, pFolderDataObj);
				if (!comName.IsEmpty())
				{
					name = comName;
					bFromCom = true;
				}
			}

			// Strategy 3: Fallback — use InprocServer32 DLL name
			if (name.IsEmpty())
			{
				name = GetClsidDllName(clsid);
				if (!name.IsEmpty())
					bFromFallback = true;
			}
		}

		if (!name.IsEmpty())
		{
			cacheContent += _T("[") + clsidNoBraces + _T("]\n");
			cacheContent += _T("Text = ") + name + _T("\n");
			if (bFromCom)
			{
				cacheContent += _T("; (resolved via COM)\n");
				nComResolved++;
			}
			else if (bFromFallback)
			{
				cacheContent += _T("; (fallback: DLL name)\n");
				nFallback++;
			}
			cacheContent += _T("\n");
			nResolved++;
		}
	}

	// Cleanup temp files
	if (pFileDataObj) pFileDataObj->Release();
	if (pFolderDataObj) pFolderDataObj->Release();
	DeleteFile(szTempFile);
	RemoveDirectory(szTempDir);

	if (bComInitialized)
		CoUninitialize();

	// Save cache to file
	CString cachePath = GetCachePath();
	if (!cachePath.IsEmpty())
	{
		if (WriteFileContent(cachePath, cacheContent))
		{
			// Reload dictionary with the new cache
			s_bDictLoaded = false;
			LoadGuidDictionary();

			// Refresh the list
			OnBnClickedRefresh();

			CString msg;
			msg.Format(_T("字典重建完成：%d/%d 个 CLSID 已解析（注册表: %d, COM: %d, 回退: %d）。缓存路径：%s"),
				nResolved, nTotal, nResolved - nComResolved - nFallback, nComResolved, nFallback, cachePath);
			UpdateStatus(msg);
			return;
		}
	}

	CString msg;
	msg.Format(_T("字典重建完成：%d/%d 个 CLSID 已解析，但缓存保存失败。"),
		nResolved, nTotal);
	UpdateStatus(msg);
}

void CContextMenuDlg::OnBnClickedRebuild()
{
	CString msg = _T("将扫描所有已注册的 ShellEx CLSID，并通过注册表和 COM 接口解析其显示名称。\n");
	msg += _T("此过程可能需要几秒钟，是否继续？");
	if (MessageBox(msg, _T("重建字典"), MB_ICONINFORMATION | MB_YESNO) != IDYES)
		return;

	RebuildDictionary();
}