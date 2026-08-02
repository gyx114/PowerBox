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
  <img src="https://img.shields.io/badge/release-v1.0.0-brightgreen" alt="v1.0.0">
</p>

---

## ✨ 功能亮点

### 🧠 AI 助手
集成 OpenAI / DeepSeek API，支持对话问答、进程 AI 分析、全进程 AI 扫描，智能识别可疑/无用进程。

| 功能 | 说明 |
|------|------|
| AI 对话 | 右侧面板直接对话，支持历史记录保存与加载 |
| AI 分析 | 右键进程 → AI 分析，获取安全等级与操作建议 |
| AI 扫描 | 一键扫描所有进程，弹窗列出可疑进程，支持批量结束 |

### 📋 核心工具

| 工具 | 说明 |
|------|------|
| 进程管理 | 枚举进程，显示 CPU/内存，支持排序过滤、结束进程、定位文件 |
| 启动项管理 | 查看/添加/删除注册表启动项，双击复制路径 |
| 剪贴板历史 | 记录最近 10 条文本，双击回写 |
| 窗口处理 | 定位窗口、置顶、透明度调节、强制结束、截图保存 |
| 文件管理 | 拖入文件生成副本、重命名、删除、复制、移动 |
| Git 工具箱 | 常用 Git 命令速查，双击/右键复制 |

### 🛠 工具窗口

| 工具 | 说明 |
|------|------|
| 二维码生成器 | 输入文本生成二维码，支持复制和保存 PNG |
| 截图 OCR | 框选区域截图，OCR 识别文字，支持翻译 |
| 批量重命名 | 前缀/后缀/替换/编号/正则，支持文件夹 |
| 便签 | 置顶便签，折叠/展开，双击标题栏切换，自动保存 |
| Markdown 预览 | 左右分栏编辑+实时渲染，可拖拽分隔条 |
| 编码转换 | 检测并转换文件编码（UTF-8/GBK 等），支持批量 |
| 右键菜单管理 | 扫描/启用/禁用右键菜单项 |
| 环境变量管理 | 查看/编辑/导出系统/用户环境变量，PATH 编辑器 |
| 文件占用查看 | 拖入文件查看占用进程 |

### ⌨️ 快捷键

| 快捷键 | 功能 |
|--------|------|
| Ctrl+Alt+Space | 显示/隐藏主窗口 |
| Alt+1~6 | 切换标签页 |
| F5 | 刷新当前列表 |
| Ctrl+Alt+D | 进入窗口定位模式 |

---

## 🖼️ 界面预览

| 主界面 | AI 助手 |
|:-----:|:------:|
| ![主界面](docs/screenshots/main.png) | ![AI助手](docs/screenshots/ai-assistant.png) |

| 进程管理 | 工具窗口 |
|:-------:|:--------:|
| ![进程管理](docs/screenshots/process.png) | ![工具窗口](docs/screenshots/qrcode.png) |

> 更多截图请查看 [docs/screenshots/](docs/screenshots/) 目录。

---

## 📥 安装

### 下载 Releases
前往 [Releases](https://github.com/gyx114/PowerBox/releases) 页面下载最新版 `PowerBox.exe`，直接运行即可。

### 自行编译
需要 Visual Studio 2022（或更高版本）以及 C++ 桌面开发工作负载。

```bash
git clone https://github.com/gyx114/PowerBox.git
cd PowerBox
msbuild MFCApplication1/MFCApplication1.vcxproj /p:Configuration=Release /p:Platform=x64
```

编译产物位于 `x64/Release/PowerBox.exe`。

---

## 🛠 技术栈

- **语言**: C++20
- **框架**: MFC (Microsoft Foundation Classes)
- **AI 集成**: WinHTTP + OpenAI / DeepSeek API
- **二维码**: Nayuki QR Code Generator
- **OCR**: Windows OCR API
- **Markdown 渲染**: WebBrowser + marked.js
- **构建工具**: Visual Studio 2022 + MSBuild

---

## 📄 许可证

[MIT](LICENSE) © 2026 管宇轩
