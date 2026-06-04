#include "game.h"

#include <stdlib.h>

#define ENEMY_UNREACHABLE 1000000
#define PICKUP_COLLECT_DISTANCE 0.8f
#define PICKUP_BONUS_ROLL_MAX 1000
#define PICKUP_TREASURE_CHEST_CHANCE 55

static int PickupPriority(PickupType type)
{
    switch (type) {
        case PICKUP_XP:
            return 0;
        case PICKUP_TREASURE_CHEST:
            return 1;
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

/* 적들이 벽을 돌아서 추적하도록 플레이어 기준 거리 맵을 만든다 */
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

/* 시간 경과에 따른 체력/속도 스케일을 포함해 적 기본 스탯을 생성 */
static Enemy CreateEnemy(EnemyType type, int x, int y, float elapsed)
{
    const int steps = (int)(elapsed / 120.0f);
    float speedScale = 1.0f - (float)steps * 0.03f;
    const float hpMult = 1.0f + (float)steps * 0.10f;
    const int hpBat = (int)(20.0f * hpMult + 0.5f);
    const int hpZomb = (int)(50.0f * hpMult + 0.5f);
    const int hpVamp = (int)(120.0f * hpMult + 0.5f);

    if (speedScale < 0.5f) {
        speedScale = 0.5f;
    }

    if (type == ENEMY_ONE_HP) {
        return (Enemy){true, type, {(float)x, (float)y}, hpBat, hpBat, 1, 2, 10, 0.0f, 0.30f * speedScale, 'b'};
    }
    if (type == ENEMY_THREE_HP) {
        return (Enemy){true, type, {(float)x, (float)y}, hpZomb, hpZomb, 2, 6, 35, 0.0f, 0.50f * speedScale, 'G'};
    }
    return (Enemy){true, type, {(float)x, (float)y}, hpVamp, hpVamp, 6, 50, 450, 0.0f, 0.95f * speedScale, 'V'};
}

/* 방패가 깨질 때 남은 적을 정리하고 폭발 연출 타이머를 켠다 */
static void ShieldBreak(Game *game)
{
    game->player.shieldTimer = 0.0f;
    game->player.shieldHits  = 0;
    game->shieldBreakTimer   = 1.2f;
    for (int si = 0; si < MAX_ENEMIES; si++) {
        Enemy *se = &game->enemies[si];
        if (se->active) {
            EnemyDefeat(game, se);
        }
    }
}

/* 플레이어 이동과 패시브 아이템 타이머를 한 프레임씩 진행 */
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

    /* 회복의 붕대: 180초 활성 창, 만료 시 소멸 */
    if (game->player.hpRecoveryLevel > 0 && game->player.hpRecoveryTimer > 0.0f) {
        game->player.hpRecoveryTimer -= dt;
        if (game->player.hpRecoveryTimer <= 0.0f) {
            game->player.hpRecoveryTimer  = 0.0f;
            game->player.hpRecoveryLevel  = 0;
            game->player.bandageKillCount = 0;
        }
    }

    /* 무적의 방패: 5번 방어 또는 20초 만료 시 화면 몬스터 전멸 */
    if (game->player.shieldTimer > 0.0f) {
        game->player.shieldTimer -= dt;
        if (game->player.shieldTimer <= 0.0f) {
            ShieldBreak(game);
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

static bool SpawnEnemyAt(Game *game, EnemyType type, int x, int y)
{
    int slot = -1;

    if (GameMapIsBlocked(game, x, y) || IsEnemyAt(game, x, y)) {
        return false;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return false;
    }

    game->enemies[slot] = CreateEnemy(type, x, y, game->elapsed);

    return true;
}

/* 일반 외곽 웨이브를 하나 생성하고 난이도/시간에 따라 적 종류를 섞는다 */
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

    game->enemies[slot] = CreateEnemy(type, x, y, game->elapsed);
}

/* 박쥐 폭풍 이벤트용 약한 적을 외곽에 한꺼번에 소환 */
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

/* 적 접촉 피해와 거리 맵 기반 추적 이동을 처리 */
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
            if (game->player.invulnerableTimer <= 0.0f) {
                if (game->player.shieldTimer > 0.0f) {
                    game->player.shieldHits--;
                    game->player.invulnerableTimer = 0.85f;
                    if (game->player.shieldHits <= 0) {
                        ShieldBreak(game);
                    }
                } else {
                    game->player.health -= enemy->damage;
                    game->player.invulnerableTimer = 0.85f;
                    GameRequestSound(game, SOUND_HIT);
                }
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

/* 처치 수 증가와 회복의 붕대 20킬 회복 규칙을 한곳에서 처리 */
static void ApplyKillProgress(Game *game)
{
    game->player.kills++;

    if (game->player.hpRecoveryLevel > 0 && game->player.hpRecoveryTimer > 0.0f) {
        game->player.bandageKillCount++;
        if (game->player.bandageKillCount >= 20) {
            game->player.bandageKillCount = 0;
            if (game->player.health < game->player.maxHealth) {
                game->player.health++;
            }
        }
    }
}

/* 적 처치 시 XP/상자 드롭, 점수, 킬 진행도를 모두 반영 */
void EnemyDefeat(Game *game, Enemy *enemy)
{
    if (!enemy->active) {
        return;
    }

    PickupSpawn(game, enemy->position, enemy->xpValue);
    if ((rand() % PICKUP_BONUS_ROLL_MAX) < PICKUP_TREASURE_CHEST_CHANCE) {
        PickupSpawnTyped(game, enemy->position, PICKUP_TREASURE_CHEST, 0);
    }
    game->player.score += enemy->scoreValue;
    ApplyKillProgress(game);
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
}

/* pickup 슬롯이 꽉 찼을 때 XP 병합/즉시 지급/교체 우선순위를 결정 */
static Pickup *FindReusablePickupSlot(Game *game, PickupType type, int value)
{
    Pickup *emptyTarget = NULL;
    Pickup *mergeTarget = NULL;
    Pickup *replaceTarget = NULL;
    int replacePriority = 1000;

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (!pickup->active) {
            if (emptyTarget == NULL) {
                emptyTarget = pickup;
            }
            continue;
        }

        if (type == PICKUP_XP &&
            pickup->type == PICKUP_XP &&
            mergeTarget == NULL) {
            mergeTarget = pickup;
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

    if (emptyTarget != NULL) {
        return emptyTarget;
    }

    if (type == PICKUP_XP) {
        game->player.xp += value;
        game->player.score += value;
        return NULL;
    }

    if (replaceTarget != NULL && PickupPriority(type) >= replacePriority) {
        if (replaceTarget->type == PICKUP_XP) {
            game->player.xp += replaceTarget->value;
            game->player.score += replaceTarget->value;
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

/* pickup 획득 효과를 XP와 보물상자 두 갈래로 제한 */
static void ApplyPickupEffect(Game *game, PickupType type, int value, Vec2 position)
{
    (void)position;

    switch (type) {
        case PICKUP_XP:
            game->player.xp += value;
            game->player.score += value;
            GameRequestSound(game, SOUND_XP_PICKUP);
            break;
        case PICKUP_TREASURE_CHEST:
            GameStartRewardChoice(game);
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

/* pickup 획득 판정과 XP 자석 이동을 한 프레임씩 진행 */
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
