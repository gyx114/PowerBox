// LocalizationManager.cpp: Implementation of the localization manager
#include "pch.h"
#include "framework.h"
#include "LocalizationManager.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

CLocalizationManager& CLocalizationManager::GetInstance()
{
    static CLocalizationManager instance;
    return instance;
}

CLocalizationManager::CLocalizationManager()
{
    LoadBuiltinDefaults();
}

CString CLocalizationManager::MakeKey(LPCTSTR section, LPCTSTR key)
{
    return CString(section) + _T("|") + CString(key);
}

CString CLocalizationManager::GetLangDir() const
{
    TCHAR szExePath[MAX_PATH] = {};
    GetModuleFileName(nullptr, szExePath, MAX_PATH);
    fs::path exePath(szExePath);
    fs::path langDir = exePath.parent_path() / L"lang";
    return CString(langDir.c_str());
}

bool CLocalizationManager::LoadLanguage(const CString& langId)
{
    m_currentLang = langId;

    CString langFile;
    langFile.Format(_T("%s\\%s.ini"), GetLangDir().GetString(), langId.GetString());

    // Refresh INI file cache to ensure changes are picked up immediately
    // Without this, GetPrivateProfileString may return stale cached data
    WritePrivateProfileString(NULL, NULL, NULL, langFile);

    // For zh-CN, built-in defaults are always available, no file needed
    if (langId == _T("zh-CN"))
        return true;

    // For other languages, check that the .ini file exists
    if (GetFileAttributes(langFile) == INVALID_FILE_ATTRIBUTES)
    {
        // File not found, fall back to zh-CN
        m_currentLang = _T("zh-CN");
        return false;
    }

    return true;
}

CString CLocalizationManager::GetString(LPCTSTR section, LPCTSTR key, LPCTSTR defaultVal) const
{
    CString result;

    // Try the external .ini file first (all languages, including zh-CN)
    CString langFile;
    langFile.Format(_T("%s\\%s.ini"), GetLangDir().GetString(), m_currentLang.GetString());

    TCHAR buf[4096] = {};
    GetPrivateProfileString(section, key, _T(""), buf, 4096, langFile);
    if (buf[0] != _T('\0'))
    {
        result = CString(buf);
        // Convert literal escape sequences to actual characters
        // INI files don't support escape sequences natively, so we handle common ones
        result.Replace(_T("\\r\\n"), _T("\r\n"));
        result.Replace(_T("\\n"), _T("\n"));
        result.Replace(_T("\\\""), _T("\""));
        result.Replace(_T("\\\\"), _T("\\"));
        return result;
    }

    // Fall back to built-in defaults
    auto it = m_defaults.find(MakeKey(section, key));
    if (it != m_defaults.end())
        return it->second;

    // Final fallback: return default value or key name
    return (defaultVal != nullptr) ? CString(defaultVal) : CString(key);
}

CString CLocalizationManager::GetLanguageDisplayName() const
{
    if (m_currentLang == _T("zh-CN")) return _T("中文");
    if (m_currentLang == _T("en-US")) return _T("English");
    return m_currentLang;
}

std::vector<std::pair<CString, CString>> CLocalizationManager::GetAvailableLanguages() const
{
    std::vector<std::pair<CString, CString>> result;
    result.push_back({ _T("zh-CN"), _T("中文") });

    // Scan lang\ directory for *.ini files
    CString langDir = GetLangDir();
    if (GetFileAttributes(langDir) != INVALID_FILE_ATTRIBUTES)
    {
        for (const auto& entry : fs::directory_iterator(langDir.GetString()))
        {
            if (entry.is_regular_file())
            {
                fs::path p = entry.path();
                if (p.extension() == L".ini")
                {
                    CString langId(p.stem().c_str());
                    // Skip zh-CN since it's already added
                    if (langId == _T("zh-CN"))
                        continue;

                    // Read display name from the language file itself
                    CString filePath(p.c_str());
                    TCHAR displayName[64] = {};
                    GetPrivateProfileString(_T("Language"), _T("DisplayName"), _T(""), displayName, 64, filePath);
                    CString name = (displayName[0] != _T('\0')) ? CString(displayName) : langId;
                    result.push_back({ langId, name });
                }
            }
        }
    }

    return result;
}

// ============================================================================
// Built-in Chinese defaults (embedded fallback, no external file needed)
// These are extracted from the .rc and .cpp files of the project.
// ============================================================================

void CLocalizationManager::LoadBuiltinDefaults()
{
    // ===== Main Dialog =====
    // Tab labels
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab1"))] = _T("进程管理");
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab2"))] = _T("启动项管理");
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab3"))] = _T("剪贴板");
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab4"))] = _T("窗口处理");
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab5"))] = _T("文件管理");
    m_defaults[MakeKey(_T("MainDlg"), _T("Tab6"))] = _T("git工具箱");

    // Quick tabs
    m_defaults[MakeKey(_T("MainDlg"), _T("QuickTab1"))] = _T("AI助手");
    m_defaults[MakeKey(_T("MainDlg"), _T("QuickTabLaunch"))] = _T("快捷打开");
    m_defaults[MakeKey(_T("MainDlg"), _T("QuickTab2"))] = _T("系统");
    m_defaults[MakeKey(_T("MainDlg"), _T("QuickTab3"))] = _T("工具");

    // Process tab columns
    m_defaults[MakeKey(_T("ProcessTab"), _T("ColName"))] = _T("进程名");
    m_defaults[MakeKey(_T("ProcessTab"), _T("ColCPU"))] = _T("CPU%");
    m_defaults[MakeKey(_T("ProcessTab"), _T("ColPath"))] = _T("路径");
    m_defaults[MakeKey(_T("ProcessTab"), _T("ColMemory"))] = _T("内存(KB)");
    m_defaults[MakeKey(_T("ProcessTab"), _T("ColPID"))] = _T("PID");

    // Startup tab columns
    m_defaults[MakeKey(_T("StartupTab"), _T("ColName"))] = _T("启动项名");
    m_defaults[MakeKey(_T("StartupTab"), _T("ColCmd"))] = _T("命令(路径)");

    // Clipboard tab
    m_defaults[MakeKey(_T("ClipboardTab"), _T("ColText"))] = _T("文本内容(双击复制)");

    // Window tab columns
    m_defaults[MakeKey(_T("WindowTab"), _T("ColField"))] = _T("字段");
    m_defaults[MakeKey(_T("WindowTab"), _T("ColValue"))] = _T("值");
    m_defaults[MakeKey(_T("WindowTab"), _T("ColType"))] = _T("类型");
    m_defaults[MakeKey(_T("WindowTab"), _T("ColTitle"))] = _T("窗口标题");
    m_defaults[MakeKey(_T("WindowTab"), _T("ColIndex"))] = _T("序号");
    m_defaults[MakeKey(_T("WindowTab"), _T("TransparencyLabel"))] = _T("透明度: %d%%");

    // Git tab columns
    m_defaults[MakeKey(_T("GitTab"), _T("ColDesc"))] = _T("说明");
    m_defaults[MakeKey(_T("GitTab"), _T("ColCmd"))] = _T("命令");

    // ===== Git commands (default descriptions) =====
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd1Desc"))] = _T("初始化本地仓库");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd2Desc"))] = _T("暂存所有文件");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd3Desc"))] = _T("提交到本地仓库");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd4Desc"))] = _T("添加远程仓库");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd5Desc"))] = _T("分支重命名为 main");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd6Desc"))] = _T("推送并设置上游");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd7Desc"))] = _T("拉取远程更新");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd8Desc"))] = _T("克隆远程仓库");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd9Desc"))] = _T("查看状态");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd10Desc"))] = _T("暂存所有更改");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd11Desc"))] = _T("提交到本地仓库");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd12Desc"))] = _T("推送到远程");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd13Desc"))] = _T("列出所有分支（含远程）");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd14Desc"))] = _T("创建并切换分支");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd15Desc"))] = _T("切换分支");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd16Desc"))] = _T("合并分支到当前");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd17Desc"))] = _T("删除已合并分支");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd18Desc"))] = _T("查看简洁提交日志");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd19Desc"))] = _T("恢复工作区文件");
    m_defaults[MakeKey(_T("GitCommands"), _T("Cmd20Desc"))] = _T("取消暂存文件");

    // ===== Common messages =====
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmEndProcess"))] = _T("确定要结束进程\n%s (PID: %s) 吗？");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmEndSameName"))] = _T("确定要结束所有 \"%s\" 进程吗？\n共 %d 个实例。");
    m_defaults[MakeKey(_T("Msg"), _T("ProcessEnded"))] = _T("进程已成功结束。");
    m_defaults[MakeKey(_T("Msg"), _T("ProcessEndFail"))] = _T("结束进程失败：%s");
    m_defaults[MakeKey(_T("Msg"), _T("CannotOpenProcess"))] = _T("无法打开进程以终止。错误：%s");
    m_defaults[MakeKey(_T("Msg"), _T("BatchEndResult"))] = _T("已结束 %d 个进程，失败 %d 个。");
    m_defaults[MakeKey(_T("Msg"), _T("Success"))] = _T("成功");
    m_defaults[MakeKey(_T("Msg"), _T("Error"))] = _T("错误");
    m_defaults[MakeKey(_T("Msg"), _T("Warning"))] = _T("提示");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmDelete"))] = _T("确认删除");
    m_defaults[MakeKey(_T("Msg"), _T("AccessDenied"))] = _T("权限不足");
    m_defaults[MakeKey(_T("Msg"), _T("Completed"))] = _T("完成");
    m_defaults[MakeKey(_T("Msg"), _T("Info"))] = _T("提示");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmForceKill"))] = _T("确定要强制结束进程 PID=%u 吗？\n未保存的数据可能会丢失。");
    m_defaults[MakeKey(_T("Msg"), _T("ProcessForceKilled"))] = _T("进程已强制结束。");
    m_defaults[MakeKey(_T("Msg"), _T("ProcessForceKillFail"))] = _T("强制结束进程失败。");
    m_defaults[MakeKey(_T("Msg"), _T("InvalidWindow"))] = _T("请先定位一个窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("CannotGetPID"))] = _T("无法获取进程ID。");
    m_defaults[MakeKey(_T("Msg"), _T("NoWindowSelected"))] = _T("未选中有效窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("TopmostFail"))] = _T("置顶失败，可能权限不足或窗口不允许。");
    m_defaults[MakeKey(_T("Msg"), _T("FileNotFound"))] = _T("请先拖入文件！");
    m_defaults[MakeKey(_T("Msg"), _T("CopySuccess"))] = _T("生成副本成功！\n保存路径：%s");
    m_defaults[MakeKey(_T("Msg"), _T("CopyFail"))] = _T("生成副本失败，错误代码：%u");
    m_defaults[MakeKey(_T("Msg"), _T("RenameSuccess"))] = _T("重命名成功！");
    m_defaults[MakeKey(_T("Msg"), _T("RenameFail"))] = _T("重命名失败，错误代码：%u");
    m_defaults[MakeKey(_T("Msg"), _T("DeleteConfirm"))] = _T("确定要将以下文件移到回收站？\n%s");
    m_defaults[MakeKey(_T("Msg"), _T("DeleteSuccess"))] = _T("文件已移到回收站。");
    m_defaults[MakeKey(_T("Msg"), _T("DeleteFail"))] = _T("无法删除文件，错误代码：%d");
    m_defaults[MakeKey(_T("Msg"), _T("StartupAdded"))] = _T("启动项已添加。");
    m_defaults[MakeKey(_T("Msg"), _T("StartupAddFail"))] = _T("添加启动项失败！请检查权限。");
    m_defaults[MakeKey(_T("Msg"), _T("StartupRemoved"))] = _T("启动项已删除。");
    m_defaults[MakeKey(_T("Msg"), _T("StartupRemoveFail"))] = _T("删除启动项失败！请检查权限。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmRemoveStartup"))] = _T("确定要删除启动项\n%s 吗？");
    m_defaults[MakeKey(_T("Msg"), _T("NeedAdminRestart"))] = _T("无法以管理员权限重新启动。请手动以管理员身份运行程序。");
    m_defaults[MakeKey(_T("Msg"), _T("OleInitFail"))] = _T("初始化 OLE 失败，无法使用拖放功能。");
    m_defaults[MakeKey(_T("Msg"), _T("EnterLocateModeFail"))] = _T("无法进入定位模式。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfigDefaultName"))] = _T("请先在 文件→设置→文件命名 中配置默认文件名。");
    m_defaults[MakeKey(_T("Msg"), _T("CannotGenUniqueName"))] = _T("无法生成唯一文件名，请检查目标目录权限或手动选择不同名称。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmBatchEnd"))] = _T("确认批量结束");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmForceKillTitle"))] = _T("确认强制结束");
    m_defaults[MakeKey(_T("Msg"), _T("NoProcessSelected"))] = _T("未选择任何进程。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfigApiKeyFirst"))] = _T("请先在设置中配置AI API密钥。");
    m_defaults[MakeKey(_T("Msg"), _T("AnalyzingProcesses"))] = _T("正在分析 %d 个进程，请稍候...");
    m_defaults[MakeKey(_T("Msg"), _T("AiAnalysisFailed"))] = _T("AI分析失败: ");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmEndProcessTitle"))] = _T("确认结束进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgEndProcess"))] = _T("结束进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgEndAllSameName"))] = _T("结束所有同名进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAiAnalyze"))] = _T("AI分析");
    m_defaults[MakeKey(_T("Msg"), _T("MsgBatchEndComplete"))] = _T("批量结束完成");
    m_defaults[MakeKey(_T("Msg"), _T("MsgFileNotFound"))] = _T("未找到文件");
    m_defaults[MakeKey(_T("Msg"), _T("MsgNoProcessSelected"))] = _T("未选择任何进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfigApiKeyFirst"))] = _T("请先在设置中配置AI API密钥。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAiScanFail"))] = _T("AI扫描");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAiScanFailed"))] = _T("AI扫描失败，请检查网络连接和API密钥。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgScanning"))] = _T("扫描中");
    m_defaults[MakeKey(_T("Msg"), _T("MsgForceKill"))] = _T("强制结束");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmForceKill"))] = _T("确认强制结束");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTopmostFail"))] = _T("置顶失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgLocateWindowFirst"))] = _T("请先定位一个窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotGetPID"))] = _T("无法获取PID");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCloseWindow"))] = _T("关闭窗口");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmClose"))] = _T("确认关闭");
    m_defaults[MakeKey(_T("Msg"), _T("MsgScreenshotSaved"))] = _T("截图已保存");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCopySuccess"))] = _T("复制成功");
    m_defaults[MakeKey(_T("Msg"), _T("MsgRenameSuccess"))] = _T("重命名成功");
    m_defaults[MakeKey(_T("Msg"), _T("MsgMoveSuccess"))] = _T("移动成功");
    m_defaults[MakeKey(_T("Msg"), _T("MsgDeleteSuccess"))] = _T("删除成功");
    m_defaults[MakeKey(_T("Msg"), _T("MsgStartup"))] = _T("启动项");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmDelete"))] = _T("确认删除");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCompleted"))] = _T("完成");
    m_defaults[MakeKey(_T("Msg"), _T("MsgError"))] = _T("错误");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotOpenProcess"))] = _T("无法打开进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAccessDenied"))] = _T("权限不足");
    m_defaults[MakeKey(_T("Msg"), _T("MsgForceKillFail"))] = _T("强制结束失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgInvalidWindow"))] = _T("无效窗口");
    m_defaults[MakeKey(_T("Msg"), _T("MsgWindowSizeInvalid"))] = _T("窗口大小无效");
    m_defaults[MakeKey(_T("Msg"), _T("MsgClipboardFail"))] = _T("剪贴板错误");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotGetPath"))] = _T("无法获取路径");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotBrowse"))] = _T("无法浏览");
    m_defaults[MakeKey(_T("Msg"), _T("MsgWarning"))] = _T("提示");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirm"))] = _T("确认");
    m_defaults[MakeKey(_T("Msg"), _T("MsgNotAvailable"))] = _T("N/A");
    m_defaults[MakeKey(_T("Msg"), _T("MsgSigned"))] = _T("有效签名");
    m_defaults[MakeKey(_T("Msg"), _T("MsgUnsigned"))] = _T("无效/无签名");
    m_defaults[MakeKey(_T("Msg"), _T("MsgNoCompany"))] = _T("无公司信息");
    m_defaults[MakeKey(_T("Msg"), _T("MsgNoOrigName"))] = _T("无原始文件名");
    m_defaults[MakeKey(_T("Msg"), _T("MsgProcessListEmpty"))] = _T("进程列表为空");
    m_defaults[MakeKey(_T("Msg"), _T("MsgPleaseRefresh"))] = _T("请先刷新进程列表。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgEndProcessTitle"))] = _T("结束进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgPleaseEnterCommand"))] = _T("请输入要运行的命令。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgRunCmdFailed"))] = _T("运行命令失败");
    
    m_defaults[MakeKey(_T("Msg"), _T("MsgPreventLockFail"))] = _T("设置防锁屏失败。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgPowerShellFail"))] = _T("启动PowerShell失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgPowerShellAdminFail"))] = _T("以管理员权限启动PowerShell失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgWSLFail"))] = _T("启动WSL失败，请手动运行wsl.exe");
    m_defaults[MakeKey(_T("Msg"), _T("MsgLinkFail"))] = _T("无法打开链接");
    m_defaults[MakeKey(_T("Msg"), _T("MsgGitBashNotFound"))] = _T("未找到git-bash.exe，请在设置中配置Git Bash路径。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCmdWindowFail"))] = _T("无法创建Git命令窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgSetWorkDirFirst"))] = _T("请先设置工作目录。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmExec"))] = _T("确认执行");
    m_defaults[MakeKey(_T("Msg"), _T("MsgExecCmd"))] = _T("执行命令");
    m_defaults[MakeKey(_T("Msg"), _T("MsgResultWindowFail"))] = _T("无法创建结果窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgEndProcessAll"))] = _T("结束所有同名进程");
    m_defaults[MakeKey(_T("Msg"), _T("MsgBatchEndConfirm"))] = _T("批量结束确认");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTaskManagerFail"))] = _T("启动任务管理器失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgProcessPathEmpty"))] = _T("进程路径为空。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgFileNotFoundBrowse"))] = _T("找不到文件，可能无法访问或进程已退出。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmDeleteFile"))] = _T("确认删除");
    m_defaults[MakeKey(_T("Msg"), _T("MsgFileNameEmpty"))] = _T("文件名不能为空。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgInvalidFileName"))] = _T("文件名或扩展名包含无效字符。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTargetFileExists"))] = _T("目标文件已存在，请选择其他名称。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTargetPathInvalid"))] = _T("目标路径无效，请输入或选择一个有效目录。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTargetNotDir"))] = _T("目标路径不是目录，请选择一个文件夹。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotGenUniqueName"))] = _T("无法生成唯一文件名，请检查目录或手动选择其他名称。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgTargetExists"))] = _T("目标目录已存在同名文件，请删除或更改目标路径。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCannotGetAppPath"))] = _T("无法获取应用程序路径。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgRegAccessFail"))] = _T("无法打开注册表键，请检查权限。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgStartupRemoved"))] = _T("启动项已删除。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgStartupRemoveFail"))] = _T("删除启动项失败，请检查权限。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgStartupAdded"))] = _T("启动项已添加。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgStartupAddFail"))] = _T("添加启动项失败，请检查权限。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmRemoveStartup"))] = _T("确定要删除启动项吗？");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAiNoApiKey"))] = _T("AI分析");
    m_defaults[MakeKey(_T("Msg"), _T("MsgSelectProcessFirst"))] = _T("请先选择一个进程进行分析。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfigApiKey"))] = _T("请先在设置中配置AI API密钥。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgRefreshProcessList"))] = _T("请先刷新进程列表。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgCreateProcessFail"))] = _T("创建进程失败");
    m_defaults[MakeKey(_T("Msg"), _T("MsgAdminRestartFail"))] = _T("无法以管理员权限重新启动，请手动以管理员身份运行程序。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgOleInitFail"))] = _T("初始化OLE失败，无法使用拖放功能。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgEnterLocateModeFail"))] = _T("无法进入定位模式。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfigDefaultName"))] = _T("请先在 文件→设置→文件命名 中配置默认文件名。");
    m_defaults[MakeKey(_T("Msg"), _T("MsgConfirmDeleteStartup"))] = _T("确定要删除启动项吗？");
    m_defaults[MakeKey(_T("Msg"), _T("MsgYesNo"))] = _T("是/否");
    m_defaults[MakeKey(_T("Msg"), _T("PromptRestartAdminMsg"))] = _T("操作需要管理员权限。是否以管理员权限重新启动程序？");
    m_defaults[MakeKey(_T("Msg"), _T("NeedPermission"))] = _T("需要权限");
    m_defaults[MakeKey(_T("Msg"), _T("ShellWindowFailed"))] = _T("无法获取 Shell 窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("ShellPidFailed"))] = _T("无法获取 Shell 进程 ID。");

    // ===== Process tab right-click menu =====
    m_defaults[MakeKey(_T("ProcessMenu"), _T("EndProcess"))] = _T("结束进程");
    m_defaults[MakeKey(_T("ProcessMenu"), _T("EndSameName"))] = _T("结束所有同名进程");
    m_defaults[MakeKey(_T("ProcessMenu"), _T("Locate"))] = _T("定位");
    m_defaults[MakeKey(_T("ProcessMenu"), _T("AiAnalyze"))] = _T("AI分析");

    // ===== Startup tab right-click menu =====
    m_defaults[MakeKey(_T("StartupMenu"), _T("Add"))] = _T("添加启动项");
    m_defaults[MakeKey(_T("StartupMenu"), _T("Remove"))] = _T("删除启动项");
    m_defaults[MakeKey(_T("StartupMenu"), _T("CopyPath"))] = _T("复制路径");

    // ===== Tray menu =====
    m_defaults[MakeKey(_T("TrayMenu"), _T("ShowWindow"))] = _T("显示窗口");
    m_defaults[MakeKey(_T("TrayMenu"), _T("Exit"))] = _T("退出程序");

    // ===== Window tab controls =====
    m_defaults[MakeKey(_T("WindowTab"), _T("TranparencyLabel"))] = _T("透明度: 100%");
    m_defaults[MakeKey(_T("WindowTab"), _T("LocateBtn"))] = _T("定位窗口");
    m_defaults[MakeKey(_T("WindowTab"), _T("ForceKillBtn"))] = _T("强制结束");
    m_defaults[MakeKey(_T("WindowTab"), _T("ScreenshotBtn"))] = _T("截图到剪贴板");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickTopmost"))] = _T("置顶");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickTopmostCancel"))] = _T("取消置顶");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickCloseWindow"))] = _T("关闭窗口");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickCancel"))] = _T("取消");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickTopmostOn"))] = _T("置顶");
    m_defaults[MakeKey(_T("WindowTab"), _T("RClickTopmostOff"))] = _T("取消置顶");
    m_defaults[MakeKey(_T("WindowTab"), _T("ShortcutTitle"))] = _T("快捷键");
    m_defaults[MakeKey(_T("WindowTab"), _T("ShortcutContent"))] = _T("Ctrl+Alt+Space   - 显示/隐藏工具箱\nAlt+1~6          - 切换标签页\nF5               - 刷新当前列表\nCtrl+Alt+D       - 定位窗口\n\n更多功能请查看 视图/工具/窗口 菜单。");

    // ===== File tab =====
    m_defaults[MakeKey(_T("FileTab"), _T("DropHint"))] = _T("拖拽文件到此");

    // ===== Git tab =====
    m_defaults[MakeKey(_T("GitTab"), _T("OpenGitHub"))] = _T("打开github");
    m_defaults[MakeKey(_T("GitTab"), _T("ClearPath"))] = _T("清空路径");
    m_defaults[MakeKey(_T("GitTab"), _T("GitBash"))] = _T("git bash");
    m_defaults[MakeKey(_T("GitTab"), _T("CmdWindow"))] = _T("命令窗口");
    m_defaults[MakeKey(_T("GitTab"), _T("Locate"))] = _T("定位");
    m_defaults[MakeKey(_T("GitTab"), _T("DefaultDir"))] = _T("(默认)");

    // ===== MainCtrl (main dialog controls) =====
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnRegex"))] = _T("正则");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnAiScan"))] = _T("AI扫描");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnRegexHelp"))] = _T("帮助");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnNextTrack"))] = _T("下一首");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelShutdown"))] = _T("关机");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnExecute"))] = _T("执行");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnCancelShutdown"))] = _T("解除关机");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelHourUnit"))] = _T("时");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelMinuteUnit"))] = _T("分");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelSecondUnit"))] = _T("秒");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelVolume"))] = _T("音量");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnApply"))] = _T("应用");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnMute"))] = _T("静音");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelSystem"))] = _T("系统");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnTaskManager"))] = _T("任务管理器");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelCmdLine"))] = _T("命令行");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelRunCmd"))] = _T("运行");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnRun"))] = _T("运行");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnClear"))] = _T("清空");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnSend"))] = _T("发送");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnStop"))] = _T("停止");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnNewChat"))] = _T("新对话");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnHistory"))] = _T("对话历史");
    m_defaults[MakeKey(_T("MainCtrl"), _T("LabelLocateHint"))] = _T("点击定位窗口按钮后点击目标窗口");
    m_defaults[MakeKey(_T("MainCtrl"), _T("GroupGenerate"))] = _T("生成文件副本");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnGenerate"))] = _T("生成");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnLocateWindow"))] = _T("定位窗口");
    m_defaults[MakeKey(_T("MainCtrl"), _T("DropHint"))] = _T("拖拽文件到此");
    m_defaults[MakeKey(_T("MainCtrl"), _T("CheckAutoStart"))] = _T("开机自启动");
    m_defaults[MakeKey(_T("MainCtrl"), _T("CheckTopmost"))] = _T("窗口置顶");
    m_defaults[MakeKey(_T("MainCtrl"), _T("CheckAutoClicker"))] = _T("连点器");
    m_defaults[MakeKey(_T("MainCtrl"), _T("CheckPreventLock"))] = _T("禁止自动锁屏");
    m_defaults[MakeKey(_T("MainCtrl"), _T("GroupRename"))] = _T("修改文件名");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnModify"))] = _T("修改");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnCopyTo"))] = _T("复制到");
    m_defaults[MakeKey(_T("MainCtrl"), _T("BtnMoveTo"))] = _T("移动到");
    m_defaults[MakeKey(_T("MainCtrl"), _T("GroupCopyMove"))] = _T("复制 / 移动");
    m_defaults[MakeKey(_T("MainCtrl"), _T("GitDropHint"))] = _T("拖拽文件夹或文件目录到此");

    // ===== Shutdown combo =====
    m_defaults[MakeKey(_T("Shutdown"), _T("Restart1Min"))] = _T("1分钟后重启");
    m_defaults[MakeKey(_T("Shutdown"), _T("Shutdown3Min"))] = _T("默认3分钟关机");
    m_defaults[MakeKey(_T("Shutdown"), _T("CustomTime"))] = _T("设定时间关机");

    // ===== Auto-clicker =====
    m_defaults[MakeKey(_T("AutoClicker"), _T("StoppedTitle"))] = _T("连点器");
    m_defaults[MakeKey(_T("AutoClicker"), _T("StoppedMsg"))] = _T("连点已停止");
    m_defaults[MakeKey(_T("AutoClicker"), _T("StatusStopped"))] = _T("已停止");
    m_defaults[MakeKey(_T("AutoClicker"), _T("StatusClicking"))] = _T("正在点击 (按 %c 停止)");
    m_defaults[MakeKey(_T("AutoClicker"), _T("StatusWaiting"))] = _T("等待触发 (按 %c 开始, %c 停止)");

    // ===== AI Assistant =====
    m_defaults[MakeKey(_T("AI"), _T("ReadyMsg"))] = _T("AI Assistant Ready<br><span style='font-size:13px;'>Ask me anything about this toolbox!</span>");
    m_defaults[MakeKey(_T("AI"), _T("NoApiKey"))] = _T("<div style='color:red;'>[Error] No API Key configured for %s. Please go to File > Settings > AI Assistant to configure it.</div>");

    // ===== Settings dialog =====
    m_defaults[MakeKey(_T("Settings"), _T("LanguageLabel"))] = _T("语言");
    m_defaults[MakeKey(_T("Settings"), _T("LanguageRestartHint"))] = _T("切换语言后应用将自动重启。");
    m_defaults[MakeKey(_T("Settings"), _T("TabPath"))] = _T("路径配置");
    m_defaults[MakeKey(_T("Settings"), _T("TabTemplate"))] = _T("文件命名");
    m_defaults[MakeKey(_T("Settings"), _T("TabAutoClicker"))] = _T("连点器");
    m_defaults[MakeKey(_T("Settings"), _T("TabAI"))] = _T("AI 助手");
    m_defaults[MakeKey(_T("Settings"), _T("AIVendor"))] = _T("AI 供应商");
    m_defaults[MakeKey(_T("Settings"), _T("AIKey"))] = _T("API Key");
    m_defaults[MakeKey(_T("Settings"), _T("AIShow"))] = _T("显示");
    m_defaults[MakeKey(_T("Settings"), _T("AIHide"))] = _T("隐藏");
    m_defaults[MakeKey(_T("Settings"), _T("TabScreenshot"))] = _T("截图");
    m_defaults[MakeKey(_T("Settings"), _T("TabStickyNote"))] = _T("便签");
    m_defaults[MakeKey(_T("Settings"), _T("TabSites"))] = _T("网址");
    m_defaults[MakeKey(_T("Settings"), _T("TabLanguage"))] = _T("语言");
    m_defaults[MakeKey(_T("Settings"), _T("GroupFileNaming"))] = _T("文件命名");
    m_defaults[MakeKey(_T("Settings"), _T("LabelDefaultName"))] = _T("默认文件名:");
    m_defaults[MakeKey(_T("Settings"), _T("GroupPathSettings"))] = _T("路径设置");
    m_defaults[MakeKey(_T("Settings"), _T("LabelScreenshot"))] = _T("截图保存目录:");
    m_defaults[MakeKey(_T("Settings"), _T("LabelStickyDir"))] = _T("便签保存目录:");
    m_defaults[MakeKey(_T("Settings"), _T("LabelClickInterval"))] = _T("连点器间隔(ms):");
    m_defaults[MakeKey(_T("Settings"), _T("LabelStartKey"))] = _T("开始键:");
    m_defaults[MakeKey(_T("Settings"), _T("LabelStopKey"))] = _T("停止键:");
    m_defaults[MakeKey(_T("Settings"), _T("BtnBrowse"))] = _T("浏览");
    m_defaults[MakeKey(_T("Settings"), _T("GroupSites"))] = _T("网址");
    m_defaults[MakeKey(_T("Settings"), _T("GroupAI"))] = _T("AI助手");
    m_defaults[MakeKey(_T("Settings"), _T("LabelVendor"))] = _T("供应商:");
    m_defaults[MakeKey(_T("Settings"), _T("LabelApiKey"))] = _T("API Key:");
    m_defaults[MakeKey(_T("Settings"), _T("BtnShow"))] = _T("显示");
    m_defaults[MakeKey(_T("Settings"), _T("BtnHide"))] = _T("隐藏");
    m_defaults[MakeKey(_T("Settings"), _T("GroupLanguage"))] = _T("语言");
    m_defaults[MakeKey(_T("Settings"), _T("LabelLanguage"))] = _T("语言:");
    m_defaults[MakeKey(_T("Settings"), _T("BtnOK"))] = _T("确定");
    m_defaults[MakeKey(_T("Settings"), _T("BtnCancel"))] = _T("取消");
    m_defaults[MakeKey(_T("Settings"), _T("GroupAutoClicker"))] = _T("连点器");
    m_defaults[MakeKey(_T("Settings"), _T("DlgTitleScreenshot"))] = _T("选择截图保存目录");
    m_defaults[MakeKey(_T("Settings"), _T("DlgTitleSticky"))] = _T("选择便签保存目录");
    m_defaults[MakeKey(_T("Settings"), _T("DlgTitleGitBash"))] = _T("选择 Git Bash 路径");
    m_defaults[MakeKey(_T("Settings"), _T("GroupGit"))] = _T("Git");
    m_defaults[MakeKey(_T("Settings"), _T("LabelGitBash"))] = _T("Git Bash路径:");
    m_defaults[MakeKey(_T("Settings"), _T("MsgKeySame"))] = _T("开始键和停止键不能相同，请重新设置。");

    // ===== Context Menu Manager =====
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColLocation"))] = _T("位置");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColDisplayName"))] = _T("显示名称");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColType"))] = _T("类型");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColVisibility"))] = _T("可见性");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColKeyName"))] = _T("键名");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ColCommand"))] = _T("命令");
    m_defaults[MakeKey(_T("ContextMenu"), _T("TypeStatic"))] = _T("静态");
    m_defaults[MakeKey(_T("ContextMenu"), _T("TypeShellEx"))] = _T("ShellEx");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Visible"))] = _T("可见");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Hidden"))] = _T("隐藏");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Extended"))] = _T("扩展");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Disabled"))] = _T("已禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Enabled"))] = _T("已启用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickEnable"))] = _T("启用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickDisable"))] = _T("禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickEnableSelected"))] = _T("启用选中");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickDisableSelected"))] = _T("禁用选中");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickCustomParse"))] = _T("自定义名称解析");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickAiAnalyze"))] = _T("AI解析");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickEnableAll"))] = _T("启用全部");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RClickLocateReg"))] = _T("定位(注册表)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("DictInfoCurrent"))] = _T("当前字典: %s  |  缓存: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("DictInfoEmbeddedCache"))] = _T("当前字典: 内置 + 缓存  |  缓存: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("DictInfoEmbedded"))] = _T("当前字典: 内置(硬编码)  |  缓存: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("FolderMenuLabel"))] = _T("文件夹右键菜单: 用本程序打开");
    m_defaults[MakeKey(_T("ContextMenu"), _T("Win11ClassicLabel"))] = _T("Win11经典菜单(Shift右键效果)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnRefresh"))] = _T("刷新");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnRebuild"))] = _T("重建字典");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnDictPath"))] = _T("字典路径");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnDictOpen"))] = _T("打开字典");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnUndo"))] = _T("撤销");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnHistory"))] = _T("历史记录");
    m_defaults[MakeKey(_T("ContextMenu"), _T("BtnQuery"))] = _T("查询");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ExtensionPlaceholder"))] = _T("输入后缀名(可省略.)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("LabelLocation"))] = _T("位置筛选:");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusReady"))] = _T("就绪");

    // ===== Environment Variable Manager =====
    m_defaults[MakeKey(_T("EnvVar"), _T("ColName"))] = _T("变量名");
    m_defaults[MakeKey(_T("EnvVar"), _T("ColValue"))] = _T("变量值");
    m_defaults[MakeKey(_T("EnvVar"), _T("SystemLabel"))] = _T("系统变量");
    m_defaults[MakeKey(_T("EnvVar"), _T("UserLabel"))] = _T("用户变量");
    m_defaults[MakeKey(_T("EnvVar"), _T("BtnAdd"))] = _T("添加");
    m_defaults[MakeKey(_T("EnvVar"), _T("BtnEdit"))] = _T("编辑");
    m_defaults[MakeKey(_T("EnvVar"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("EnvVar"), _T("BtnRefresh"))] = _T("刷新");
    m_defaults[MakeKey(_T("EnvVar"), _T("BtnExport"))] = _T("导出");
    m_defaults[MakeKey(_T("EnvVar"), _T("SearchPlaceholder"))] = _T("搜索环境变量...");
    m_defaults[MakeKey(_T("EnvVar"), _T("LabelSearch"))] = _T("搜索:");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickEdit"))] = _T("编辑");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickCopyName"))] = _T("复制名称");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickCopyValue"))] = _T("复制值");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickCopy"))] = _T("复制");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickMoveUp"))] = _T("上移");
    m_defaults[MakeKey(_T("EnvVar"), _T("RClickMoveDown"))] = _T("下移");

    // ===== File Lock Viewer =====
    m_defaults[MakeKey(_T("FileLock"), _T("ColFilePath"))] = _T("文件路径");
    m_defaults[MakeKey(_T("FileLock"), _T("ColProcessName"))] = _T("进程名");
    m_defaults[MakeKey(_T("FileLock"), _T("ColPID"))] = _T("PID");
    m_defaults[MakeKey(_T("FileLock"), _T("ColType"))] = _T("进程类型");
    m_defaults[MakeKey(_T("FileLock"), _T("ColProcessPath"))] = _T("进程路径");
    m_defaults[MakeKey(_T("FileLock"), _T("BtnEnd"))] = _T("结束");
    m_defaults[MakeKey(_T("FileLock"), _T("BtnEndAll"))] = _T("全部结束");
    m_defaults[MakeKey(_T("FileLock"), _T("BtnLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("FileLock"), _T("BtnRefresh"))] = _T("刷新");
    m_defaults[MakeKey(_T("FileLock"), _T("BtnClear"))] = _T("清除");
    m_defaults[MakeKey(_T("FileLock"), _T("Hint"))] = _T("拖放文件到此处查看占用进程（需管理员权限）");

    // ===== Batch Rename =====
    m_defaults[MakeKey(_T("BatchRename"), _T("ColOriginal"))] = _T("原文件名");
    m_defaults[MakeKey(_T("BatchRename"), _T("ColNew"))] = _T("新文件名");
    m_defaults[MakeKey(_T("BatchRename"), _T("ColStatus"))] = _T("状态");
    m_defaults[MakeKey(_T("BatchRename"), _T("ColSerial"))] = _T("#");
    m_defaults[MakeKey(_T("BatchRename"), _T("ColFolderName"))] = _T("文件夹名");
    m_defaults[MakeKey(_T("BatchRename"), _T("TabFolder"))] = _T("文件夹操作");
    m_defaults[MakeKey(_T("BatchRename"), _T("TabFile"))] = _T("文件批量处理");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnBrowse"))] = _T("浏览");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnPreview"))] = _T("预览");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnExecute"))] = _T("执行");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnUndo"))] = _T("撤销");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnRefresh"))] = _T("刷新");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnAiAssistant"))] = _T("AI助手");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnResetAll"))] = _T("全部重置");
    m_defaults[MakeKey(_T("BatchRename"), _T("PrefixLabel"))] = _T("前缀");
    m_defaults[MakeKey(_T("BatchRename"), _T("SuffixLabel"))] = _T("后缀");
    m_defaults[MakeKey(_T("BatchRename"), _T("ReplaceFromLabel"))] = _T("替换");
    m_defaults[MakeKey(_T("BatchRename"), _T("ReplaceToLabel"))] = _T("为");
    m_defaults[MakeKey(_T("BatchRename"), _T("NumberLabel"))] = _T("自动编号");
    m_defaults[MakeKey(_T("BatchRename"), _T("RegexLabel"))] = _T("正则");
    m_defaults[MakeKey(_T("BatchRename"), _T("IgnoreExtLabel"))] = _T("忽略扩展名");
    m_defaults[MakeKey(_T("BatchRename"), _T("IgnoreMatchLabel"))] = _T("忽略匹配");
    m_defaults[MakeKey(_T("BatchRename"), _T("TrackExtLabel"))] = _T("跟踪扩展名");
    m_defaults[MakeKey(_T("BatchRename"), _T("TrackMatchLabel"))] = _T("跟踪匹配");
    m_defaults[MakeKey(_T("BatchRename"), _T("DeleteMatchLabel"))] = _T("匹配删除");
    m_defaults[MakeKey(_T("BatchRename"), _T("NumberAfterExt"))] = _T("编号在扩展名后");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnClear"))] = _T("清空");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnRenameDir"))] = _T("重命名目录");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnMoveDir"))] = _T("移动目录");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnDeleteDir"))] = _T("删除目录");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnRename"))] = _T("重命名");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnMove"))] = _T("移动");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnSelectAll"))] = _T("全选");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnDeselectAll"))] = _T("取消全选");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnClearDelete"))] = _T("清除删除标记");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnRegexHelp"))] = _T("正则表达式手册");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnClearIgnore"))] = _T("取消全部忽略");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnClearTrack"))] = _T("取消全部跟踪");
    m_defaults[MakeKey(_T("BatchRename"), _T("CheckInvert"))] = _T("反选");
    m_defaults[MakeKey(_T("BatchRename"), _T("CheckIgnoreExt"))] = _T("忽略后缀(英文;相隔)");
    m_defaults[MakeKey(_T("BatchRename"), _T("CheckIgnoreMatch"))] = _T("忽略匹配(包括后缀)");
    m_defaults[MakeKey(_T("BatchRename"), _T("CheckTrackExt"))] = _T("跟踪后缀(英文;相隔)");
    m_defaults[MakeKey(_T("BatchRename"), _T("CheckTrackMatch"))] = _T("跟踪匹配(包括后缀)");
    m_defaults[MakeKey(_T("BatchRename"), _T("LabelFolder"))] = _T("文件夹:");
    m_defaults[MakeKey(_T("BatchRename"), _T("LabelCurrentDir"))] = _T("当前目录:");
    m_defaults[MakeKey(_T("BatchRename"), _T("LabelSubdir"))] = _T("子目录操作:");
    m_defaults[MakeKey(_T("BatchRename"), _T("LabelStartNum"))] = _T("起始:");
    m_defaults[MakeKey(_T("BatchRename"), _T("LabelTip"))] = _T("提示: 扩展名不会被修改, 右键文件列表可标记删除/忽略/跟踪");
    m_defaults[MakeKey(_T("BatchRename"), _T("GroupIgnore"))] = _T("忽略规则");
    m_defaults[MakeKey(_T("BatchRename"), _T("GroupTrack"))] = _T("跟踪规则");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgNoValidMapping"))] = _T("AI 没有生成有效的重命名映射。\n跳过了 %d 个无效/重复条目。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgCleanedIllegal"))] = _T("\n清理了 %d 个含非法字符的文件名。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgDesc"))] = _T("描述你的重命名需求（支持自然语言）：");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgPreview"))] = _T("AI 建议的新文件名预览：");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnAiSend"))] = _T("发送给AI");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnApply"))] = _T("应用");
    m_defaults[MakeKey(_T("BatchRename"), _T("BtnCancel"))] = _T("取消");

    // ===== Process Scan =====
    m_defaults[MakeKey(_T("ProcessScan"), _T("ColProcessName"))] = _T("进程名");
    m_defaults[MakeKey(_T("ProcessScan"), _T("ColPID"))] = _T("PID");
    m_defaults[MakeKey(_T("ProcessScan"), _T("ColSecurityLevel"))] = _T("安全等级");
    m_defaults[MakeKey(_T("ProcessScan"), _T("ColDescription"))] = _T("分析说明");
    m_defaults[MakeKey(_T("ProcessScan"), _T("BtnEnd"))] = _T("结束");
    m_defaults[MakeKey(_T("ProcessScan"), _T("BtnLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("ProcessScan"), _T("BtnEndAll"))] = _T("全部结束");
    m_defaults[MakeKey(_T("ProcessScan"), _T("BtnStartScan"))] = _T("开始扫描");
    m_defaults[MakeKey(_T("ProcessScan"), _T("LevelLabel"))] = _T("审查级别");
    m_defaults[MakeKey(_T("ProcessScan"), _T("LevelConservative"))] = _T("保守");
    m_defaults[MakeKey(_T("ProcessScan"), _T("LevelStandard"))] = _T("标准");
    m_defaults[MakeKey(_T("ProcessScan"), _T("LevelAggressive"))] = _T("激进");
    m_defaults[MakeKey(_T("ProcessScan"), _T("RClickEnd"))] = _T("结束进程");
    m_defaults[MakeKey(_T("ProcessScan"), _T("RClickLocate"))] = _T("定位");
    m_defaults[MakeKey(_T("ProcessScan"), _T("RClickCopyPath"))] = _T("复制路径");

    // ===== QR Code =====
    m_defaults[MakeKey(_T("QRCode"), _T("InputPlaceholder"))] = _T("请输入文本或链接...");
    m_defaults[MakeKey(_T("QRCode"), _T("BtnGenerate"))] = _T("生成二维码");
    m_defaults[MakeKey(_T("QRCode"), _T("BtnCopy"))] = _T("复制到剪贴板");
    m_defaults[MakeKey(_T("QRCode"), _T("BtnSave"))] = _T("保存");
    m_defaults[MakeKey(_T("QRCode"), _T("LabelInput"))] = _T("输入文本(URL/文字):");

    // ===== Screenshot OCR =====
    m_defaults[MakeKey(_T("OCR"), _T("BtnCapture"))] = _T("开始截图");
    m_defaults[MakeKey(_T("OCR"), _T("BtnCopy"))] = _T("复制结果");
    m_defaults[MakeKey(_T("OCR"), _T("BtnTranslate"))] = _T("翻译 >>");
    m_defaults[MakeKey(_T("OCR"), _T("LabelResult"))] = _T("识别结果:");
    m_defaults[MakeKey(_T("OCR"), _T("LabelTranslated"))] = _T("翻译结果:");
    m_defaults[MakeKey(_T("OCR"), _T("StatusHint"))] = _T("点击按钮后拖拽选择屏幕区域,按ESC取消");
    m_defaults[MakeKey(_T("OCR"), _T("LangChinese"))] = _T("中文");
    m_defaults[MakeKey(_T("OCR"), _T("LangEnglish"))] = _T("英文");
    m_defaults[MakeKey(_T("OCR"), _T("LangJapanese"))] = _T("日文");
    m_defaults[MakeKey(_T("OCR"), _T("LangKorean"))] = _T("韩文");

    // ===== Markdown =====
    m_defaults[MakeKey(_T("Markdown"), _T("BtnOpen"))] = _T("打开");
    m_defaults[MakeKey(_T("Markdown"), _T("ExecuteBtn"))] = _T("执行");
    m_defaults[MakeKey(_T("Markdown"), _T("StatusReady"))] = _T("就绪");
    m_defaults[MakeKey(_T("Markdown"), _T("PurposeLabel"))] = _T("用途：");
    m_defaults[MakeKey(_T("Markdown"), _T("RiskLevelLabel"))] = _T("风险等级：");
    m_defaults[MakeKey(_T("Markdown"), _T("RiskLow"))] = _T("低");
    m_defaults[MakeKey(_T("Markdown"), _T("RiskMedium"))] = _T("中");
    m_defaults[MakeKey(_T("Markdown"), _T("RiskHigh"))] = _T("高");

    // ===== Encoding Converter =====
    m_defaults[MakeKey(_T("Encoding"), _T("BtnOpen"))] = _T("打开");
    m_defaults[MakeKey(_T("Encoding"), _T("BtnSaveAs"))] = _T("另存为");
    m_defaults[MakeKey(_T("Encoding"), _T("BtnOverwrite"))] = _T("覆盖");
    m_defaults[MakeKey(_T("Encoding"), _T("BtnConvert"))] = _T("转换");
    m_defaults[MakeKey(_T("Encoding"), _T("LabelSrcEnc"))] = _T("源编码:");
    m_defaults[MakeKey(_T("Encoding"), _T("LabelDstEnc"))] = _T("目标编码:");

    // ===== Git Command Dialog =====
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("StatusReady"))] = _T("状态: 准备就绪");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("ColDesc"))] = _T("说明");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("ColCmd"))] = _T("命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnAddCmd"))] = _T("添加命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnClearCmds"))] = _T("清空");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnCopyOutput"))] = _T("复制输出");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnClose"))] = _T("关闭");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("OutputLabel"))] = _T("输出:");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickExecute"))] = _T("执行命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickCopy"))] = _T("复制命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickEdit"))] = _T("编辑命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickDelete"))] = _T("删除命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickExecuteSelected"))] = _T("执行选中命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("RClickDeleteSelected"))] = _T("删除选中命令");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("LabelWorkDir"))] = _T("工作目录:");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("LabelAiAsk"))] = _T("向AI提问:");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnAiAsk"))] = _T("提问");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("LabelCmdList"))] = _T("命令列表 (右键执行/复制/编辑/删除):");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("LabelCmdInput"))] = _T("命令输入:");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnCmdExecute"))] = _T("执行");
    m_defaults[MakeKey(_T("GitCmdDlg"), _T("BtnCmdInputClear"))] = _T("清空");

    // ===== Git command execution =====
    m_defaults[MakeKey(_T("GitExec"), _T("ConfirmTitle"))] = _T("确认执行");
    m_defaults[MakeKey(_T("GitExec"), _T("ConfirmMsg"))] = _T("确定要执行以下命令？\n\n命令: %s\n工作目录: %s");
    m_defaults[MakeKey(_T("GitExec"), _T("Running"))] = _T("状态: 运行中...");
    m_defaults[MakeKey(_T("GitExec"), _T("Done"))] = _T("状态: 已完成");
    m_defaults[MakeKey(_T("GitExec"), _T("Failed"))] = _T("状态: 执行失败");
    m_defaults[MakeKey(_T("GitExec"), _T("Cancelled"))] = _T("状态: 已取消");

    // ===== Conversation History =====
    m_defaults[MakeKey(_T("ConvHistory"), _T("ColTitle"))] = _T("标题");
    m_defaults[MakeKey(_T("ConvHistory"), _T("ColCreated"))] = _T("创建时间");
    m_defaults[MakeKey(_T("ConvHistory"), _T("ColModified"))] = _T("修改时间");
    m_defaults[MakeKey(_T("ConvHistory"), _T("BtnLoad"))] = _T("加载");
    m_defaults[MakeKey(_T("ConvHistory"), _T("BtnRename"))] = _T("重命名");
    m_defaults[MakeKey(_T("ConvHistory"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("ConvHistory"), _T("BtnPath"))] = _T("保存位置");
    m_defaults[MakeKey(_T("ConvHistory"), _T("BtnClose"))] = _T("关闭");
    m_defaults[MakeKey(_T("ConvHistory"), _T("RClickLoad"))] = _T("加载");
    m_defaults[MakeKey(_T("ConvHistory"), _T("RClickDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("ConvHistory"), _T("RClickRename"))] = _T("重命名");

    // ===== Sticky Note =====
    m_defaults[MakeKey(_T("StickyNote"), _T("BtnBrowse"))] = _T("浏览");
    m_defaults[MakeKey(_T("StickyNote"), _T("RClickExit"))] = _T("退出便签");

    // ===== Quick buttons =====
    m_defaults[MakeKey(_T("QuickBtn"), _T("NextTrack"))] = _T("下一首");

    // ===== Volume =====
    m_defaults[MakeKey(_T("Volume"), _T("BtnApply"))] = _T("应用");
    m_defaults[MakeKey(_T("Volume"), _T("BtnMute"))] = _T("静音");
    m_defaults[MakeKey(_T("Volume"), _T("Btn10Percent"))] = _T("10%");
    m_defaults[MakeKey(_T("Volume"), _T("OpenTaskMgr"))] = _T("任务管理器");

    // ===== Run command =====
    m_defaults[MakeKey(_T("RunCmd"), _T("BtnRun"))] = _T("运行");
    m_defaults[MakeKey(_T("RunCmd"), _T("BtnClear"))] = _T("清空");
    m_defaults[MakeKey(_T("RunCmd"), _T("InputPlaceholder"))] = _T("输入命令...");

    // ===== WSL/PowerShell =====
    m_defaults[MakeKey(_T("ToolLaunch"), _T("PowerShell"))] = _T("PowerShell");
    m_defaults[MakeKey(_T("ToolLaunch"), _T("WSL"))] = _T("WSL");
    m_defaults[MakeKey(_T("ToolLaunch"), _T("GitHub"))] = _T("GitHub");

    // ===== Filter controls =====
    m_defaults[MakeKey(_T("Filter"), _T("RegexCheck"))] = _T("正则");
    m_defaults[MakeKey(_T("Filter"), _T("RegexHelp"))] = _T("帮助");

    // ===== Not used any more in code but kept for reference =====
    m_defaults[MakeKey(_T("MainDlg"), _T("WindowTitleSuffix"))] = _T("(ctrl+alt+空格唤起此窗口)");

    // ===== Menu items =====
    m_defaults[MakeKey(_T("Menu"), _T("File"))] = _T("文件(&F)");
    m_defaults[MakeKey(_T("Menu"), _T("Settings"))] = _T("设置...(&S)");
    m_defaults[MakeKey(_T("Menu"), _T("Exit"))] = _T("退出(&X)");
    m_defaults[MakeKey(_T("Menu"), _T("View"))] = _T("视图(&V)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewProcess"))] = _T("进程管理(&P)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewStartup"))] = _T("启动项管理(&S)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewClipboard"))] = _T("剪贴板(&C)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewWindow"))] = _T("窗口处理(&W)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewFile"))] = _T("文件管理(&F)");
    m_defaults[MakeKey(_T("Menu"), _T("ViewGit"))] = _T("git工具箱(&G)");
    m_defaults[MakeKey(_T("Menu"), _T("Open"))] = _T("打开(&O)");
    
    
    m_defaults[MakeKey(_T("Menu"), _T("OpenPowerShell"))] = _T("PowerShell(&P)");
    m_defaults[MakeKey(_T("Menu"), _T("OpenWSL"))] = _T("WSL(&S)");
    m_defaults[MakeKey(_T("Menu"), _T("OpenGitBash"))] = _T("Git Bash(&G)");
    m_defaults[MakeKey(_T("Menu"), _T("Window"))] = _T("窗口(&W)");
    m_defaults[MakeKey(_T("Menu"), _T("WindowLocate"))] = _T("窗口定位(&L)");
    m_defaults[MakeKey(_T("Menu"), _T("WindowUntopmost"))] = _T("取消全部置顶(&T)");
    m_defaults[MakeKey(_T("Menu"), _T("WindowClose"))] = _T("关闭选中窗口(&C)");
    m_defaults[MakeKey(_T("Menu"), _T("MenuMinimizeToTray"))] = _T("最小化到托盘(&M)");
    m_defaults[MakeKey(_T("Menu"), _T("Tools"))] = _T("工具(&T)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsText"))] = _T("文本工具(&T)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsMarkdown"))] = _T("Markdown 预览(&M)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsEncoding"))] = _T("编码转换(&E)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsImage"))] = _T("图像工具(&I)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsQRCode"))] = _T("二维码生成(&Q)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsScreenshotOCR"))] = _T("截图OCR(&O)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsFile"))] = _T("文件工具(&F)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsBatchRename"))] = _T("文件夹处理(&R)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsSystem"))] = _T("系统工具(&S)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsContextMenu"))] = _T("右键菜单管理(&R)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsEnvVar"))] = _T("环境变量管理(&E)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsFileLock"))] = _T("文件占用查看(&F)");
    m_defaults[MakeKey(_T("Menu"), _T("ToolsStickyNote"))] = _T("简易便签(&N)");
    m_defaults[MakeKey(_T("Menu"), _T("Help"))] = _T("帮助(&H)");
    m_defaults[MakeKey(_T("Menu"), _T("HelpAbout"))] = _T("关于...(&A)");
    m_defaults[MakeKey(_T("Menu"), _T("HelpShortcuts"))] = _T("快捷键列表(&K)");
    m_defaults[MakeKey(_T("Menu"), _T("HelpRegex"))] = _T("正则表达式规则(&R)");
    m_defaults[MakeKey(_T("Menu"), _T("HelpGitHub"))] = _T("GitHub(&G)");

    // ===== Dialog Captions =====
    m_defaults[MakeKey(_T("DlgCaption"), _T("AboutDlg"))] = _T("关于 PowerBox");
    m_defaults[MakeKey(_T("DlgCaption"), _T("MainDlg"))] = _T("PowerBox");
    m_defaults[MakeKey(_T("DlgCaption"), _T("SettingsDlg"))] = _T("配置");
    m_defaults[MakeKey(_T("DlgCaption"), _T("ClickSpeedDlg"))] = _T("连点器速度");
    m_defaults[MakeKey(_T("DlgCaption"), _T("QRCodeDlg"))] = _T("二维码生成");
    m_defaults[MakeKey(_T("DlgCaption"), _T("OCRDlg"))] = _T("截图OCR");
    m_defaults[MakeKey(_T("DlgCaption"), _T("BatchRenameDlg"))] = _T("文件夹处理（支持拖入文件夹）");
    m_defaults[MakeKey(_T("DlgCaption"), _T("InputDlg"))] = _T("输入");
    m_defaults[MakeKey(_T("DlgCaption"), _T("AiRenameDlg"))] = _T("AI 批量重命名助手");
    m_defaults[MakeKey(_T("DlgCaption"), _T("RegexGuideDlg"))] = _T("正则表达式规则指南");
    m_defaults[MakeKey(_T("DlgCaption"), _T("StickyNoteDlg"))] = _T("简易便签");
    m_defaults[MakeKey(_T("DlgCaption"), _T("MarkdownDlg"))] = _T("Markdown Preview");
    m_defaults[MakeKey(_T("DlgCaption"), _T("EncodingDlg"))] = _T("编码转换器");
    m_defaults[MakeKey(_T("DlgCaption"), _T("ContextMenuDlg"))] = _T("右键菜单管理器");
    m_defaults[MakeKey(_T("DlgCaption"), _T("EnvVarDlg"))] = _T("环境变量管理");
    m_defaults[MakeKey(_T("DlgCaption"), _T("EnvVarEditDlg"))] = _T("环境变量");
    m_defaults[MakeKey(_T("DlgCaption"), _T("EnvVarPathEditDlg"))] = _T("编辑 PATH 变量");
    m_defaults[MakeKey(_T("DlgCaption"), _T("FileLockDlg"))] = _T("文件占用查看");
    m_defaults[MakeKey(_T("DlgCaption"), _T("ProcessScanDlg"))] = _T("AI进程扫描结果");
    m_defaults[MakeKey(_T("DlgCaption"), _T("ProcessAiResultDlg"))] = _T("AI进程分析结果");
    m_defaults[MakeKey(_T("DlgCaption"), _T("GitCmdDlg"))] = _T("Git 命令");
    m_defaults[MakeKey(_T("DlgCaption"), _T("ConvHistoryDlg"))] = _T("对话历史");

    // ===== Shortcut dialog =====
    m_defaults[MakeKey(_T("Shortcut"), _T("ShortcutList"))] = _T("Ctrl+Alt+Space   - 显示/隐藏工具箱\nAlt+1~6          - 切换标签页\nF5               - 刷新当前列表\nCtrl+Alt+D       - 定位窗口\n\n更多功能请查看 视图/工具/窗口 菜单。");
    m_defaults[MakeKey(_T("Shortcut"), _T("ShortcutListTitle"))] = _T("快捷键");

    // ===== About dialog =====
    m_defaults[MakeKey(_T("AboutDlg"), _T("Version"))] = _T("PowerBox，版本 1.0");
    m_defaults[MakeKey(_T("AboutDlg"), _T("Copyright"))] = _T("版权所有 (C) 2026");
    m_defaults[MakeKey(_T("AboutDlg"), _T("BtnOK"))] = _T("确定");

    // ===== High-risk confirmation dialog =====
    m_defaults[MakeKey(_T("Msg"), _T("HighRiskWarningTitle"))] = _T("高危操作警告");
    m_defaults[MakeKey(_T("Msg"), _T("HighRiskPrompt"))] = _T("此操作风险较高，请输入 \"确认执行\" 以继续：");

    // ===== Window tab topmost label =====
    m_defaults[MakeKey(_T("WindowTab"), _T("TopmostLabel"))] = _T("置顶[%zu]");

    // ===== Batch rename =====
    m_defaults[MakeKey(_T("BatchRename"), _T("ResetAllConfirm"))] = _T("确定要清除所有改动吗？\n将重置前缀、后缀、替换、序号、删除标记、AI 生成名称。");
    m_defaults[MakeKey(_T("BatchRename"), _T("Confirm"))] = _T("确认");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectFolderFirst"))] = _T("请先选择文件夹加载文件列表。");

    // ===== Auto-clicker VBS input =====
    m_defaults[MakeKey(_T("AutoClicker"), _T("VbsPrompt"))] = _T("点击频率：(ms/次)");
    m_defaults[MakeKey(_T("AutoClicker"), _T("VbsTitle"))] = _T("连点器");
    m_defaults[MakeKey(_T("AutoClicker"), _T("LabelInterval"))] = _T("间隔(ms):");

    // ===== Process scan AI fail messages =====
    m_defaults[MakeKey(_T("Msg"), _T("AiScanFailedStatus"))] = _T("AI扫描失败: %s");
    m_defaults[MakeKey(_T("Msg"), _T("AiScanFailedMsg"))] = _T("AI扫描失败，请检查网络连接和API密钥。\n%s");

    // ===== Process AI result dialog =====
    m_defaults[MakeKey(_T("ProcessAiResult"), _T("DefaultFileName"))] = _T("AI分析结果.txt");
    m_defaults[MakeKey(_T("ProcessAiResult"), _T("FileFilter"))] = _T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||");
    m_defaults[MakeKey(_T("ProcessAiResult"), _T("BtnCopy"))] = _T("复制结果");
    m_defaults[MakeKey(_T("ProcessAiResult"), _T("BtnSave"))] = _T("保存结果");
    m_defaults[MakeKey(_T("ProcessAiResult"), _T("BtnClose"))] = _T("关闭");

    // ===== OCR translation error messages =====
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrInitNetwork"))] = _T("翻译失败：无法初始化网络。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrConnectServer"))] = _T("翻译失败：无法连接服务器。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrCreateRequest"))] = _T("翻译失败：无法创建请求。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrTimeout"))] = _T("翻译失败：请求超时或网络错误。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrPrefix"))] = _T("翻译失败：");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrParseResponse"))] = _T("翻译失败：无法解析响应。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateErrResponseFormat"))] = _T("翻译失败：响应格式异常。");
    m_defaults[MakeKey(_T("OCR"), _T("CheckUseAiTranslate"))] = _T("使用AI翻译");
    m_defaults[MakeKey(_T("OCR"), _T("AiTranslateNoApiKey"))] = _T("AI翻译：未配置API密钥，请先在设置中配置。");
    m_defaults[MakeKey(_T("OCR"), _T("TranslateQueryLimitExceeded"))] = _T("翻译内容过长，MyMemory API 拒绝翻译。建议勾选\"使用AI翻译\"后重试。");

    // ===== OCR language pair display names =====
    m_defaults[MakeKey(_T("OCR"), _T("LangPair0"))] = _T("中文 → 英文");
    m_defaults[MakeKey(_T("OCR"), _T("LangPair1"))] = _T("中文 → 日文");
    m_defaults[MakeKey(_T("OCR"), _T("LangPair2"))] = _T("中文 → 韩文");
    m_defaults[MakeKey(_T("OCR"), _T("LangPair3"))] = _T("英文 → 中文");
    m_defaults[MakeKey(_T("OCR"), _T("LangPair4"))] = _T("日文 → 中文");
    m_defaults[MakeKey(_T("OCR"), _T("LangPair5"))] = _T("韩文 → 中文");

    // ===== Regex guide =====
    m_defaults[MakeKey(_T("RegexGuide"), _T("GuideContent"))] =
        _T("========== 正则表达式规则指南 ==========\r\n\r\n")
        _T("本工具使用 C++ std::regex，默认 ECMAScript 语法（与 JavaScript 正则兼容）。\r\n\r\n")
        _T("【基本语法】\r\n")
        _T("  .        匹配任意单个字符（换行符除外）\r\n")
        _T("  \\d       匹配任意数字 [0-9]\r\n")
        _T("  \\D       匹配非数字\r\n")
        _T("  \\w       匹配字母、数字、下划线 [a-zA-Z0-9_]\r\n")
        _T("  \\W       匹配非单词字符\r\n")
        _T("  \\s       匹配空白字符（空格、制表符等）\r\n")
        _T("  \\S       匹配非空白字符\r\n")
        _T("  [abc]    匹配 a、b 或 c 中的任意一个\r\n")
        _T("  [^abc]   匹配除了 a、b、c 之外的任意字符\r\n")
        _T("  [a-z]    匹配小写字母 a 到 z\r\n\r\n")
        _T("【量词】\r\n")
        _T("  *        匹配前面的表达式 0 次或多次\r\n")
        _T("  +        匹配前面的表达式 1 次或多次\r\n")
        _T("  ?        匹配前面的表达式 0 次或 1 次\r\n")
        _T("  {n}      匹配前面的表达式恰好 n 次\r\n")
        _T("  {n,}     匹配前面的表达式至少 n 次\r\n")
        _T("  {n,m}    匹配前面的表达式 n 到 m 次\r\n\r\n")
        _T("【位置锚定】\r\n")
        _T("  ^        匹配字符串开头\r\n")
        _T("  $        匹配字符串结尾\r\n")
        _T("  \\b       匹配单词边界\r\n\r\n")
        _T("【捕获组与替换】\r\n")
        _T("  (pattern)   捕获组，匹配的内容可在替换中引用\r\n")
        _T("  (?:pattern) 非捕获组，不保存匹配内容\r\n")
        _T("  $1, $2, ... 在替换文本中引用第 1、第 2 个捕获组\r\n")
        _T("  $&           在替换文本中引用整个匹配\r\n\r\n")
        _T("【常用示例】\r\n")
        _T("  删除所有数字:\r\n")
        _T("    查找: \\d+    替换: (空)\r\n\r\n")
        _T("  删除前导序号（如 \"01_\"）:\r\n")
        _T("    查找: ^\\d+_    替换: (空)\r\n\r\n")
        _T("  将 \"IMG_001\" 改为 \"Photo_001\":\r\n")
        _T("    查找: ^IMG    替换: Photo\r\n\r\n")
        _T("  交换 \"name_date\" 为 \"date_name\":\r\n")
        _T("    查找: ^(\\w+)_(\\w+)$    替换: $2_$1\r\n\r\n")
        _T("  删除括号及内容:\r\n")
        _T("    查找: \\(.*?\\)    替换: (空)\r\n\r\n")
        _T("  删除末尾编号 \"(1)\", \"(2)\":\r\n")
        _T("    查找: \\(\\d+\\)$    替换: (空)\r\n\r\n")
        _T("  将空格替换为下划线:\r\n")
        _T("    查找: \\s+    替换: _\r\n\r\n")
        _T("【注意事项】\r\n")
        _T("  - 替换仅作用于文件名主干（不含扩展名）\r\n")
        _T("  - 扩展名不会被修改\r\n")
        _T("  - 正则无效时，替换规则将被忽略\r\n")
        _T("  - 特殊字符需转义: . * + ? ( ) [ ] { } \\ ^ $ |");

    // ===== Conversation history =====
    m_defaults[MakeKey(_T("ConvHistory"), _T("UnnamedTitle"))] = _T("未命名对话");

    // ===== AI command execution messages =====
    m_defaults[MakeKey(_T("Msg"), _T("InvalidJsonCmd"))] = _T("无法解析命令，JSON 格式无效。");
    m_defaults[MakeKey(_T("Msg"), _T("AiCmdConfirmFmt"))] = _T("AI 请求执行以下命令：\n\n命令：%s\n\n用途：%s\n\n风险等级：%s");
    m_defaults[MakeKey(_T("Msg"), _T("HighRiskConfirmText"))] = _T("确认执行");
    m_defaults[MakeKey(_T("Msg"), _T("InputMismatchCancel"))] = _T("输入不匹配，操作已取消。");
    m_defaults[MakeKey(_T("Msg"), _T("Cancelled"))] = _T("取消");
    m_defaults[MakeKey(_T("Msg"), _T("ExecConfirm"))] = _T("执行确认");
    m_defaults[MakeKey(_T("Msg"), _T("CmdCancelledFmt"))] = _T("【命令执行结果】\n命令：%s\n状态：已取消（用户未确认）\n");
    m_defaults[MakeKey(_T("Msg"), _T("ExecFailedErrCode"))] = _T("执行失败，错误代码：%d");
    m_defaults[MakeKey(_T("Msg"), _T("CannotCreatePipe"))] = _T("错误：无法创建输出管道");
    m_defaults[MakeKey(_T("Msg"), _T("OutputLabel"))] = _T("输出：\n");
    m_defaults[MakeKey(_T("Msg"), _T("ExitCodeFmt"))] = _T("退出代码：%d\n");

    // ===== Batch Rename - additional keys =====
    m_defaults[MakeKey(_T("BatchRename"), _T("RenameFolder"))] = _T("重命名");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveTo"))] = _T("移动");
    m_defaults[MakeKey(_T("BatchRename"), _T("MarkedDeleted"))] = _T("标记删除");
    m_defaults[MakeKey(_T("BatchRename"), _T("LocateInExplorer"))] = _T("在资源管理器中打开");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectAll"))] = _T("全选");
    m_defaults[MakeKey(_T("BatchRename"), _T("DeselectAll"))] = _T("取消全选");
    m_defaults[MakeKey(_T("BatchRename"), _T("Ignore"))] = _T("忽略");
    m_defaults[MakeKey(_T("BatchRename"), _T("Unignore"))] = _T("取消忽略");
    m_defaults[MakeKey(_T("BatchRename"), _T("Track"))] = _T("跟踪");
    m_defaults[MakeKey(_T("BatchRename"), _T("Untrack"))] = _T("取消跟踪");
    m_defaults[MakeKey(_T("BatchRename"), _T("MarkDelete"))] = _T("标记删除");
    m_defaults[MakeKey(_T("BatchRename"), _T("UnmarkDelete"))] = _T("取消删除标记");
    m_defaults[MakeKey(_T("BatchRename"), _T("ChangeExt"))] = _T("修改扩展名");
    m_defaults[MakeKey(_T("BatchRename"), _T("RestoreExt"))] = _T("恢复原始扩展名");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveUpOne"))] = _T("上移一个");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveUpTo"))] = _T("移动到指定位置");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveUpMenu"))] = _T("上移");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveDownOne"))] = _T("下移一个");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveDownTo"))] = _T("移动到指定位置");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveDownMenu"))] = _T("下移");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiAnalyze"))] = _T("AI分析");
    m_defaults[MakeKey(_T("BatchRename"), _T("CancelAiMark"))] = _T("取消AI标记");
    m_defaults[MakeKey(_T("BatchRename"), _T("BrowseTitle"))] = _T("选择文件夹");
    m_defaults[MakeKey(_T("BatchRename"), _T("DragFolderOnly"))] = _T("请拖入文件夹，不支持拖入文件。");
    m_defaults[MakeKey(_T("BatchRename"), _T("Tip"))] = _T("提示");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectedCount"))] = _T("已选 %d / %d 个文件夹");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectFoldersFirst"))] = _T("请先勾选要操作的文件夹。");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterNewFolderName"))] = _T("输入新文件夹名称：");
    m_defaults[MakeKey(_T("BatchRename"), _T("InvalidFolderName"))] = _T("文件夹名无效。");
    m_defaults[MakeKey(_T("BatchRename"), _T("Error"))] = _T("错误");
    m_defaults[MakeKey(_T("BatchRename"), _T("RenameFailed"))] = _T("重命名失败。");
    m_defaults[MakeKey(_T("BatchRename"), _T("RenameSuccess"))] = _T("重命名成功！");
    m_defaults[MakeKey(_T("BatchRename"), _T("Result"))] = _T("结果");
    m_defaults[MakeKey(_T("BatchRename"), _T("RenameResult"))] = _T("重命名完成：成功 %d 个，失败 %d 个。");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectFoldersToMove"))] = _T("请先勾选要移动的文件夹。");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectTargetFolder"))] = _T("选择目标文件夹");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveResult"))] = _T("移动完成：成功 %d 个，失败 %d 个。");
    m_defaults[MakeKey(_T("BatchRename"), _T("SelectFoldersToDelete"))] = _T("请先勾选要删除的文件夹。");
    m_defaults[MakeKey(_T("BatchRename"), _T("ConfirmDeleteFolders"))] = _T("确定要删除选中的 %d 个文件夹吗？\n此操作不可撤销！");
    m_defaults[MakeKey(_T("BatchRename"), _T("ConfirmDelete"))] = _T("确认删除");
    m_defaults[MakeKey(_T("BatchRename"), _T("DeleteResult"))] = _T("删除完成：成功 %d 个，失败 %d 个。");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterNewDirName"))] = _T("输入新目录名：");
    m_defaults[MakeKey(_T("BatchRename"), _T("RenameCurrentDir"))] = _T("重命名当前目录");
    m_defaults[MakeKey(_T("BatchRename"), _T("InvalidDirName"))] = _T("目录名无效。");
    m_defaults[MakeKey(_T("BatchRename"), _T("ConfirmMoveDir"))] = _T("确定要将 \"%s\" 移动到 \"%s\"？");
    m_defaults[MakeKey(_T("BatchRename"), _T("ConfirmMove"))] = _T("确认移动");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveFailed"))] = _T("移动失败。");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveSuccess"))] = _T("移动成功！");
    m_defaults[MakeKey(_T("BatchRename"), _T("ConfirmDeleteCurrentDir"))] = _T("确定要删除当前目录 \"%s\" 吗？\n此操作不可撤销！");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveToRecycleFailed"))] = _T("移动到回收站失败。");
    m_defaults[MakeKey(_T("BatchRename"), _T("DeleteSuccess"))] = _T("删除成功！");
    m_defaults[MakeKey(_T("BatchRename"), _T("AlreadyAtTop"))] = _T("已在顶部。");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterTargetPosition"))] = _T("输入目标位置 (1~%d)：");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterValidNumber"))] = _T("请输入有效数字 (1~%d)。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AlreadyAtBottom"))] = _T("已在底部。");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterTargetPositionRange"))] = _T("输入目标位置 (%d~%d)：");
    m_defaults[MakeKey(_T("BatchRename"), _T("MoveToBottom"))] = _T("移动到末尾");
    m_defaults[MakeKey(_T("BatchRename"), _T("EnterValidNumberRange"))] = _T("请输入有效数字 (%d~%d)。");

    // ===== Batch Rename AI Dialog =====
    m_defaults[MakeKey(_T("BatchRename"), _T("ColCurrentName"))] = _T("当前文件名");
    m_defaults[MakeKey(_T("BatchRename"), _T("ColAiSuggestion"))] = _T("AI 建议新名");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgStatus"))] = _T("共 %d 个文件");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgEnterDesc"))] = _T("请先输入重命名描述。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgWaiting"))] = _T("正在请求 AI，请稍候...");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgEmptyResponse"))] = _T("AI 返回为空，请重试。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgGenerated"))] = _T("已生成 %d 个映射");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgNoJson"))] = _T("AI 未返回有效 JSON，请重试。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgParseError"))] = _T("解析 AI 返回的 JSON 失败。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgParseErrorTitle"))] = _T("解析错误");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgInvalidJson"))] = _T("AI 返回的 JSON 格式无效。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgWarning"))] = _T("警告：");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgSkipped"))] = _T("跳过了 %d 个无效条目。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgIllegal"))] = _T("清理了 %d 个含非法字符的文件名。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgDupFixed"))] = _T("修正了 %d 个重复项。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgProcessNote"))] = _T("处理提示");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgJsonParseFail"))] = _T("JSON 解析失败：%s");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgUnchanged"))] = _T("(保持不变)");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgSendFirst"))] = _T("请先发送描述给 AI。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgConflict"))] = _T("文件名 \"%s\" 已存在（冲突）。");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgConflictTitle"))] = _T("文件名冲突");
    m_defaults[MakeKey(_T("BatchRename"), _T("AiDlgDuplicate"))] = _T("\"%s\" 在列表中重复。");

    // ===== Environment Variable Manager - additional keys =====
    m_defaults[MakeKey(_T("EnvVar"), _T("PathListCol"))] = _T("路径条目");

    // ===== Conversation History - additional keys =====
    m_defaults[MakeKey(_T("ConvHistory"), _T("ColMessageCount"))] = _T("消息数");
    m_defaults[MakeKey(_T("ConvHistory"), _T("ColUpdated"))] = _T("更新时间");
    m_defaults[MakeKey(_T("ConvHistory"), _T("SaveLocation"))] = _T("保存位置: ");

    // ===== Msg - Additional keys =====
    m_defaults[MakeKey(_T("Msg"), _T("GitMenuExec"))] = _T("执行命令");
    m_defaults[MakeKey(_T("Msg"), _T("GitMenuCopy"))] = _T("复制命令");
    m_defaults[MakeKey(_T("Msg"), _T("GitMenuEdit"))] = _T("编辑命令");
    m_defaults[MakeKey(_T("Msg"), _T("GitMenuDelete"))] = _T("删除命令");
    m_defaults[MakeKey(_T("Msg"), _T("DropGitFolderHint"))] = _T("拖拽文件夹或文件目录到此");
    m_defaults[MakeKey(_T("Msg"), _T("DropFileHint"))] = _T("拖拽文件到此");
    m_defaults[MakeKey(_T("Msg"), _T("GitRepoDetached"))] = _T("HEAD 分离");
    m_defaults[MakeKey(_T("Msg"), _T("GitRepoBranch"))] = _T("分支: %s");
    m_defaults[MakeKey(_T("Msg"), _T("GitNotRepo"))] = _T("不是 Git 仓库");
    m_defaults[MakeKey(_T("Msg"), _T("GitNoWorkDir"))] = _T("请先设置工作目录。");
    m_defaults[MakeKey(_T("Msg"), _T("GitCurrentBranch"))] = _T("当前分支: ");
    m_defaults[MakeKey(_T("Msg"), _T("GitConfirmExec"))] = _T("确定要执行以下命令？\n\n%s\n\n%s");
    m_defaults[MakeKey(_T("Msg"), _T("GitConfirmTitle"))] = _T("确认执行 Git 命令");
    m_defaults[MakeKey(_T("Msg"), _T("GitResultWindowFailed"))] = _T("无法创建结果窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("GitCmdWindowFailed"))] = _T("无法创建 Git 命令窗口。");
    m_defaults[MakeKey(_T("Msg"), _T("GitEditPrompt"))] = _T("编辑命令：");
    m_defaults[MakeKey(_T("Msg"), _T("EnterCommand"))] = _T("请输入命令。");
    m_defaults[MakeKey(_T("Msg"), _T("ExecCmdFailed"))] = _T("执行命令失败：%s");
    m_defaults[MakeKey(_T("Msg"), _T("PreventLockFailed"))] = _T("设置防锁屏失败。");
    m_defaults[MakeKey(_T("Msg"), _T("PowerShellConfirm"))] = _T("请选择 PowerShell 打开方式：\n是(Y) - 以管理员身份运行\n否(N) - 普通模式");
    m_defaults[MakeKey(_T("Msg"), _T("PowerShellTitle"))] = _T("打开 PowerShell");
    m_defaults[MakeKey(_T("Msg"), _T("PowerShellNonAdminFailed"))] = _T("启动 PowerShell 失败：%s");
    m_defaults[MakeKey(_T("Msg"), _T("PowerShellAdminFailed"))] = _T("以管理员权限启动 PowerShell 失败：%s");
    m_defaults[MakeKey(_T("Msg"), _T("WslLaunchFailed"))] = _T("启动 WSL 失败。");
    m_defaults[MakeKey(_T("Msg"), _T("OpenLinkFailed"))] = _T("无法打开链接。");
    m_defaults[MakeKey(_T("Msg"), _T("GitNoPathError"))] = _T("请先设置Git工作目录。");
    m_defaults[MakeKey(_T("Msg"), _T("GitBashNotFound"))] = _T("未找到 Git Bash，请检查设置。");
    
    m_defaults[MakeKey(_T("Msg"), _T("PleaseSelectProcess"))] = _T("请先选择进程。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmEndProcesses"))] = _T("确定要结束选中的 %zu 个进程吗？");
    m_defaults[MakeKey(_T("Msg"), _T("EndMoreHint"))] = _T("\n（仅显示前 10 个，共 %zu 个进程）");
    m_defaults[MakeKey(_T("Msg"), _T("UnsavedDataWarning"))] = _T("\n\n警告：未保存的数据可能会丢失！");
    m_defaults[MakeKey(_T("Msg"), _T("NoLockFound"))] = _T("未找到文件占用信息。");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmEndAllLocks"))] = _T("确定要结束所有 %zu 个占用进程吗？");
    m_defaults[MakeKey(_T("Msg"), _T("ConfirmEndAllTitle"))] = _T("确认全部结束");

    // ===== File Lock - additional keys =====
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeScanning"))] = _T("扫描中...");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeMainWindow"))] = _T("主窗口");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeOtherWindow"))] = _T("其他窗口");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeService"))] = _T("服务");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeExplorer"))] = _T("资源管理器");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeConsole"))] = _T("控制台");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeCritical"))] = _T("关键进程");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeUnknown"))] = _T("未知");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeLocked"))] = _T("已锁定");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeNotLocked"))] = _T("未锁定");
    m_defaults[MakeKey(_T("FileLock"), _T("AppTypeNotFound"))] = _T("未找到");
    m_defaults[MakeKey(_T("FileLock"), _T("HintLoaded"))] = _T("已加载 %d 个文件，%d 个占用进程");

    // ===== Menu - right-click items =====
    m_defaults[MakeKey(_T("Menu"), _T("EndProcess"))] = _T("结束进程");
    m_defaults[MakeKey(_T("Menu"), _T("EndSameName"))] = _T("结束所有同名进程");
    m_defaults[MakeKey(_T("Menu"), _T("Locate"))] = _T("定位");
    m_defaults[MakeKey(_T("Menu"), _T("AiAnalyze"))] = _T("AI分析");
    m_defaults[MakeKey(_T("Menu"), _T("CloseWindow"))] = _T("关闭窗口");
    m_defaults[MakeKey(_T("Menu"), _T("Cancel"))] = _T("取消");

    // ===== ContextMenu - Scene names (for location filter dropdown) =====
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneAll"))] = _T("全部");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneFile"))] = _T("文件 (*)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneFolder"))] = _T("文件夹(所有文件夹)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneVirtualFolder"))] = _T("文件夹(虚拟文件夹)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneDirBackground"))] = _T("目录背景");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneDesktopBg"))] = _T("桌面背景");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneDrive"))] = _T("驱动器");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneAllFiles"))] = _T("所有文件");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneThisPC"))] = _T("此电脑");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneRecycleBin"))] = _T("回收站");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneLibrary"))] = _T("库文件夹");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneUWP"))] = _T("UWP快捷方式");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneExe"))] = _T("exe 文件");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneUnknown"))] = _T("未知文件");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneShortcut"))] = _T("快捷方式");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneJpg"))] = _T(".jpg / .jpeg (JPEG图片)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ScenePng"))] = _T(".png (PNG图片)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneGif"))] = _T(".gif (GIF图片)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneBmp"))] = _T(".bmp (BMP图片)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneTxt"))] = _T(".txt (文本文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ScenePdf"))] = _T(".pdf (PDF文档)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneDoc"))] = _T(".doc / .docx (Word文档)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneMp4"))] = _T(".mp4 (视频文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneMp3"))] = _T(".mp3 (音频文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneZip"))] = _T(".zip / .rar (压缩文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneExeFile"))] = _T(".exe (可执行文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneDll"))] = _T(".dll (库文件)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("SceneHtml"))] = _T(".html / .htm (网页文件)");

    // ===== ContextMenu - Status messages =====
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusFoundItems"))] = _T("发现 %d 个右键菜单项");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusDictPath"))] = _T("  |  字典: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusDictEmbeddedCache"))] = _T("  |  字典: 内置 + 缓存 (%s)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusDictEmbedded"))] = _T("  |  字典: 内置(硬编码)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusQueryExt"))] = _T("查询后缀 %s: 发现 %d 个右键菜单项");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusDictPathUpdated"))] = _T("字典文件夹已更新: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusFolderMenuAdded"))] = _T("文件夹右键菜单项已添加");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusFolderMenuRemoved"))] = _T("文件夹右键菜单项已移除");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusWin11Enabled"))] = _T("Win11经典菜单已启用，正在重启资源管理器...");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusWin11Disabled"))] = _T("Win11新菜单已恢复，正在重启资源管理器...");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusToggleFormat"))] = _T("%s: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusActionEnabled"))] = _T("已启用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusActionDisabled"))] = _T("已禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusDisabledCount"))] = _T("%d 项已禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusEnabledCount"))] = _T("%d 项已启用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusCustomParseDeleted"))] = _T("\"%s\" 的自定义解析已删除");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusCustomParseSaved"))] = _T("\"%s\" 的自定义解析已保存为: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusRebuildProgress"))] = _T("正在重建字典: %d 个CLSID (%d 个场景)，请稍候...");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusRebuildComplete"))] = _T("字典重建完成: %d/%d 个CLSID已解析 (注册表: %d, COM: %d, 回退: %d)。缓存: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusRebuildFailed"))] = _T("字典重建完成: %d/%d 个CLSID已解析，但缓存保存失败。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusNoClsidFound"))] = _T("未找到ShellEx CLSID，无需重建。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusBackupDirFail"))] = _T("无法创建备份目录");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusUndoFormat"))] = _T("已撤销: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("StatusHistoryRecent"))] = _T("... (仅显示最近50条记录)");

    // ===== ContextMenu - Dialog messages =====
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgNoSelection"))] = _T("请先选择一个项目。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgNoHistory"))] = _T("无操作历史。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgNoHistoryUndo"))] = _T("没有可撤销的操作。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgItemNotFound"))] = _T("找不到对应的菜单项，可能已被删除。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgAllSelectedDisabled"))] = _T("所选项目均已禁用。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgAllSelectedEnabled"))] = _T("所选项目均已启用。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgNoClsid"))] = _T("无法获取该项目的CLSID，无法进行自定义名称解析。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgEnterCustomName"))] = _T("CLSID: %s\n当前名称: %s\n\n输入自定义显示名称(留空则删除自定义解析):");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgDictFolderNotFound"))] = _T("字典文件夹不存在。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCantParseShellExPath"))] = _T("无法解析ShellEx注册表路径。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCantOpenRegKey"))] = _T("无法打开注册表键: %s\n错误代码: %d");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCopyRegKeyFailed"))] = _T("复制注册表键失败。\n\n源: %s\\%s\n目标: %s\\%s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgDeleteOldRegFailed"))] = _T("删除旧注册表键失败，已回滚。\n错误代码: %d");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgUnrecognizedShellEx"))] = _T("无法识别的ShellEx文件夹: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgAiNoApiKey"))] = _T("请先在 文件→设置→AI助手 中配置API Key。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgAiAnalyzeFail"))] = _T("AI分析失败: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgAiAnalyzeResult"))] = _T("AI分析结果");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgConfirmDisableCount"))] = _T("确定要禁用 %d 个右键菜单项吗？\n禁用后可通过右键菜单重新启用。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgConfirmEnableCount"))] = _T("确定要启用 %d 个右键菜单项吗？");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCriticalWarning"))] = _T("警告：部分选中项是系统关键右键菜单项！\n\n禁用它们可能会影响正常的Windows资源管理器行为。\n修改前将自动创建注册表备份。\n\n确定要继续吗？");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCriticalWarningTitle"))] = _T("严重警告 - 确认禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgUndoConfirm"))] = _T("确定要撤销以下操作吗？\n\n时间: %s\n操作: %s\n项目: %s\n详情: %s\n\n这将恢复项目到之前的状态。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgUndoConfirmTitle"))] = _T("确认撤销");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgOpenSourceParentFail"))] = _T("无法打开源注册表键。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgCreateTargetFail"))] = _T("无法创建目标注册表键。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgRebuildPrompt"))] = _T("将扫描所有已注册的ShellEx CLSID并通过注册表和COM接口解析显示名称。\n此过程可能需要几秒钟，是否继续？");
    m_defaults[MakeKey(_T("ContextMenu"), _T("MsgHistoryTitle"))] = _T("操作历史 (最近在先)");
    m_defaults[MakeKey(_T("ContextMenu"), _T("CustomParseTitle"))] = _T("自定义名称解析");
    m_defaults[MakeKey(_T("ContextMenu"), _T("RebuildTitle"))] = _T("重建字典");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiAnalyzeTitle"))] = _T("AI分析");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptHeader"))] = _T("分析Windows右键菜单注册表项:");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptScene"))] = _T("场景: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptKeyName"))] = _T("键名: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptDisplayName"))] = _T("显示名称: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptCommand"))] = _T("命令: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptType"))] = _T("类型: %s");
    m_defaults[MakeKey(_T("ContextMenu"), _T("AiPromptFooter"))] = _T("猜测此右键菜单项属于哪个软件及提供什么功能。30字以内回答。");
    m_defaults[MakeKey(_T("ContextMenu"), _T("InputExtHint"))] = _T("请输入要查询的文件后缀名，如 .mp4、.txt 等");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ConfirmDisable"))] = _T("确认禁用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ConfirmEnable"))] = _T("确认启用");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ConfirmUndo"))] = _T("确认撤销");
    m_defaults[MakeKey(_T("ContextMenu"), _T("ConfirmRebuild"))] = _T("确认重建");

    // ===== Environment Variable Manager - Status messages =====
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusRefreshed"))] = _T("已刷新");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusTotal"))] = _T("系统: %d 个  用户: %d 个");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusSearch"))] = _T("搜索 \"%s\": 系统 %d/%d  用户 %d/%d");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusAdded"))] = _T("已添加 %s 变量: %s (备份: %s)");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusUpdated"))] = _T("已更新 %s 变量: %s (备份: %s)");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusDeleted"))] = _T("已删除 %s 变量: %s (备份: %s)");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusCopiedName"))] = _T("已复制变量名: %s");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusCopiedValue"))] = _T("已复制变量值: %s");
    m_defaults[MakeKey(_T("EnvVar"), _T("StatusExported"))] = _T("已导出到: %s");

    // ===== Environment Variable Manager - Error messages =====
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrNameEmpty"))] = _T("变量名不能为空。");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrSelectEdit"))] = _T("请先选择要编辑的路径条目。");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrSelectDelete"))] = _T("请先选择要删除的路径条目。");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrSelectVar"))] = _T("请先选择一个环境变量。");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrSelectVarDel"))] = _T("请先选择要删除的环境变量。");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrWriteFail"))] = _T("写入环境变量失败: %s");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrDeleteFail"))] = _T("删除环境变量失败: %s");
    m_defaults[MakeKey(_T("EnvVar"), _T("ErrCreateFile"))] = _T("无法创建文件，请检查权限。");

    // ===== Environment Variable Manager - Dialog titles =====
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgAddTitle"))] = _T("添加环境变量");
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgEditTitle"))] = _T("编辑环境变量");
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgWriteFail"))] = _T("写入失败");
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgDeleteFail"))] = _T("删除失败");
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgExportFail"))] = _T("导出失败");
    m_defaults[MakeKey(_T("EnvVar"), _T("DlgConfirmDelete"))] = _T("确认删除");
    m_defaults[MakeKey(_T("EnvVar"), _T("ConfirmAddType"))] = _T("添加到系统变量(是)还是用户变量(否)？");
    m_defaults[MakeKey(_T("EnvVar"), _T("ConfirmDeleteVar"))] = _T("确定要删除 %s 变量 \"%s\" 吗？\n此操作不可撤销，建议先导出备份。");
    m_defaults[MakeKey(_T("EnvVar"), _T("PathEditTitle"))] = _T("编辑 %s - PATH 变量");
    m_defaults[MakeKey(_T("EnvVar"), _T("PathSelectFolder"))] = _T("选择要添加的文件夹路径");
    m_defaults[MakeKey(_T("EnvVar"), _T("PathSelectNewFolder"))] = _T("选择新的文件夹路径");
    m_defaults[MakeKey(_T("EnvVar"), _T("ExportFilter"))] = _T("文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*||");

    // ===== Quick Launch =====
    m_defaults[MakeKey(_T("QuickLaunch"), _T("DlgTitle"))] = _T("管理快速打开");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ColName"))] = _T("名称");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ColType"))] = _T("类型");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ColPath"))] = _T("路径");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnAdd"))] = _T("添加");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnEdit"))] = _T("编辑");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnDelete"))] = _T("删除");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnUp"))] = _T("上移");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnDown"))] = _T("下移");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnOK"))] = _T("确定");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("AddTitle"))] = _T("添加快速打开项目");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("EditTitle"))] = _T("编辑快速打开项目");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("NoSelection"))] = _T("请先选择一个项目");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ConfirmDelete"))] = _T("确定要删除 \"%s\" 吗？");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ConfirmDeleteTitle"))] = _T("确认删除");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("ItemNotFound"))] = _T("找不到项目路径：%s");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnManage"))] = _T("管理");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BrowseFolder"))] = _T("选择文件夹");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("TypeExecutable"))] = _T("可执行文件");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("TypeFolder"))] = _T("文件夹");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("TypeUrl"))] = _T("网址");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("TypeOtherFile"))] = _T("其他文件");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnChangeIcon"))] = _T("更改图标");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("BtnResetIcon"))] = _T("恢复默认");
    m_defaults[MakeKey(_T("QuickLaunch"), _T("SelectIconFile"))] = _T("选择图标文件");

    // ===== Terminal =====
    m_defaults[MakeKey(_T("Terminal"), _T("Title"))] = _T("终端");
    m_defaults[MakeKey(_T("Terminal"), _T("Shell"))] = _T("Shell");
    m_defaults[MakeKey(_T("Terminal"), _T("Clear"))] = _T("清空");
    m_defaults[MakeKey(_T("Terminal"), _T("Copy"))] = _T("复制");
    m_defaults[MakeKey(_T("Terminal"), _T("Paste"))] = _T("粘贴");
    m_defaults[MakeKey(_T("Terminal"), _T("Restart"))] = _T("重启终端");
    m_defaults[MakeKey(_T("Terminal"), _T("New"))] = _T("新建终端");
    m_defaults[MakeKey(_T("Terminal"), _T("CloseTab"))] = _T("关闭终端");
    m_defaults[MakeKey(_T("Terminal"), _T("Exited"))] = _T("[进程已退出]");
}
