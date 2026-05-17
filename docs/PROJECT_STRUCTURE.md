# 프로젝트 파일 구조 및 코드 설명

## 전체 구조

```txt
.
├── Makefile
├── build-windows.bat
├── README.md
├── assets/
├── docs/
└── src/
    ├── main.c
    ├── game.h
    ├── game.c
    ├── entities.c
    ├── weapons.c
    ├── ui.c
    ├── platform.h
    └── platform.c
```

## 루트 파일

- `Makefile`: macOS 또는 MSYS2/Git Bash 환경에서 빌드와 실행을 담당한다.
- `build-windows.bat`: Windows에서 MinGW-w64 `gcc`로 실행 파일을 만드는 배치 파일이다.
- `README.md`: 프로젝트 소개, 실행 방법, 주요 문서 링크를 제공한다.
- `assets/`: 이후 이미지, 사운드, 맵 데이터 같은 리소스를 넣을 수 있는 폴더다.
- `docs/`: 회의록, 협업 규칙, 코드 구조, 마일스톤 등 프로젝트 문서를 보관한다.

## 소스 파일

### `src/main.c`

프로그램의 시작점이다.

- 초기 화면, 랭킹 화면, 게임 화면 전환
- 입력 읽기
- 게임 루프 실행
- 게임 종료 후 랭킹 저장 흐름 관리

### `src/game.h`

게임 전체에서 공유하는 타입과 함수 선언을 모아둔 헤더 파일이다.

- `Game`, `Player`, `Enemy`, `Weapon`, `Projectile`, `Pickup`
- 게임 모드, 적 종류, 무기 종류, 사운드 이벤트
- 게임 업데이트, 랭킹 저장, UI 호출 함수 선언

### `src/game.c`

게임의 중심 규칙을 담당한다.

- 게임 초기화
- 터미널 크기에 맞춰 조절되는 맵 타일과 벽 판정
- 레벨업 선택지 생성
- 승리와 패배 조건
- 랭킹 파일 읽기와 저장

### `src/entities.c`

플레이어, 적, 경험치 아이템을 담당한다.

- 플레이어 이동
- 적 생성과 추적 이동
- 적 접촉 피해
- 경험치 아이템 생성, 끌림, 획득

### `src/weapons.c`

무기와 전투 판정을 담당한다.

- 가장 가까운 적 자동 조준
- 마법탄 투사체 발사
- 신성 오라 범위 공격
- 투사체 이동과 적 충돌 처리

### `src/ui.c`

터미널 화면 표시를 담당한다.

- 초기 화면
- 랭킹 화면
- 게임 화면
- 체력바, 경험치바, 점수, 시간 표시
- 색상과 문자 기반 디자인
- 터미널 bell 사운드 출력

### `src/platform.h`, `src/platform.c`

macOS와 Windows의 터미널 처리 차이를 감추는 플랫폼 호환 레이어다.

- macOS: `termios`, `select`, `nanosleep`
- Windows: `conio.h`, Windows Console API
- 공통 기능:
  - 터미널 모드 진입과 복구
  - 비차단 키 입력
  - 시간 측정
  - 프레임 대기

## 주요 데이터 흐름

```txt
main.c
-> 입력 읽기
-> GameUpdate()
   -> PlayerUpdate()
   -> EnemiesSpawnWave()
   -> WeaponsUpdate()
   -> ProjectilesUpdate()
   -> EnemiesUpdate()
   -> CombatResolve()
   -> PickupsUpdate()
-> UiDrawGame()
```

## 빌드 결과물

- macOS: `build/vampire-survivors-c`
- Windows: `build/vampire-survivors-c.exe`

`build/` 폴더는 Git에 올리지 않는다.
