#include "game.h"

#include <stdlib.h>

#define ENEMY_UNREACHABLE 1000000
#define PICKUP_COLLECT_DISTANCE 0.8f
#define PICKUP_BONUS_ROLL_MAX 1000
#define PICKUP_TREASURE_CHEST_CHANCE 55


// [실행 시] value가 음수면 -1, 양수면 1, 0이면 0을 반환한다.
// pickup 자석 이동 방향 계산에 사용된다.
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

// [실행 시] 활성화된 모든 적을 순회하며 반올림한 좌표가 (x, y)와 일치하는 적이
// 있으면 true를 반환한다. 한 명도 없으면 false를 반환한다.
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

// [실행 시] 플레이어 위치(playerX, playerY)를 시작점으로 BFS(너비우선탐색)를 수행한다.
// 1. 맵 전체를 ENEMY_UNREACHABLE로 초기화한다.
// 2. 플레이어 칸을 거리 0으로 설정하고 큐에 삽입한다.
// 3. 큐에서 꺼낸 칸의 상하좌우를 탐색하며 벽이 아니고 아직 더 짧은 거리가
//    없는 칸에 거리를 기록하고 큐에 추가한다.
// 4. 큐가 비면 distances 배열에 각 칸에서 플레이어까지의 최단 거리가 채워진다.
// 적들이 벽을 돌아서 추적하도록 플레이어 기준 거리 맵을 만든다
static void BuildEnemyDistanceMap(const Game *game, int distances[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int playerX, int playerY)
{
    static int queueX[MAX_MAP_WIDTH * MAX_MAP_HEIGHT]; // 큐 : 먼저 넣은 것이 먼저 나오는 자료구조
    static int queueY[MAX_MAP_WIDTH * MAX_MAP_HEIGHT]; //x와 y를 배열로 관리하기
    static const int directions[4][2] = { // 맵 전체에 플레이어까지의 거리 기록 -> 적이 숫자가 작은쪽으로 한 칸씩 이동 계산
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1}
    };
    int head = 0;
    int tail = 0;

    for (int y = 0; y < game->mapHeight; y++) {
        for (int x = 0; x < game->mapWidth; x++) {
            distances[y][x] = ENEMY_UNREACHABLE; // 맵 전체 칸을 BFS전에 도달 불가 상태로 초기화
        }
    }

    if (GameMapIsBlocked(game, playerX, playerY)) { //방어적 프로그래밍. 플레이어가 벽 안에 있으면 BFS종료
        return;
    }

    distances[playerY][playerX] = 0;//플레이어칸을 0으로 처리 행렬, 컴퓨터 처리 방식 때문에 y가 먼저
    queueX[tail] = playerX;
    queueY[tail] = playerY;
    tail++;

    while (head < tail) {
        const int currentX = queueX[head]; //플레이어의 현재 좌표
        const int currentY = queueY[head];
        const int nextDistance = distances[currentY][currentX] + 1; //바로 옆은 0 + 1 되는 식
        head++;

        for (int i = 0; i < 4; i++) {
            const int nextX = currentX + directions[i][0]; //플레이어의 다음 네방향 위치 지정
            const int nextY = currentY + directions[i][1];

            if (nextX < 0 ||
                nextX >= game->mapWidth ||
                nextY < 0 ||
                nextY >= game->mapHeight ||
                GameMapIsBlocked(game, nextX, nextY) ||
                distances[nextY][nextX] <= nextDistance) {
                continue; //계산하는 칸이 맵 밖이면 건너뛰기
            }

            distances[nextY][nextX] = nextDistance;
            queueX[tail] = nextX;
            queueY[tail] = nextY;
            tail++;
        }
    }
}

// [실행 시] 적의 현재 위치에서 8방향을 모두 검사한다. (BFS)
// 1. 맵 경계 밖·벽·다른 적이 있는 칸·도달 불가 칸은 건너뛴다.
// 2. 유효한 칸 중 distances 값이 가장 작은 칸(플레이어에 가장 가까운 칸)을 선택한다.
// 3. 이동 가능한 칸이 있으면 적의 position을 해당 칸으로 갱신하고 true를 반환한다.
// 4. 이동할 곳이 없으면 false를 반환한다.
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
    bool foundMove = false; //8방향 다 false면 이동할 곳 X

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
            continue;//맵밖이거나 벽이거나 적이 있다면(도달불가) 건너뛰기
        }

        if (distances[nextY][nextX] < bestDistance) {
            bestX = nextX;
            bestY = nextY;
            bestDistance = distances[nextY][nextX];
            foundMove = true; // 더 가까운칸이며 이동가능하면 true로 설정
        }
    }

    enemy->position.x = (float)bestX;
    enemy->position.y = (float)bestY;
    return true; // 최종선택 칸으로 적 이동
}

// [실행 시] (dx, dy) 방향으로 이동할 다음 칸을 계산한다.
// 1. 맵 경계를 벗어나지 않도록 클램프(값을 특정 범위안에 가두기)한다.
// 2. 다음 칸이 벽이 아니면 플레이어 position을 해당 칸으로 갱신한다.
// 3. 벽이면 이동하지 않는다.
static void TryMovePlayer(Game *game, int dx, int dy)
{
    const int nextX = GameClampInt(GameRound(game->player.position.x) + dx, 1, game->mapWidth - 2);
    const int nextY = GameClampInt(GameRound(game->player.position.y) + dy, 1, game->mapHeight - 2);
    //clamp함수는 최솟값보다 작으면 min,최댓값보다 크면 max 반환.
    if (!GameMapIsBlocked(game, nextX, nextY)) {
        game->player.position.x = (float)nextX;
        game->player.position.y = (float)nextY;
    }
}

// [실행 시] elapsed를 90으로 나눈 단계 수로 속도/HP 배율을 산출한다. (1분 30초마다 단계 증가)
// 1. 단계당 HP는 +10%, 속도는 +3% 증가(최대 200% 유지)한다.
// 2. type에 따라 박쥐(b)·좀비(G)·뱀파이어(V) 중 하나의 Enemy 구조체를 반환한다.
// 시간 경과에 따른 체력/속도 스케일을 포함해 적 기본 스탯을 생성
static Enemy CreateEnemy(EnemyType type, int x, int y, float elapsed)
{
    const int steps = (int)(elapsed / 90.0f);
    float speedScale = 1.0f + (float)steps * 0.03f;
    const float hpMult = 1.0f + (float)steps * 0.10f;
    const int hpBat = (int)(20.0f * hpMult + 0.5f);
    const int hpZomb = (int)(50.0f * hpMult + 0.5f);
    const int hpVamp = (int)(120.0f * hpMult + 0.5f); //반올림 하기위해 0.5씩 덧셈

    if (speedScale > 2.0f) {
        speedScale = 2.0f;
    }

    if (type == ENEMY_ONE_HP) {
        return (Enemy) { true, type, { (float)x, (float)y }, hpBat, hpBat, 1, 2, 10, 0.0f, 0.30f * speedScale, 'b' };
    }
    if (type == ENEMY_THREE_HP) {
        return (Enemy) { true, type, { (float)x, (float)y }, hpZomb, hpZomb, 2, 6, 35, 0.0f, 0.50f * speedScale, 'G' };
    }
    return (Enemy) { true, type, { (float)x, (float)y }, hpVamp, hpVamp, 6, 50, 450, 0.0f, 0.95f * speedScale, 'V' };
} //one = 박쥐, three = 좀비 , forty = 뱀파이어 ->이름 바꾸자



// [실행 시] 방패 타이머와 히트 카운트를 0으로 초기화한다.
// 1. shieldBreakTimer를 1.2초로 설정해 폭발 연출을 시작한다.
// 2. 활성화된 모든 적을 EnemyDefeat로 제거한다.
// 방패가 깨질 때 남은 적을 정리하고 폭발 연출 타이머를 켠다
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

// [실행 시] dt만큼 이동 쿨타임과 무적 타이머를 감소시킨다.
// 1. 회복의 붕대가 활성 중이면 활성 창 타이머를 감소하고 만료 시 붕대를 제거한다.
// 2. 무적의 방패가 활성 중이면 방패 타이머를 감소하고 만료 시 ShieldBreak를 호출한다.
// 3. 입력(left/right/up/down)이 있고 이동 쿨타임이 0 이하면 TryMovePlayer를 호출한다.
// 4. 이동 후 이동속도 배율을 반영한 새 쿨타임을 설정한다.
// 플레이어 이동과 패시브 아이템 타이머를 한 프레임씩 진행
void PlayerUpdate(Game *game, const InputState *input, float dt)
{
    int dx = 0; //매 프레임마다 함수 처음부터 실행
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

    // 회복의 붕대: 180초 활성 창, 만료 시 소멸
    if (game->player.hpRecoveryLevel > 0 && game->player.hpRecoveryTimer > 0.0f) {
        game->player.hpRecoveryTimer -= dt;
        if (game->player.hpRecoveryTimer <= 0.0f) {
            game->player.hpRecoveryTimer  = 0.0f;
            game->player.hpRecoveryLevel  = 0;
            game->player.bandageKillCount = 0;
        }
    }

    // 무적의 방패: 5번 방어 또는 20초 만료 시 화면 몬스터 전멸
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

    if ((dx != 0 || dy != 0) && game->player.moveCooldown <= 0.0f) { //방향키를 눌렀는지 쿨타임이 끝났는지
        TryMovePlayer(game, dx, dy);
        // 이동속도 배율 적용: 쿨타임을 배율로 나눔
        game->player.moveCooldown = 0.105f / game->player.moveSpeedMult;
    }
}

// [실행 시] 목표 칸이 벽이거나 이미 적이 있으면 false를 반환한다.
// 1. 비어있는 enemies 슬롯을 탐색한다.
// 2. 슬롯이 없으면 false를 반환한다.
// 3. 슬롯이 있으면 CreateEnemy로 적을 생성하고 true를 반환한다.
static bool SpawnEnemyAt(Game *game, EnemyType type, int x, int y)
{//박쥐폭풍 소환 함수에서 사용
    int slot = -1;//초기값

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

// [실행 시] 빈 enemies 슬롯이 없으면 즉시 반환한다.
// 1. elapsed와 확률로 ENEMY_THREE_HP 또는 ENEMY_FORTY_HP 중 타입을 결정한다.
// 2. 플레이어에서 최소 거리(10) 이상인 맵 외곽 위치를 최대 20회 시도해 찾는다.
// 3. 유효한 위치에 CreateEnemy로 적을 생성하고 슬롯에 저장한다.
// 일반 외곽 웨이브를 하나 생성하고 난이도/시간에 따라 적 종류를 섞는다
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

   // elapsed > highEnemyStart — 충분한 시간이 지났는가 ?
   // rand() % 100 < highEnemyChance — 확률 체크
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


    game->enemies[slot] = CreateEnemy(type, x, y, game->elapsed);
}

// [실행 시] count번 반복하며 매번 맵 외곽 4면 중 하나를 무작위로 선택한다.
// 1. 선택된 면의 무작위 위치에 SpawnEnemyAt으로 ENEMY_ONE_HP(박쥐) 소환을 시도한다.
// 2. 슬롯이 없거나 칸이 막혔으면 해당 반복만 건너뛴다.
// 박쥐 폭풍 이벤트용 약한 적을 외곽에 한꺼번에 소환
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

// [실행 시] BuildEnemyDistanceMap으로 플레이어 기준 거리 맵을 갱신한다.
// 1. 모든 활성 적을 순회한다.
// 2. 적이 플레이어와 1칸 이내이면 이동을 건너뛰고 피해를 처리한다.
//    - 방패 활성 중이면 shieldHits를 줄이고 방패가 0이 되면 ShieldBreak를 호출한다.
//    - 방패 없이 무적이 아니면 health를 감소시키고 피격음을 재생한다.
// 3. 그렇지 않으면 moveCooldown을 dt만큼 감소시키고, 쿨타임이 0 이하이면
//    TryMoveEnemyTowardPlayer로 한 칸 이동 후 moveDelay를 다시 설정한다.
// 적 접촉 피해와 거리 맵 기반 추적 이동을 처리
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
            // 방패 무적(shieldTimer) 또는 피격 무적(invulnerableTimer) 중이면 피해 없음
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

// [실행 시] 플레이어 킬 수를 1 증가시킨다.
// 1. 회복의 붕대가 활성 중이면 bandageKillCount를 1 증가시킨다.
// 2. bandageKillCount가 20에 도달하면 초기화하고 체력이 최대치 미만이면 1 회복한다.
// 처치 수 증가와 회복의 붕대 20킬 회복 규칙을 한곳에서 처리
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

// [실행 시] 이미 비활성 적이면 즉시 반환한다.
// 1. 적의 위치에 XP pickup을 생성한다.
// 2. 1000분의 55 확률로 보물상자 pickup도 추가로 생성한다.
// 3. 적의 scoreValue를 플레이어 score에 더한다.
// 4. ApplyKillProgress로 킬 수 및 붕대 회복을 처리한다.
// 5. 적을 비활성화한다.
// 적 처치 시 XP/상자 드롭, 점수, 킬 진행도를 모두 반영
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

// [실행 시] 모든 활성 적을 순회하며 center까지의 거리 제곱을 계산한다.
// 1. radius 제곱 이내에 있는 적에게 damage만큼 체력을 감소시킨다.
// 2. 체력이 0 이하가 되면 EnemyDefeat를 호출한다.
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

// [실행 시] MAX_PICKUPS 슬롯을 전부 순회하며 빈 슬롯을 찾는다.
// 1. emptyTarget: 비어있는 슬롯 (최우선 사용).
// 2. 슬롯이 꽉 찼으면 NULL 반환 (생성 안 함).
// pickup 슬롯에서 빈 자리를 찾아 반환
static Pickup *FindReusablePickupSlot(Game *game)
{
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pickup = &game->pickups[i];
        if (!pickup->active) {
            return pickup;
        }
    }

    return NULL;
}

// [실행 시] FindReusablePickupSlot으로 사용 가능한 슬롯을 얻는다.
// 1. 슬롯이 NULL이면(병합·즉시 지급 처리됨) 반환한다.
// 2. 슬롯이 있으면 active, type, position, value, moveCooldown을 설정한다.
void PickupSpawnTyped(Game *game, Vec2 position, PickupType type, int value)
{
    Pickup *pickup = FindReusablePickupSlot(game);

    if (pickup == NULL) {
        return;
    }

    pickup->active = true;
    pickup->type = type;
    pickup->position = position;
    pickup->value = value;
    pickup->moveCooldown = 0.0f;
}

// [실행 시] type을 PICKUP_XP로 고정해 PickupSpawnTyped를 호출한다.
// 내부에서 슬롯 확보와 병합 처리가 이루어진다.
void PickupSpawn(Game *game, Vec2 position, int value)
{
    PickupSpawnTyped(game, position, PICKUP_XP, value);
}

// [실행 시] type에 따라 분기한다.
// 1. PICKUP_XP: player.xp와 player.score에 value를 더하고 XP 획득음을 재생한다.
// 2. PICKUP_TREASURE_CHEST: GameStartRewardChoice를 호출해 보상 선택 화면을 시작한다.
// pickup 획득 효과를 XP와 보물상자 두 갈래로 제한
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

// [실행 시] pickup을 비활성화한 뒤 type·value·position을 복사해 ApplyPickupEffect를
// 호출한다. 비활성화를 먼저 해야 효과 처리 중 슬롯 재사용이 가능하다.
static void CollectPickup(Game *game, Pickup *pickup)
{
    const PickupType type = pickup->type;
    const int value = pickup->value;
    const Vec2 position = pickup->position;

    pickup->active = false;
    ApplyPickupEffect(game, type, value, position);
}

// [실행 시] 모든 활성 pickup을 순회한다.
// 1. 플레이어와의 거리 제곱이 PICKUP_COLLECT_DISTANCE² 이하이면 CollectPickup을 호출한다.
// 2. XP pickup이 자석 범위 이내이고 moveCooldown이 0 이하이면
//    플레이어 방향으로 한 칸 이동시키고 cooldown을 0.07초로 재설정한다.
// 3. 이동 목표 칸이 벽이면 이동하지 않는다.
// pickup 획득 판정과 XP 자석 이동을 한 프레임씩 진행
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
