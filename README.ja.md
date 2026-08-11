<p align="center">
  [🌐 English](README.md) · [中文](README.zh-CN.md) · [日本語](README.ja.md)
</p>

---

<p align="center">
  <img src="docs/screenshots/main.png" alt="PowerBox" width="720">
</p>

<h1 align="center">⚡ PowerBox</h1>

<p align="center">
  <strong>Windows デスクトップツールボックス — AI アシスタント、プロセス管理、スクリーン OCR、Git ツールボックスなど 20 以上のユーティリティを統合</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/framework-MFC-%230078D6?logo=microsoft" alt="MFC">
  <img src="https://img.shields.io/badge/release-v1.1.0-brightgreen" alt="v1.1.0">
  <img src="https://img.shields.io/badge/platform-Windows-%230078D6?logo=windows" alt="Windows">
</p>

<p align="center">
  <a href="https://github.com/gyx114/PowerBox/wiki">📖 Wiki ドキュメント</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/Quick-Start">🚀 クイックスタート</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/Feature-Details">📋 機能詳細</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/AI-Configuration">🤖 AI 設定</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/FAQ">❓ FAQ</a>
</p>

---

## ✨ 機能概要

### 🧠 AI アシスタント
6 つの AI プロバイダー（OpenAI、DeepSeek、Tongyi Qianwen、Zhipu AI、Moonshot、SiliconFlow）を統合し、マルチモードでの対話をサポートします。

| モード | 説明 |
|------|-------------|
| AI チャット | 右パネルで直接対話、ストリーミング出力、Markdown レンダリング、履歴の保存/読み込みに対応。独立した AI アシスタントウィンドウも利用可能 |
| プロセス AI 分析 | プロセスを右クリック → AI 分析、セキュリティ評価とアクション推奨を表示 |
| プロセス AI スキャン | 全プロセスをワンクリックでスキャン、スキャンレベルを選択可能。AI が不審/不要なプロセスを識別し、一括終了に対応 |
| AI 一括リネーム | 自然言語で要件を記述するだけで、AI が自動的にファイル名マッピングを生成。他のリネームルールとの重ね合わせも可能 |

AI アシスタントパネルには ConPTY ターミナルが内蔵されており、複数のターミナルセッションをサポートします。AI がコマンドを実行する際、自動的にターミナルで実行し結果を返します。

### 🌐 多言語インターフェース
中国語/英語の切り替えに対応。言語ファイルは `lang/` ディレクトリに配置されており、簡単に他の言語に拡張できます。詳細は [機能詳細](https://github.com/gyx114/PowerBox/wiki/Feature-Details) の「多言語インターフェース」セクションをご参照ください。

### 📋 コアツール

| ツール | 説明 |
|------|-------------|
| プロセス管理 | プロセスの一覧表示、CPU/メモリの表示、ソート/フィルター（正規表現対応）、プロセス終了、ファイルの場所を開く、AI 分析 |
| スタートアップ管理 | レジストリスタートアップ項目の表示/追加/削除/有効化/無効化、ダブルクリックでパスをコピー、マシンレベルのスタートアップ項目の追加に対応 |
| クリップボード履歴 | 最新 10 件のテキストエントリを記録、ダブルクリックで貼り付け、連続重複は自動的に重複排除 |
| ウィンドウツール | ウィンドウの特定（Ctrl+Alt+D）、最前面固定（複数ウィンドウ対応）、透明度調整（10%～100%）、強制終了、スクリーンショット保存。特定履歴を自動保存 |
| ファイル管理 | ファイルをドラッグ＆ドロップでコピー作成（名前カスタマイズ可能）、リネーム、削除（ごみ箱）、コピー、移動。フォルダをドロップすると一括リネームを開く |
| Git ツールボックス | 20 のプリセットコマンド、カスタムリスト対応（config.ini [GitCommands]）、リポジトリブランチを自動検出、AI 支援コマンド生成、Git Bash クイック起動、独立した Git コマンド結果ウィンドウ |

### 🛠 ツール一覧

| ツール | 説明 |
|------|-------------|
| QR コード生成 | テキストから QR コードを生成、クリップボードにコピーまたは PNG/BMP で保存対応（4px 白マージン） |
| スクリーン OCR | 領域選択スクリーンショット、Windows OCR テキスト認識、中国語/英語/日本語/韓国語に対応、MyMemory API 翻訳対応（6 言語ペア） |
| 一括リネーム | 接頭辞/接尾辞/置換/連番/正規表現/一致削除、除外ルールと追跡ルール対応、AI スマートリネーム、元に戻す、フォルダ名の変更に対応 |
| 付箋 | 最前面表示の付箋、折りたたみ/展開（タイトルバーをダブルクリック）、X ボタンで折りたたみ/終了、内容は設定フォルダに自動保存 |
| Markdown プレビュー | 分割ペインエディター + ライブプレビュー、GitHub スタイル CSS（marked.js）、ドラッグ可能な分割バー、.md ファイルの開くに対応 |
| エンコーディング変換 | エンコーディング自動検出（UTF-8/UTF-8 BOM/UTF-16LE/UTF-16LE BOM/UTF-16BE/GBK/Big5/Shift-JIS/Latin-1）、一括変換に対応 |
| コンテキストメニュー管理 | コンテキストメニュー項目のスキャン/有効化/無効化、28 以上のシナリオ、14 の拡張プリセット + カスタムに対応、AI による未翻訳項目の解析、Win11 クラシックメニュー切り替え対応 |
| 環境変数 | システム/ユーザー環境変数の表示/編集/エクスポート、PATH エディター（独立行編集）、変更前に自動バックアップ |
| ファイルロック表示 | ファイルをドラッグ＆ドロップしてロック中のプロセスを表示（Restart Manager API）、プロセス終了/すべて終了/プロセスフォルダを開くに対応 |
| クイック起動 | ユーザー設定可能なクイックボタン（最大 36 個）、5 種類対応：実行可能ファイル/フォルダ/URL/その他ファイル/ショートカットキー。実行可能ファイルには起動ホットキーを割り当て可能。ショートカットキータイプはキーコンビネーションのシミュレートに対応。管理ダイアログ、ドラッグ＆ドロップ並べ替え、ファイル/フォルダのドラッグ＆ドロップによるクイック追加に対応 |
| カスタムホットキー | グローバルホットキー（メインウィンドウの表示/非表示、ウィンドウ特定）を設定からカスタマイズ可能、ホットキーキャプチャダイアログ付き。クイック起動項目は起動ホットキーとショートカットキーシミュレーションに対応 |
| ConPTY ターミナル | AI アシスタントパネルに内蔵されたターミナル、複数のターミナルセッションに対応、水平タブバー切り替え（Ctrl+Tab/スクロールホイール/中クリックで閉じる）、AI が自動的に新しいタブでコマンドを実行 |
| 独立 AI アシスタントウィンドウ | メインウィンドウから独立して使用可能な AI アシスタントウィンドウ、チャット、ターミナル、履歴管理の全機能を完備 |

### ⌨️ ショートカット

| ショートカット | 機能 |
|----------|----------|
| `Ctrl+Alt+Space` | グローバルホットキー、メインウィンドウの表示/非表示（設定からカスタマイズ可能） |
| `Ctrl+Alt+D` | ウィンドウ特定モードに入る（設定からカスタマイズ可能） |
| `Alt+1` ~ `Alt+6` | 左サイドバータブの切り替え |
| `F5` | 現在のリストを更新 |
| `Del` | 選択したプロセスを終了 |
| `Ctrl+Enter` | AI チャットメッセージを送信（AI パネルフォーカス時） |

---

## 🖼️ スクリーンショット

| メイン画面 | AI アシスタント | プロセス管理 |
|:-----:|:------:|:-------:|
| ![メイン画面](docs/screenshots/main.png) | ![AI アシスタント](docs/screenshots/ai-assistant.png) | ![プロセス管理](docs/screenshots/process.png) |

| スタートアップ管理 | Git ツールボックス | 一括リネーム |
|:---------:|:--------:|:---------:|
| ![スタートアップ管理](docs/screenshots/startup.png) | ![Git ツールボックス](docs/screenshots/git.png) | ![一括リネーム](docs/screenshots/rename.png) |

| コンテキストメニュー管理 | スクリーン OCR | QR コード |
|:----------:|:--------:|:---------:|
| ![コンテキストメニュー管理](docs/screenshots/contextmenu.png) | ![スクリーン OCR](docs/screenshots/ocr.png) | ![QR コード](docs/screenshots/qrcode.png) |

> その他のスクリーンショットは [docs/screenshots/](docs/screenshots/) ディレクトリをご覧ください。

---

## 📖 ドキュメントナビゲーション

詳細な使用手順と設定ガイドは Wiki をご参照ください：

| ドキュメント | 説明 |
|----------|-------------|
| [📋 機能詳細](https://github.com/gyx114/PowerBox/wiki/Feature-Details) | 各機能の詳細な説明 |
| [🤖 AI 設定](https://github.com/gyx114/PowerBox/wiki/AI-Configuration) | AI プロバイダーの設定と API キーの取得方法 |
| [🚀 クイックスタート](https://github.com/gyx114/PowerBox/wiki/Quick-Start) | ダウンロード、インストール、初回利用ガイド |
| [⌨️ ショートカット](https://github.com/gyx114/PowerBox/wiki/Shortcuts) | 完全なショートカットキーリファレンス |
| [🔧 ビルドガイド](https://github.com/gyx114/PowerBox/wiki/Build-Guide) | ソースコードからのビルド方法 |
| [❓ FAQ](https://github.com/gyx114/PowerBox/wiki/FAQ) | よくある質問と回答 |

---

## 📥 インストール

### リリースからダウンロード
[Releases ページ](https://github.com/gyx114/PowerBox/releases) から最新の `PowerBox.zip` をダウンロードして解凍するだけでそのまま使用できます（ポータブルソフトウェア、インストール不要）。ZIP には `PowerBox.exe` と `lang/` 言語フォルダが含まれており、すぐに使い始められます。

### ソースコードからビルド
Visual Studio 2022（またはそれ以降）が必要です。C++ デスクトップ開発ワークロード（MFC コンポーネントを含む）をインストールしてください。

```bash
git clone https://github.com/gyx114/PowerBox.git
cd PowerBox
msbuild MFCApplication1/MFCApplication1.vcxproj /p:Configuration=Release /p:Platform=x64
```

ビルド出力は `x64/Release/PowerBox.exe` に配置されます。ビルド後、ソースの `MFCApplication1/lang/` フォルダを実行可能ファイルと同じディレクトリにコピーしてください。コピーしない場合、プログラムは内蔵の中国語 UI を使用し、言語切り替えはできません。

---

## 🛠 技術スタック

| カテゴリ | 技術 |
|----------|------------|
| プログラミング言語 | C++20 |
| UI フレームワーク | MFC (Microsoft Foundation Classes) |
| AI 連携 | WinHTTP + 6 AI プロバイダー（OpenAI、DeepSeek、Tongyi Qianwen、Zhipu AI、Moonshot、SiliconFlow） |
| QR コード | Nayuki QR Code Generator |
| OCR | Windows OCR API (Windows.Media.Ocr) + MyMemory 翻訳 API |
| Markdown | WebBrowser コントロール + marked.js（GitHub スタイル CSS） |
| ターミナル | ConPTY (Windows Pseudo Console) |
| 多言語 | INI ファイル (UTF-16 LE) + 組み込みデフォルト値、中国語/英語の切り替えに対応、言語ファイルは `lang/` ディレクトリ |
| ビルド | Visual Studio 2022 + MSBuild |

---

## 📄 ライセンス

[MIT](LICENSE) © 2026 管宇軒