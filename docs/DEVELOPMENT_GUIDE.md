# 개발 가이드 및 체크리스트

## 개발 환경

이 프로젝트는 Windows와 Visual Studio 2022만 공식 지원한다.
Visual Studio 2022에서 실행하려면 설치 시 `Desktop development with C++` 워크로드를 포함한다.

```txt
VampireSurvivorsC.sln 열기
x64 Debug 또는 x64 Release 선택
F5 실행
```

Visual Studio 빌드 결과:

```bat
build\Debug-x64\vampire-survivors-c.exe
build\Release-x64\vampire-survivors-c.exe
```

Windows Terminal 또는 PowerShell에서 실행하면 ANSI 색상이 가장 안정적으로 표시된다.

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

4. Visual Studio 2022에서 `VampireSurvivorsC.sln`을 열고 `x64 Debug` 빌드가 되는지 확인한다.

## 작업 후 체크리스트

1. 변경 파일을 확인한다.

```sh
git status
```

2. Visual Studio에서 `x64 Debug` 또는 `x64 Release`로 빌드한 뒤 게임을 직접 켜서 확인한다.

```bat
build\Debug-x64\vampire-survivors-c.exe
build\Release-x64\vampire-survivors-c.exe
```

3. 스모크 테스트 확인:

```bat
build\Debug-x64\vampire-survivors-c.exe --smoke-test
```

4. 게임 규칙, 조작법, 담당 작업이 바뀌었다면 문서를 같이 수정한다.

5. 한글 커밋 메시지로 커밋한다.

```sh
git add .
git commit -m "기능: 작업 내용 요약"
```

6. 자기 브랜치에 push한다.

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
- 게임 도메인 구조체는 `game.h`, 랭킹 기록 구조체는 `ranking.h`에서 확인하고 수정한다.
- Windows 터미널 코드는 `platform.c` 안에서 처리한다.
- 터미널 UI 변경은 `ui.h`, `ui.c`에서 처리한다.

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
