# UnrealCodex (한국어 안내)

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?style=flat&logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win64%20%7C%20Linux-0078D6?style=flat&logo=windows&logoColor=white)
![Codex CLI](https://img.shields.io/badge/Codex%20CLI-Integration-0A7EA4?style=flat)

OpenAI Codex CLI를 Unreal Engine 5.7 에디터에 통합한 플러그인입니다.

Language: [English](README.md) | 한국어

## 개요

UnrealCodex는 에디터 내부에서 `codex` CLI를 직접 실행해 AI 보조 기능을 제공합니다.

- 에디터 내 채팅 패널(스트리밍 응답)
- MCP 서버(20+ 도구: 액터/레벨/블루프린트/머티리얼/입력 등)
- UE 5.7 + 프로젝트 컨텍스트 자동 수집
- 세션 저장/복원

지원 플랫폼: Windows(Win64), Linux

## 사전 준비

1) Codex CLI 설치

```bash
npm i -g @openai/codex
```

2) 로그인

```bash
codex login
```

3) 설치 확인

```bash
codex --version
codex exec --skip-git-repo-check "Reply with OK"
```

## 설치

### 1) 플러그인 빌드

```bash
# Windows
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude\UnrealClaude\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64

# Linux
Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/path/to/UnrealClaude/UnrealClaude/UnrealClaude.uplugin" -Package="/output/path" -TargetPlatforms=Linux
```

참고: 호환성 때문에 플러그인 루트/`.uplugin` 파일명은 `UnrealClaude`를 유지하고, C++ 모듈 소스 경로는 `Source/UnrealCodex`를 사용합니다.

### 2) 프로젝트 또는 엔진에 복사

- 권장: `YourProject/Plugins/UnrealClaude/`
- 엔진 공용: `Engine/Plugins/Marketplace/UnrealClaude/`

### 3) MCP 브리지 의존성 설치

```bash
cd <PluginPath>/UnrealClaude/Resources/mcp-bridge
npm install
```

### 4) 에디터 실행

에디터 실행 후 자동 로드됩니다.

## 사용법

- 메뉴 경로: `Tools -> Codex Assistant`
- 입력: `Enter` 전송 / `Shift+Enter` 줄바꿈 / `Esc` 취소

예시 질문:

```text
UE5.7에서 커스텀 Actor Component를 C++로 만드는 방법 알려줘.
GAS 기반 체력 시스템 구조를 제안해줘.
World Partition 스트리밍 구성 추천해줘.
```

## 설정

### 프로젝트 커스텀 지시문

프로젝트 루트에 `CLAUDE.md`를 만들면 기본 컨텍스트에 추가됩니다.
템플릿은 `UnrealClaude/CLAUDE.md.default`, `UnrealClaude/CODEX.md.default`를 사용할 수 있습니다.

```markdown
# My Project Context

## Architecture
- Dedicated Server 기반 멀티플레이

## Coding Standards
- Blueprint 노출 멤버는 UPROPERTY 사용
```

### MCP 서버 ID

- 기본 예시: `unrealcodex`
- 하위 호환: `unrealclaude`도 허용

## 트러블슈팅

### Codex CLI를 못 찾는 경우

```bash
codex --version
```

PATH 점검:

- Windows: `where codex`
- Linux: `which codex`

### 인증 필요 메시지

```bash
codex login
```

### MCP 도구가 안 보이는 경우

```bash
cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
npm install
curl http://localhost:3000/mcp/status
```

출력 로그에서 `LogUnrealClaude` 카테고리를 확인하세요.

## 라이선스

MIT License - `UnrealClaude/LICENSE`
