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

static char CryptMapTile(const Game *game, int x, int y)
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

bool GameGraveyardSpawnPoint(const Game *game, int index, int *x, int *y)
{
    const int centerX = game->mapWidth / 2;
    const int centerY = game->mapHeight / 2;
    static const int offsets[4][2] = {
        {-12, -5},
        { 12, -5},
        {-12,  5},
        { 12,  5}
    };

    if (index < 0 || index >= 4) {
        return false;
    }

    if (x != NULL) {
        *x = centerX + offsets[index][0];
    }
    if (y != NULL) {
        *y = centerY + offsets[index][1];
    }

    return true;
}

static bool IsNearGraveyardSpawn(const Game *game, int x, int y)
{
    for (int i = 0; i < 4; i++) {
        int graveX;
        int graveY;

        if (!GameGraveyardSpawnPoint(game, i, &graveX, &graveY)) {
            continue;
        }

        if (abs(x - graveX) <= 1 && abs(y - graveY) <= 1) {
            return true;
        }
    }

    return false;
}

static char GraveyardMapTile(const Game *game, int x, int y)
{
    const int mapWidth = game->mapWidth;
    const int mapHeight = game->mapHeight;
    const int centerX = mapWidth / 2;
    const int centerY = mapHeight / 2;

    if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) {
        return '#';
    }

    if (x == 0 || x == mapWidth - 1 || y == 0 || y == mapHeight - 1) {
        return '#';
    }

    if (IsNearGraveyardSpawn(game, x, y)) {
        int graveX;
        int graveY;

        for (int i = 0; i < 4; i++) {
            GameGraveyardSpawnPoint(game, i, &graveX, &graveY);
            if (x == graveX && y == graveY) {
                return 'M';
            }
        }

        if ((x + y) % 2 == 0) {
            return 'T';
        }
    }

    if ((x == centerX - 20 || x == centerX + 20) &&
        y >= 4 &&
        y <= mapHeight - 5 &&
        y != centerY &&
        y != centerY - 1 &&
        y != 7 &&
        y != mapHeight - 8) {
        return '#';
    }

    if ((y == centerY - 8 || y == centerY + 8) &&
        x >= 10 &&
        x <= mapWidth - 11 &&
        x != centerX &&
        x != centerX - 1 &&
        x != 18 &&
        x != mapWidth - 19) {
        return '#';
    }

    if ((x == centerX - 4 || x == centerX + 4) &&
        y >= centerY - 3 &&
        y <= centerY + 3 &&
        y != centerY) {
        return 'T';
    }

    if ((y == centerY - 3 || y == centerY + 3) &&
        x >= centerX - 8 &&
        x <= centerX + 8 &&
        x != centerX - 1 &&
        x != centerX &&
        x != centerX + 1) {
        return 'T';
    }

    if ((x == 12 || x == mapWidth - 13) &&
        y >= 5 &&
        y <= mapHeight - 6 &&
        y != centerY &&
        y != centerY - 1) {
        return 'T';
    }

    if ((y == 5 || y == mapHeight - 6) &&
        x >= 12 &&
        x <= mapWidth - 13 &&
        x % 5 != 0) {
        return 'T';
    }

    if ((x + y * 3) % 23 == 0 &&
        abs(x - centerX) > 7 &&
        abs(y - centerY) > 4) {
        return 'T';
    }

    return '.';
}

char GameMapTile(const Game *game, int x, int y)
{
    if (game->mapPhase == MAP_PHASE_GRAVEYARD) {
        return GraveyardMapTile(game, x, y);
    }

    return CryptMapTile(game, x, y);
}

bool GameMapIsBlocked(const Game *game, int x, int y)
{
    const char tile = GameMapTile(game, x, y);
    return tile == '#' || tile == 'T' || tile == 'M';
}

static Vec2 FindNearestOpenTile(const Game *game, Vec2 preferred)
{
    const int originX = GameRound(preferred.x);
    const int originY = GameRound(preferred.y);

    if (!GameMapIsBlocked(game, originX, originY)) {
        return (Vec2){(float)originX, (float)originY};
    }

    for (int radius = 1; radius <= RELOCATE_SEARCH_RADIUS; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                const int x = GameClampInt(originX + dx, 1, game->mapWidth - 2);
                const int y = GameClampInt(originY + dy, 1, game->mapHeight - 2);

                if (!GameMapIsBlocked(game, x, y)) {
                    return (Vec2){(float)x, (float)y};
                }
            }
        }
    }

    return (Vec2){game->mapWidth / 2.0f, game->mapHeight / 2.0f};
}

static void RelocateActorsForCurrentMap(Game *game)
{
    game->player.position = FindNearestOpenTile(game, game->player.position);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &game->enemies[i];
        if (enemy->active) {
            enemy->position = FindNearestOpenTile(game, enemy->position);
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (pickup->active) {
            pickup->position = FindNearestOpenTile(game, pickup->position);
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (game->projectiles[i].active &&
            GameMapIsBlocked(game,
                GameRound(game->projectiles[i].position.x),
                GameRound(game->projectiles[i].position.y))) {
            game->projectiles[i].active = false;
        }
    }
}

static void BossSetMessage(Game *game, const char *message)
{
    (void)snprintf(game->bossMessage, sizeof(game->bossMessage), "%s", message);
    game->bossMessageTimer = 4.0f;
}

static bool BossFightActive(const Game *game)
{
    return game->bossPhase != BOSS_PHASE_NONE &&
        game->bossStatus != BOSS_STATUS_DEFEATED;
}

static void BossClearPuzzleObjects(Game *game)
{
    game->bossAltarsActive = false;
    game->bossOrbActive = false;
    game->bossOrbCarried = false;
    game->bossSealAltarsActive = false;
    game->bossLastPlayerAltar = -1;
}

static Vec2 BossOpenTileNear(Game *game, int x, int y)
{
    Vec2 preferred;

    x = GameClampInt(x, 1, game->mapWidth - 2);
    y = GameClampInt(y, 1, game->mapHeight - 2);
    preferred = (Vec2){(float)x, (float)y};
    return FindNearestOpenTile(game, preferred);
}

static void BossPlaceSequenceAltars(Game *game)
{
    const int centerX = game->mapWidth / 2;
    const int centerY = game->mapHeight / 2;
    static const int offsets[BOSS_ALTAR_COUNT][2] = {
        {-14, -5},
        { 14, -5},
        {-14,  5},
        { 14,  5}
    };
    static const char glyphs[BOSS_ALTAR_COUNT] = {'R', 'B', 'G', 'Y'};

    for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
        game->bossAltars[i] = BossOpenTileNear(game,
            centerX + offsets[i][0],
            centerY + offsets[i][1]);
        game->bossAltarGlyphs[i] = glyphs[i];
    }
    game->bossAltarsActive = true;
}

static void BossBuildRandomSequence(Game *game)
{
    int altarIndexes[BOSS_ALTAR_COUNT] = {0, 1, 2, 3};

    for (int i = BOSS_ALTAR_COUNT - 1; i > 0; i--) {
        const int swapIndex = rand() % (i + 1);
        const int temp = altarIndexes[i];
        altarIndexes[i] = altarIndexes[swapIndex];
        altarIndexes[swapIndex] = temp;
    }

    for (int i = 0; i < BOSS_SEQUENCE_LEN; i++) {
        game->bossSequence[i] = altarIndexes[i];
    }
    game->bossSequenceProgress = 0;
}

static void BossSpawnPunishWave(Game *game, int waves)
{
    for (int i = 0; i < waves; i++) {
        EnemiesSpawnWave(game);
    }
}

static int BossPlayerAltarIndex(const Game *game, const Vec2 altars[BOSS_ALTAR_COUNT], bool active)
{
    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);

    if (!active) {
        return -1;
    }

    for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
        if (GameRound(altars[i].x) == playerX &&
            GameRound(altars[i].y) == playerY) {
            return i;
        }
    }

    return -1;
}

static void BossStartOrbPuzzle(Game *game);
static void BossStartPhaseTwo(Game *game);
static void BossStartFinalDamage(Game *game);

static void BossStartDamageWindow(Game *game)
{
    BossClearPuzzleObjects(game);
    game->bossPuzzle = BOSS_PUZZLE_NONE;
    game->bossStatus = BOSS_STATUS_VULNERABLE;

    if (game->bossPhase == BOSS_PHASE_ONE) {
        game->phaseOneDamageWindow++;
        if (game->phaseOneDamageWindow == 1) {
            BossSetMessage(game, "딜타임 시작! HP 50%까지 공격하세요.");
        } else {
            BossSetMessage(game, "두 번째 딜타임! 보스를 쓰러뜨리세요.");
        }
    }
}

static void BossStartSequencePuzzle(Game *game)
{
    game->bossPhase = BOSS_PHASE_ONE;
    game->bossStatus = BOSS_STATUS_SHIELDED;
    game->bossPuzzle = BOSS_PUZZLE_SEQUENCE;
    game->bossCount = 1;
    game->trueBossIndex = 0;
    game->phaseOneDamageWindow = 0;
    game->bosses[0] = (Boss){
        true,
        BossOpenTileNear(game, game->mapWidth / 2, game->mapHeight / 2 - 6),
        game->difficulty == DIFFICULTY_HARD ? BOSS_PHASE1_HP_HARD : BOSS_PHASE1_HP_EASY,
        game->difficulty == DIFFICULTY_HARD ? BOSS_PHASE1_HP_HARD : BOSS_PHASE1_HP_EASY,
        'W'
    };
    BossPlaceSequenceAltars(game);
    BossBuildRandomSequence(game);
    BossSetMessage(game, "보스전 시작! 순서대로 제단을 밟으세요.");
}

static void BossSpawnOrb(Game *game)
{
    static const int offsets[3][2] = {
        {-18, 0},
        { 18, 0},
        {  0, -9}
    };
    const int centerX = game->mapWidth / 2;
    const int centerY = game->mapHeight / 2;
    const int index = game->bossOrbDeliveries % 3;

    game->bossOrbPosition = BossOpenTileNear(game,
        centerX + offsets[index][0],
        centerY + offsets[index][1]);
    game->bossOrbActive = true;
    game->bossOrbCarried = false;
}

static void BossStartOrbPuzzle(Game *game)
{
    BossClearPuzzleObjects(game);
    game->bossStatus = BOSS_STATUS_SHIELDED;
    game->bossPuzzle = BOSS_PUZZLE_ORB;
    game->bossOrbDeliveries = 0;
    game->bossCentralAltar = BossOpenTileNear(game, game->mapWidth / 2, game->mapHeight / 2);
    BossSpawnOrb(game);
    BossSetMessage(game, "보호막 재생! 피의 구슬을 중앙 제단으로 3회 운반하세요.");
}

static void BossPlaceSealAltars(Game *game)
{
    const int centerX = game->mapWidth / 2;
    const int centerY = game->mapHeight / 2;
    const int jitter = rand() % 5 - 2;
    static const int offsets[BOSS_ALTAR_COUNT][2] = {
        {-18, -7},
        { 18, -7},
        {-18,  7},
        { 18,  7}
    };

    for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
        game->bossSealAltars[i] = BossOpenTileNear(game,
            centerX + offsets[i][0] + (i % 2 == 0 ? jitter : -jitter),
            centerY + offsets[i][1] + (i < 2 ? -jitter : jitter));
    }
    game->bossSealAltarsActive = true;
    game->bossLastPlayerAltar = -1;
}

static void BossPlaceClonesNearSealAltars(Game *game)
{
    static const int cloneOffsets[BOSS_ALTAR_COUNT][2] = {
        {1, 0},
        {-1, 0},
        {1, 0},
        {-1, 0}
    };

    for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
        game->bosses[i] = (Boss){
            true,
            BossOpenTileNear(game,
                GameRound(game->bossSealAltars[i].x) + cloneOffsets[i][0],
                GameRound(game->bossSealAltars[i].y) + cloneOffsets[i][1]),
            1,
            1,
            'W'
        };
    }
    game->bossCount = BOSS_ALTAR_COUNT;
}

static void BossSwitchTrueClone(Game *game)
{
    int next = rand() % BOSS_ALTAR_COUNT;

    if (game->bossCount <= 1) {
        return;
    }

    if (next == game->trueBossIndex) {
        next = (next + 1) % BOSS_ALTAR_COUNT;
    }
    game->trueBossIndex = next;
    game->bossTrueSwapTimer = BOSS_TRUE_SWAP_SECONDS;
    game->bossFlashTimer = 1.5f;
}

static void BossStartPhaseTwo(Game *game)
{
    BossClearPuzzleObjects(game);
    game->bossPhase = BOSS_PHASE_TWO;
    game->bossStatus = BOSS_STATUS_CLONES;
    game->bossPuzzle = BOSS_PUZZLE_SEAL;
    game->bossSealCount = 0;
    BossPlaceSealAltars(game);
    BossPlaceClonesNearSealAltars(game);
    game->trueBossIndex = rand() % BOSS_ALTAR_COUNT;
    game->bossTrueSwapTimer = BOSS_TRUE_SWAP_SECONDS;
    game->bossSealRelocateTimer = BOSS_SEAL_RELOCATE_SECONDS;
    game->bossFlashTimer = 1.5f;
    BossSetMessage(game, "2페이즈! 진짜 보스를 봉인하세요.");
    GameRequestSound(game, SOUND_POWER_PICKUP);
}

static void BossStartFinalDamage(Game *game)
{
    const Boss trueBoss = game->bosses[game->trueBossIndex];
    const int finalHp = game->difficulty == DIFFICULTY_HARD
        ? BOSS_FINAL_HP_HARD
        : BOSS_FINAL_HP_EASY;

    BossClearPuzzleObjects(game);
    game->bossPhase = BOSS_PHASE_TWO;
    game->bossStatus = BOSS_STATUS_FINAL_DAMAGE;
    game->bossPuzzle = BOSS_PUZZLE_NONE;
    game->bossCount = 1;
    game->trueBossIndex = 0;
    game->bosses[0] = (Boss){true, trueBoss.position, finalHp, finalHp, 'W'};
    game->bossFlashTimer = 1.5f;
    BossSetMessage(game, "최종 딜타임! 진짜 보스를 처치하세요.");
    GameRequestSound(game, SOUND_POWER_PICKUP);
}

static void BossStartFight(Game *game)
{
    game->bossSpawnTimer = 0.0f;
    game->bossShadowSpawnTimer = 2.0f;
    game->bossShadowActive = false;
    game->bossShadowTimer = 0.0f;
    game->bossShadowDamageCooldown = 0.0f;
    game->bossForbiddenKey = '\0';
    game->bossForbiddenTimer = 0.0f;
    game->bossForbiddenRollTimer = 3.0f;
    game->bossMessage[0] = '\0';
    game->bossMessageTimer = 0.0f;
    BossStartSequencePuzzle(game);
}

static void ActivateGraveyardMap(Game *game)
{
    game->mapPhase = MAP_PHASE_GRAVEYARD;
    game->graveyardWarningTimer = GRAVEYARD_WARNING_SECONDS;
    game->graveyardSpawnTimer = 0.0f;
    game->graveyardSpawnInterval = game->difficulty == DIFFICULTY_HARD
        ? GRAVEYARD_SPAWN_HARD
        : GRAVEYARD_SPAWN_EASY;
    game->speedWarningTimer = 0.0f;
    RelocateActorsForCurrentMap(game);
    GameRequestSound(game, SOUND_POWER_PICKUP);
}

static void ActivateBossArena(Game *game)
{
    game->mapPhase = MAP_PHASE_BOSS_ARENA;
    game->graveyardWarningTimer = GRAVEYARD_WARNING_SECONDS;
    game->graveyardSpawnTimer = 0.0f;
    game->graveyardSpawnInterval = 0.0f;
    game->speedWarningTimer = 0.0f;
    RelocateActorsForCurrentMap(game);
    BossStartFight(game);
    GameRequestSound(game, SOUND_POWER_PICKUP);
}

bool GameBossTargetPosition(const Game *game, Vec2 *position, int range)
{
    const float maxDistance = (float)(range * range);
    int index = 0;

    if (position == NULL || !BossFightActive(game)) {
        return false;
    }

    if (game->bossStatus != BOSS_STATUS_VULNERABLE &&
        game->bossStatus != BOSS_STATUS_FINAL_DAMAGE) {
        return false;
    }

    if (game->bossStatus == BOSS_STATUS_FINAL_DAMAGE) {
        index = game->trueBossIndex;
    }

    if (index < 0 || index >= game->bossCount || !game->bosses[index].active) {
        return false;
    }

    if (GameDistanceSquared(game->player.position, game->bosses[index].position) > maxDistance) {
        return false;
    }

    *position = game->bosses[index].position;
    return true;
}

static void BossApplyDamage(Game *game, int bossIndex, int damage)
{
    Boss *boss;

    if (!BossFightActive(game) ||
        bossIndex < 0 ||
        bossIndex >= game->bossCount ||
        !game->bosses[bossIndex].active ||
        damage <= 0) {
        return;
    }

    if (game->bossStatus != BOSS_STATUS_VULNERABLE &&
        game->bossStatus != BOSS_STATUS_FINAL_DAMAGE) {
        return;
    }

    boss = &game->bosses[bossIndex];
    boss->health -= damage;

    if (game->bossPhase == BOSS_PHASE_ONE &&
        game->bossStatus == BOSS_STATUS_VULNERABLE &&
        game->phaseOneDamageWindow == 1 &&
        boss->health <= boss->maxHealth / 2) {
        boss->health = boss->maxHealth / 2;
        BossStartOrbPuzzle(game);
        return;
    }

    if (game->bossPhase == BOSS_PHASE_ONE &&
        game->bossStatus == BOSS_STATUS_VULNERABLE &&
        game->phaseOneDamageWindow >= 2 &&
        boss->health <= 0) {
        boss->health = 0;
        BossStartPhaseTwo(game);
        return;
    }

    if (game->bossStatus == BOSS_STATUS_FINAL_DAMAGE && boss->health <= 0) {
        boss->health = 0;
        boss->active = false;
        game->bossStatus = BOSS_STATUS_DEFEATED;
        game->bossPhase = BOSS_PHASE_NONE;
        game->mode = GAME_MODE_VICTORY;
        game->player.score += 2000;
        BossSetMessage(game, "최종 보스 처치! 승리했습니다.");
        GameRequestSound(game, SOUND_VICTORY);
    }
}

bool GameBossApplyProjectileHit(Game *game, Projectile *projectile)
{
    if (projectile == NULL || !projectile->active || !BossFightActive(game)) {
        return false;
    }

    for (int i = 0; i < game->bossCount; i++) {
        Boss *boss = &game->bosses[i];

        if (!boss->active) {
            continue;
        }
        if (GameDistanceSquared(projectile->position, boss->position) > 0.85f) {
            continue;
        }

        if (game->bossStatus == BOSS_STATUS_VULNERABLE ||
            game->bossStatus == BOSS_STATUS_FINAL_DAMAGE) {
            BossApplyDamage(game, i, projectile->damage);
        }
        projectile->pierce--;
        if (projectile->pierce <= 0) {
            projectile->active = false;
        }
        return true;
    }

    return false;
}

void GameBossDamageInRadius(Game *game, Vec2 center, int radius, int damage)
{
    const float radiusSquared = (float)(radius * radius);

    if (!BossFightActive(game) ||
        (game->bossStatus != BOSS_STATUS_VULNERABLE &&
         game->bossStatus != BOSS_STATUS_FINAL_DAMAGE)) {
        return;
    }

    for (int i = 0; i < game->bossCount; i++) {
        if (game->bosses[i].active &&
            GameDistanceSquared(game->bosses[i].position, center) <= radiusSquared) {
            BossApplyDamage(game, i, damage);
        }
    }
}

void GameBossOnPlayerHit(Game *game)
{
    if (game->bossPuzzle == BOSS_PUZZLE_ORB && game->bossOrbCarried) {
        game->bossOrbCarried = false;
        game->bossOrbActive = true;
        game->bossOrbPosition = game->player.position;
        BossSetMessage(game, "피격! 피의 구슬을 떨어뜨렸습니다.");
    }
}

static void BossUpdateTimers(Game *game, float dt)
{
    if (game->bossMessageTimer > 0.0f) {
        game->bossMessageTimer -= dt;
        if (game->bossMessageTimer <= 0.0f) {
            game->bossMessageTimer = 0.0f;
            game->bossMessage[0] = '\0';
        }
    }
    if (game->bossFlashTimer > 0.0f) {
        game->bossFlashTimer -= dt;
        if (game->bossFlashTimer < 0.0f) {
            game->bossFlashTimer = 0.0f;
        }
    }
    if (game->bossShadowDamageCooldown > 0.0f) {
        game->bossShadowDamageCooldown -= dt;
        if (game->bossShadowDamageCooldown < 0.0f) {
            game->bossShadowDamageCooldown = 0.0f;
        }
    }
}

static void BossSpawnShadow(Game *game)
{
    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);

    for (int attempt = 0; attempt < 24; attempt++) {
        const int dx = (rand() % 9) - 4;
        const int dy = (rand() % 7) - 3;
        const int x = GameClampInt(playerX + dx, 1, game->mapWidth - 2);
        const int y = GameClampInt(playerY + dy, 1, game->mapHeight - 2);

        if ((dx != 0 || dy != 0) && !GameMapIsBlocked(game, x, y)) {
            game->bossShadowPosition = (Vec2){(float)x, (float)y};
            game->bossShadowActive = true;
            game->bossShadowTimer = BOSS_SHADOW_DURATION;
            BossSetMessage(game, "그림자가 분리되었습니다. 지키세요!");
            return;
        }
    }
}

static void BossUpdateShadow(Game *game, float dt)
{
    if (!game->bossShadowActive) {
        game->bossShadowSpawnTimer -= dt;
        if (game->bossShadowSpawnTimer <= 0.0f) {
            BossSpawnShadow(game);
            game->bossShadowSpawnTimer = game->difficulty == DIFFICULTY_HARD ? 12.0f : 16.0f;
        }
        return;
    }

    game->bossShadowTimer -= dt;
    if (game->bossShadowTimer <= 0.0f) {
        game->bossShadowActive = false;
        game->bossShadowTimer = 0.0f;
        BossSetMessage(game, "그림자가 사라졌습니다.");
        return;
    }

    if (game->bossShadowDamageCooldown <= 0.0f) {
        const int shadowX = GameRound(game->bossShadowPosition.x);
        const int shadowY = GameRound(game->bossShadowPosition.y);

        for (int i = 0; i < MAX_ENEMIES; i++) {
            const Enemy *enemy = &game->enemies[i];
            if (!enemy->active) {
                continue;
            }
            if (abs(GameRound(enemy->position.x) - shadowX) <= 0 &&
                abs(GameRound(enemy->position.y) - shadowY) <= 0) {
                game->player.health -= 1;
                game->bossShadowDamageCooldown = 1.0f;
                BossSetMessage(game, "그림자가 공격받았습니다!");
                GameRequestSound(game, SOUND_HIT);
                return;
            }
        }
    }
}

static void BossUpdateForbiddenKey(Game *game, float dt)
{
    static const char keys[4] = {'W', 'A', 'S', 'D'};

    if (game->bossForbiddenTimer > 0.0f) {
        game->bossForbiddenTimer -= dt;
        if (game->bossForbiddenTimer <= 0.0f) {
            game->bossForbiddenTimer = 0.0f;
            game->bossForbiddenKey = '\0';
            game->bossForbiddenRollTimer = 6.0f;
        }
        return;
    }

    game->bossForbiddenRollTimer -= dt;
    if (game->bossForbiddenRollTimer <= 0.0f) {
        game->bossForbiddenKey = keys[rand() % 4];
        game->bossForbiddenTimer = BOSS_FORBIDDEN_DURATION;
        game->bossForbiddenRollTimer = 12.0f;
        BossSetMessage(game, "금지된 방향키가 지정되었습니다!");
    }
}

static void BossUpdateExtraSpawns(Game *game, float dt)
{
    const bool puzzleActive = game->bossPuzzle != BOSS_PUZZLE_NONE;
    const float interval = puzzleActive
        ? (game->difficulty == DIFFICULTY_HARD ? 2.2f : 3.0f)
        : (game->difficulty == DIFFICULTY_HARD ? 4.0f : 5.5f);

    game->bossSpawnTimer += dt;
    while (game->bossSpawnTimer >= interval) {
        EnemiesSpawnWave(game);
        game->bossSpawnTimer -= interval;
    }
}

static void BossUpdateSequencePuzzle(Game *game)
{
    const int altarIndex = BossPlayerAltarIndex(game, game->bossAltars, game->bossAltarsActive);

    if (altarIndex < 0) {
        game->bossLastPlayerAltar = -1;
        return;
    }
    if (altarIndex == game->bossLastPlayerAltar) {
        return;
    }

    game->bossLastPlayerAltar = altarIndex;
    if (altarIndex == game->bossSequence[game->bossSequenceProgress]) {
        game->bossSequenceProgress++;
        GameRequestSound(game, SOUND_UI_CONFIRM);
        if (game->bossSequenceProgress >= BOSS_SEQUENCE_LEN) {
            BossStartDamageWindow(game);
        } else {
            BossSetMessage(game, "정답 제단입니다. 다음 순서를 밟으세요.");
        }
    } else {
        game->bossSequenceProgress = 0;
        BossSpawnPunishWave(game, 3);
        BossSetMessage(game, "순서가 틀렸습니다! 몬스터가 몰려옵니다.");
        GameRequestSound(game, SOUND_HIT);
    }
}

static void BossUpdateOrbPuzzle(Game *game)
{
    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);

    if (game->bossOrbActive &&
        GameRound(game->bossOrbPosition.x) == playerX &&
        GameRound(game->bossOrbPosition.y) == playerY) {
        game->bossOrbActive = false;
        game->bossOrbCarried = true;
        BossSetMessage(game, "피의 구슬 획득! 중앙 제단으로 운반하세요.");
        GameRequestSound(game, SOUND_UI_CONFIRM);
    }

    if (game->bossOrbCarried) {
        game->bossOrbPosition = game->player.position;
        if (GameRound(game->bossCentralAltar.x) == playerX &&
            GameRound(game->bossCentralAltar.y) == playerY) {
            game->bossOrbDeliveries++;
            game->bossOrbCarried = false;
            GameRequestSound(game, SOUND_POWER_PICKUP);
            if (game->bossOrbDeliveries >= 3) {
                BossStartDamageWindow(game);
            } else {
                BossSetMessage(game, "운반 성공! 다음 구슬을 가져오세요.");
                BossSpawnOrb(game);
            }
        }
    }
}

static int BossTrueNearSealAltar(const Game *game)
{
    const Boss *trueBoss;

    if (game->trueBossIndex < 0 ||
        game->trueBossIndex >= game->bossCount ||
        !game->bosses[game->trueBossIndex].active) {
        return -1;
    }

    trueBoss = &game->bosses[game->trueBossIndex];
    for (int i = 0; i < BOSS_ALTAR_COUNT; i++) {
        if (GameDistanceSquared(trueBoss->position, game->bossSealAltars[i]) <= 9.0f) {
            return i;
        }
    }

    return -1;
}

static void BossUpdateSealPuzzle(Game *game)
{
    const int playerAltar = BossPlayerAltarIndex(game, game->bossSealAltars, game->bossSealAltarsActive);
    const int trueNearAltar = BossTrueNearSealAltar(game);

    if (playerAltar < 0) {
        game->bossLastPlayerAltar = -1;
    } else if (playerAltar != game->bossLastPlayerAltar) {
        game->bossLastPlayerAltar = playerAltar;
        if (trueNearAltar >= 0 && playerAltar != trueNearAltar) {
            game->bossSealCount++;
            game->bossSealRelocateTimer = BOSS_SEAL_RELOCATE_SECONDS;
            GameRequestSound(game, SOUND_UI_CONFIRM);
            if (game->bossSealCount >= 3) {
                BossStartFinalDamage(game);
                return;
            }
            BossSetMessage(game, "봉인 성공! 진짜 보스를 다시 추적하세요.");
            BossSwitchTrueClone(game);
        } else {
            BossSpawnPunishWave(game, 3);
            BossSetMessage(game, "봉인 실패! 잘못된 분신이 반응했습니다.");
            GameRequestSound(game, SOUND_HIT);
        }
    }
}

static void BossUpdatePhaseTwoTimers(Game *game, float dt)
{
    if (game->bossStatus != BOSS_STATUS_CLONES ||
        game->bossPuzzle != BOSS_PUZZLE_SEAL) {
        return;
    }

    game->bossTrueSwapTimer -= dt;
    if (game->bossTrueSwapTimer <= 0.0f) {
        BossSwitchTrueClone(game);
        BossSetMessage(game, "진짜 보스가 다른 분신으로 이동했습니다!");
    }

    game->bossSealRelocateTimer -= dt;
    if (game->bossSealRelocateTimer <= 0.0f) {
        BossPlaceSealAltars(game);
        BossPlaceClonesNearSealAltars(game);
        game->bossSealRelocateTimer = BOSS_SEAL_RELOCATE_SECONDS;
        BossSetMessage(game, "봉인 제단이 재배치되었습니다.");
    }
}

static void BossUpdate(Game *game, float dt)
{
    if (!BossFightActive(game)) {
        BossUpdateTimers(game, dt);
        return;
    }

    BossUpdateTimers(game, dt);
    BossUpdateExtraSpawns(game, dt);
    BossUpdateShadow(game, dt);
    BossUpdateForbiddenKey(game, dt);
    BossUpdatePhaseTwoTimers(game, dt);

    if (game->bossPuzzle == BOSS_PUZZLE_SEQUENCE) {
        BossUpdateSequencePuzzle(game);
    } else if (game->bossPuzzle == BOSS_PUZZLE_ORB) {
        BossUpdateOrbPuzzle(game);
    } else if (game->bossPuzzle == BOSS_PUZZLE_SEAL) {
        BossUpdateSealPuzzle(game);
    }
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
    game->mapPhase = MAP_PHASE_CRYPT;
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
    game->graveyardWarningTimer = 0.0f;
    game->graveyardSpawnTimer = 0.0f;
    game->graveyardSpawnInterval = 0.0f;
    game->frenzyTimer = 0.0f;
    game->frenzyShotTimer = 0.0f;
    game->activeMiniEvent = MINI_EVENT_NONE;
    game->nextMiniEventTime = 30.0f;
    game->miniEventTimer = 0.0f;
    game->miniEventMessageTimer = 0.0f;
    game->diceMessage[0] = '\0';
    game->diceMessageTimer = 0.0f;
    game->bossPhase = BOSS_PHASE_NONE;
    game->bossStatus = BOSS_STATUS_NONE;
    game->bossPuzzle = BOSS_PUZZLE_NONE;
    game->bossCount = 0;
    game->trueBossIndex = -1;
    game->bossLastPlayerAltar = -1;
    game->bossForbiddenKey = '\0';
    game->bossMessage[0] = '\0';
    game->bossMessageTimer = 0.0f;
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
    GameRequestSound(game, SOUND_UI_CONFIRM);
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
        const int previousUpgrade = game->selectedUpgrade;

        if (input->left || input->up) {
            game->selectedUpgrade = (game->selectedUpgrade + UPGRADE_CHOICES - 1) % UPGRADE_CHOICES;
        }
        if (input->right || input->down) {
            game->selectedUpgrade = (game->selectedUpgrade + 1) % UPGRADE_CHOICES;
        }
        if (game->selectedUpgrade != previousUpgrade) {
            GameRequestSound(game, SOUND_UI_MOVE);
        }
        if (input->number >= 1 && input->number <= 3) {
            GameApplyUpgrade(game, input->number - 1);
        } else if (input->select) {
            GameApplyUpgrade(game, game->selectedUpgrade);
        }
        return;
    }

    game->elapsed += dt;

    if (game->mapPhase == MAP_PHASE_CRYPT &&
        game->elapsed >= GRAVEYARD_START_SECONDS) {
        ActivateGraveyardMap(game);
    }
    if (game->mapPhase == MAP_PHASE_GRAVEYARD &&
        game->elapsed >= BOSS_START_SECONDS) {
        ActivateBossArena(game);
    }

    while (game->elapsed >= game->nextMiniEventTime) {
        TriggerMiniEvent(game);
        game->nextMiniEventTime += 30.0f;
    }

    if (game->miniEventTimer > 0.0f) {
        game->miniEventTimer -= dt;
        if (game->miniEventTimer <= 0.0f) {
            game->miniEventTimer = 0.0f;
            game->activeMiniEvent = MINI_EVENT_NONE;
        }
    }
    if (game->miniEventMessageTimer > 0.0f) {
        game->miniEventMessageTimer -= dt;
        if (game->miniEventMessageTimer < 0.0f) {
            game->miniEventMessageTimer = 0.0f;
        }
    }
    if (game->diceMessageTimer > 0.0f) {
        game->diceMessageTimer -= dt;
        if (game->diceMessageTimer <= 0.0f) {
            game->diceMessageTimer = 0.0f;
            game->diceMessage[0] = '\0';
        }
    }

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

    if (game->graveyardWarningTimer > 0.0f) {
        game->graveyardWarningTimer -= dt;
        if (game->graveyardWarningTimer < 0.0f) {
            game->graveyardWarningTimer = 0.0f;
        }
    }

    game->spawnInterval = game->spawnStartInterval - (float)game->elapsed * game->spawnRampPerSecond;
    if (game->spawnInterval < game->spawnMinInterval) {
        game->spawnInterval = game->spawnMinInterval;
    }

    PlayerUpdate(game, input, dt);
    BossUpdate(game, dt);

    game->spawnTimer += dt;
    while (game->spawnTimer >= game->spawnInterval) {
        EnemiesSpawnWave(game);
        game->spawnTimer -= game->spawnInterval;
    }

    if (game->mapPhase == MAP_PHASE_GRAVEYARD) {
        game->graveyardSpawnTimer += dt;
        while (game->graveyardSpawnTimer >= game->graveyardSpawnInterval) {
            EnemiesSpawnGraveyardWave(game);
            game->graveyardSpawnTimer -= game->graveyardSpawnInterval;
            if (game->graveyardSpawnInterval > (game->difficulty == DIFFICULTY_HARD
                    ? GRAVEYARD_SPAWN_MIN_HARD
                    : GRAVEYARD_SPAWN_MIN_EASY)) {
                game->graveyardSpawnInterval -= 0.03f;
                if (game->difficulty == DIFFICULTY_HARD &&
                    game->graveyardSpawnInterval < GRAVEYARD_SPAWN_MIN_HARD) {
                    game->graveyardSpawnInterval = GRAVEYARD_SPAWN_MIN_HARD;
                } else if (game->difficulty == DIFFICULTY_EASY &&
                    game->graveyardSpawnInterval < GRAVEYARD_SPAWN_MIN_EASY) {
                    game->graveyardSpawnInterval = GRAVEYARD_SPAWN_MIN_EASY;
                }
            }
        }
    }

    WeaponsUpdate(game, dt);
    ProjectilesUpdate(game, dt);
    EnemiesUpdate(game, dt);
    CombatResolve(game);
    if (game->mode == GAME_MODE_VICTORY) {
        return;
    }
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
