# 터미널 서바이버즈: C MVP

터미널 환경에서 실행되는 뱀파이어 서바이벌 스타일 C언어 MVP입니다. `raylib`, `SDL`, `ncurses` 같은 외부 그래픽 라이브러리를 쓰지 않고 ANSI escape sequence와 터미널 bell 소리만 사용합니다.

현재 지원 대상은 Windows와 Visual Studio 2022입니다.
게임 맵은 `96 x 30` 타일로 고정되어 있어 터미널 창 크기와 디스플레이 크기가 달라도 같은 좌표와 규칙으로 진행됩니다.

## 협업자 시작점

처음 작업하는 협업자는 먼저 [협업 작업 현황판](docs/TEAM_TASKS.md)을 확인합니다. 현재 완료된 기능, 다음 작업, 담당자별 역할, Pull Request 전 확인할 항목이 정리되어 있습니다.

## 빠른 실행

1. Visual Studio 2022를 설치할 때 `Desktop development with C++` 워크로드를 포함합니다.
2. `VampireSurvivorsC.sln`을 Visual Studio에서 엽니다.
3. 상단 구성을 `x64` / `Debug` 또는 `Release`로 두고 `F5`를 눌러 실행합니다.

Visual Studio 빌드 결과는 `build\Debug-x64\vampire-survivors-c.exe` 또는 `build\Release-x64\vampire-survivors-c.exe`에 생성됩니다.

## 조작법

- 이동: `WASD` 또는 방향키
- 설정 화면 열기: 초기 화면에서 `S` 또는 `Enter`
- 난이도 선택: 설정 화면에서 `1` Easy, `2` Hard 또는 방향키 후 `Enter`
- 랭킹 화면: 초기 화면에서 `R`
- 레벨업 선택: `1`, `2`, `3` 또는 방향키로 선택 후 `Enter` / `Space`
- 일시정지/재개: 게임 중 `Esc`
- 게임 종료 후 재시작: `R`
- 랭킹/결과 화면에서 초기 화면으로 돌아가기: `B` 또는 `Esc`
- 소리 켜기/끄기: `M`
- 현재 런 종료: 게임 중 `Q`
- 프로그램 종료: 초기 화면에서 `Q` 또는 `Esc`

## 프로젝트 문서

- [협업 작업 현황판](docs/TEAM_TASKS.md)
- [2026-05-17 1차 회의록](docs/MEETING_2026-05-17.md)
- [GitHub 협업 가이드](docs/GITHUB_WORKFLOW.md)
- [프로젝트 파일 구조 및 코드 설명](docs/PROJECT_STRUCTURE.md)
- [게임 규칙 문서](docs/GAME_RULES.md)
- [마일스톤 문서](docs/MILESTONES.md)
- [개발 가이드 및 체크리스트](docs/DEVELOPMENT_GUIDE.md)

## 현재 협업 브랜치

- `JAE`: JAE 작업 브랜치
- `TAE`: TAE 작업 브랜치
- `JONG`: JONG 작업 브랜치
- `main`: Pull Request로 검토가 끝난 안정 버전

작업은 각자 브랜치에서 진행하고, 완성된 내용은 Pull Request로 `main`에 합칩니다.
