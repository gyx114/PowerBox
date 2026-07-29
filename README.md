# a-desktop-toolbox

基于 VS MFC 实现的 Windows 桌面工具箱程序，集成进程管理、启动项管理、剪贴板历史、窗口处理、文件管理、Git 工具箱、AI 助手、连点器、二维码生成、截图 OCR、批量重命名、Markdown 预览、编码转换、右键菜单管理、环境变量管理、文件占用查看等功能。

## 标签页功能

| 标签页 | 功能描述 |
|--------|----------|
| 进程管理 | 枚举系统进程，支持结束进程、定位程序文件 |
| 启动项管理 | 查看/添加/删除注册表启动项，双击复制路径 |
| 剪贴板 | 记录最近 10 条剪贴板文本，双击回写剪贴板 |
| 窗口处理 | 窗口定位、置顶/取消置顶、透明度调节、强制结束进程、截图保存 |
| 文件管理 | 拖入文件生成副本、重命名、删除、复制、移动 |
| Git 工具箱 | 常用 Git 命令列表，双击或右键复制命令 |

## 控件功能表

### 全局控件

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 标签页 | IDC_TAB1 | Tab Control | 切换 6 个功能标签页 |
| 关机/重启 | IDC_BUTTON1 | Button | 执行关机/重启（根据 Combo 选择） |
| 取消关机 | IDC_BUTTON2 | Button | 取消已计划的关机 |
| 关机选项 | IDC_COMBO1 | Combo Box | 选择关机模式（1分钟后重启/默认3分钟关机/设定时间关机） |
| 定时关机-时 | IDC_EDIT1 | Edit Control | 输入定时关机的小时数 |
| 定时关机-分 | IDC_EDIT2 | Edit Control | 输入定时关机的分钟数 |
| 定时关机-秒 | IDC_EDIT3 | Edit Control | 输入定时关机的秒数 |
| 自身置顶 | IDC_CHECK3 | Check Box | 将工具箱自身窗口置顶/取消置顶 |
| 连点器开关 | IDC_CHECK4 | Check Box | 开启/关闭连点器（A键开始，B键停止） |
| 开机自启 | IDC_CHECK1 | Check Box | 设置程序开机自启动 |
| 最小化到托盘 | IDC_CHECK2 | Check Box | 关闭时最小化到系统托盘 |
| 防锁屏 | IDC_CHECK5 | Check Box | 阻止系统自动锁屏/休眠 |
| 非管理员 PowerShell | IDC_CHECK6 | Check Box | 以非管理员权限启动 PowerShell |

### 进程管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 进程列表 | IDC_LIST1 | List Control | 显示进程名/PID/路径/内存(KB)，右键菜单支持结束进程和定位文件 |

### 启动项管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 启动项列表 | IDC_LIST2 | List Control | 显示启动项名和命令，右键菜单支持添加/删除/复制路径，双击复制路径 |

### 剪贴板标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 剪贴板历史 | IDC_LIST3 | List Control | 显示最近 10 条剪贴板文本，双击复制到剪贴板 |

### 窗口处理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 窗口信息 | IDC_LIST5 | List Control | 显示选中窗口的句柄/进程名/PID/路径/标题，双击复制值 |
| 置顶窗口列表 | IDC_LIST6 | List Control | 显示所有已置顶窗口，单击加载详情，右键取消置顶/删除 |
| 历史定位列表 | IDC_LIST7 | List Control | 显示历史定位窗口，单击加载详情，右键置顶/取消置顶/删除 |
| 定位窗口 | IDC_BUTTON19 | Button | 进入十字光标定位模式，点击目标窗口后可选择置顶或关闭 |
| 透明度标签 | IDC_STATIC18 | Static Text | 显示当前透明度百分比 |
| 透明度滑块 | IDC_SLIDER2 | Slider Control | 调节选中窗口透明度（10~255） |
| 强制结束进程 | IDC_BUTTON15 | Button | 强制终止选中窗口的进程 |
| 截图 | IDC_BUTTON16 | Button | 截取选中窗口画面，复制到剪贴板并保存为 PNG 文件 |

### 文件管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 拖放路径 | IDC_STATIC_PATH | Static Text | 显示拖入的文件路径 |
| 副本名称 | IDC_EDIT4 | Edit Control | 输入/显示生成副本的文件名（不含扩展名） |
| 生成副本 | IDC_BUTTON3 | Button | 生成拖入文件的副本 |
| 新文件名 | IDC_EDIT7 | Edit Control | 输入新文件名（不含扩展名） |
| 新扩展名 | IDC_EDIT8 | Edit Control | 输入新扩展名（不含点） |
| 重命名 | IDC_BUTTON23 | Button | 执行文件重命名 |
| 删除文件 | IDC_BUTTON24 | Button | 将拖入的文件移到回收站 |
| 复制文件 | IDC_BUTTON25 | Button | 复制文件到指定目标目录 |
| 移动文件 | IDC_BUTTON26 | Button | 移动文件到指定目标目录 |
| 目标路径 | IDC_MFCEDITBROWSE2 | MFC Edit Browse | 选择复制/移动的目标目录 |

### Git 工具箱标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 命令列表 | IDC_LIST4 | List Control | 显示 Git 命令说明和命令，双击或右键复制命令 |
| 打开 GitHub | IDC_BUTTON30 | Button | 在浏览器中打开 GitHub 网站 |
| 启动 Git Bash | IDC_BUTTON31 | Button | 在拖入路径对应的目录打开 Git Bash |
| 清除路径 | IDC_BUTTON32 | Button | 清除拖入的文件/目录路径 |

### AI 助手

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 供应商选择 | IDC_COMBO_AI_VENDOR | Combo Box | 选择 AI 供应商（OpenAI / DeepSeek） |
| 对话历史 | IDC_EDIT_AI_HISTORY | Edit Control | 只读，显示对话历史记录 |
| 输入框 | IDC_EDIT_AI_INPUT | Edit Control | 输入问题后点击发送或按回车 |
| 发送 | IDC_BUTTON_AI_SEND | Button | 发送消息给 AI |
| 清除 | IDC_BUTTON_AI_CLEAR | Button | 清除对话历史 |

### 工具箱按钮

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 启动微信 | IDC_BUTTON4 | Button | 启动微信（已运行时发送 Ctrl+Alt+W 激活） |
| 启动 QQ | IDC_BUTTON5 | Button | 启动 QQ（已运行时发送 Ctrl+Alt+X 激活） |
| 启动 VS Code | IDC_BUTTON6 | Button | 启动 VS Code |
| 启动 Visual Studio | IDC_BUTTON7 | Button | 启动 Visual Studio |
| 启动哔哩哔哩 | IDC_BUTTON8 | Button | 启动哔哩哔哩客户端 |
| 学习文件夹 | IDC_BUTTON9 | Button | 打开学习文件夹 |
| 本地服务器 | IDC_BUTTON10 | Button | 打开配置的本地服务器 URL |
| 中国大学 MOOC | IDC_BUTTON11 | Button | 打开中国大学 MOOC |
| 音量应用 | IDC_BUTTON12 | Button | 应用编辑框中的音量值 |
| 音量设为 0 | IDC_BUTTON13 | Button | 设置系统音量为 0 |
| 音量设为 10 | IDC_BUTTON14 | Button | 设置系统音量为 10 |
| 音量滑块 | IDC_SLIDER1 | Slider Control | 调节系统音量（0~100） |
| 音量显示 | IDC_EDIT5 | Edit Control | 显示/输入当前音量值 |
| 运行命令 | IDC_BUTTON17 | Button | 运行输入的命令（支持 exe/URL/cmd 内置命令） |
| 清空命令 | IDC_BUTTON18 | Button | 清空命令输入框 |
| 命令输入 | IDC_EDIT6 | Edit Control | 输入命令（Enter 键触发运行） |
| 任务管理器 | IDC_BUTTON20 | Button | 打开 Windows 任务管理器 |
| 下载文件夹 | IDC_BUTTON21 | Button | 打开下载文件夹 |
| 启动元宝 | IDC_BUTTON22 | Button | 启动元宝 |
| 启动 PowerShell | IDC_BUTTON27 | Button | 启动 PowerShell（支持管理员/非管理员模式） |
| 启动 WSL | IDC_BUTTON28 | Button | 启动 WSL |
| 打开 LeetCode | IDC_BUTTON29 | Button | 打开 LeetCode 中文站 |
| 哔哩哔哩下一首 | IDC_BUTTON33 | Button | 哔哩哔哩播放下一首（自动定位窗口发送 ] 键） |

## 工具窗口

| 工具 | 入口 | 功能描述 |
|------|------|----------|
| 设置 | 菜单 文件→设置 | 配置各应用可执行文件路径、文件夹路径、截图保存目录、AI 供应商和 API Key |
| 二维码生成器 | 菜单 工具→二维码生成器 | 输入文本生成二维码，支持复制到剪贴板和保存为 PNG |
| 截图 OCR | 菜单 工具→截图OCR | 框选屏幕区域截图，后台 OCR 识别文字，支持翻译为多语言 |
| 批量重命名 | 菜单 工具→批量重命名 | 文件批量重命名（前缀/后缀/替换/编号/正则），支持文件夹操作 |
| 便签 | 菜单 工具→便签 | 置顶便签窗口，支持折叠/展开、双击标题栏切换，自动保存 |
| Markdown 预览 | 菜单 工具→Markdown预览 | 左右分栏编辑+实时渲染，支持可拖拽分隔条 |
| 编码转换 | 菜单 工具→编码转换 | 检测并转换文件编码（UTF-8/GBK/等），支持批量 |
| 右键菜单管理 | 菜单 工具→右键菜单管理 | 扫描/启用/禁用右键菜单项，支持 ShellEx 和静态 verb |
| 环境变量管理 | 菜单 工具→环境变量管理 | 查看/编辑/导出系统/用户环境变量，PATH 编辑器 |
| 文件占用查看 | 菜单 工具→文件占用查看 | 拖入文件查看占用进程，支持结束/定位进程 |
| 正则表达式指南 | 菜单 帮助→正则表达式指南 | 常用正则表达式速查 |
| 连点器速度调节 | 勾选连点器复选框后自动弹出 | 实时调节连点间隔，显示连点状态（等待中/连点中） |

## AI 助手

AI 助手位于右侧面板第一个选项卡的下半部分，可帮助用户解答工具箱使用问题。

**配置方法：**
1. 菜单 文件 → 设置 → 找到「AI 助手」区域
2. 选择供应商（OpenAI 或 DeepSeek）
3. 输入 API Key（点击「显示」可查看明文）
4. 点击「确定」保存

**使用方法：**
1. 在输入框中输入问题
2. 点击「发送」或按回车键
3. AI 回复将显示在对话历史中
4. 点击「清除」清空对话历史

**支持的供应商：**
- OpenAI（默认模型：gpt-3.5-turbo）
- DeepSeek（默认模型：deepseek-chat）

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| Ctrl+Alt+Space | 显示/隐藏工具箱主窗口 |
| Alt+1~6 | 切换到对应标签页 |
| F5 | 刷新当前标签页列表 |
| Ctrl+Alt+D | 进入窗口定位模式 |
| Enter | 命令输入框获得焦点时触发运行 |

## 菜单栏

| 菜单 | 菜单项 | 功能 |
|------|--------|------|
| 文件 | 设置 | 打开设置对话框（配置应用路径、AI 供应商和 API Key） |
| 文件 | 退出 | 退出程序 |
| 视图 | 进程管理/启动项管理/剪贴板/窗口处理/文件管理/Git 工具箱 | 快速切换到对应标签页 |
| 工具 | 微信/QQ/VS Code/VS/哔哩哔哩/学习/下载/PowerShell/WSL/Git Bash/二维码生成器/截图OCR/批量重命名/便签/Markdown预览/编码转换/右键菜单管理/环境变量管理/文件占用查看 | 快速启动工具或打开工具窗口 |
| 窗口 | 窗口定位/取消全部置顶/关闭选中窗口 | 窗口管理快捷操作 |
| 帮助 | 快捷键列表/GitHub/正则表达式指南/关于 | 帮助信息 |
