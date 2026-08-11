🌐 [English](README.md) · **中文** · [日本語](README.ja.md) · [Русский](README.ru.md) · [한국어](README.ko.md)

---

<p align="center">
  <img src="docs/screenshots/main.png" alt="PowerBox" width="720">
</p>

<h1 align="center">⚡ PowerBox</h1>

<p align="center">
  <strong>Windows 桌面工具箱 — 集成 AI 助手、进程管理、截图 OCR、Git 工具箱等 20+ 实用工具</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/framework-MFC-%230078D6?logo=microsoft" alt="MFC">
  <img src="https://img.shields.io/badge/release-v1.1.0-brightgreen" alt="v1.1.0">
  <img src="https://img.shields.io/badge/platform-Windows-%230078D6?logo=windows" alt="Windows">
</p>

<p align="center">
  <a href="https://github.com/gyx114/PowerBox/wiki">📖 Wiki 文档</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/快速开始">🚀 快速开始</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/功能详解">📋 功能详解</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/AI助手配置">🤖 AI 配置</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/常见问题FAQ">❓ FAQ</a>
</p>

---

## ✨ 功能概览

### 🧠 AI 助手
集成 6 家 AI 供应商（OpenAI、DeepSeek、通义千问、智谱AI、Moonshot、硅基流动），支持多模式交互。

| 模式 | 说明 |
|------|------|
| AI 对话 | 右侧面板直接对话，支持流式输出、Markdown 渲染、历史记录保存与加载；支持独立 AI 助手窗口 |
| 进程 AI 分析 | 右键进程 → AI 分析，获取安全等级与操作建议 |
| 进程 AI 扫描 | 一键扫描所有进程，选择扫描等级后 AI 识别可疑/无用进程，支持批量结束 |
| AI 批量重命名 | 自然语言描述需求，AI 自动生成文件名映射，支持与其他重命名规则叠加 |

AI 助手面板内置 ConPTY 终端，支持多个终端会话，AI 执行命令时自动在终端中运行并回传结果。

### 🌐 多语言界面
支持中文/英文切换，语言文件位于 `lang/` 目录，可自由扩展更多语言。参见[功能详解](https://github.com/gyx114/PowerBox/wiki/功能详解)中的"多语言界面"章节。

### 📋 核心工具

| 工具 | 说明 |
|------|------|
| 进程管理 | 枚举进程，显示 CPU/内存，支持排序过滤（含正则）、结束进程、定位文件、AI 分析 |
| 启动项管理 | 查看/添加/删除/启用/禁用注册表启动项，双击复制路径，支持添加机器级启动项 |
| 剪贴板历史 | 记录最近 10 条文本，双击回写，连续重复条目自动去重 |
| 窗口处理 | 定位窗口（Ctrl+Alt+D）、置顶（支持多窗口）、透明度调节（10%-100%）、强制结束、截图保存；自动保存定位历史 |
| 文件管理 | 拖入文件生成副本（可自定义名称）、重命名、删除（回收站）、复制、移动；拖入文件夹打开批量重命名 |
| Git 工具箱 | 20 条预设命令，支持自定义列表（config.ini [GitCommands]），自动检测仓库分支，AI 辅助生成命令，Git Bash 快捷启动，独立的 Git 命令结果窗口 |

### 🛠 工具窗口

| 工具 | 说明 |
|------|------|
| 二维码生成器 | 输入文本生成二维码，支持复制到剪贴板和保存为 PNG/BMP（4px 白色边距） |
| 截图 OCR | 框选区域截图，Windows OCR 识别文字，支持中/英/日/韩文，支持 MyMemory API 翻译（6 种语言对） |
| 批量重命名 | 前缀/后缀/替换/编号/正则/删除匹配，支持忽略规则和跟踪规则，支持 AI 智能重命名、撤销操作，支持文件夹重命名 |
| 便签 | 置顶便签，折叠/展开（双击标题栏切换），X 按钮折叠/退出，内容自动保存到配置文件夹 |
| Markdown 预览 | 左右分栏编辑+实时渲染，GitHub 风格 CSS（marked.js），可拖拽分隔条，支持打开 .md 文件 |
| 编码转换 | 自动检测编码（UTF-8/UTF-8 BOM/UTF-16LE/UTF-16LE BOM/UTF-16BE/GBK/Big5/Shift-JIS/Latin-1），支持批量转换 |
| 右键菜单管理 | 扫描/启用/禁用右键菜单项，支持 28+ 种场景，14 个扩展名预设+自定义，AI 解析未翻译项，支持 Win11 经典菜单切换 |
| 环境变量管理 | 查看/编辑/导出系统/用户环境变量，PATH 编辑器（独立行编辑），修改前自动备份 |
| 文件占用查看 | 拖入文件查看占用进程（Restart Manager API），支持结束进程/全部结束/定位进程文件夹 |
| 快捷打开 | 用户可配置的快捷按钮（最多 36 个），支持可执行文件/文件夹/网址/其他文件/快捷键共 5 种类型，可执行文件可配置唤醒快捷键，快捷键类型可直接模拟按键组合，带管理对话框，支持拖放排序、拖放文件/文件夹快速添加 |
| 自定义快捷键 | 全局热键（显示/隐藏主窗口、窗口定位）可在设置中自定义，支持热键捕捉对话框；快捷打开项支持唤醒快捷键和快捷键模拟 |
| ConPTY 终端 | AI 助手面板内置终端，支持多个终端会话、横向标签栏切换（Ctrl+Tab/滚轮/中键关闭），AI 执行命令时自动在新终端 tab 中运行 |
| 独立 AI 助手窗口 | 可脱离主窗口独立使用的 AI 助手窗口，包含完整对话、终端、历史记录管理功能 |

### ⌨️ 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Alt+Space` | 全局热键，显示/隐藏主窗口（可在设置中自定义） |
| `Ctrl+Alt+D` | 进入窗口定位模式（可在设置中自定义） |
| `Alt+1` ~ `Alt+6` | 切换左侧标签页 |
| `F5` | 刷新当前列表 |
| `Del` | 结束选中的进程 |
| `Enter` | AI 对话发送（AI 面板聚焦时）。`Shift+Enter` 换行。 |

---

## 🖼️ 界面预览

| 主界面 | AI 助手 |
|:-----:|:------:|
| ![主界面](docs/screenshots/main.png) | ![AI 助手](docs/screenshots/ai-assistant.png) |

| 进程管理 | 截图 OCR | 二维码生成 |
|:-------:|:--------:|:---------:|
| ![进程管理](docs/screenshots/process.png) | ![截图 OCR](docs/screenshots/ocr.png) | ![二维码](docs/screenshots/qrcode.png) |

> 更多截图请查看 [docs/screenshots/](docs/screenshots/) 目录。

---

## 📖 文档导航

详细的使用说明和配置指南请查阅 Wiki 文档：

| 文档 | 说明 |
|------|------|
| [📋 功能详解](https://github.com/gyx114/PowerBox/wiki/功能详解) | 各功能模块详细说明 |
| [🤖 AI 助手配置](https://github.com/gyx114/PowerBox/wiki/AI助手配置) | AI 供应商配置与 API Key 获取 |
| [🚀 快速开始](https://github.com/gyx114/PowerBox/wiki/快速开始) | 下载安装与首次使用指南 |
| [⌨️ 快捷键大全](https://github.com/gyx114/PowerBox/wiki/快捷键大全) | 全部快捷键参考 |
| [🔧 编译指南](https://github.com/gyx114/PowerBox/wiki/编译指南) | 从源码编译构建 |
| [❓ 常见问题](https://github.com/gyx114/PowerBox/wiki/常见问题FAQ) | 常见问题与解答 |

---

## 📥 安装

### 下载 Releases
前往 [Releases 页面](https://github.com/gyx114/PowerBox/releases) 下载最新版 `PowerBox.zip`，解压即可使用（绿色软件，无需安装）。压缩包内含 `PowerBox.exe` 和 `lang/` 语言文件夹，开箱即用。

### 自行编译
需要 Visual Studio 2022（或更高版本）以及 C++ 桌面开发工作负载（含 MFC 组件）。

```bash
git clone https://github.com/gyx114/PowerBox.git
cd PowerBox
msbuild MFCApplication1/MFCApplication1.vcxproj /p:Configuration=Release /p:Platform=x64
```

编译产物位于 `x64/Release/PowerBox.exe`。编译后需将源码中的 `MFCApplication1/lang/` 文件夹复制到 exe 所在目录，否则程序将使用内置中文界面，无法切换语言。

---

## 🛠 技术栈

| 类别 | 技术 |
|------|------|
| 编程语言 | C++20 |
| 界面框架 | MFC (Microsoft Foundation Classes) |
| AI 集成 | WinHTTP + 6 家 AI 供应商（OpenAI、DeepSeek、通义千问、智谱AI、Moonshot、硅基流动） |
| 二维码生成 | Nayuki QR Code Generator |
| OCR 识别 | Windows OCR API (Windows.Media.Ocr) + MyMemory 翻译 API |
| Markdown 渲染 | WebBrowser 控件 + marked.js（GitHub 风格 CSS） |
| 终端 | ConPTY (Windows Pseudo Console) |
| 多语言 | INI 文件（UTF-16 LE 编码）+ 内置默认值，支持中文/英文切换，语言文件位于 `lang/` 目录 |
| 构建工具 | Visual Studio 2022 + MSBuild |

---

## 📄 许可证

[MIT](LICENSE) © 2026 管宇轩