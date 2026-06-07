#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum CellColor {
    CELL_DIM = 0,
    CELL_WALL,
    CELL_PLAYER,
    CELL_ENEMY_LOW,
    CELL_ENEMY_MID,
    CELL_ENEMY_HIGH,
    CELL_PROJECTILE,
    CELL_XP,
    CELL_PICKUP_POWER,
    CELL_AURA,
    CELL_TEXT,
    CELL_BLOOD
} CellColor;

typedef struct Cell {
    char glyph;
    CellColor color;
} Cell;

static const char *ColorCode(CellColor color)
{
    switch (color) {
        case CELL_DIM:
            return "\033[2;37m";
        case CELL_WALL:
            return "\033[1;37m";
        case CELL_PLAYER:
            return "\033[1;36m";
        case CELL_ENEMY_LOW:
            return "\033[1;31m";
        case CELL_ENEMY_MID:
            return "\033[38;5;208m";
        case CELL_ENEMY_HIGH:
            return "\033[1;35m";
        case CELL_PROJECTILE:
            return "\033[1;33m";
        case CELL_XP:
            return "\033[1;32m";
        case CELL_PICKUP_POWER:
            return "\033[1;35m";
        case CELL_AURA:
            return "\033[1;34m";
        case CELL_TEXT:
            return "\033[1;37m";
        case CELL_BLOOD:
            return "\033[38;5;196m";
    }

    return "\033[0m";
}

void UiClearScreen(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* 게임 화면용: 커서 숨기고 맨 위로 이동 (깜빡임 최소화) */
static void BeginFrame(void)
{
    printf("\033[?25l");   /* 커서 숨기기 → 위아래 흔들림 방지 */
    printf("\033[H");      /* 커서를 맨 위로 */
}

/* 메뉴/랭킹용: 전체 화면 완전히 지운 뒤 그리기 (이전 화면 잔상 제거) */
static void BeginFrameFull(void)
{
    printf("\033[?25l");
    printf("\033[2J\033[H");  /* 화면 전체 지우기 + 맨 위로 */
}

static void BeginCleanFrame(void)
{
    printf("\033[H\033[2J");
}

static void EndFrame(void)
{
    printf("\033[0m\033[J");  /* 남은 내용 지우기 */
    printf("\033[?25h");      /* 커서 다시 보이기 */
    fflush(stdout);
}

static void DrawBar(const char *label, int current, int max, int width, const char *barColor)
{
    int filled;

    if (max <= 0) {
        max = 1;
    }

    filled = current * width / max;
    filled = GameClampInt(filled, 0, width);

    printf("%s%s\033[0m [", barColor, label);
    fputs(barColor, stdout);
    for (int i = 0; i < width; i++) {
        if (i == filled) {
            fputs("\033[2;37m", stdout);
        }
        putchar(i < filled ? '=' : ' ');
    }
    printf("\033[0m] %d/%d", current, max);
}

static void PutCell(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int x, int y, char glyph, CellColor color)
{
    if (x < 0 || x >= game->mapWidth || y < 0 || y >= game->mapHeight) {
        return;
    }

    grid[y][x].glyph = glyph;
    grid[y][x].color = color;
}

static void PutTextOnGrid(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int x, int y, const char *text, CellColor color)
{
    for (int i = 0; text[i] != '\0'; i++) {
        PutCell(game, grid, x + i, y, text[i], color);
    }
}

static void PickupAppearance(PickupType type, char *glyph, CellColor *color)
{
    *glyph = '+';
    *color = CELL_XP;

    if (type == PICKUP_TREASURE_CHEST) {
        *glyph = 'C';
        *color = CELL_PICKUP_POWER;
    }
}

/* 게임 상태를 문자/색상 그리드로 합성해 출력 순서를 한곳에서 통제 */
static void BuildGrid(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH])
{
    for (int y = 0; y < game->mapHeight; y++) {
        for (int x = 0; x < game->mapWidth; x++) {
            const char tile = GameMapTile(game, x, y);
            grid[y][x].glyph = tile == '.' ? ' ' : tile;
            grid[y][x].color = tile == '.' ? CELL_DIM : CELL_WALL;
        }
    }

    if (game->shieldBreakTimer > 0.0f) {
        /* 방패 폭발: 맵 전체 바닥 타일을 깜빡이는 * 로 덮음 */
        if (((int)(game->shieldBreakTimer * 8.0f) % 2) == 0) {
            for (int y = 0; y < game->mapHeight; y++) {
                for (int x = 0; x < game->mapWidth; x++) {
                    if (!GameMapIsBlocked(game, x, y)) {
                        PutCell(game, grid, x, y, '*', CELL_AURA);
                    }
                }
            }
        }
    }

    if (game->auraPulseTimer > 0.0f) {
        const int centerX = GameRound(game->player.position.x);
        const int centerY = GameRound(game->player.position.y);
        const int radius = game->weapons[WEAPON_HOLY_AURA].range;
        const int radiusSquared = radius * radius;

        for (int y = centerY - radius; y <= centerY + radius; y++) {
            for (int x = centerX - radius; x <= centerX + radius; x++) {
                const int dx = x - centerX;
                const int dy = y - centerY;
                if (dx * dx + dy * dy <= radiusSquared && !GameMapIsBlocked(game, x, y)) {
                    PutCell(game, grid, x, y, '~', CELL_AURA);
                }
            }
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *pickup = &game->pickups[i];
        if (pickup->active) {
            char glyph;
            CellColor color;

            PickupAppearance(pickup->type, &glyph, &color);
            PutCell(game, grid, GameRound(pickup->position.x), GameRound(pickup->position.y), glyph, color);
        }
    }

    /* 레이저 렌더링: 플레이어 기준 해당 방향만 'r'로 표시 */
    if (game->laser.active) {
        const int px = game->laser.playerX;
        const int py = game->laser.playerY;
        if (game->laser.goLeft) {
            for (int x = 1; x < px; x++)
                if (!GameMapIsBlocked(game, x, py))
                    PutCell(game, grid, x, py, 'r', CELL_PROJECTILE);
        }
        if (game->laser.goRight) {
            for (int x = px + 1; x < game->mapWidth - 1; x++)
                if (!GameMapIsBlocked(game, x, py))
                    PutCell(game, grid, x, py, 'r', CELL_PROJECTILE);
        }
        if (game->laser.goUp) {
            for (int y = 1; y < py; y++)
                if (!GameMapIsBlocked(game, px, y))
                    PutCell(game, grid, px, y, 'r', CELL_PROJECTILE);
        }
        if (game->laser.goDown) {
            for (int y = py + 1; y < game->mapHeight - 1; y++)
                if (!GameMapIsBlocked(game, px, y))
                    PutCell(game, grid, px, y, 'r', CELL_PROJECTILE);
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *projectile = &game->projectiles[i];
        if (projectile->active) {
            PutCell(game, grid, GameRound(projectile->position.x), GameRound(projectile->position.y), projectile->glyph,
                projectile->glyph == 'o' ? CELL_BLOOD : CELL_PROJECTILE);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *enemy = &game->enemies[i];
        CellColor color = CELL_ENEMY_LOW;

        if (!enemy->active) {
            continue;
        }

        if (enemy->type == ENEMY_THREE_HP) {
            color = CELL_ENEMY_MID;
        } else if (enemy->type == ENEMY_FORTY_HP) {
            color = CELL_ENEMY_HIGH;
        }

        PutCell(game, grid, GameRound(enemy->position.x), GameRound(enemy->position.y), enemy->glyph, color);
    }

    if (game->player.shieldTimer > 0.0f) {
        /* 방패 무적 중: 파란색으로 점멸 */
        if (((int)(game->player.shieldTimer * 4.0f) % 2) == 0) {
            PutCell(game, grid, GameRound(game->player.position.x), GameRound(game->player.position.y), '@', CELL_AURA);
        } else {
            PutCell(game, grid, GameRound(game->player.position.x), GameRound(game->player.position.y), '@', CELL_PLAYER);
        }
    } else if (game->player.invulnerableTimer <= 0.0f || ((int)(game->player.invulnerableTimer * 12.0f) % 2) == 0) {
        PutCell(game, grid, GameRound(game->player.position.x), GameRound(game->player.position.y), '@', CELL_PLAYER);
    }
}

static void DrawGridLine(const Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int y, int width)
{
    CellColor currentColor = CELL_TEXT;
    bool hasColor = false;

    for (int x = 0; x < width; x++) {
        if (!hasColor || grid[y][x].color != currentColor) {
            currentColor = grid[y][x].color;
            fputs(ColorCode(currentColor), stdout);
            hasColor = true;
        }
        putchar(grid[y][x].glyph);
    }

    printf("\033[0m\n");
}

/* 첫 화면: 뱀파이어 서바이버즈 스타일 타이틀 */
void UiDrawTitle(void)
{
    BeginCleanFrame();

    printf("\033[1;31m");
    printf("╔══════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                                              ║\n");
    printf("║                                                                                              ║\n");
    printf("║          V  A  M  P  I  R  E      S  U  R  V  I  V  O  R  S                                ║\n");
    printf("║                                                                                              ║\n");
    printf("║                      ~  T E R M I N A L   E D I T I O N  ~                                  ║\n");
    printf("║                                                                                              ║\n");
    printf("║                                                                                              ║\n");
    printf("║  ┌─────────────────────┐  ┌──────────────────────────────────────┐  ┌────────────────────┐  ║\n");
    printf("║  │                     │  │                                      │  │                    │  ║\n");
    printf("║  │    [ 이  동 ]       │  │          [ 게   임 ]                 │  │    [ 기  타 ]      │  ║\n");
    printf("║  │                     │  │                                      │  │                    │  ║\n");
    printf("║  │  WASD / 방향키      │  │  5분동안 몬스터를 피해 생존하시오    │  │  R  랭킹 / 재시작  │  ║\n");
    printf("║  │                     │  │                                      │  │  Q  게임 종료      │  ║\n");
    printf("║  └─────────────────────┘  └──────────────────────────────────────┘  └────────────────────┘  ║\n");
    printf("║                                                                                              ║\n");
    printf("║                                                                                              ║\n");
    printf("║                           ▶   S  시  작  하  기   ◀                                         ║\n");
    printf("║                          ───────────────────────────                                        ║\n");
    printf("║                                                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\033[0m");

    EndFrame();
}

void UiDrawSetup(GameDifficulty selectedDifficulty)
{
    BeginCleanFrame();
    printf("\033[1;31m============================== SETUP ================================\033[0m\n\n");
    printf("게임 시작 전 난이도를 선택하세요.\n\n");
    printf("%s[1] 이지\033[0m  HP 14, 느린 웨이브, 늦은 흡혈귀\n",
        selectedDifficulty == DIFFICULTY_EASY ? "\033[1;32m> " : "  ");
    printf("%s[2] 하드\033[0m  HP 10, 빠른 웨이브, 빠른 흡혈귀\n\n",
        selectedDifficulty == DIFFICULTY_HARD ? "\033[1;31m> " : "  ");
    printf("현재: \033[1;33m%s\033[0m\n\n", GameDifficultyName(selectedDifficulty));
    printf("방향키 또는 1/2로 변경  Enter 시작  B/Esc 뒤로\n");
    EndFrame();
}

/* 저장된 랭킹을 터미널 표 형태로 출력 */
void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count)
{
    BeginCleanFrame();
    printf("\033[1;33m============================== RANKING ==============================\033[0m\n\n");

    if (count == 0) {
        printf("기록된 게임이 없습니다.\n");
    } else {
        printf(" #  %-15s  %-4s  %6s  %5s  %5s  %4s\n",
            "이름", "결과", "점수", "시간", "처치", "레벨");
        printf(" -- --------------- ---- ------- ------ ------ ----\n");
        for (int i = 0; i < count; i++) {
            printf(" %d  %-15s  %-4s  %6d  %02d:%02d  %5d  %4d\n",
                i + 1,
                entries[i].name,
                entries[i].result,
                entries[i].score,
                entries[i].seconds / 60,
                entries[i].seconds % 60,
                entries[i].kills,
                entries[i].level);
        }
    }

    printf("\nB/Esc 뒤로  S/Enter 게임시작  Q 종료\n");
    EndFrame();
}

/* 게임 종료 후 랭킹 저장 직전 이름 입력 화면 */
void UiDrawNameInput(const Game *game, const char *name, int nameLen)
{
    const int seconds = (int)game->elapsed;
    const char *resultLabel = game->mode == GAME_MODE_VICTORY
        ? "\033[1;32mGAME CLEAR\033[0m"
        : "\033[1;31mGAME OVER\033[0m";

    BeginFrameFull();
    printf("\033[1;33m============================== RANKING ==============================\033[0m\n\n");
    printf("%s\n\n", resultLabel);
    printf("  Score : %d\n", game->player.score + seconds);
    printf("  Time  : %02d:%02d\n", seconds / 60, seconds % 60);
    printf("  Kills : %d\n", game->player.kills);
    printf("  Level : %d\n\n", game->player.level);
    printf("\033[1;37m닉네임을 입력하세요 (%d/%d자):\033[0m\n\n", nameLen, MAX_NAME_LEN);
    printf("  \033[1;36m");
    if (nameLen > 0) {
        printf("%s", name);
    }
    printf("_\033[0m\n\n");
    printf("영문, 숫자, -, _ 사용 가능\n");
    printf("\033[1;33mEnter\033[0m 저장  \033[1;33mEsc\033[0m 건너뛰기\n");
    EndFrame();
}

/* 실제 플레이 화면 전체를 그리는 메인 렌더 함수 */
void UiDrawGame(const Game *game)
{
    Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const int seconds = (int)game->elapsed;
    const int remaining = GameClampInt((int)SURVIVAL_SECONDS - seconds, 0, (int)SURVIVAL_SECONDS);

    BuildGrid(game, grid);
    if (game->mode == GAME_MODE_VICTORY) {
        static const char *clearText = "GAME CLEAR";
        PutTextOnGrid(game,
            grid,
            (game->mapWidth - (int)strlen(clearText)) / 2,
            game->mapHeight / 2,
            clearText,
            CELL_TEXT);
    }
    BeginFrame();

    /* 줄1: 타이틀 + 난이도 + 경과 시간 + 남은 시간 + 킬 + 점수 */
    printf("\033[1;36mTerminal Survivors\033[0m  ");
    printf("[%s]  ", GameDifficultyName(game->difficulty));
    printf("Time %02d:%02d  ", seconds / 60, seconds % 60);
    printf("Remain %02d:%02d  ", remaining / 60, remaining % 60);
    printf("Kills %d  Score %d\033[K\n", game->player.kills, game->player.score);

    /* 줄2: HP 바 + XP 바 + LV */
    DrawBar("HP", game->player.health, game->player.maxHealth, 18, "\033[1;31m");
    printf("   ");
    DrawBar("XP", game->player.xp, game->player.xpToNextLevel, 18, "\033[1;32m");
    printf("   LV %d\033[K\n", game->player.level);

    /* 줄3: 무기 정보 */
    {
        static const char *weaponNames[WEAPON_COUNT] = {
            "원형(○)", "삼각형(△)", "사각형(□)", "별(★)"
        };
        const Weapon *aw = &game->weapons[game->activeWeapon];

        printf("\033[1;33m무기\033[0m: %s  Lv%d  Dmg%d  CD%.2fs",
            weaponNames[game->activeWeapon], aw->level, aw->damage, aw->cooldown);

        if (game->player.attackSpeedMult > 1.0f) {
            printf("  \033[1;33m공격속도x%.1f\033[0m", game->player.attackSpeedMult);
        }
        if (game->player.moveSpeedMult > 1.0f) {
            printf("  \033[1;32m이동속도x%.1f\033[0m", game->player.moveSpeedMult);
        }
        if (game->player.hpRecoveryLevel > 0 && game->player.hpRecoveryTimer > 0.0f) {
            printf("  \033[1;31m[붕대 %d/20킬 %.0fs]\033[0m",
                game->player.bandageKillCount, game->player.hpRecoveryTimer);
        }
        if (game->player.shieldTimer > 0.0f) {
            printf("  \033[1;34mSHIELD %d회 남음 / %.0fs\033[0m",
                game->player.shieldHits, game->player.shieldTimer);
        }
        printf("\033[K\n");

        /* Legend */
        printf("\033[1;36m@\033[0m you  \033[1;31mb\033[0m 박쥐  "
               "\033[38;5;208mG\033[0m 좀비  \033[1;35mV\033[0m 뱀파이어  "
               "\033[38;5;196mo\033[0m 원형  \033[1;33m^\033[0m 삼각형  "
               "\033[1;33m+\033[0m 사각형  \033[1;33m*\033[0m 별  "
               "\033[1;33mr\033[0m 레이저  \033[1;33m$\033[0m 보물상자  "
               "\033[1;34m@\033[0m 방패  \033[1;32m+\033[0m XP\033[K\n\n");
    }

    for (int y = 0; y < game->mapHeight; y++) {
        DrawGridLine(grid, y, game->mapWidth);
    }

    /* 경고 문구: 항상 1줄 차지 */
    if (game->speedWarningTimer > 0.0f && ((int)(game->speedWarningTimer / 0.7f) % 2) == 0) {
        printf("\033[1;31m  !! 몬스터가 더 강력해집니다! !!\033[0m\033[K\n");
    } else if (game->miniEventMessageTimer > 0.0f && game->activeMiniEvent == MINI_EVENT_BAT_STORM) {
        printf("\033[1;31m  MINI EVENT: 박쥐 폭풍! 약한 적이 몰려옵니다.\033[0m\033[K\n");
    } else {
        printf("\033[K\n");
    }

    /* 모드별 하단 표시 */
    if (game->mode == GAME_MODE_LEVEL_UP) {
        int i;
        printf("\033[1;33m  ========== LEVEL UP! ==========\033[0m\033[K\n");
        for (i = 0; i < UPGRADE_CHOICES; i++) {
            const bool sel = (game->selectedUpgrade == i);
            printf("  %s\033[1;37m[%d]\033[0m ",
                sel ? "\033[1;36m>\033[0m" : " ",
                i + 1);
            if (sel) { printf("\033[1;36m"); }
            printf("%-22s\033[0m\033[K\n", game->upgrades[i].name);
            printf("       \033[2;37m%s\033[0m\033[K\n", game->upgrades[i].description);
        }
        printf("  \033[1;33m================================\033[0m\033[K\n");
        printf("  1~3 키 또는 <-> 방향키로 선택, Enter 확인\033[K\n");
    } else if (game->mode == GAME_MODE_PAUSED) {
        printf("\033[1;33mPAUSED\033[0m  Esc 재개  Q 게임종료\033[K\n");
    } else if (game->mode == GAME_MODE_GAME_OVER) {
        printf("\033[1;31mGAME OVER\033[0m  HP가 0이 됐습니다.  R 재시작  B/Esc 타이틀\033[K\n");
    } else if (game->mode == GAME_MODE_VICTORY) {
        printf("\033[1;32mGAME CLEAR\033[0m  5분 생존 성공!  Enter/R 랭킹 등록  B/Esc 타이틀\033[K\n");
    } else {
        printf("이동으로 회피하세요. 공격은 자동입니다.  Esc 일시정지  Q 게임종료  M 사운드\033[K\n");
    }

    EndFrame();
}

/* 여러 사운드 플래그 중 이번 프레임에 재생할 대표 효과음을 고른다 */
PlatformSound UiSoundEvent(unsigned int flags)
{
    if ((flags & SOUND_UI_MOVE) != 0u) {
        return PLATFORM_SOUND_UI_MOVE;
    }
    if ((flags & SOUND_UI_CONFIRM) != 0u) {
        return PLATFORM_SOUND_UI_CONFIRM;
    }
    if ((flags & SOUND_SHOOT) != 0u) {
        return PLATFORM_SOUND_SHOOT;
    }
    if ((flags & SOUND_XP_PICKUP) != 0u) {
        return PLATFORM_SOUND_XP_PICKUP;
    }
    if ((flags & SOUND_HEAL_PICKUP) != 0u) {
        return PLATFORM_SOUND_HEAL_PICKUP;
    }
    if ((flags & SOUND_POWER_PICKUP) != 0u) {
        return PLATFORM_SOUND_POWER_PICKUP;
    }
    if ((flags & SOUND_LEVEL_UP) != 0u) {
        return PLATFORM_SOUND_LEVEL_UP;
    }
    if ((flags & SOUND_HIT) != 0u) {
        return PLATFORM_SOUND_HIT;
    }
    if ((flags & SOUND_GAME_OVER) != 0u) {
        return PLATFORM_SOUND_GAME_OVER;
    }
    if ((flags & SOUND_VICTORY) != 0u) {
        return PLATFORM_SOUND_VICTORY;
    }

    return PLATFORM_SOUND_NONE;
}

/* UI/게임 로직에서 쌓은 사운드 요청을 플랫폼 레이어로 넘긴다 */
void UiPlaySounds(unsigned int flags, bool enabled)
{
    const PlatformSound sound = UiSoundEvent(flags);

    if (!enabled || sound == PLATFORM_SOUND_NONE) {
        return;
    }

    PlatformPlaySound(sound);
}
