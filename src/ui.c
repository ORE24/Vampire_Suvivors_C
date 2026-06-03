#include "game.h"

#include <stdio.h>
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
    CELL_AURA,
    CELL_TEXT
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
        case CELL_AURA:
            return "\033[1;34m";
        case CELL_TEXT:
            return "\033[1;37m";
    }

    return "\033[0m";
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

static void BuildGrid(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH])
{
    for (int y = 0; y < game->mapHeight; y++) {
        for (int x = 0; x < game->mapWidth; x++) {
            const char tile = GameMapTile(game, x, y);
            grid[y][x].glyph = tile == '.' ? ' ' : tile;
            grid[y][x].color = tile == '.' ? CELL_DIM : CELL_WALL;
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
            PutCell(game, grid, GameRound(pickup->position.x), GameRound(pickup->position.y), '+', CELL_XP);
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *projectile = &game->projectiles[i];
        if (projectile->active) {
            PutCell(game, grid, GameRound(projectile->position.x), GameRound(projectile->position.y), projectile->glyph, CELL_PROJECTILE);
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

void UiDrawTitle(void)
{
    BeginFrameFull();
    printf("\033[1;36m");
    printf("======================================================================\n");
    printf("                    TERMINAL SURVIVORS: CRYPT MVP                    \n");
    printf("======================================================================\n");
    printf("\033[0m\n");
    printf("A C terminal survival game. No raylib, no graphics window.\n\n");
    printf("\033[1;37mGoal\033[0m\n");
    printf("  Survive for 10 minutes. If HP reaches 0, the run ends.\n\n");
    printf("\033[1;37mControls\033[0m\n");
    printf("  WASD / Arrow keys : move\n");
    printf("  1, 2, 3 or Enter  : choose level-up upgrade\n");
    printf("  1 / 2             : choose Easy / Hard on setup\n");
    printf("  R                 : ranking on this screen, restart after run\n");
    printf("  Esc               : pause/resume during a run\n");
    printf("  M                 : toggle terminal bell sound\n");
    printf("  Q                 : quit or end current run\n");
    printf("  Esc on title      : quit\n\n");
    printf("\033[1;33mPress S or Enter to open setup. Press R for rankings.\033[0m\n");
    EndFrame();
}

void UiDrawSetup(GameDifficulty selectedDifficulty)
{
    BeginFrameFull();
    printf("\033[1;36m============================== SETUP ================================\033[0m\n\n");
    printf("Choose difficulty before starting the run.\n\n");
    printf("%s[1] Easy\033[0m  HP 14, slower waves, late vampires\n",
        selectedDifficulty == DIFFICULTY_EASY ? "\033[1;32m> " : "  ");
    printf("%s[2] Hard\033[0m  HP 10, faster waves, early vampires\n\n",
        selectedDifficulty == DIFFICULTY_HARD ? "\033[1;31m> " : "  ");
    printf("Current: \033[1;33m%s\033[0m\n\n", GameDifficultyName(selectedDifficulty));
    printf("Use arrow keys or 1/2 to change. Enter starts. B/Esc returns.\n");
    EndFrame();
}

void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count)
{
    BeginFrameFull();
    printf("\033[1;33m============================== RANKING ==============================\033[0m\n\n");

    if (count == 0) {
        printf("No recorded runs yet.\n");
    } else {
        printf(" #  Result  Score   Time   Kills  Level\n");
        printf(" -- ------- ------- ------ ------ -----\n");
        for (int i = 0; i < count; i++) {
            printf(" %d  %-6s  %6d  %02d:%02d  %5d  %5d\n",
                i + 1,
                entries[i].result,
                entries[i].score,
                entries[i].seconds / 60,
                entries[i].seconds % 60,
                entries[i].kills,
                entries[i].level);
        }
    }

    printf("\nPress B/Esc to go back, S/Enter for setup, Q to quit.\n");
    EndFrame();
}

void UiDrawNameInput(const Game *game, const char *name, int nameLen)
{
    const int seconds = (int)game->elapsed;
    const char *resultLabel = game->mode == GAME_MODE_VICTORY
        ? "\033[1;32mVICTORY\033[0m"
        : "\033[1;31mGAME OVER\033[0m";

    BeginFrameFull();
    printf("\033[1;33m============================== RANKING ==============================\033[0m\n\n");
    printf("%s\n\n", resultLabel);
    printf("  Score : %d\n", game->player.score + seconds);
    printf("  Time  : %02d:%02d\n", seconds / 60, seconds % 60);
    printf("  Kills : %d\n", game->player.kills);
    printf("  Level : %d\n\n", game->player.level);
    printf("\033[1;37mEnter your name (%d/%d chars):\033[0m\n\n", nameLen, MAX_NAME_LEN);
    printf("  \033[1;36m");
    if (nameLen > 0) {
        printf("%s", name);
    }
    printf("_\033[0m\n\n");
    printf("Letters, numbers, - and _ allowed.\n");
    printf("Press \033[1;33mEnter\033[0m to save. Press \033[1;33mEsc\033[0m to skip.\n");
    EndFrame();
}


void UiDrawGame(const Game *game)
{
    Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const int seconds = (int)game->elapsed;
    /* #3: 다음 웨이브까지 남은 시간 */
    float nextWave = game->spawnInterval - game->spawnTimer;
    if (nextWave < 0.0f) nextWave = 0.0f;

    BuildGrid(game, grid);
    BeginFrame();

    /* #5: 현재 웨이브 (1분마다 증가, 최대 7) */
    {
        int wave = seconds / 60 + 1;
        if (wave > 7) wave = 7;

        /* 줄1: 헤더 */
        printf("\033[1;36mTerminal Survivors\033[0m  ");
        printf("Mode %s  ", GameDifficultyName(game->difficulty));
        printf("Time %02d:%02d  ", seconds / 60, seconds % 60);
        printf("\033[1;33mWave %d/7\033[0m  ", wave);
        printf("다음 스폰 %.1f초  ", nextWave);
        printf("Kills %d  Score %d\033[K\n", game->player.kills, game->player.score);
    }

    /* 줄2: HP 바 넓게 + LV (#1) */
    DrawBar("HP", game->player.health, game->player.maxHealth, 30, "\033[1;31m");
    printf("   \033[1;37mLV %d\033[0m\033[K\n", game->player.level);

    /* 줄3: XP 바 넓게 (#1) */
    DrawBar("XP", game->player.xp, game->player.xpToNextLevel, 30, "\033[1;32m");
    printf("\033[K\n");

    /* 줄4: 무기 정보 + 특징 설명 (#5) */
    {
        static const char *weaponNames[WEAPON_COUNT] = {
            "원형(o)", "삼각형(^)", "사각형(+)", "별(*)"
        };
        static const char *weaponDescs[WEAPON_COUNT] = {
            "가장 가까운 적 조준 1발",
            "가장 가까운 적 방향 3발 부채꼴",
            "상하좌우 4방향 동시 발사",
            "가장 가까운 적 2명 강타 2발"
        };
        const Weapon *aw = &game->weapons[game->activeWeapon];

        /* #5: Dmg/CD 제거, 공격력/공격속도/이동속도 항상 표시 */
        printf("\033[1;33m무기\033[0m: %s | %s | Lv%d",
            weaponNames[game->activeWeapon],
            weaponDescs[game->activeWeapon],
            aw->level);
        printf("  공격력 x%d", aw->damage);
        printf("  \033[1;33m공격속도 x%.2f\033[0m", game->player.attackSpeedMult);
        printf("  \033[1;32m이동속도 x%.2f\033[0m", game->player.moveSpeedMult);
        if (game->player.hpRecoveryLevel > 0) {
            printf("  \033[1;31m붕대Lv%d\033[0m", game->player.hpRecoveryLevel);
        }
        if (game->player.shieldLevel > 0) {
            if (game->player.shieldTimer > 0.0f) {
                printf("  \033[1;34mSHIELD %.0fs\033[0m", game->player.shieldTimer);
            } else {
                printf("  \033[2;34m방패%.0fs후\033[0m", game->player.shieldCooldown);
            }
        }
        printf("\033[K\n");

        /* 줄5: Legend */
        printf("Legend: \033[1;36m@\033[0m you  \033[1;31mb\033[0m 박쥐  "
               "\033[38;5;208mG\033[0m 좀비  \033[1;35mV\033[0m 뱀파이어  "
               "\033[1;33mo\033[0m 원형  \033[1;33m^\033[0m 삼각형  "
               "\033[1;33m+\033[0m 사각형  \033[1;33m*\033[0m 별  "
               "\033[1;34m@\033[0m 방패무적  \033[1;32m+\033[0m XP\033[K\n\n");
    }

    for (int y = 0; y < game->mapHeight; y++) {
        DrawGridLine(grid, y, game->mapWidth);
    }

    /* 모드별 하단 표시 (#5: 레벨업 세로 배치) */
    if (game->mode == GAME_MODE_LEVEL_UP) {
        int i;
        printf("\n\033[1;33m  ========== LEVEL UP! ==========\033[0m\n");
        for (i = 0; i < UPGRADE_CHOICES; i++) {
            const bool sel = (game->selectedUpgrade == i);
            printf("  %s\033[1;37m[%d]\033[0m ",
                sel ? "\033[1;36m>\033[0m" : " ",
                i + 1);
            if (sel) { printf("\033[1;36m"); }
            printf("%-22s\033[0m\n", game->upgrades[i].name);
            printf("       \033[2;37m%s\033[0m\033[K\n", game->upgrades[i].description);
        }
        printf("  \033[1;33m================================\033[0m\n");
        printf("  1~3 키 또는 <-> 방향키로 선택, Enter 확인\033[K\n");
    } else if (game->mode == GAME_MODE_PAUSED) {
        printf("\n\033[1;33mPAUSED\033[0m  Esc 재개 / Q 종료\033[K\n");
    } else if (game->mode == GAME_MODE_GAME_OVER) {
        printf("\n\033[1;31mGAME OVER\033[0m  HP 0. R 재시작 / B,Esc 타이틀\033[K\n");
    } else if (game->mode == GAME_MODE_VICTORY) {
        printf("\n\033[1;32mVICTORY\033[0m  생존 성공! R 재시작 / B,Esc 타이틀\033[K\n");
    } else {
        printf("\nWASD/방향키 이동 | 공격 자동 | Esc 일시정지 | Q 종료 | M 음소거\033[K\n");
    }

    EndFrame();
}

void UiPlaySounds(unsigned int flags, bool enabled)
{
    int count = 0;

    if (!enabled || flags == 0u) {
        return;
    }

    if ((flags & SOUND_ATTACK) != 0u) {
        count += 1;
    }
    if ((flags & SOUND_XP) != 0u) {
        count += 2;
    }
    if ((flags & SOUND_LEVEL_UP) != 0u) {
        count += 3;
    }
    if ((flags & SOUND_HIT) != 0u) {
        count += 2;
    }
    if ((flags & SOUND_GAME_OVER) != 0u || (flags & SOUND_VICTORY) != 0u) {
        count += 4;
    }

    if (count > 5) {
        count = 5;
    }

    for (int i = 0; i < count; i++) {
        putchar('\a');
    }
    fflush(stdout);
}
