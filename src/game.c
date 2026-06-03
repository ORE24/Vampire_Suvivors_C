#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCORE_FILE = "scores.txt";

/* PPT 무기 이름/설명 (교체 선택지용) */
static const char *WEAPON_SWITCH_NAMES[WEAPON_COUNT] = {
    "무기교체:원형(○)", "무기교체:삼각형(△)", "무기교체:사각형(□)", "무기교체:별(★)"
};
static const char *WEAPON_SWITCH_DESCS[WEAPON_COUNT] = {
    "Dmg20 1발조준 쿨1.0s", "Dmg8 3발부채꼴 쿨0.4s",
    "Dmg15 4방향 쿨1.0s",   "Dmg40 1발강타 쿨2.0s"
};

static void GenerateUpgrades(Game *game)
{
    Weapon *aw = &game->weapons[game->activeWeapon];
    WeaponType switchTarget = (WeaponType)((game->activeWeapon + 1 +
        rand() % (WEAPON_COUNT - 1)) % WEAPON_COUNT);

    /* #5: 동적 설명 버퍼 — 현재값→다음값 표시 */
    static char moveDesc[64];
    static char atkDesc[64];
    static char levelupDesc[64];
    static const char *hpDescs[4] = {
        "Lv0->1: 30초마다 HP+2 자동회복",
        "Lv1->2: 20초마다 HP+3 자동회복",
        "Lv2->3: 15초마다 HP+5 자동회복",
        "이미 최대 레벨(Lv.3)"
    };
    static const char *shieldDescs[4] = {
        "Lv0->1: 120초마다 15초 무적 발동",
        "Lv1->2: 90초마다 15초 무적 발동",
        "Lv2->3: 60초마다 15초 무적 발동",
        "이미 최대 레벨(Lv.3)"
    };
    int hpIdx;
    int shieldIdx;
    UpgradeOption pool[6];
    bool used[6] = {false, false, false, false, false, false};

    {
        float nMove = game->player.moveSpeedMult * 1.2f;
        float nAtk  = game->player.attackSpeedMult * 1.1f; /* #1: +10% */
        if (nMove > 1.44f) nMove = 1.44f;
        snprintf(moveDesc, sizeof(moveDesc), "이동속도 x%.2f -> x%.2f (+20%%)",
                 game->player.moveSpeedMult, nMove);
        snprintf(atkDesc,  sizeof(atkDesc),  "공격속도 x%.2f -> x%.2f (+10%%)",
                 game->player.attackSpeedMult, nAtk);
    }
    if (aw->level < 3) {
        snprintf(levelupDesc, sizeof(levelupDesc), "Dmg %d->%d, 사거리 %d->%d (+30%%/+20%%)",
            aw->damage, (int)((float)aw->damage * 1.30f + 0.5f),
            aw->range,  (int)((float)aw->range  * 1.20f + 0.5f));
    } else {
        strncpy(levelupDesc, "이미 최대 레벨(Lv.3)", sizeof(levelupDesc) - 1);
        levelupDesc[sizeof(levelupDesc) - 1] = '\0';
    }

    hpIdx     = game->player.hpRecoveryLevel < 3 ? game->player.hpRecoveryLevel : 3;
    shieldIdx = game->player.shieldLevel     < 3 ? game->player.shieldLevel     : 3;

    pool[0] = (UpgradeOption){"이동속도 증가", moveDesc,               WEAPON_COUNT,       0};
    pool[1] = (UpgradeOption){"공격속도 증가", atkDesc,                WEAPON_COUNT,       1};
    pool[2] = (UpgradeOption){"무기 레벨업",   levelupDesc,            game->activeWeapon, 2};
    pool[3] = (UpgradeOption){WEAPON_SWITCH_NAMES[switchTarget],
                               WEAPON_SWITCH_DESCS[switchTarget],
                               switchTarget,                            3};
    pool[4] = (UpgradeOption){"회복의 붕대",   hpDescs[hpIdx],         WEAPON_COUNT,       4};
    pool[5] = (UpgradeOption){"무적의 방패",   shieldDescs[shieldIdx], WEAPON_COUNT,       5};

    for (int i = 0; i < UPGRADE_CHOICES; i++) {
        int optionIndex = rand() % 6;
        while (used[optionIndex]) {
            optionIndex = (optionIndex + 1) % 6;
        }
        used[optionIndex] = true;
        game->upgrades[i] = pool[optionIndex];
    }
    game->selectedUpgrade = 0;
}

/* 무기 교체 시 초기 스펙으로 리셋 — #6: 레벨은 유지 */
static void ResetWeaponToBase(Game *game, WeaponType type)
{
    const int savedLevel = game->weapons[type].level; /* 교체 전 레벨 보존 */
    int i;

    switch (type) {
        case WEAPON_MAGIC_BOLT:
            game->weapons[type] = (Weapon){type, 1, 20, 1, 18, 0.60f, 0.0f, 'o'};
            break;
        case WEAPON_HOLY_AURA:
            game->weapons[type] = (Weapon){type, 1,  8, 3, 18, 0.36f, 0.0f, '^'};
            break;
        case WEAPON_PIERCING_LANCE:
            game->weapons[type] = (Weapon){type, 1, 15, 4, 18, 0.65f, 0.0f, '+'};
            break;
        case WEAPON_STAR_BURST:
            game->weapons[type] = (Weapon){type, 1, 40, 1, 14, 1.40f, 0.0f, '*'};
            break;
        default:
            return;
    }

    /* 보존된 레벨만큼 스탯 재적용 */
    for (i = 1; i < savedLevel && game->weapons[type].level < 3; i++) {
        game->weapons[type].level++;
        game->weapons[type].damage = (int)((float)game->weapons[type].damage * 1.30f + 0.5f);
        game->weapons[type].range  = (int)((float)game->weapons[type].range  * 1.20f + 0.5f);
    }
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
    game->player.shieldLevel = 0;
    game->player.shieldTimer = 0.0f;
    game->player.shieldCooldown = 0.0f;
    game->player.attackSpeedMult = 1.0f;
    game->player.moveSpeedMult = 1.0f;

    /* PPT 기반 무기 초기 스펙 (쿨타임 조정: 좀 더 빠르게) */
    game->activeWeapon = WEAPON_MAGIC_BOLT;   /* 시작 무기: 원형(○) */
    game->weapons[WEAPON_MAGIC_BOLT]    = (Weapon){WEAPON_MAGIC_BOLT,    1, 20, 1, 18, 0.60f, 0.0f, 'o'};
    game->weapons[WEAPON_HOLY_AURA]     = (Weapon){WEAPON_HOLY_AURA,     1,  8, 3, 18, 0.36f, 0.0f, '^'}; /* #4: 30% 느리게 0.25→0.36 */
    game->weapons[WEAPON_PIERCING_LANCE]= (Weapon){WEAPON_PIERCING_LANCE,1, 15, 4, 18, 0.65f, 0.0f, '+'};
    game->weapons[WEAPON_STAR_BURST]    = (Weapon){WEAPON_STAR_BURST,    1, 40, 1, 14, 1.40f, 0.0f, '*'};

    game->elapsed = 0.0f;
    game->spawnTimer = 0.0f;
    /* #2: 웨이브 3(2분)부터 좀비, 웨이브 5(4분)부터 뱀파이어 */
    if (difficulty == DIFFICULTY_HARD) {
        game->spawnStartInterval = 0.95f;
        game->spawnRampPerSecond = 0.0018f;
        game->spawnMinInterval = 0.24f;
        game->midEnemyStart = 120.0f;
        game->highEnemyStart = 240.0f;
        game->midEnemyChance = 55;
        game->highEnemyChance = 22;
    } else {
        game->spawnStartInterval = 1.35f;
        game->spawnRampPerSecond = 0.0011f;
        game->spawnMinInterval = 0.38f;
        game->midEnemyStart = 120.0f;
        game->highEnemyStart = 240.0f;
        game->midEnemyChance = 40;
        game->highEnemyChance = 15;
    }
    game->spawnInterval = game->spawnStartInterval;
    game->auraPulseTimer = 0.0f;
    game->pendingSounds = 0;
    GenerateUpgrades(game);
}

void GameApplyUpgrade(Game *game, int index)
{
    UpgradeOption *upgrade;
    static const float hpIntervals[4]    = {0.0f, 30.0f, 20.0f, 15.0f};
    static const float shieldCooldowns[4]= {0.0f,120.0f, 90.0f, 60.0f};

    if (index < 0 || index >= UPGRADE_CHOICES) {
        return;
    }

    upgrade = &game->upgrades[index];

    switch (upgrade->kind) {
        case 0: /* 이동속도 증가 */
            game->player.moveSpeedMult *= 1.2f;
            if (game->player.moveSpeedMult > 1.44f) {
                game->player.moveSpeedMult = 1.44f;
            }
            break;
        case 1: /* 공격속도 증가 — #1: +10% */
            game->player.attackSpeedMult *= 1.1f;
            break;
        case 2: /* 무기 레벨업 (최대 Lv.3) */
            {
                Weapon *weapon = &game->weapons[game->activeWeapon];
                if (weapon->level < 3) {
                    weapon->level++;
                    weapon->damage = (int)((float)weapon->damage * 1.30f + 0.5f);
                    weapon->range  = (int)((float)weapon->range  * 1.20f + 0.5f);
                }
            }
            break;
        case 3: /* 무기 교체 */
            game->activeWeapon = upgrade->weapon;
            ResetWeaponToBase(game, upgrade->weapon);
            break;
        case 4: /* 회복의 붕대 */
            if (game->player.hpRecoveryLevel < 3) {
                game->player.hpRecoveryLevel++;
                game->player.hpRecoveryTimer =
                    hpIntervals[game->player.hpRecoveryLevel];
            }
            break;
        case 5: /* 무적의 방패 */
            if (game->player.shieldLevel < 3) {
                game->player.shieldLevel++;
                game->player.shieldCooldown =
                    shieldCooldowns[game->player.shieldLevel];
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

    if (game->player.xp >= game->player.xpToNextLevel) {
        game->player.xp -= game->player.xpToNextLevel;
        game->player.level++;
        game->player.score += 50;
        game->player.xpToNextLevel = (int)((float)game->player.xpToNextLevel * 1.35f) + 4;
        /* #4: 레벨업 시 자동 속도 2% 향상 */
        game->player.attackSpeedMult *= 1.02f;
        game->player.moveSpeedMult   *= 1.02f;
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
