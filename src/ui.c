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
    printf("터미널 생존 게임입니다.\n\n");
    printf("\033[1;37m목표\033[0m\n");
    printf("  15분을 생존하세요. HP가 0이 되면 게임이 종료됩니다.\n\n");
    printf("\033[1;37m조작\033[0m\n");
    printf("  WASD / 방향키       : 이동\n");
    printf("  1, 2, 3 또는 Enter  : 레벨업 보상 선택\n");
    printf("  1 / 2               : 난이도 선택\n");
    printf("  R                   : 랭킹 / 재시작\n");
    printf("  Esc                 : 일시정지 / 재개\n");
    printf("  M                   : 사운드 토글\n");
    printf("  Q                   : 종료\n");
    printf("  타이틀에서 Esc      : 게임 종료\n\n");
    printf("\033[1;33mS 또는 Enter 게임시작  R 랭킹\033[0m\n");
    EndFrame();
}

void UiDrawSetup(GameDifficulty selectedDifficulty)
{
    BeginFrameFull();
    printf("\033[1;36m============================== SETUP ================================\033[0m\n\n");
    printf("게임 시작 전 난이도를 선택하세요.\n\n");
    printf("%s[1] 이지\033[0m  HP 14, 느린 웨이브, 늦은 흡혈귀\n",
        selectedDifficulty == DIFFICULTY_EASY ? "\033[1;32m> " : "  ");
    printf("%s[2] 하드\033[0m  HP 10, 빠른 웨이브, 빠른 흡혈귀\n\n",
        selectedDifficulty == DIFFICULTY_HARD ? "\033[1;31m> " : "  ");
    printf("현재: \033[1;33m%s\033[0m\n\n", GameDifficultyName(selectedDifficulty));
    printf("방향키 또는 1/2로 변경  Enter 시작  B/Esc 뒤로\n");
    EndFrame();
}

void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count)
{
    BeginFrameFull();
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


void UiDrawGame(const Game *game)
{
    Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const int seconds = (int)game->elapsed;
    const int remaining = (int)SURVIVAL_SECONDS - seconds;

    BuildGrid(game, grid);
    BeginFrame();

    printf("\033[1;36mTerminal Survivors\033[0m  ");
    printf("Mode %s  ", GameDifficultyName(game->difficulty));
    printf("Time %02d:%02d / 10:00  ", seconds / 60, seconds % 60);
    printf("Remain %02d:%02d  ", remaining / 60, remaining % 60);
    printf("Kills %d  Score %d\n", game->player.kills, game->player.score);

    DrawBar("HP", game->player.health, game->player.maxHealth, 18, "\033[1;31m");
    printf("   ");
    DrawBar("XP", game->player.xp, game->player.xpToNextLevel, 18, "\033[1;32m");
    printf("   LV %d\n", game->player.level);

    {
        static const char *weaponNames[WEAPON_COUNT] = {
            "원형(○)", "삼각형(△)", "사각형(□)", "별(★)"
        };
        static const char *weaponGlyphs[WEAPON_COUNT] = {"o", "^", "+", "*"};
        const Weapon *aw = &game->weapons[game->activeWeapon];

        printf("\033[1;33m무기\033[0m: %s  Lv%d  Dmg%d  CD%.2fs",
            weaponNames[game->activeWeapon], aw->level, aw->damage,
            aw->cooldown);

        /* 속도 버프 표시 */
        if (game->player.attackSpeedMult > 1.0f) {
            printf("  \033[1;33m공격속도x%.1f\033[0m", game->player.attackSpeedMult);
        }
        if (game->player.moveSpeedMult > 1.0f) {
            printf("  \033[1;32m이동속도x%.1f\033[0m", game->player.moveSpeedMult);
        }
        /* 패시브 아이템 표시 */
        if (game->player.hpRecoveryLevel > 0) {
            printf("  \033[1;31m❤Lv%d\033[0m", game->player.hpRecoveryLevel);
        }
        if (game->player.shieldLevel > 0) {
            if (game->player.shieldTimer > 0.0f) {
                printf("  \033[1;34mSHIELD %.0fs\033[0m", game->player.shieldTimer);
            } else {
                printf("  \033[2;34m방패%.0fs후\033[0m", game->player.shieldCooldown);
            }
        }
        printf("\033[K\n");
        printf("Legend: \033[1;36m@\033[0m you  \033[1;31mb\033[0m 박쥐  "
               "\033[38;5;208mG\033[0m 좀비  \033[1;35mV\033[0m 뱀파이어  "
               "\033[1;33mo\033[0m 원형  \033[1;33m^\033[0m 삼각형  "
               "\033[1;33m+\033[0m 사각형  \033[1;33m*\033[0m 별  "
               "\033[1;34m@\033[0m 방패무적  \033[1;32m+\033[0m XP\n\n");
        (void)weaponGlyphs;
    }

    for (int y = 0; y < game->mapHeight; y++) {
        DrawGridLine(grid, y, game->mapWidth);
    }

    if (game->mode == GAME_MODE_LEVEL_UP) {
        printf("\n\033[1;33mLEVEL UP\033[0m  "
               "%s[1] %-16s\033[0m  %s[2] %-16s\033[0m  %s[3] %s\033[0m\033[K\n",
            game->selectedUpgrade == 0 ? "\033[1;36m>" : " ",
            game->upgrades[0].name,
            game->selectedUpgrade == 1 ? "\033[1;36m>" : " ",
            game->upgrades[1].name,
            game->selectedUpgrade == 2 ? "\033[1;36m>" : " ",
            game->upgrades[2].name);
        printf("\033[0;36m→ %s\033[0m\033[K\n",
            game->upgrades[game->selectedUpgrade].description);
    } else if (game->mode == GAME_MODE_PAUSED) {
        printf("\n\033[1;33mPAUSED\033[0m  Esc 재개  Q 게임종료\033[K\n");
    } else if (game->mode == GAME_MODE_GAME_OVER) {
        printf("\n\033[1;31mGAME OVER\033[0m  HP가 0이 됐습니다.  R 재시작  B/Esc 타이틀\033[K\n");
    } else if (game->mode == GAME_MODE_VICTORY) {
        printf("\n\033[1;32mVICTORY\033[0m  15분을 생존했습니다!  R 재시작  B/Esc 타이틀\033[K\n");
    } else {
        printf("\n이동으로 회피하세요. 공격은 자동입니다.  Esc 일시정지  Q 게임종료  M 사운드\033[K\n");
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
