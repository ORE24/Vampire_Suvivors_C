#include "game.h"
#include "platform.h"

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
    CELL_PICKUP_HEAL,
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
        case CELL_PICKUP_HEAL:
            return "\033[1;31m";
        case CELL_PICKUP_POWER:
            return "\033[1;35m";
        case CELL_AURA:
            return "\033[1;34m";
        case CELL_BOSS:
            return "\033[1;35m";
        case CELL_BOSS_TRUE:
            return "\033[1;33m";
        case CELL_SHADOW:
            return "\033[2;36m";
        case CELL_PUZZLE:
            return "\033[1;32m";
        case CELL_TEXT:
            return "\033[1;37m";
        case CELL_BLOOD:
            return "\033[38;5;196m";
    }

    return "\033[0m";
}

/* 게임 화면용: 커서 숨기고 맨 위로 이동 (깜빡임 최소화) */
static void BeginFrame(void)
{
    printf("\033[?25l");   /* 커서 숨기기 → 위아래 흔들림 방지 */
    printf("\033[H");      /* 커서를 맨 위로 */
    printf("\033[J");      /* 이번 프레임보다 긴 이전 텍스트 잔상 제거 */
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

static void PickupAppearance(PickupType type, char *glyph, CellColor *color)
{
    *glyph = '+';
    *color = CELL_XP;

    if (type == PICKUP_HEAL_PACK) {
        *glyph = 'H';
        *color = CELL_PICKUP_HEAL;
    } else if (type == PICKUP_VACUUM) {
        *glyph = 'M';
        *color = CELL_PICKUP_POWER;
    } else if (type == PICKUP_ROSARY) {
        *glyph = 'R';
        *color = CELL_PICKUP_POWER;
    } else if (type == PICKUP_GAMBLER_DICE) {
        *glyph = '?';
        *color = CELL_PICKUP_POWER;
    } else if (type == PICKUP_FRENZY_MAGAZINE) {
        *glyph = 'B';
        *color = CELL_PICKUP_POWER;
    } else if (type == PICKUP_PINATA_SKULL) {
        *glyph = 'C';
        *color = CELL_PICKUP_POWER;
    }
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

    if (game->bossAltarsActive) {
        for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
            PutCell(game, grid,
                GameRound(game->bossAltars[i].x),
                GameRound(game->bossAltars[i].y),
                game->bossAltarGlyphs[i],
                CELL_PUZZLE);
        }
    }

    if (game->bossPuzzle == BOSS_PUZZLE_ORB) {
        PutCell(game, grid,
            GameRound(game->bossCentralAltar.x),
            GameRound(game->bossCentralAltar.y),
            'A',
            CELL_PUZZLE);
        if (game->bossOrbActive || game->bossOrbCarried) {
            PutCell(game, grid,
                GameRound(game->bossOrbPosition.x),
                GameRound(game->bossOrbPosition.y),
                'O',
                CELL_BOSS_TRUE);
        }
    }

    if (game->bossSealAltarsActive) {
        for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
            PutCell(game, grid,
                GameRound(game->bossSealAltars[i].x),
                GameRound(game->bossSealAltars[i].y),
                'A',
                CELL_PUZZLE);
        }
    }

    if (game->bossShadowActive) {
        PutCell(game, grid,
            GameRound(game->bossShadowPosition.x),
            GameRound(game->bossShadowPosition.y),
            '&',
            CELL_SHADOW);
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

    for (int i = 0; i < game->bossCount && i < MAX_BOSSES; i++) {
        const Boss *boss = &game->bosses[i];
        CellColor color = CELL_BOSS;
        char glyph;

        if (!boss->active) {
            continue;
        }

        glyph = boss->glyph;
        if ((game->bossStatus == BOSS_STATUS_SHIELDED ||
             game->bossStatus == BOSS_STATUS_CLONES) &&
            i != game->trueBossIndex) {
            color = CELL_BOSS;
        } else if (i == game->trueBossIndex ||
                   game->bossStatus == BOSS_STATUS_FINAL_DAMAGE ||
                   game->bossStatus == BOSS_STATUS_VULNERABLE) {
            color = CELL_BOSS_TRUE;
        }
        if (i == game->trueBossIndex &&
            game->bossFlashTimer > 0.0f &&
            ((int)(game->bossFlashTimer * 8.0f) % 2) == 0) {
            glyph = '!';
            color = CELL_PROJECTILE;
        }
        if (game->bossStatus == BOSS_STATUS_SHIELDED) {
            color = CELL_AURA;
        }

        PutCell(game, grid, GameRound(boss->position.x), GameRound(boss->position.y), glyph, color);
    }

    if (game->activeMiniEvent == MINI_EVENT_BLOOD_MIST) {
        const int playerX = GameRound(game->player.position.x);
        const int playerY = GameRound(game->player.position.y);

        for (int y = 1; y < game->mapHeight - 1; y++) {
            for (int x = 1; x < game->mapWidth - 1; x++) {
                if (abs(x - playerX) <= 1 && abs(y - playerY) <= 1) {
                    continue;
                }
                if (((x * 17 + y * 31 + (int)(game->elapsed * 10.0f)) % 11) < 4) {
                    PutCell(game, grid, x, y, '~', CELL_ENEMY_HIGH);
                }
            }
        }
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
    BeginCleanFrame();
    printf("\033[1;36m");
    printf("======================================================================\n");
    printf("                    TERMINAL SURVIVORS: CRYPT MVP                    \n");
    printf("======================================================================\n");
    printf("\033[0m\n");
    printf("터미널 생존 게임입니다.\n\n");
    printf("\033[1;37m목표\033[0m\n");
    printf("  3~5분 묘지 생존 뒤 시작맵 지형의 보스전을 돌파하고 최종 보스를 처치하세요.\n\n");
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
    BeginCleanFrame();
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

static const char *BossPhaseText(BossPhase phase)
{
    switch (phase) {
        case BOSS_PHASE_ONE:
            return "1페이즈";
        case BOSS_PHASE_TWO:
            return "2페이즈";
        case BOSS_PHASE_NONE:
        default:
            return "없음";
    }
}

static const char *BossStatusText(BossStatus status)
{
    switch (status) {
        case BOSS_STATUS_SHIELDED:
            return "보호막";
        case BOSS_STATUS_VULNERABLE:
            return "공격 가능";
        case BOSS_STATUS_CLONES:
            return "분신";
        case BOSS_STATUS_FINAL_DAMAGE:
            return "최종 딜타임";
        case BOSS_STATUS_DEFEATED:
            return "처치됨";
        case BOSS_STATUS_NONE:
        default:
            return "대기";
    }
}

static void PrintBossSequenceGoal(const Game *game)
{
    printf("목표: ");
    for (int i = 0; i < BOSS_SEQUENCE_LEN; i++) {
        const int altarIndex = game->bossSequence[i];
        putchar(game->bossAltarGlyphs[altarIndex]);
        if (i + 1 < BOSS_SEQUENCE_LEN) {
            putchar(' ');
        }
    }
    printf(" 순서대로 제단을 밟으세요\033[K\n");

    printf("입력: ");
    for (int i = 0; i < BOSS_SEQUENCE_LEN; i++) {
        if (i < game->bossSequenceProgress) {
            putchar(game->bossAltarGlyphs[game->bossSequence[i]]);
        } else {
            putchar('_');
        }
        if (i + 1 < BOSS_SEQUENCE_LEN) {
            putchar(' ');
        }
    }
    printf("\033[K\n");
}

static void DrawBossPanel(const Game *game)
{
    if (game->bossPhase == BOSS_PHASE_NONE &&
        game->bossStatus != BOSS_STATUS_DEFEATED) {
        return;
    }

    printf("\033[1;35m보스\033[0m: %s / %s",
        BossPhaseText(game->bossPhase),
        BossStatusText(game->bossStatus));
    if ((game->bossStatus == BOSS_STATUS_VULNERABLE ||
         game->bossStatus == BOSS_STATUS_FINAL_DAMAGE) &&
        game->trueBossIndex >= 0 &&
        game->trueBossIndex < game->bossCount) {
        const Boss *boss = &game->bosses[game->trueBossIndex];
        printf("  HP %d/%d", boss->health, boss->maxHealth);
    } else if (game->bossPhase == BOSS_PHASE_ONE && game->bossCount > 0) {
        printf("  HP %d/%d", game->bosses[0].health, game->bosses[0].maxHealth);
    }
    printf("\033[K\n");

    if (game->bossPuzzle == BOSS_PUZZLE_SEQUENCE) {
        PrintBossSequenceGoal(game);
    } else if (game->bossPuzzle == BOSS_PUZZLE_ORB) {
        printf("목표: 피의 구슬 O를 중앙 제단 A로 운반하세요\033[K\n");
        printf("진행: %d/3 운반  %s\033[K\n",
            game->bossOrbDeliveries,
            game->bossOrbCarried ? "구슬 소지(이동속도 감소)" : "구슬 미소지");
    } else if (game->bossPuzzle == BOSS_PUZZLE_SEAL) {
        printf("목표: 진짜 보스가 있는 제단과 다른 A를 밟아 봉인하세요\033[K\n");
        printf("진행: 봉인 %d/3  진짜 교체 %.1fs  제단 재배치 %.1fs\033[K\n",
            game->bossSealCount,
            game->bossTrueSwapTimer,
            game->bossSealRelocateTimer);
    } else if (game->bossStatus == BOSS_STATUS_FINAL_DAMAGE) {
        printf("목표: 가짜는 사라졌습니다. 진짜 보스를 처치하세요\033[K\n");
        printf("진행: 최종 딜타임\033[K\n");
    } else {
        printf("목표: 보스 패턴을 회피하며 다음 기믹을 기다리세요\033[K\n");
        printf("진행: -\033[K\n");
    }

    printf("특수: ");
    if (game->bossShadowActive) {
        printf("그림자 활성 %.1fs  ", game->bossShadowTimer);
    } else {
        printf("그림자 대기  ");
    }
    if (game->bossForbiddenTimer > 0.0f && game->bossForbiddenKey != '\0') {
        printf("%c 방향 금지 %.1fs", game->bossForbiddenKey, game->bossForbiddenTimer);
    } else {
        printf("방향 금지 없음");
    }
    printf("\033[K\n");
}

static bool BossPanelVisible(const Game *game)
{
    return game->bossPhase != BOSS_PHASE_NONE ||
        game->bossStatus == BOSS_STATUS_DEFEATED;
}


void UiDrawGame(const Game *game)
{
    Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const int seconds = (int)game->elapsed;

    BuildGrid(game, grid);
    BeginFrame();

    printf("\033[1;36mTerminal Survivors\033[0m  ");
    printf("[%s]  ", GameDifficultyName(game->difficulty));
    printf("Time %02d:%02d / 10:00  ", seconds / 60, seconds % 60);
    printf("Remain %02d:%02d\033[K\n", remaining / 60, remaining % 60);
    printf("Kills %d  Score %d\033[K\n", game->player.kills, game->player.score);

    DrawBar("HP", game->player.health, game->player.maxHealth, 18, "\033[1;31m");
    printf("   ");
    DrawBar("XP", game->player.xp, game->player.xpToNextLevel, 18, "\033[1;32m");
    printf("   LV %d\033[K\n", game->player.level);

    {
        static const char *weaponNames[WEAPON_COUNT] = {
            "피의 고리", "혼돈의 살점", "십자 저주", "부패한 혜성"
        };
        const Weapon *aw = &game->weapons[game->activeWeapon];

        printf("\033[1;33m무기\033[0m: %s  Lv%d  Dmg%d  CD%.2fs",
            weaponNames[game->activeWeapon], aw->level, aw->damage,
            aw->cooldown);

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
        printf("\033[1;36m@\033[0m you  \033[1;31mb\033[0m 박쥐  "
               "\033[38;5;208mG\033[0m 좀비  \033[1;35mV\033[0m 뱀파이어  "
               "\033[38;5;196mo\033[0m 피의고리  \033[1;33m^\033[0m 혼돈의살점  "
               "\033[1;33m+\033[0m 십자저주  \033[1;33m*\033[0m 부패한혜성  "
               "\033[1;34m@\033[0m 방패무적  \033[1;32m+\033[0m XP\033[K\n\033[K\n");
    }

    for (int y = 0; y < game->mapHeight; y++) {
        DrawGridLine(grid, y, game->mapWidth);
    }

    /* 경고 문구: 항상 1줄 차지 */
    if (game->speedWarningTimer > 0.0f && ((int)(game->speedWarningTimer / 0.7f) % 2) == 0) {
        printf("\033[1;31m  !! 몬스터가 더 강력해집니다! !!\033[0m\033[K\n");
    } else if (game->miniEventMessageTimer > 0.0f && game->activeMiniEvent == MINI_EVENT_BLOOD_MIST) {
        printf("\033[1;31m  MINI EVENT: 피의 안개! 시야가 흐려지고 적이 빨라집니다.\033[0m\033[K\n");
    } else if (game->miniEventMessageTimer > 0.0f && game->activeMiniEvent == MINI_EVENT_BAT_STORM) {
        printf("\033[1;31m  MINI EVENT: 박쥐 폭풍! 약한 적이 몰려옵니다.\033[0m\033[K\n");
    } else {
        printf("\033[K\n");
    }

    if (game->mode == GAME_MODE_LEVEL_UP) {
        printf("\033[1;33mLEVEL UP\033[0m  "
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
        printf("\033[1;33mPAUSED\033[0m  Esc 재개  Q 게임종료\033[K\n");
    } else if (game->mode == GAME_MODE_GAME_OVER) {
        printf("\033[1;31mGAME OVER\033[0m  HP가 0이 됐습니다.  R 재시작  B/Esc 타이틀\033[K\n");
    } else if (game->mode == GAME_MODE_VICTORY) {
        printf("\033[1;32mVICTORY\033[0m  최종 보스를 처치했습니다!  R 재시작  B/Esc 타이틀\033[K\n");
    } else {
        printf("이동으로 회피하세요. 공격은 자동입니다.  Esc 일시정지  Q 게임종료  M 사운드\033[K\n");
    }

    EndFrame();
}

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

void UiPlaySounds(unsigned int flags, bool enabled)
{
    const PlatformSound sound = UiSoundEvent(flags);

    if (!enabled || sound == PLATFORM_SOUND_NONE) {
        return;
    }

    PlatformPlaySound(sound);
}
