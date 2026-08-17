🌐 **English** · [中文](README.zh-CN.md) · [日本語](README.ja.md) · [Русский](README.ru.md) · [한국어](README.ko.md)

---

<p align="center">
  <img src="docs/screenshots/main.png" alt="PowerBox" width="720">
</p>

<h1 align="center">⚡ PowerBox</h1>

<p align="center">
  <strong>Windows Desktop Toolbox — Integrated AI Assistant, Process Manager, Screen OCR, Git Toolbox, and 20+ Utilities</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/framework-MFC-%230078D6?logo=microsoft" alt="MFC">
  <img src="https://img.shields.io/badge/release-v1.1.0-brightgreen" alt="v1.1.0">
  <img src="https://img.shields.io/badge/platform-Windows-%230078D6?logo=windows" alt="Windows">
</p>

<p align="center">
  <a href="https://github.com/gyx114/PowerBox/wiki">📖 Wiki Documentation</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/快速开始">🚀 Quick Start</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/功能详解">📋 Features</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/AI助手配置">🤖 AI Config</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/常见问题FAQ">❓ FAQ</a>
</p>

---

## ✨ Feature Overview

### 🧠 AI Assistant
Integrated with 6 AI providers (OpenAI, DeepSeek, Tongyi Qianwen, Zhipu AI, Moonshot, SiliconFlow), supporting multi-mode interaction.

| Mode | Description |
|------|-------------|
| AI Chat | Direct conversation in the right panel, supports streaming output, Markdown rendering, history save/load; standalone AI assistant window available |
| Process AI Analysis | Right-click a process → AI Analysis, get security rating and action recommendations |
| Process AI Scan | One-click scan of all processes, select scan level, AI identifies suspicious/unnecessary processes, supports batch termination |
| AI Batch Rename | Describe your needs in natural language, AI auto-generates file name mappings, supports stacking with other rename rules |

The AI assistant panel includes a built-in ConPTY terminal supporting multiple terminal sessions. When AI executes commands, it automatically runs them in the terminal and returns results.

### 🌐 Multi-language Interface
Supports Chinese/English switching, language files located in the `lang/` directory, easily extensible to more languages. See the "Multi-language Interface" section in [Feature Details](https://github.com/gyx114/PowerBox/wiki/功能详解).

### 📋 Core Tools

| Tool | Description |
|------|-------------|
| Process Manager | Enumerate processes, show CPU/Memory, support sorting/filtering (including regex), end process, locate file, AI analysis |
| Startup Manager | View/Add/Delete/Enable/Disable registry startup items, double-click to copy path, supports adding machine-level startup items |
| Clipboard History | Records recent text/files/images; double-click to paste back; persists across program restarts; the "Clipboard History" tab lists all entries (text/files/images); the standalone "Clipboard History" window offers search, preview, pin, delete, clear and storage size-limit settings, opened via the tab toolbar button or the tray menu |
| Window Tool | Locate window (Ctrl+Alt+D), pin to top (supports multiple windows), transparency adjustment (10%-100%), force close, screenshot save; auto-saves locate history |
| File Manager | Drag in files to create copies (customizable name), rename, delete (recycle bin), copy, move; drag in folder to open batch rename |
| Git Toolbox | 20 preset commands, supports custom list (config.ini [GitCommands]), auto-detects repo branch, AI-assisted command generation, Git Bash quick launch, standalone Git command result window |
| Media Control | "Previous"/"Next" buttons send system media keys to control the currently playing media session; the tray menu's "Media" submenu offers the same |

### 🛠 Tool Windows

| Tool | Description |
|------|-------------|
| QR Code Generator | Generate QR codes from text, supports copy to clipboard and save as PNG/BMP (4px white margin) |
| Screen OCR | Select area screenshot, Windows OCR text recognition, supports Chinese/English/Japanese/Korean, supports MyMemory API translation (6 language pairs) |
| Batch Rename | Prefix/Suffix/Replace/Numbering/Regex/Delete matching, supports ignore rules and tracking rules, supports AI smart rename, undo, and folder renaming |
| Sticky Notes | Topmost notes, collapse/expand (double-click title bar), X button to collapse/exit, content auto-saved to config folder |
| Markdown Preview | Split-pane editor + live preview, GitHub-style CSS (marked.js), draggable splitter, supports opening .md files |
| Encoding Conversion | Auto-detect encoding (UTF-8/UTF-8 BOM/UTF-16LE/UTF-16LE BOM/UTF-16BE/GBK/Big5/Shift-JIS/Latin-1), supports batch conversion |
| Context Menu Manager | Scan/Enable/Disable context menu items, supports 28+ scenarios, 14 extension presets + custom, AI parsing of untranslated items, supports Win11 classic menu toggle |
| Environment Variables | View/Edit/Export system/user environment variables, PATH editor (independent line editing), auto-backup before modification |
| File Lock Viewer | Drag in files to view locking processes (Restart Manager API), supports end process/end all/locate process folder |
| Quick Launch | User-configurable quick buttons (up to 36), supports 5 types: executable/folder/URL/other file/shortcut key; executables can be assigned activation hotkeys; shortcut key type can simulate key combinations; includes management dialog, supports drag-and-drop sorting, drag-and-drop files/folders for quick add |
| Custom Hotkeys | Global hotkeys (show/hide main window, window locate) customizable in settings, with hotkey capture dialog; quick launch items support activation hotkeys and shortcut key simulation |
| ConPTY Terminal | Built-in terminal in the AI assistant panel, supports multiple terminal sessions, horizontal tab bar switching (Ctrl+Tab/scroll wheel/middle-click to close), AI automatically runs commands in a new terminal tab |
| Standalone AI Assistant Window | A standalone AI assistant window that can be used independently from the main window, with full chat, terminal, and history management features |

### ⌨️ 快捷键大全

| Shortcut | Function |
|----------|----------|
| `Ctrl+Alt+Space` | Global hotkey, show/hide main window (customizable in settings) |
| `Ctrl+Alt+D` | Enter window locate mode (customizable in settings) |
| `Alt+1` ~ `Alt+6` | Switch left sidebar tabs |
| `F5` | Refresh current list |
| `Del` | End selected process |
| `Enter` | Send AI chat message (when AI panel is focused). `Shift+Enter` for newline. |

---

## 🖼️ Screenshots

| Main Screen | AI Assistant | Process Manager |
|:-----:|:------:|:-------:|
| ![Main Screen](docs/screenshots/main.png) | ![AI Assistant](docs/screenshots/ai-assistant.png) | ![Process Manager](docs/screenshots/process.png) |

| Startup Manager | Git Toolbox | Batch Rename |
|:---------:|:--------:|:---------:|
| ![Startup Manager](docs/screenshots/startup.png) | ![Git Toolbox](docs/screenshots/git.png) | ![Batch Rename](docs/screenshots/rename.png) |

| Context Menu Manager | Screen OCR | QR Code |
|:----------:|:--------:|:---------:|
| ![Context Menu Manager](docs/screenshots/contextmenu.png) | ![Screen OCR](docs/screenshots/ocr.png) | ![QR Code](docs/screenshots/qrcode.png) |

> More screenshots in [docs/screenshots/](docs/screenshots/) directory.

---

## 📖 Documentation Navigation

Detailed usage instructions and configuration guides can be found in the Wiki:

| Document | Description |
|----------|-------------|
| [📋 Features](https://github.com/gyx114/PowerBox/wiki/功能详解) | Detailed feature descriptions |
| [🤖 AI Config](https://github.com/gyx114/PowerBox/wiki/AI助手配置) | AI provider configuration and API key acquisition |
| [🚀 Quick Start](https://github.com/gyx114/PowerBox/wiki/快速开始) | Download, installation, and first-time usage guide |
| [⌨️ 快捷键大全](https://github.com/gyx114/PowerBox/wiki/快捷键大全) | Complete shortcut key reference |
| [🔧 Build Guide](https://github.com/gyx114/PowerBox/wiki/编译指南) | Building from source code |
| [❓ FAQ](https://github.com/gyx114/PowerBox/wiki/常见问题FAQ) | Frequently asked questions and answers |

---

## 📥 Installation

### Download Releases
Go to the [Releases page](https://github.com/gyx114/PowerBox/releases) to download the latest `PowerBox.zip`, extract and use it directly (portable software, no installation required). The package includes `PowerBox.exe` and the `lang/` language folder—ready to use out of the box.

### Build from Source
Requires Visual Studio 2022 (or higher) with the C++ desktop development workload (including MFC components).

```bash
git clone https://github.com/gyx114/PowerBox.git
cd PowerBox
msbuild MFCApplication1/MFCApplication1.vcxproj /p:Configuration=Release /p:Platform=x64
```

The build output is located at `x64/Release/PowerBox.exe`. After building, copy the `MFCApplication1/lang/` folder from the source to the executable directory; otherwise, the program will use the built-in Chinese UI and cannot switch languages.

---

## 🛠 Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 |
| UI Framework | MFC (Microsoft Foundation Classes) |
| AI Integration | WinHTTP + 6 AI providers (OpenAI, DeepSeek, Tongyi Qianwen, Zhipu AI, Moonshot, SiliconFlow) |
| QR Code | Nayuki QR Code Generator |
| OCR | Windows OCR API (Windows.Media.Ocr) + MyMemory Translation API |
| Markdown | WebBrowser control + marked.js (GitHub-style CSS) |
| Terminal | ConPTY (Windows Pseudo Console) |
| Multi-language | INI files (UTF-16 LE) + built-in defaults, supports Chinese/English switching, language files in `lang/` directory |
| Build | Visual Studio 2022 + MSBuild |

---

## 📄 License

[MIT](LICENSE) © 2026 GuanyuXuan