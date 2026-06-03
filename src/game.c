#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCORE_FILE = "scores.txt";

/* 무기 교체 선택지 이름/설명 */
static const char *WEAPON_SWITCH_NAMES[WEAPON_COUNT] = {
    "무기 교체 : 피의 고리",
    "무기 교체 : 혼돈의 살점",
    "무기 교체 : 십자 저주",
    "무기 교체 : 부패한 혜성"
};
static const char *WEAPON_SWITCH_DESCS[WEAPON_COUNT] = {
    "DMG 20, 궤도 10개 회전 타격",
    "DMG 4, 공격속도 2.5, 3발 부채꼴",
    "DMG 10, 공격속도 1, 4방향",
    "DMG 40, 공격속도 0.5, 1발 강타"
};

/* 레벨 기반으로 무기 스탯 재계산 */
static void ApplyWeaponLevelStats(Game *game, WeaponType type)
{
    Weapon *w = &game->weapons[type];
    switch (type) {
        case WEAPON_MAGIC_BOLT:
            w->damage         = 20 + 2 * (w->level - 1);
            w->cooldown       = 1.0f / (1.0f + 0.5f * (float)(w->level - 1));
            w->projectileCount = 1;
            break;
        case WEAPON_HOLY_AURA:
            w->damage         = 4 + 2 * (w->level - 1);
            w->cooldown       = 1.0f / (2.5f + 0.2f * (float)(w->level - 1));
            w->projectileCount = (w->level >= 7) ? 5 : 3;
            break;
        case WEAPON_PIERCING_LANCE:
            w->damage         = 10 + 2 * (w->level - 1);
            w->cooldown       = 1.0f / (1.0f + 0.3f * (float)(w->level - 1));
            w->projectileCount = (w->level >= 7) ? 8 : 4;
            break;
        case WEAPON_STAR_BURST:
            w->damage         = 40 + 5 * (w->level - 1);
            if (w->level >= 7) {
                w->cooldown        = 0.2f;
                w->projectileCount = 4;
            } else {
                w->cooldown        = 1.0f / (0.5f + 0.5f * (float)(w->level - 1));
                w->projectileCount = 1;
            }
            break;
        default:
            break;
    }
}

static void GenerateUpgrades(Game *game)
{
    /* 풀 8종: 4무기(교체+레벨업) + 이동속도 + 공격속도 + 회복붕대 + 무적방패 */
    static char descC[80], descT[80], descS[80], descSt[80];
    const Weapon *wC  = &game->weapons[WEAPON_MAGIC_BOLT];
    const Weapon *wT  = &game->weapons[WEAPON_HOLY_AURA];
    const Weapon *wS  = &game->weapons[WEAPON_PIERCING_LANCE];
    const Weapon *wSt = &game->weapons[WEAPON_STAR_BURST];

    if (wC->level == 0)
        snprintf(descC,  sizeof(descC),  "궤도 10개 회전 타격 [처음 획득]");
    else if (wC->level < 7)
        snprintf(descC,  sizeof(descC),  "Lv.%d->%d: DMG+2, 공격속도+0.5", wC->level, wC->level + 1);
    else
        snprintf(descC,  sizeof(descC),  "만랩 Lv.7");

    if (wT->level == 0)
        snprintf(descT,  sizeof(descT),  "DMG 4, 공격속도 2.5, 3발 부채꼴 [처음 획득]");
    else if (wT->level < 7)
        snprintf(descT,  sizeof(descT),  "Lv.%d->%d: DMG+2, 공격속도+0.2 (Lv7: 5발)", wT->level, wT->level + 1);
    else
        snprintf(descT,  sizeof(descT),  "만랩 Lv.7: 5발 부채꼴");

    if (wS->level == 0)
        snprintf(descS,  sizeof(descS),  "DMG 10, 공격속도 1, 4방향 [처음 획득]");
    else if (wS->level < 7)
        snprintf(descS,  sizeof(descS),  "Lv.%d->%d: DMG+2, 공격속도+0.3 (Lv7: 8방향)", wS->level, wS->level + 1);
    else
        snprintf(descS,  sizeof(descS),  "만랩 Lv.7: 4방향 각 2발");

    if (wSt->level == 0)
        snprintf(descSt, sizeof(descSt), "DMG 40, 공격속도 0.5, 1발 강타 [처음 획득]");
    else if (wSt->level < 7)
        snprintf(descSt, sizeof(descSt), "Lv.%d->%d: DMG+5, 공격속도+0.5 (Lv7: 3발x3연사)", wSt->level, wSt->level + 1);
    else
        snprintf(descSt, sizeof(descSt), "만랩 Lv.7: 3발 부채꼴 x3연사");

    const UpgradeOption pool[8] = {
        {"이동속도 증가",  "이동속도 +20%",   WEAPON_COUNT,           0},
        {"공격속도 증가",  "공격속도 +10%",   WEAPON_COUNT,           1},
        {"피의 고리",      descC,             WEAPON_MAGIC_BOLT,      2},
        {"혼돈의 살점",    descT,             WEAPON_HOLY_AURA,       3},
        {"십자 저주",      descS,             WEAPON_PIERCING_LANCE,  4},
        {"부패한 혜성",    descSt,            WEAPON_STAR_BURST,      5},
        {"회복의 붕대",
            game->player.hpRecoveryLevel > 0
                ? "이미 활성 중"
                : "180초간 활성: 20킬마다 HP+1, 만료 시 소멸",
            WEAPON_COUNT, 6},
        {"무적의 방패",
            game->player.shieldTimer > 0.0f
                ? "방패 활성 중"
                : "5회 방어 or 20초 후 화면 몬스터 전멸  \033[1;31m감히 나를 부순 자들이여, 함께 소멸하라",
            WEAPON_COUNT, 7}
    };

    bool used[8] = {false, false, false, false, false, false, false, false};

    for (int i = 0; i < UPGRADE_CHOICES; i++) {
        int optionIndex = rand() % 8;
        while (used[optionIndex]) {
            optionIndex = (optionIndex + 1) % 8;
        }
        used[optionIndex] = true;
        game->upgrades[i] = pool[optionIndex];
    }
    game->selectedUpgrade = 0;
}

float GameDistanceSquared(Vec2 a, Vec2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vec2 GameNormalize(Vec2 v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y;
    float invLength;

    if (lengthSquared <= 0.0001f) {
        return (Vec2){0.0f, 0.0f};
    }

    invLength = 1.0f / sqrtf(lengthSquared);
    return (Vec2){v.x * invLength, v.y * invLength};
}

Vec2 GameAdd(Vec2 a, Vec2 b)
{
    return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 GameScale(Vec2 v, float scale)
{
    return (Vec2){v.x * scale, v.y * scale};
}

int GameRound(float value)
{
    if (value >= 0.0f) {
        return (int)(value + 0.5f);
    }

    return (int)(value - 0.5f);
}

int GameClampInt(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

char GameMapTile(const Game *game, int x, int y)
{
    const int mapWidth = game->mapWidth;
    const int mapHeight = game->mapHeight;
    const int leftWallX = mapWidth * 28 / 100;
    const int rightWallX = mapWidth * 72 / 100;
    const int wallTopY = mapHeight * 22 / 100;
    const int wallBottomY = mapHeight * 78 / 100;
    const int centerY = mapHeight / 2;
    const int upperWallY = mapHeight * 32 / 100;
    const int lowerWallY = mapHeight * 68 / 100;
    const int hallLeftX = mapWidth * 39 / 100;
    const int hallRightX = mapWidth * 61 / 100;
    const int centerX = mapWidth / 2;
    const int tombLeftX = mapWidth * 14 / 100;
    const int tombRightX = mapWidth * 86 / 100;
    const int tombTopY = mapHeight * 23 / 100;
    const int tombBottomY = mapHeight * 77 / 100;

    if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) {
        return '#';
    }

    if (x == 0 || x == mapWidth - 1 || y == 0 || y == mapHeight - 1) {
        return '#';
    }

    if ((x == leftWallX || x == rightWallX) &&
        y >= wallTopY &&
        y <= wallBottomY &&
        y != centerY &&
        y != centerY - 1) {
        return '#';
    }

    if (y == upperWallY &&
        x >= hallLeftX &&
        x <= hallRightX &&
        x != centerX &&
        x != centerX - 1) {
        return '#';
    }

    if (y == lowerWallY &&
        x >= hallLeftX &&
        x <= hallRightX &&
        x != centerX &&
        x != centerX - 1) {
        return '#';
    }

    if ((x == tombLeftX && y == tombTopY) ||
        (x == tombRightX && y == tombTopY) ||
        (x == tombLeftX && y == tombBottomY) ||
        (x == tombRightX && y == tombBottomY) ||
        (x == centerX - 12 && y == centerY) ||
        (x == centerX + 12 && y == centerY)) {
        return 'T';
    }

    return '.';
}

bool GameMapIsBlocked(const Game *game, int x, int y)
{
    const char tile = GameMapTile(game, x, y);
    return tile == '#' || tile == 'T';
}

const char *GameDifficultyName(GameDifficulty difficulty)
{
    if (difficulty == DIFFICULTY_HARD) {
        return "Hard";
    }

    return "Easy";
}

void GameRequestSound(Game *game, unsigned int flags)
{
    game->pendingSounds |= flags;
}

void GameInit(Game *game, GameDifficulty difficulty)
{
    memset(game, 0, sizeof(*game));

    game->difficulty = difficulty;
    game->mapWidth = DEFAULT_MAP_WIDTH;
    game->mapHeight = DEFAULT_MAP_HEIGHT;
    game->mode = GAME_MODE_PLAYING;
    game->player.position = (Vec2){game->mapWidth / 2.0f, game->mapHeight / 2.0f};
    game->player.maxHealth = difficulty == DIFFICULTY_HARD ? 10 : 14;
    game->player.health = game->player.maxHealth;
    game->player.level = 1;
    game->player.xp = 0;
    game->player.xpToNextLevel = 6;
    game->player.score = 0;
    game->player.kills = 0;
    game->player.moveCooldown = 0.0f;
    game->player.invulnerableTimer = 0.0f;
    game->player.magnetRange = 4;
    /* 패시브 아이템 초기화 */
    game->player.hpRecoveryLevel = 0;
    game->player.hpRecoveryTimer = 0.0f;
    game->player.bandageKillCount = 0;
    game->player.shieldLevel = 0;
    game->player.shieldTimer = 0.0f;
    game->player.shieldCooldown = 0.0f;
    game->player.shieldHits = 0;
    game->shieldBreakTimer = 0.0f;
    game->player.attackSpeedMult = 1.0f;
    game->player.moveSpeedMult = 1.0f;

    /* 시작 무기: 피의 고리 Lv.1, 나머지는 미획득(Lv.0) */
    game->activeWeapon = WEAPON_MAGIC_BOLT;
    game->weapons[WEAPON_MAGIC_BOLT]    = (Weapon){WEAPON_MAGIC_BOLT,    1, 20, 1, 18, 1.00f, 0.0f, 'o'};
    game->weapons[WEAPON_HOLY_AURA]     = (Weapon){WEAPON_HOLY_AURA,     0,  4, 3, 18, 0.40f, 0.0f, '^'};
    game->weapons[WEAPON_PIERCING_LANCE]= (Weapon){WEAPON_PIERCING_LANCE,0, 10, 4, 18, 1.00f, 0.0f, '+'};
    game->weapons[WEAPON_STAR_BURST]    = (Weapon){WEAPON_STAR_BURST,    0, 40, 1, 14, 2.00f, 0.0f, '*'};

    game->elapsed = 0.0f;
    game->speedWarningTimer = 0.0f;
    game->lastSpeedStep = 0;
    game->spawnTimer = 0.0f;
    if (difficulty == DIFFICULTY_HARD) {
        game->spawnStartInterval = 0.95f;
        game->spawnRampPerSecond = 0.0030f;
        game->spawnMinInterval = 0.16f;
        game->midEnemyStart = 15.0f;
        game->highEnemyStart = 55.0f;
        game->midEnemyChance = 48;
        game->highEnemyChance = 13;
    } else {
        game->spawnStartInterval = 1.35f;
        game->spawnRampPerSecond = 0.0020f;
        game->spawnMinInterval = 0.25f;
        game->midEnemyStart = 35.0f;
        game->highEnemyStart = 105.0f;
        game->midEnemyChance = 32;
        game->highEnemyChance = 6;
    }
    game->spawnInterval = game->spawnStartInterval;
    game->auraPulseTimer = 0.0f;
    game->pendingSounds = 0;
    GenerateUpgrades(game);
}

void GameApplyUpgrade(Game *game, int index)
{
    UpgradeOption *upgrade;

    if (index < 0 || index >= UPGRADE_CHOICES) {
        return;
    }

    upgrade = &game->upgrades[index];

    switch (upgrade->kind) {
        case 0: /* 이동속도 +20% */
            game->player.moveSpeedMult *= 1.2f;
            break;
        case 1: /* 공격속도 +10% */
            game->player.attackSpeedMult *= 1.1f;
            break;
        case 2: /* 피의 고리 — 선택 시 교체 + 레벨업 */
        case 3: /* 혼돈의 살점 */
        case 4: /* 십자 저주 */
        case 5: /* 부패한 혜성 */
            {
                WeaponType wt = upgrade->weapon;
                Weapon *w = &game->weapons[wt];
                if (w->level < 7) w->level++;
                ApplyWeaponLevelStats(game, wt);
                game->activeWeapon = wt;
            }
            break;
        case 6: /* 회복의 붕대 (일회성 획득) */
            if (game->player.hpRecoveryLevel == 0) {
                game->player.hpRecoveryLevel = 1;
                game->player.bandageKillCount = 0;
                game->player.hpRecoveryTimer = 0.0f;
            }
            break;
        case 7: /* 무적의 방패: 5회 방어 또는 20초 후 화면 전멸 */
            if (game->player.shieldTimer <= 0.0f) {
                game->player.shieldTimer = 20.0f;
                game->player.shieldHits  = 5;
                game->auraPulseTimer = 0.22f;
            }
            break;
        default:
            break;
    }

    game->mode = GAME_MODE_PLAYING;
}

void GameUpdate(Game *game, const InputState *input, float dt)
{
    if (game->mode == GAME_MODE_GAME_OVER || game->mode == GAME_MODE_VICTORY) {
        return;
    }

    /* 테스트: = 키로 레벨업 보상 강제 트리거 */
    if (input->typedChar == '=' && game->mode == GAME_MODE_PLAYING) {
        GenerateUpgrades(game);
        game->mode = GAME_MODE_LEVEL_UP;
    }

    if (input->pauseToggle && game->mode == GAME_MODE_PLAYING) {
        game->mode = GAME_MODE_PAUSED;
        return;
    }

    if (game->mode == GAME_MODE_PAUSED) {
        if (input->pauseToggle) {
            game->mode = GAME_MODE_PLAYING;
        }
        return;
    }

    if (game->mode == GAME_MODE_LEVEL_UP) {
        if (input->left || input->up) {
            game->selectedUpgrade = (game->selectedUpgrade + UPGRADE_CHOICES - 1) % UPGRADE_CHOICES;
        }
        if (input->right || input->down) {
            game->selectedUpgrade = (game->selectedUpgrade + 1) % UPGRADE_CHOICES;
        }
        if (input->number >= 1 && input->number <= 3) {
            GameApplyUpgrade(game, input->number - 1);
        } else if (input->select) {
            GameApplyUpgrade(game, game->selectedUpgrade);
        }
        return;
    }

    game->elapsed += dt;

    {
        int curStep = (int)(game->elapsed / 120.0f);
        if (curStep > game->lastSpeedStep) {
            game->lastSpeedStep = curStep;
            game->speedWarningTimer = 5.0f;
        }
        if (game->speedWarningTimer > 0.0f) {
            game->speedWarningTimer -= dt;
        }
    }

    if (game->elapsed >= SURVIVAL_SECONDS) {
        game->mode = GAME_MODE_VICTORY;
        game->player.score += 1000;
        GameRequestSound(game, SOUND_VICTORY);
        return;
    }

    game->spawnInterval = game->spawnStartInterval - (float)game->elapsed * game->spawnRampPerSecond;
    if (game->spawnInterval < game->spawnMinInterval) {
        game->spawnInterval = game->spawnMinInterval;
    }

    PlayerUpdate(game, input, dt);

    game->spawnTimer += dt;
    while (game->spawnTimer >= game->spawnInterval) {
        EnemiesSpawnWave(game);
        game->spawnTimer -= game->spawnInterval;
    }

    WeaponsUpdate(game, dt);
    ProjectilesUpdate(game, dt);
    EnemiesUpdate(game, dt);
    CombatResolve(game);
    PickupsUpdate(game, dt);

    if (game->auraPulseTimer > 0.0f) {
        game->auraPulseTimer -= dt;
        if (game->auraPulseTimer < 0.0f) {
            game->auraPulseTimer = 0.0f;
        }
    }

    if (game->shieldBreakTimer > 0.0f) {
        game->shieldBreakTimer -= dt;
        if (game->shieldBreakTimer < 0.0f) {
            game->shieldBreakTimer = 0.0f;
        }
    }

    if (game->player.xp >= game->player.xpToNextLevel) {
        game->player.xp -= game->player.xpToNextLevel;
        game->player.level++;
        game->player.score += 50;
        game->player.xpToNextLevel = (int)((float)game->player.xpToNextLevel * 1.35f) + 4;
        GenerateUpgrades(game);
        game->mode = GAME_MODE_LEVEL_UP;
        GameRequestSound(game, SOUND_LEVEL_UP);
    }

    if (game->player.health <= 0) {
        game->player.health = 0;
        game->mode = GAME_MODE_GAME_OVER;
        GameRequestSound(game, SOUND_GAME_OVER);
    }
}

void RankingLoad(RankingEntry entries[MAX_RANKINGS], int *count)
{
    FILE *file = fopen(SCORE_FILE, "r");
    int loaded = 0;

    if (count == NULL) {
        return;
    }

    *count = 0;
    if (file == NULL) {
        return;
    }

    while (loaded < MAX_RANKINGS &&
           fscanf(file, "%15s %15s %d %d %d %d",
               entries[loaded].name,
               entries[loaded].result,
               &entries[loaded].score,
               &entries[loaded].seconds,
               &entries[loaded].kills,
               &entries[loaded].level) == 6) {
        loaded++;
    }

    fclose(file);
    *count = loaded;
}

void RankingAddAndSave(const Game *game, const char *name, const char *result)
{
    RankingEntry entries[MAX_RANKINGS + 1];
    int count = 0;
    FILE *file;
    int insertIndex;

    RankingLoad(entries, &count);

    if (count < MAX_RANKINGS + 1) {
        strncpy(entries[count].name,   name,   sizeof(entries[count].name)   - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        strncpy(entries[count].result, result, sizeof(entries[count].result) - 1);
        entries[count].result[sizeof(entries[count].result) - 1] = '\0';
        entries[count].score = game->player.score + (int)game->elapsed;
        entries[count].seconds = (int)game->elapsed;
        entries[count].kills = game->player.kills;
        entries[count].level = game->player.level;
        count++;
    }

    for (int i = 1; i < count; i++) {
        RankingEntry key = entries[i];
        insertIndex = i - 1;
        while (insertIndex >= 0 && entries[insertIndex].score < key.score) {
            entries[insertIndex + 1] = entries[insertIndex];
            insertIndex--;
        }
        entries[insertIndex + 1] = key;
    }

    if (count > MAX_RANKINGS) {
        count = MAX_RANKINGS;
    }

    file = fopen(SCORE_FILE, "w");
    if (file == NULL) {
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(file, "%s %s %d %d %d %d\n",
            entries[i].name[0] ? entries[i].name : "NONAME",
            entries[i].result,
            entries[i].score,
            entries[i].seconds,
            entries[i].kills,
            entries[i].level);
    }

    fclose(file);
}
