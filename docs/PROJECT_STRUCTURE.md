# 프로젝트 파일 구조 및 코드 설명

## 전체 구조

```txt
.
├── VampireSurvivorsC.sln
├── VampireSurvivorsC.vcxproj
├── VampireSurvivorsC.vcxproj.filters
├── README.md
├── assets/
├── docs/
└── src/
    ├── main.c
    ├── game_config.h
    ├── game.h
    ├── game.c
    ├── entities.c
    ├── weapons.c
    ├── ui.h
    ├── ui.c
    ├── ranking.h
    ├── ranking.c
    ├── smoke_test.h
    ├── smoke_test.c
    ├── platform.h
    └── platform.c
```

## 루트 파일

- `VampireSurvivorsC.sln`: Windows에서 Visual Studio 2022로 열어 실행하는 솔루션 파일이다.
- `VampireSurvivorsC.vcxproj`: MSVC가 C 소스 파일을 컴파일하고 링크하는 프로젝트 설정 파일이다.
- `VampireSurvivorsC.vcxproj.filters`: Visual Studio 솔루션 탐색기에서 소스와 헤더를 분류하는 표시용 파일이다.
- `README.md`: 프로젝트 소개, 실행 방법, 주요 문서 링크를 제공한다.
- `assets/`: 이후 이미지, 사운드, 맵 데이터 같은 리소스를 넣을 수 있는 폴더다.
- `docs/`: 작업 현황판, 회의록, 협업 규칙, 코드 구조, 마일스톤 등 프로젝트 문서를 보관한다.

## 소스 파일

### `src/main.c`

프로그램의 시작점이다.

- 초기 화면, 설정 화면, 랭킹 화면, 게임 화면 전환
- 입력 읽기
- 게임 루프 실행
- `Esc` 일시정지 입력 처리
- 게임 종료 후 랭킹 저장 화면으로 넘기는 앱 흐름 관리
- 시작/설정/게임 화면별 배경음악 선택
- `--smoke-test` 실행 진입점

### `src/game_config.h`

게임 전역 설정 상수를 모아둔 헤더 파일이다.

- 맵 크기와 배열 최대치
- 5분 생존 목표 시간
- 회복의 붕대 활성 시간 같은 보상 밸런스

### `src/game.h`

게임 도메인에서 공유하는 타입과 함수 선언을 모아둔 헤더 파일이다.

- `Game`, `Player`, `Enemy`, `Weapon`, `Projectile`, `Pickup`
- 게임 모드, 난이도, 적 종류, 무기 종류, 사운드 이벤트
- 일시정지, 레벨업, 게임오버, 승리 상태 정의
- 게임 업데이트, 엔티티, 무기 함수 선언

### `src/game.c`

게임의 중심 규칙을 담당한다.

- 게임 초기화
- Easy/Hard 난이도별 기본 체력, 스폰 속도, 강한 적 등장 시점 설정
- 고정 크기 맵 타일과 벽 판정
- 5분 `GAME CLEAR`와 승리/패배 조건
- 일시정지 중 게임 진행 정지
- 레벨업 선택지 생성

### `src/entities.c`

플레이어, 적, pickup을 담당한다.

- 플레이어 이동
- 난이도별 적 생성과 거리 맵 기반 추적 이동
- 적 3종의 속도, 피해, 경험치, 점수 차별화
- 적 접촉 피해
- XP와 보물상자 pickup 생성과 획득 효과
- XP pickup 끌림, 적 처치 시 보너스 pickup 드롭
- 30초 박쥐 폭풍 미니 이벤트용 박쥐 소환

### `src/weapons.c`

무기와 전투 판정을 담당한다.

- 가장 가까운 적 자동 조준
- 마법탄 투사체 발사
- 신성 오라 범위 공격
- 관통창과 별탄막 자동 공격
- 투사체 이동과 적 충돌 처리

### `src/ranking.h`, `src/ranking.c`

랭킹 파일 I/O를 담당한다.

- `scores.txt`에서 상위 기록 읽기
- 게임 종료 기록 정렬과 저장
- 점수, 시간, 처치 수, 레벨, 결과 문자열 관리
- `RankingEntry`와 `RankingResult` 데이터 구조 소유

### `src/smoke_test.h`, `src/smoke_test.c`

간단한 회귀 검증 시나리오를 담당한다.

- 난이도 설정과 무기 수 검증
- 사운드/BGM 라우팅 검증
- 중간 시점 맵 전환 미발생, 5분 `GAME CLEAR`, 박쥐 이벤트 검증
- XP와 보물상자 pickup, 자동 공격, 일시정지 검증

### `src/ui.h`, `src/ui.c`

터미널 화면 표시를 담당한다.

- 초기 화면
- 설정 화면
- 랭킹 화면
- 게임 화면
- 5분 `GAME CLEAR` 오버레이와 플레이 상태 표시
- 체력바, 경험치바, 점수, 시간 표시
- HP 빨간색 바, XP 초록색 바 표시
- 색상과 문자 기반 디자인
- UI 이벤트별 사운드 요청 전달

### `src/platform.h`, `src/platform.c`

Windows 터미널 처리를 담당하는 레이어다.

- `conio.h`와 Windows Console API를 사용한다.
- 터미널 모드 진입과 복구
- 비차단 키 입력
- 시간 측정
- 프레임 대기
- Windows WinMM wav 효과음과 mp3 배경음악 재생

### `assets/audio`

Windows WinMM으로 재생하는 wav 효과음과 mp3 배경음악을 보관한다.

- UI 이동/확정
- 총쏘기, XP 획득, 보상/보물상자 획득
- 레벨업, 피격, 게임오버, 승리
- 메뉴/설정 배경음악, 게임 배경음악

## 주요 데이터 흐름

```txt
main.c
-> 입력 읽기
-> 화면별 배경음악 선택
-> GameUpdate()
   -> PlayerUpdate()
   -> EnemiesSpawnWave()
   -> WeaponsUpdate()
   -> ProjectilesUpdate()
   -> EnemiesUpdate()
   -> CombatResolve()
   -> PickupsUpdate()
-> UiDrawGame()
-> RankingLoad()/RankingAddAndSave()
```

## 빌드 결과물

- Windows Visual Studio: `build/Debug-x64/vampire-survivors-c.exe` 또는 `build/Release-x64/vampire-survivors-c.exe`

`build/` 폴더는 Git에 올리지 않는다.
