#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ENEMY_UNREACHABLE 1000000
#define PICKUP_COLLECT_DISTANCE 0.8f
#define PICKUP_BONUS_ROLL_MAX 1000
#define PICKUP_HEAL_PACK_CHANCE 20
#define PICKUP_GAMBLER_DICE_CHANCE 12
#define PICKUP_FRENZY_MAGAZINE_CHANCE 10
#define PICKUP_PINATA_SKULL_CHANCE 10
#define PICKUP_VACUUM_CHANCE 8
#define PICKUP_ROSARY_CHANCE 6
#define FRENZY_MAGAZINE_SECONDS 5.0f

static int PickupPriority(PickupType type)
{
    switch (type) {
        case PICKUP_XP:
            return 0;
        case PICKUP_HEAL_PACK:
            return 1;
        case PICKUP_GAMBLER_DICE:
        case PICKUP_FRENZY_MAGAZINE:
        case PICKUP_PINATA_SKULL:
            return 2;
        case PICKUP_VACUUM:
            return 3;
        case PICKUP_ROSARY:
            return 4;
        default:
            return 0;
    }
}

static int SignInt(int value)
{
    if (value < 0) {
        return -1;
    }
    if (value > 0) {
        return 1;
    }
    return 0;
}

static bool IsEnemyAt(const Game *game, int x, int y)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *enemy = &game->enemies[i];
        if (enemy->active && GameRound(enemy->position.x) == x && GameRound(enemy->position.y) == y) {
            return true;
        }
    }

    return false;
}

static void BuildEnemyDistanceMap(const Game *game, int distances[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int playerX, int playerY)
{
    static int queueX[MAX_MAP_WIDTH * MAX_MAP_HEIGHT];
    static int queueY[MAX_MAP_WIDTH * MAX_MAP_HEIGHT];
    static const int directions[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1}
    };
    int head = 0;
    int tail = 0;

    for (int y = 0; y < game->mapHeight; y++) {
        for (int x = 0; x < game->mapWidth; x++) {
            distances[y][x] = ENEMY_UNREACHABLE;
        }
    }

    if (GameMapIsBlocked(game, playerX, playerY)) {
        return;
    }

    distances[playerY][playerX] = 0;
    queueX[tail] = playerX;
    queueY[tail] = playerY;
    tail++;

    while (head < tail) {
        const int currentX = queueX[head];
        const int currentY = queueY[head];
        const int nextDistance = distances[currentY][currentX] + 1;
        head++;

        for (int i = 0; i < 4; i++) {
            const int nextX = currentX + directions[i][0];
            const int nextY = currentY + directions[i][1];

            if (nextX < 0 ||
                nextX >= game->mapWidth ||
                nextY < 0 ||
                nextY >= game->mapHeight ||
                GameMapIsBlocked(game, nextX, nextY) ||
                distances[nextY][nextX] <= nextDistance) {
                continue;
            }

            distances[nextY][nextX] = nextDistance;
            queueX[tail] = nextX;
            queueY[tail] = nextY;
            tail++;
        }
    }
}

static bool TryMoveEnemyTowardPlayer(Game *game, Enemy *enemy, int distances[MAX_MAP_HEIGHT][MAX_MAP_WIDTH])
{
    static const int directions[8][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
        {1, 1},
        {1, -1},
        {-1, 1},
        {-1, -1}
    };
    const int enemyX = GameRound(enemy->position.x);
    const int enemyY = GameRound(enemy->position.y);
    int bestX = enemyX;
    int bestY = enemyY;
    int bestDistance = ENEMY_UNREACHABLE;
    bool foundMove = false;

    for (int i = 0; i < 8; i++) {
        const int nextX = enemyX + directions[i][0];
        const int nextY = enemyY + directions[i][1];

        if (nextX < 0 ||
            nextX >= game->mapWidth ||
            nextY < 0 ||
            nextY >= game->mapHeight ||
            GameMapIsBlocked(game, nextX, nextY) ||
            IsEnemyAt(game, nextX, nextY) ||
            distances[nextY][nextX] >= ENEMY_UNREACHABLE) {
            continue;
        }

        if (!foundMove || distances[nextY][nextX] < bestDistance) {
            bestX = nextX;
            bestY = nextY;
            bestDistance = distances[nextY][nextX];
            foundMove = true;
        }
    }

    if (!foundMove) {
        return false;
    }

    enemy->position.x = (float)bestX;
    enemy->position.y = (float)bestY;
    return true;
}

static void TryMovePlayer(Game *game, int dx, int dy)
{
    const int nextX = GameClampInt(GameRound(game->player.position.x) + dx, 1, game->mapWidth - 2);
    const int nextY = GameClampInt(GameRound(game->player.position.y) + dy, 1, game->mapHeight - 2);

    if (!GameMapIsBlocked(game, nextX, nextY)) {
        game->player.position.x = (float)nextX;
        game->player.position.y = (float)nextY;
    }
}

static bool SpawnEnemyAt(Game *game, EnemyType type, int x, int y)
{
    int slot = -1;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0 || GameMapIsBlocked(game, x, y)) {
        return false;
    }

    if (type == ENEMY_ONE_HP) {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 1, 1, 1, 2, 10, 0.0f, 0.24f, 'b'};
    } else if (type == ENEMY_THREE_HP) {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 3, 3, 2, 6, 35, 0.0f, 0.45f, 'G'};
    } else {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 40, 40, 6, 50, 450, 0.0f, 0.90f, 'V'};
    }

    return true;
}

void PlayerUpdate(Game *game, const InputState *input, float dt)
{
    int dx = 0;
    int dy = 0;

    if (game->player.moveCooldown > 0.0f) {
        game->player.moveCooldown -= dt;
    }
    if (game->player.invulnerableTimer > 0.0f) {
        game->player.invulnerableTimer -= dt;
        if (game->player.invulnerableTimer < 0.0f) {
            game->player.invulnerableTimer = 0.0f;
        }
    }

    /* ❤️ 회복의 붕대: 일정 시간마다 HP 자동 회복 */
    if (game->player.hpRecoveryLevel > 0) {
        static const int   amounts[4]   = {0, 2, 3, 5};
        static const float intervals[4] = {0.0f, 240.0f, 240.0f, 240.0f};

        game->player.hpRecoveryTimer -= dt;
        if (game->player.hpRecoveryTimer <= 0.0f) {
            game->player.health += amounts[game->player.hpRecoveryLevel];
            if (game->player.health > game->player.maxHealth) {
                game->player.health = game->player.maxHealth;
            }
            game->player.hpRecoveryTimer = intervals[game->player.hpRecoveryLevel];
        }
    }

    /* 🛡️ 무적의 방패: 쿨타임마다 15초 자동 무적 발동 */
    if (game->player.shieldLevel > 0) {
        static const float cooldowns[4] = {0.0f, 120.0f, 90.0f, 60.0f};

        if (game->player.shieldTimer > 0.0f) {
            game->player.shieldTimer -= dt;
            if (game->player.shieldTimer < 0.0f) {
                game->player.shieldTimer = 0.0f;
            }
        } else {
            game->player.shieldCooldown -= dt;
            if (game->player.shieldCooldown <= 0.0f) {
                game->player.shieldTimer = 15.0f;
                game->player.shieldCooldown = cooldowns[game->player.shieldLevel];
                game->auraPulseTimer = 0.22f;  /* 방패 발동 시각 효과 */
            }
        }
    }

    if (input->left && game->bossForbiddenKey != 'A') {
        dx--;
    }
    if (input->right && game->bossForbiddenKey != 'D') {
        dx++;
    }
    if (input->up && game->bossForbiddenKey != 'W') {
        dy--;
    }
    if (input->down && game->bossForbiddenKey != 'S') {
        dy++;
    }

    if ((dx != 0 || dy != 0) && game->player.moveCooldown <= 0.0f) {
        TryMovePlayer(game, dx, dy);
        /* 이동속도 배율 적용: 쿨타임을 배율로 나눔 */
        game->player.moveCooldown = 0.105f / game->player.moveSpeedMult;
        if (game->bossOrbCarried) {
            game->player.moveCooldown *= 1.65f;
        }
    }
}

void EnemiesSpawnWave(Game *game)
{
    int slot = -1;
    int side;
    int x;
    int y;
    EnemyType type = ENEMY_ONE_HP;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return;
    }

    if (game->elapsed > game->highEnemyStart && rand() % 100 < game->highEnemyChance) {
        type = ENEMY_FORTY_HP;
    } else if (game->elapsed > game->midEnemyStart && rand() % 100 < game->midEnemyChance) {
        type = ENEMY_THREE_HP;
    }

    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);
    const int minDist = 10;

    for (int attempt = 0; attempt < 20; attempt++) {
        side = rand() % 4;
        if (side == 0) {
            x = 1 + rand() % (game->mapWidth - 2);
            y = 1;
        } else if (side == 1) {
            x = 1 + rand() % (game->mapWidth - 2);
            y = game->mapHeight - 2;
        } else if (side == 2) {
            x = 1;
            y = 1 + rand() % (game->mapHeight - 2);
        } else {
            x = game->mapWidth - 2;
            y = 1 + rand() % (game->mapHeight - 2);
        }
        if (abs(x - playerX) + abs(y - playerY) >= minDist && !GameMapIsBlocked(game, x, y)) {
            break;
        }
    }

    while (GameMapIsBlocked(game, x, y)) {
        x = 1 + rand() % (game->mapWidth - 2);
        y = 1 + rand() % (game->mapHeight - 2);
    }

    {
        int steps = (int)(game->elapsed / 120.0f);
        float speedScale = 1.0f - steps * 0.03f;
        if (speedScale < 0.5f) speedScale = 0.5f;
        if (type == ENEMY_ONE_HP) {
            game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 1, 1, 1, 2, 10, 0.0f, 0.30f * speedScale, 'b'};
        } else if (type == ENEMY_THREE_HP) {
            game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 3, 3, 2, 6, 35, 0.0f, 0.50f * speedScale, 'G'};
        } else {
            game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 40, 40, 6, 50, 450, 0.0f, 0.95f * speedScale, 'V'};
        }
    }
}

void EnemiesSpawnBatStorm(Game *game, int count)
{
    for (int i = 0; i < count; i++) {
        const int side = rand() % 4;
        int x;
        int y;

        if (side == 0) {
            x = 1 + rand() % (game->mapWidth - 2);
            y = 1;
        } else if (side == 1) {
            x = 1 + rand() % (game->mapWidth - 2);
            y = game->mapHeight - 2;
        } else if (side == 2) {
            x = 1;
            y = 1 + rand() % (game->mapHeight - 2);
        } else {
            x = game->mapWidth - 2;
            y = 1 + rand() % (game->mapHeight - 2);
        }

        (void)SpawnEnemyAt(game, ENEMY_ONE_HP, x, y);
    }
}

static EnemyType RollGraveyardEnemyType(const Game *game)
{
    const int roll = rand() % 100;

    if (game->difficulty == DIFFICULTY_HARD) {
        if (roll < 20) {
            return ENEMY_FORTY_HP;
        }
        if (roll < 82) {
            return ENEMY_THREE_HP;
        }
        return ENEMY_ONE_HP;
    }

    if (roll < 12) {
        return ENEMY_FORTY_HP;
    }
    if (roll < 62) {
        return ENEMY_THREE_HP;
    }
    return ENEMY_ONE_HP;
}

static bool SpawnEnemyNearGrave(Game *game, int graveIndex, EnemyType type)
{
    int graveX;
    int graveY;

    if (!GameGraveyardSpawnPoint(game, graveIndex, &graveX, &graveY)) {
        return false;
    }

    for (int radius = 1; radius <= 3; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                const int x = graveX + dx;
                const int y = graveY + dy;

                if (abs(dx) != radius && abs(dy) != radius) {
                    continue;
                }
                if (x <= 0 || x >= game->mapWidth - 1 ||
                    y <= 0 || y >= game->mapHeight - 1 ||
                    GameMapIsBlocked(game, x, y) ||
                    IsEnemyAt(game, x, y)) {
                    continue;
                }

                return SpawnEnemyAt(game, type, x, y);
            }
        }
    }

    return false;
}

void EnemiesSpawnGraveyardWave(Game *game)
{
    if (game->mapPhase != MAP_PHASE_GRAVEYARD) {
        return;
    }

    for (int graveIndex = 0; graveIndex < 4; graveIndex++) {
        (void)SpawnEnemyNearGrave(game, graveIndex, RollGraveyardEnemyType(game));
    }
}

void EnemiesUpdate(Game *game, float dt)
{
    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);
    static int distances[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];

    BuildEnemyDistanceMap(game, distances, playerX, playerY);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &game->enemies[i];
        int enemyX;
        int enemyY;

        if (!enemy->active) {
            continue;
        }

        enemyX = GameRound(enemy->position.x);
        enemyY = GameRound(enemy->position.y);

        if (abs(enemyX - playerX) <= 1 && abs(enemyY - playerY) <= 1) {
            /* 방패 무적(shieldTimer) 또는 피격 무적(invulnerableTimer) 중이면 피해 없음 */
            if (game->player.invulnerableTimer <= 0.0f &&
                game->player.shieldTimer <= 0.0f) {
                game->player.health -= enemy->damage;
                game->player.invulnerableTimer = 0.85f;
                GameBossOnPlayerHit(game);
                GameRequestSound(game, SOUND_HIT);
            }
            continue;
        }

        enemy->moveCooldown -= dt;
        if (enemy->moveCooldown > 0.0f) {
            continue;
        }

        (void)TryMoveEnemyTowardPlayer(game, enemy, distances);
        enemy->moveCooldown = enemy->moveDelay *
            (game->activeMiniEvent == MINI_EVENT_BLOOD_MIST ? 0.62f : 1.0f);
    }
}

static bool PickupDropAllowed(const Game *game, PickupType type)
{
    if (type == PICKUP_VACUUM) {
        return game->player.level >= 3 || game->elapsed >= 45.0f;
    }
    if (type == PICKUP_ROSARY) {
        return game->elapsed >= 60.0f;
    }

    return true;
}

static PickupType RollBonusPickup(const Game *game)
{
    const int diceThreshold = PICKUP_HEAL_PACK_CHANCE + PICKUP_GAMBLER_DICE_CHANCE;
    const int frenzyThreshold = diceThreshold + PICKUP_FRENZY_MAGAZINE_CHANCE;
    const int pinataThreshold = frenzyThreshold + PICKUP_PINATA_SKULL_CHANCE;
    const int vacuumThreshold = pinataThreshold + PICKUP_VACUUM_CHANCE;
    const int rosaryThreshold = vacuumThreshold + PICKUP_ROSARY_CHANCE;
    const int roll = rand() % PICKUP_BONUS_ROLL_MAX;

    if (roll < PICKUP_HEAL_PACK_CHANCE) {
        return PICKUP_HEAL_PACK;
    }
    if (roll < diceThreshold) {
        return PICKUP_GAMBLER_DICE;
    }
    if (roll < frenzyThreshold) {
        return PICKUP_FRENZY_MAGAZINE;
    }
    if (roll < pinataThreshold) {
        return PICKUP_PINATA_SKULL;
    }
    if (roll < vacuumThreshold && PickupDropAllowed(game, PICKUP_VACUUM)) {
        return PICKUP_VACUUM;
    }
    if (roll < rosaryThreshold && PickupDropAllowed(game, PICKUP_ROSARY)) {
        return PICKUP_ROSARY;
    }

    return PICKUP_XP;
}

void EnemyDefeat(Game *game, Enemy *enemy)
{
    if (!enemy->active) {
        return;
    }

    PickupSpawn(game, enemy->position, enemy->xpValue);

    {
        const PickupType bonusType = RollBonusPickup(game);
        if (bonusType != PICKUP_XP) {
            PickupSpawnTyped(game, enemy->position, bonusType, 0);
        }
    }

    game->player.score += enemy->scoreValue;
    game->player.kills++;
    enemy->active = false;
}

void EnemiesDamageInRadius(Game *game, Vec2 center, int radius, int damage)
{
    const float radiusSquared = (float)(radius * radius);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &game->enemies[i];
        if (!enemy->active) {
            continue;
        }

        if (GameDistanceSquared(enemy->position, center) <= radiusSquared) {
            enemy->health -= damage;
            if (enemy->health <= 0) {
                EnemyDefeat(game, enemy);
            }
        }
    }

    GameBossDamageInRadius(game, center, radius, damage);
}

static Pickup *FindReusablePickupSlot(Game *game, PickupType type, int value)
{
    Pickup *mergeTarget = NULL;
    Pickup *replaceTarget = NULL;
    Pickup *xpValueTarget = NULL;
    int replacePriority = 100;

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (!pickup->active) {
            return pickup;
        }
        if (pickup->type == PICKUP_XP) {
            if (mergeTarget == NULL) {
                mergeTarget = pickup;
            } else if (xpValueTarget == NULL) {
                xpValueTarget = pickup;
            }
        }
        if (PickupPriority(pickup->type) < replacePriority) {
            replaceTarget = pickup;
            replacePriority = PickupPriority(pickup->type);
        }
    }

    if (type == PICKUP_XP && mergeTarget != NULL) {
        mergeTarget->value += value;
        return NULL;
    }

    if (type == PICKUP_XP && mergeTarget == NULL) {
        game->player.xp += value;
        game->player.score += value;
        return NULL;
    }

    if (PickupPriority(type) >= replacePriority) {
        if (replaceTarget != NULL &&
            replaceTarget->type == PICKUP_XP &&
            xpValueTarget == NULL) {
            game->player.xp += replaceTarget->value;
            game->player.score += replaceTarget->value;
        }
        else if (replaceTarget != NULL &&
            replaceTarget->type == PICKUP_XP &&
            xpValueTarget != NULL) {
            xpValueTarget->value += replaceTarget->value;
        }
        return replaceTarget;
    }

    return NULL;
}

void PickupSpawnTyped(Game *game, Vec2 position, PickupType type, int value)
{
    Pickup *pickup = FindReusablePickupSlot(game, type, value);

    if (pickup == NULL) {
        return;
    }

    pickup->active = true;
    pickup->type = type;
    pickup->position = position;
    pickup->value = value;
    pickup->moveCooldown = 0.0f;
}

void PickupSpawn(Game *game, Vec2 position, int value)
{
    PickupSpawnTyped(game, position, PICKUP_XP, value);
}

static int CollectAllXpPickups(Game *game)
{
    int collected = 0;

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (!pickup->active || pickup->type != PICKUP_XP) {
            continue;
        }

        game->player.xp += pickup->value;
        game->player.score += pickup->value;
        pickup->active = false;
        collected++;
    }

    return collected;
}

static void DefeatAllEnemies(Game *game)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &game->enemies[i];
        if (!enemy->active) {
            continue;
        }

        EnemyDefeat(game, enemy);
    }
}

static void HealPlayer(Game *game, int amount)
{
    game->player.health += amount;
    if (game->player.health > game->player.maxHealth) {
        game->player.health = game->player.maxHealth;
    }
}

static const char *PickupEffectName(PickupType type)
{
    switch (type) {
        case PICKUP_HEAL_PACK:
            return "Heal Pack";
        case PICKUP_VACUUM:
            return "진공";
        case PICKUP_ROSARY:
            return "로자리";
        case PICKUP_FRENZY_MAGAZINE:
            return "폭주 탄창";
        case PICKUP_PINATA_SKULL:
            return "보물상자";
        case PICKUP_XP:
            return "XP";
        case PICKUP_GAMBLER_DICE:
            return "도박사의 주사위";
        default:
            return "알 수 없는 효과";
    }
}

static void SetDiceMessage(Game *game, const char *effectName)
{
    (void)snprintf(game->diceMessage,
        sizeof(game->diceMessage),
        "DICE: %s 효과가 발동했습니다!",
        effectName);
    game->diceMessageTimer = 5.0f;
}

static Vec2 ScatterPosition(const Game *game, Vec2 center, int index, int count, int radius)
{
    const float angle = (float)index * 6.28318530718f / (float)count;
    int x = GameRound(center.x + cosf(angle) * (float)radius);
    int y = GameRound(center.y + sinf(angle) * (float)radius);

    x = GameClampInt(x, 1, game->mapWidth - 2);
    y = GameClampInt(y, 1, game->mapHeight - 2);

    if (GameMapIsBlocked(game, x, y)) {
        x = GameClampInt(GameRound(center.x), 1, game->mapWidth - 2);
        y = GameClampInt(GameRound(center.y), 1, game->mapHeight - 2);
    }

    return (Vec2){(float)x, (float)y};
}

static void SpawnPinataRewards(Game *game, Vec2 center)
{
    for (int i = 0; i < 6; i++) {
        PickupSpawn(game, ScatterPosition(game, center, i, 6, 3), 2 + rand() % 4);
    }
    for (int i = 0; i < 4; i++) {
        PickupSpawn(game, ScatterPosition(game, center, i, 4, 5), 8 + rand() % 8);
    }
    for (int i = 0; i < 2; i++) {
        PickupSpawnTyped(game, ScatterPosition(game, center, i, 2, 2), PICKUP_HEAL_PACK, 0);
    }
}

static void UpgradeCurrentWeapon(Game *game)
{
    Weapon *weapon = &game->weapons[game->activeWeapon];

    if (weapon->level < 3) {
        weapon->level++;
    }
    weapon->damage += GameClampInt(weapon->damage / 4, 5, 20);
    weapon->range += 2;
    weapon->timer = 0.0f;
}

static void ApplyPickupEffect(Game *game, PickupType type, int value, Vec2 position);

void PickupApplyGamblerDiceEffect(Game *game, int effect)
{
    const int resolvedEffect = effect < 0 ? rand() % 4 : effect % 4;

    switch (resolvedEffect) {
        case 0: /* HP 회복 */
            HealPlayer(game, GameClampInt(game->player.maxHealth / 2, 5, 10));
            SetDiceMessage(game, "HP 회복");
            GameRequestSound(game, SOUND_HEAL_PICKUP);
            break;
        case 1: /* 폭발 */
            game->auraPulseTimer = 0.35f;
            EnemiesDamageInRadius(game, game->player.position, 7, 55);
            SetDiceMessage(game, "폭발");
            GameRequestSound(game, SOUND_POWER_PICKUP);
            break;
        case 2: /* 좋은 무기 */
            UpgradeCurrentWeapon(game);
            SetDiceMessage(game, "좋은 무기 강화");
            GameRequestSound(game, SOUND_LEVEL_UP);
            break;
        case 3: /* 무작위 pickup 효과 */
            {
                static const PickupType randomEffects[] = {
                    PICKUP_HEAL_PACK,
                    PICKUP_VACUUM,
                    PICKUP_ROSARY,
                    PICKUP_FRENZY_MAGAZINE,
                    PICKUP_PINATA_SKULL
                };
                const int count = (int)(sizeof(randomEffects) / sizeof(randomEffects[0]));
                const PickupType pickedEffect = randomEffects[rand() % count];
                SetDiceMessage(game, PickupEffectName(pickedEffect));
                ApplyPickupEffect(game, pickedEffect, 0, game->player.position);
            }
            break;
        default:
            break;
    }
}

static void ApplyPickupEffect(Game *game, PickupType type, int value, Vec2 position)
{
    switch (type) {
        case PICKUP_XP:
            game->player.xp += value;
            game->player.score += value;
            GameRequestSound(game, SOUND_XP_PICKUP);
            break;
        case PICKUP_HEAL_PACK:
            {
                const int healAmount = GameClampInt(game->player.maxHealth / 2, 4, 8);
                HealPlayer(game, healAmount);
                GameRequestSound(game, SOUND_HEAL_PICKUP);
            }
            break;
        case PICKUP_VACUUM:
            if (CollectAllXpPickups(game) > 0) {
                GameRequestSound(game, SOUND_XP_PICKUP);
            }
            GameRequestSound(game, SOUND_POWER_PICKUP);
            break;
        case PICKUP_ROSARY:
            DefeatAllEnemies(game);
            GameRequestSound(game, SOUND_POWER_PICKUP);
            break;
        case PICKUP_GAMBLER_DICE:
            PickupApplyGamblerDiceEffect(game, -1);
            break;
        case PICKUP_FRENZY_MAGAZINE:
            game->frenzyTimer = FRENZY_MAGAZINE_SECONDS;
            game->frenzyShotTimer = 0.0f;
            GameRequestSound(game, SOUND_POWER_PICKUP);
            break;
        case PICKUP_PINATA_SKULL:
            SpawnPinataRewards(game, position);
            GameRequestSound(game, SOUND_POWER_PICKUP);
            break;
        default:
            break;
    }
}

static void CollectPickup(Game *game, Pickup *pickup)
{
    const PickupType type = pickup->type;
    const int value = pickup->value;
    const Vec2 position = pickup->position;

    pickup->active = false;
    ApplyPickupEffect(game, type, value, position);
}

void PickupsUpdate(Game *game, float dt)
{
    const float magnetDistance = (float)game->player.magnetRange;

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        float distanceSquared;

        if (!pickup->active) {
            continue;
        }

        distanceSquared = GameDistanceSquared(pickup->position, game->player.position);
        if (distanceSquared <= PICKUP_COLLECT_DISTANCE * PICKUP_COLLECT_DISTANCE) {
            CollectPickup(game, pickup);
            continue;
        }

        pickup->moveCooldown -= dt;
        if (pickup->type == PICKUP_XP &&
            distanceSquared <= magnetDistance * magnetDistance &&
            pickup->moveCooldown <= 0.0f) {
            const int pickupX = GameRound(pickup->position.x);
            const int pickupY = GameRound(pickup->position.y);
            const int playerX = GameRound(game->player.position.x);
            const int playerY = GameRound(game->player.position.y);
            const int nextX = pickupX + SignInt(playerX - pickupX);
            const int nextY = pickupY + SignInt(playerY - pickupY);

            if (!GameMapIsBlocked(game, nextX, nextY)) {
                pickup->position.x = (float)nextX;
                pickup->position.y = (float)nextY;
            }
            pickup->moveCooldown = 0.07f;
        }
    }
}
