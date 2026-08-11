🌐 [English](README.md) · [中文](README.zh-CN.md) · [日本語](README.ja.md) · [Русский](README.ru.md) · **한국어**

---

<p align="center">
  <img src="docs/screenshots/main.png" alt="PowerBox" width="720">
</p>

<h1 align="center">⚡ PowerBox</h1>

<p align="center">
  <strong>Windows 데스크톱 도구 모음 — 통합 AI 어시스턴트, 프로세스 관리자, 스크린 OCR, Git 도구 모음 및 20개 이상의 유틸리티</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-%2300599C?logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/framework-MFC-%230078D6?logo=microsoft" alt="MFC">
  <img src="https://img.shields.io/badge/release-v1.1.0-brightgreen" alt="v1.1.0">
  <img src="https://img.shields.io/badge/platform-Windows-%230078D6?logo=windows" alt="Windows">
</p>

<p align="center">
  <a href="https://github.com/gyx114/PowerBox/wiki">📖 Wiki 문서</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/Quick-Start">🚀 빠른 시작</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/Feature-Details">📋 기능</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/AI-Configuration">🤖 AI 설정</a>
  ·
  <a href="https://github.com/gyx114/PowerBox/wiki/FAQ">❓ FAQ</a>
</p>

---

## ✨ 기능 개요

### 🧠 AI 어시스턴트
6개 AI 제공업체(OpenAI, DeepSeek, Tongyi Qianwen, Zhipu AI, Moonshot, SiliconFlow)와 통합되어 다중 모드 상호작용을 지원합니다.

| 모드 | 설명 |
|------|------|
| AI 채팅 | 오른쪽 패널에서 직접 대화, 스트리밍 출력, Markdown 렌더링, 기록 저장/불러오기 지원; 독립형 AI 어시스턴트 창 사용 가능 |
| 프로세스 AI 분석 | 프로세스 우클릭 → AI 분석, 보안 등급 및 조치 권장 사항 제공 |
| 프로세스 AI 스캔 | 원클릭 모든 프로세스 스캔, 스캔 수준 선택, AI가 의심스럽거나 불필요한 프로세스 식별, 일괄 종료 지원 |
| AI 일괄 이름 변경 | 자연어로 요구 사항 설명, AI가 자동으로 파일 이름 매핑 생성, 다른 이름 변경 규칙과 중첩 지원 |

AI 어시스턴트 패널에는 여러 터미널 세션을 지원하는 ConPTY 터미널이 내장되어 있습니다. AI가 명령을 실행하면 자동으로 터미널에서 실행하고 결과를 반환합니다.

### 🌐 다국어 인터페이스
중국어/영어 전환 지원, 언어 파일은 `lang/` 디렉터리에 위치, 더 많은 언어로 쉽게 확장 가능합니다. 자세한 내용은 [기능 세부 정보](https://github.com/gyx114/PowerBox/wiki/Feature-Details)의 "다국어 인터페이스" 섹션을 참조하세요.

### 📋 핵심 도구

| 도구 | 설명 |
|------|------|
| 프로세스 관리자 | 프로세스 열거, CPU/메모리 표시, 정렬/필터링(정규식 포함), 프로세스 종료, 파일 위치 찾기, AI 분석 지원 |
| 시작 프로그램 관리자 | 레지스트리 시작 항목 보기/추가/삭제/활성화/비활성화, 더블클릭하여 경로 복사, 머신 수준 시작 항목 추가 지원 |
| 클립보드 기록 | 최근 10개 텍스트 항목 기록, 더블클릭하여 다시 붙여넣기, 연속 중복 항목 자동 제거 |
| 창 도구 | 창 찾기(Ctrl+Alt+D), 항상 위에 고정(여러 창 지원), 투명도 조절(10%-100%), 강제 종료, 스크린샷 저장; 찾기 기록 자동 저장 |
| 파일 관리자 | 파일 드래그하여 복사본 생성(이름 사용자 지정 가능), 이름 변경, 삭제(휴지통), 복사, 이동; 폴더 드래그하여 일괄 이름 변경 열기 |
| Git 도구 모음 | 20개의 사전 설정 명령어, 사용자 지정 목록 지원(config.ini [GitCommands]), 저장소 브랜치 자동 감지, AI 지원 명령 생성, Git Bash 빠른 실행, 독립형 Git 명령 결과 창 |

### 🛠 도구 창

| 도구 | 설명 |
|------|------|
| QR 코드 생성기 | 텍스트로 QR 코드 생성, 클립보드에 복사 및 PNG/BMP로 저장 지원(4px 흰색 여백) |
| 스크린 OCR | 영역 선택 스크린샷, Windows OCR 텍스트 인식, 중국어/영어/일본어/한국어 지원, MyMemory API 번역 지원(6개 언어 쌍) |
| 일괄 이름 변경 | 접두사/접미사/바꾸기/번호 매기기/정규식/일치 삭제, 무시 규칙 및 추적 규칙 지원, AI 스마트 이름 변경, 실행 취소 및 폴더 이름 변경 지원 |
| 스티커 메모 | 항상 위에 표시되는 메모, 접기/펼치기(제목 표시줄 더블클릭), X 버튼으로 접기/종료, 내용이 설정 폴더에 자동 저장 |
| Markdown 미리보기 | 분할창 편집기 + 실시간 미리보기, GitHub 스타일 CSS(marked.js), 드래그 가능한 분할선, .md 파일 열기 지원 |
| 인코딩 변환 | 인코딩 자동 감지(UTF-8/UTF-8 BOM/UTF-16LE/UTF-16LE BOM/UTF-16BE/GBK/Big5/Shift-JIS/Latin-1), 일괄 변환 지원 |
| 컨텍스트 메뉴 관리자 | 컨텍스트 메뉴 항목 스캔/활성화/비활성화, 28개 이상 시나리오 지원, 14개 확장 프리셋 + 사용자 지정, 미번역 항목 AI 파싱, Win11 클래식 메뉴 토글 지원 |
| 환경 변수 | 시스템/사용자 환경 변수 보기/편집/내보내기, PATH 편집기(독립적 줄 편집), 수정 전 자동 백업 |
| 파일 잠금 뷰어 | 파일 드래그하여 잠금 프로세스 보기(Restart Manager API), 프로세스 종료/전체 종료/프로세스 폴더 위치 찾기 지원 |
| 빠른 실행 | 사용자 설정 가능한 빠른 버튼(최대 36개), 5가지 유형 지원: 실행 파일/폴더/URL/기타 파일/단축키; 실행 파일에 활성화 핫키 할당 가능; 단축키 유형은 키 조합 시뮬레이션 가능; 관리 대화상자 포함, 드래그 앤 드롭 정렬, 파일/폴더 드래그 앤 드롭 빠른 추가 지원 |
| 사용자 지정 핫키 | 전역 핫키(메인 창 표시/숨기기, 창 찾기)를 설정에서 사용자 지정 가능, 핫키 캡처 대화상자 포함; 빠른 실행 항목은 활성화 핫키 및 단축키 시뮬레이션 지원 |
| ConPTY 터미널 | AI 어시스턴트 패널 내장 터미널, 여러 터미널 세션 지원, 수평 탭 표시줄 전환(Ctrl+Tab/스크롤 휠/가운데 클릭 닫기), AI가 새 터미널 탭에서 자동으로 명령 실행 |
| 독립형 AI 어시스턴트 창 | 메인 창과 독립적으로 사용할 수 있는 AI 어시스턴트 창, 전체 채팅, 터미널 및 기록 관리 기능 제공 |

### ⌨️ 단축키

| 단축키 | 기능 |
|--------|------|
| `Ctrl+Alt+Space` | 전역 핫키, 메인 창 표시/숨기기(설정에서 사용자 지정 가능) |
| `Ctrl+Alt+D` | 창 찾기 모드 진입(설정에서 사용자 지정 가능) |
| `Alt+1` ~ `Alt+6` | 왼쪽 사이드바 탭 전환 |
| `F5` | 현재 목록 새로고침 |
| `Del` | 선택한 프로세스 종료 |
| `Enter` | AI 채팅 메시지 전송(AI 패널 포커스 시). `Shift+Enter`는 줄바꿈. |

---

## 🖼️ 스크린샷

| 메인 화면 | AI 어시스턴트 | 프로세스 관리자 |
|:-----:|:------:|:-------:|
| ![메인 화면](docs/screenshots/main.png) | ![AI 어시스턴트](docs/screenshots/ai-assistant.png) | ![프로세스 관리자](docs/screenshots/process.png) |

| 시작 프로그램 관리자 | Git 도구 모음 | 일괄 이름 변경 |
|:---------:|:--------:|:---------:|
| ![시작 프로그램 관리자](docs/screenshots/startup.png) | ![Git 도구 모음](docs/screenshots/git.png) | ![일괄 이름 변경](docs/screenshots/rename.png) |

| 컨텍스트 메뉴 관리자 | 스크린 OCR | QR 코드 |
|:----------:|:--------:|:---------:|
| ![컨텍스트 메뉴 관리자](docs/screenshots/contextmenu.png) | ![스크린 OCR](docs/screenshots/ocr.png) | ![QR 코드](docs/screenshots/qrcode.png) |

> 더 많은 스크린샷은 [docs/screenshots/](docs/screenshots/) 디렉터리에서 확인하세요.

---

## 📖 문서 탐색

자세한 사용 방법 및 설정 가이드는 Wiki에서 확인할 수 있습니다:

| 문서 | 설명 |
|------|------|
| [📋 기능](https://github.com/gyx114/PowerBox/wiki/Feature-Details) | 각 기능 모듈의 상세 설명 |
| [🤖 AI 설정](https://github.com/gyx114/PowerBox/wiki/AI-Configuration) | AI 제공업체 설정 및 API 키 획득 |
| [🚀 빠른 시작](https://github.com/gyx114/PowerBox/wiki/Quick-Start) | 다운로드, 설치 및 첫 사용 가이드 |
| [⌨️ 단축키](https://github.com/gyx114/PowerBox/wiki/Shortcuts) | 전체 단축키 참조 |
| [🔧 빌드 가이드](https://github.com/gyx114/PowerBox/wiki/Build-Guide) | 소스 코드에서 빌드하기 |
| [❓ FAQ](https://github.com/gyx114/PowerBox/wiki/FAQ) | 자주 묻는 질문과 답변 |

---

## 📥 설치

### 릴리스 다운로드
[Releases 페이지](https://github.com/gyx114/PowerBox/releases)에서 최신 `PowerBox.zip`을 다운로드하여 압축을 풀고 바로 사용하세요(포터블 소프트웨어, 설치 불필요). 패키지에는 `PowerBox.exe`와 `lang/` 언어 폴더가 포함되어 있어 바로 사용할 수 있습니다.

### 소스 코드에서 빌드
Visual Studio 2022(또는 그 이상)와 C++ 데스크톱 개발 워크로드(MFC 구성 요소 포함)가 필요합니다.

```bash
git clone https://github.com/gyx114/PowerBox.git
cd PowerBox
msbuild MFCApplication1/MFCApplication1.vcxproj /p:Configuration=Release /p:Platform=x64
```

빌드 출력은 `x64/Release/PowerBox.exe`에 위치합니다. 빌드 후 소스의 `MFCApplication1/lang/` 폴더를 실행 파일 디렉터리로 복사하세요. 그렇지 않으면 프로그램이 내장된 중국어 UI를 사용하며 언어를 전환할 수 없습니다.

---

## 🛠 기술 스택

| 분류 | 기술 |
|------|------|
| 언어 | C++20 |
| UI 프레임워크 | MFC (Microsoft Foundation Classes) |
| AI 연동 | WinHTTP + 6개 AI 제공업체(OpenAI, DeepSeek, Tongyi Qianwen, Zhipu AI, Moonshot, SiliconFlow) |
| QR 코드 | Nayuki QR Code Generator |
| OCR | Windows OCR API(Windows.Media.Ocr) + MyMemory 번역 API |
| Markdown | WebBrowser 컨트롤 + marked.js(GitHub 스타일 CSS) |
| 터미널 | ConPTY (Windows Pseudo Console) |
| 다국어 | INI 파일(UTF-16 LE) + 내장 기본값, 중국어/영어 전환 지원, 언어 파일은 `lang/` 디렉터리 |
| 빌드 | Visual Studio 2022 + MSBuild |

---

## 📄 라이선스

[MIT](LICENSE) © 2026 GuanyuXuan