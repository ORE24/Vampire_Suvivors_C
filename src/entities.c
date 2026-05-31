#include "game.h"

#include <math.h>
#include <stdlib.h>

#define ENEMY_UNREACHABLE 1000000

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
        static const float intervals[4] = {0.0f, 30.0f, 20.0f, 15.0f};

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

    if (input->left) {
        dx--;
    }
    if (input->right) {
        dx++;
    }
    if (input->up) {
        dy--;
    }
    if (input->down) {
        dy++;
    }

    if ((dx != 0 || dy != 0) && game->player.moveCooldown <= 0.0f) {
        TryMovePlayer(game, dx, dy);
        /* 이동속도 배율 적용: 쿨타임을 배율로 나눔 */
        game->player.moveCooldown = 0.105f / game->player.moveSpeedMult;
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

    while (GameMapIsBlocked(game, x, y)) {
        x = 1 + rand() % (game->mapWidth - 2);
        y = 1 + rand() % (game->mapHeight - 2);
    }

    if (type == ENEMY_ONE_HP) {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
    } else if (type == ENEMY_THREE_HP) {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 3, 3, 2, 6, 35, 0.0f, 0.50f, 'G'};
    } else {
        game->enemies[slot] = (Enemy){true, type, {(float)x, (float)y}, 40, 40, 6, 50, 450, 0.0f, 0.95f, 'V'};
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
                GameRequestSound(game, SOUND_HIT);
            }
            continue;
        }

        enemy->moveCooldown -= dt;
        if (enemy->moveCooldown > 0.0f) {
            continue;
        }

        (void)TryMoveEnemyTowardPlayer(game, enemy, distances);
        enemy->moveCooldown = enemy->moveDelay;
    }
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
                PickupSpawn(game, enemy->position, enemy->xpValue);
                game->player.score += enemy->scoreValue;
                game->player.kills++;
                enemy->active = false;
            }
        }
    }
}

void PickupSpawn(Game *game, Vec2 position, int value)
{
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (pickup->active) {
            continue;
        }

        pickup->active = true;
        pickup->position = position;
        pickup->value = value;
        pickup->moveCooldown = 0.0f;
        return;
    }
}

void PickupsUpdate(Game *game, float dt)
{
    const float collectDistance = 0.8f;
    const float magnetDistance = (float)game->player.magnetRange;

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        float distanceSquared;

        if (!pickup->active) {
            continue;
        }

        distanceSquared = GameDistanceSquared(pickup->position, game->player.position);
        if (distanceSquared <= collectDistance * collectDistance) {
            game->player.xp += pickup->value;
            game->player.score += pickup->value;
            pickup->active = false;
            GameRequestSound(game, SOUND_XP);
            continue;
        }

        pickup->moveCooldown -= dt;
        if (distanceSquared <= magnetDistance * magnetDistance && pickup->moveCooldown <= 0.0f) {
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
