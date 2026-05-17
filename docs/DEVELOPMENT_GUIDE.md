# 개발 가이드 및 체크리스트

## 개발 환경

macOS:

```sh
make
make run
```

Windows:

```bat
build-windows.bat
build\vampire-survivors-c.exe
```

Windows에서는 MinGW-w64의 `gcc`가 필요하다. Windows Terminal 또는 PowerShell 사용을 권장한다.

## 작업 전 체크리스트

1. 현재 브랜치가 자기 브랜치인지 확인한다.

```sh
git branch --show-current
```

2. 최신 코드를 받는다.

```sh
git pull
```

3. [협업 작업 현황판](TEAM_TASKS.md)에서 자기 담당 작업과 완료 기준을 확인한다.

4. 빌드가 되는지 확인한다.

```sh
make
```

## 작업 후 체크리스트

1. 변경 파일을 확인한다.

```sh
git status
```

2. 빌드 확인:

```sh
make
```

3. 스모크 테스트 확인:

```sh
./build/vampire-survivors-c --smoke-test
```

4. Windows에서는 아래 실행 파일로 게임을 직접 켜서 확인한다.

```bat
build\vampire-survivors-c.exe
```

5. 게임 규칙, 조작법, 담당 작업이 바뀌었다면 문서를 같이 수정한다.

6. 한글 커밋 메시지로 커밋한다.

```sh
git add .
git commit -m "기능: 작업 내용 요약"
```

7. 자기 브랜치에 push한다.

```sh
git push
```

## Pull Request 전 확인할 것

- 게임이 실행되는가?
- 시작 화면이 나오는가?
- 플레이어 이동이 되는가?
- 적이 이동하는가?
- 자동 공격이 나가는가?
- 경험치를 먹고 레벨업할 수 있는가?
- 게임오버 또는 승리 조건이 명확한가?
- `Esc` 일시정지와 재개가 유지되는가?
- 고정 맵 크기와 UI가 깨지지 않는가?
- README나 문서 수정이 필요한가?
- Pull Request 설명에 테스트 결과를 적었는가?

## 코드 수정 원칙

- 한 번에 너무 많은 기능을 바꾸지 않는다.
- 기능 하나를 끝낼 때마다 커밋한다.
- 전역 구조체는 `game.h`에서 확인하고 수정한다.
- 플랫폼별 코드는 `platform.c` 안에서 처리한다.
- 터미널 UI 변경은 `ui.c`에서 처리한다.

## 문서 수정 원칙

- 회의에서 정한 내용은 `docs/MEETING_YYYY-MM-DD.md`에 기록한다.
- 게임 규칙 변경은 `docs/GAME_RULES.md`에 반영한다.
- 협업 방식 변경은 `docs/GITHUB_WORKFLOW.md`에 반영한다.
- 목표 일정 변경은 `docs/MILESTONES.md`에 반영한다.

## 알려진 주의사항

- 일부 터미널은 bell 소리를 기본으로 끈다.
- Windows ANSI 색상은 Windows Terminal 또는 최신 PowerShell에서 가장 안정적이다.
- `scores.txt`는 실행 중 생성되는 개인 기록 파일이므로 Git에 올리지 않는다.
- `build/` 폴더는 빌드 결과물이므로 Git에 올리지 않는다.
