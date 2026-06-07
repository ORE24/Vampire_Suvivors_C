#include "game.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846f

/* 사거리 내 가장 가까운 적 인덱스 반환 (-1 = 없음) */
static int FindNearestEnemy(const Game *game, int range)
{
    int nearest = -1;
    float nearestDistance = 0.0f;
    const float maxDistance = (float)(range * range);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *enemy = &game->enemies[i];
        float distance;

        if (!enemy->active) {
            continue;
        }

        distance = GameDistanceSquared(game->player.position, enemy->position);
        if (distance > maxDistance) {
            continue;
        }

        if (nearest < 0 || distance < nearestDistance) {
            nearest = i;
            nearestDistance = distance;
        }
    }

    return nearest;
}

static void SpawnProjectile(Game *game, const Weapon *weapon, Vec2 direction, float speed, float lifetime, int pierce)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *projectile = &game->projectiles[i];
        if (projectile->active) {
            continue;
        }

        direction = GameNormalize(direction);
        projectile->active = true;
        projectile->position = game->player.position;
        projectile->velocity = GameScale(direction, speed);
        projectile->damage = weapon->damage;
        projectile->lifetime = lifetime;
        projectile->pierce = pierce;
        projectile->glyph = weapon->glyph;
        return;
    }
}

static bool ProjectilePathHitsWall(const Game *game, Vec2 from, Vec2 to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float distanceX = fabsf(dx);
    const float distanceY = fabsf(dy);
    const float maxDistance = distanceX > distanceY ? distanceX : distanceY;
    int steps = (int)ceilf(maxDistance * 4.0f);

    if (steps < 1) {
        steps = 1;
    }

    for (int step = 1; step <= steps; step++) {
        const float t = (float)step / (float)steps;
        const int x = GameRound(from.x + dx * t);
        const int y = GameRound(from.y + dy * t);

        if (GameMapIsBlocked(game, x, y)) {
            return true;
        }
    }

    return false;
}

/* ○ 원형: 가장 가까운 적에게 1발 조준 발사 / Lv.7: 30도씩 회전 발사 (빠른 속도) */
static void FireCircle(Game *game, Weapon *weapon)
{
    static float circleAngle = 0.0f;

    if (weapon->level >= 7) {
        /* Lv.7: 적 조준 없이 30도씩 시계방향 회전 발사 */
        const Vec2 dir = {cosf(circleAngle), sinf(circleAngle)};
        SpawnProjectile(game, weapon, dir, 15.0f, 2.0f, 1);
        circleAngle += PI / 6.0f;   /* 30도 */
        if (circleAngle >= 2.0f * PI) {
            circleAngle -= 2.0f * PI;
        }
    } else {
        const int targetIndex = FindNearestEnemy(game, weapon->range);
        Vec2 direction;
        if (targetIndex < 0) {
            return;
        }
        direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
        direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
        SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1);
    }
    GameRequestSound(game, SOUND_ATTACK);
}

/* △ 삼각형: 3발 부채꼴 / Lv.7: 5발 부채꼴 */
static void FireTriangle(Game *game, Weapon *weapon)
{
    const int targetIndex = FindNearestEnemy(game, weapon->range);
    Vec2 direction;
    float baseAngle;
    int i;
    int shotCount;

    if (targetIndex < 0) {
        return;
    }

    direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
    direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
    direction = GameNormalize(direction);
    baseAngle = atan2f(direction.y, direction.x);

    shotCount = (weapon->level >= 7) ? 5 : 3;

    /* 중심 기준 대칭 배치: 3발=-1,0,1 / 5발=-2,-1,0,1,2 (간격 0.2rad) */
    for (i = 0; i < shotCount; i++) {
        const float offset = (float)(i - shotCount / 2) * 0.2f;
        const float angle  = baseAngle + offset;
        SpawnProjectile(game, weapon, (Vec2){cosf(angle), sinf(angle)}, 13.0f, 1.8f, 1);
    }

    GameRequestSound(game, SOUND_ATTACK);
}

/* □ 사각형: 상하좌우 4방향 / Lv.7: 8방향 (45도 간격 추가) */
static void FireSquare(Game *game, Weapon *weapon)
{
    static const Vec2 dirs4[4] = {
        { 1.0f,    0.0f},   /* 오른쪽 */
        {-1.0f,    0.0f},   /* 왼쪽   */
        { 0.0f,    1.0f},   /* 아래   */
        { 0.0f,   -1.0f}    /* 위     */
    };
    /* 대각선 4방향 (정규화된 값) */
    static const Vec2 dirs8[8] = {
        { 1.0f,    0.0f},
        {-1.0f,    0.0f},
        { 0.0f,    1.0f},
        { 0.0f,   -1.0f},
        { 0.707f,  0.707f},
        { 0.707f, -0.707f},
        {-0.707f,  0.707f},
        {-0.707f, -0.707f}
    };
    int i;
    int count;
    const Vec2 *dirs;

    if (weapon->level >= 7) {
        count = 8;
        dirs  = dirs8;
    } else {
        count = 4;
        dirs  = dirs4;
    }

    for (i = 0; i < count; i++) {
        SpawnProjectile(game, weapon, dirs[i], 13.0f, 1.8f, 1);
    }

    GameRequestSound(game, SOUND_ATTACK);
}

/* ★ 별: 랜덤 방향 레이저 (그 줄 전체 적 즉시 피해) / Lv.7: 가로+세로 동시 */
static void FireStar(Game *game, Weapon *weapon)
{
    const int playerX = GameRound(game->player.position.x);
    const int playerY = GameRound(game->player.position.y);
    bool doHorizontal;
    bool doVertical;
    int i;

    if (weapon->level >= 7) {
        /* Lv.7: 4방향 (가로 + 세로) 동시 레이저 */
        doHorizontal = true;
        doVertical   = true;
    } else {
        /* 일반: 가로 또는 세로 랜덤 */
        doHorizontal = (rand() % 2 == 0);
        doVertical   = !doHorizontal;
    }

    /* 레이저 시각 효과 설정 */
    game->laser.active     = true;
    game->laser.timer      = 0.4f;
    game->laser.horizontal = doHorizontal;
    game->laser.vertical   = doVertical;
    game->laser.row        = playerY;
    game->laser.col        = playerX;

    /* 레이저 범위 내 모든 적 즉시 피해 */
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *enemy = &game->enemies[i];
        if (!enemy->active) continue;
        {
            const int ex = GameRound(enemy->position.x);
            const int ey = GameRound(enemy->position.y);
            bool hit = false;

            if (doHorizontal && ey == playerY) hit = true;
            if (doVertical   && ex == playerX) hit = true;

            if (hit) {
                enemy->health -= weapon->damage;
                if (enemy->health <= 0) {
                    PickupSpawn(game, enemy->position, enemy->xpValue);
                    game->player.score += enemy->scoreValue;
                    game->player.kills++;
                    enemy->active = false;
                }
            }
        }
    }

    GameRequestSound(game, SOUND_ATTACK);
}

/* 현재 장착 무기만 발사 (PPT: 무기는 항상 1개) */
void WeaponsUpdate(Game *game, float dt)
{
    Weapon *weapon = &game->weapons[game->activeWeapon];
    float effectiveCooldown;

    /* 원형 Lv.7: 기본 쿨타임 대신 0.4s 고속 발사 */
    if (game->activeWeapon == WEAPON_MAGIC_BOLT && weapon->level >= 7) {
        effectiveCooldown = (game->player.attackSpeedMult > 0.0f)
            ? 0.4f / game->player.attackSpeedMult
            : 0.4f;
    } else {
        effectiveCooldown = (game->player.attackSpeedMult > 0.0f)
            ? weapon->cooldown / game->player.attackSpeedMult
            : weapon->cooldown;
    }

    weapon->timer -= dt;
    if (weapon->timer <= 0.0f) {
        switch (game->activeWeapon) {
            case WEAPON_MAGIC_BOLT:
                FireCircle(game, weapon);
                break;
            case WEAPON_HOLY_AURA:
                FireTriangle(game, weapon);
                break;
            case WEAPON_PIERCING_LANCE:
                FireSquare(game, weapon);
                break;
            case WEAPON_STAR_BURST:
                FireStar(game, weapon);
                break;
            default:
                break;
        }
        weapon->timer += effectiveCooldown;
    }
}

void ProjectilesUpdate(Game *game, float dt)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *projectile = &game->projectiles[i];

        if (!projectile->active) {
            continue;
        }

        {
            const Vec2 previousPosition = projectile->position;
            const Vec2 nextPosition = GameAdd(projectile->position, GameScale(projectile->velocity, dt));

            projectile->position = nextPosition;
            projectile->lifetime -= dt;

            if (projectile->lifetime <= 0.0f ||
                ProjectilePathHitsWall(game, previousPosition, nextPosition)) {
                projectile->active = false;
            }
        }
    }
}

void CombatResolve(Game *game)
{
    for (int projectileIndex = 0; projectileIndex < MAX_PROJECTILES; projectileIndex++) {
        Projectile *projectile = &game->projectiles[projectileIndex];

        if (!projectile->active) {
            continue;
        }

        for (int enemyIndex = 0; enemyIndex < MAX_ENEMIES; enemyIndex++) {
            Enemy *enemy = &game->enemies[enemyIndex];

            if (!enemy->active) {
                continue;
            }

            if (GameDistanceSquared(projectile->position, enemy->position) > 0.70f) {
                continue;
            }

            enemy->health -= projectile->damage;
            projectile->pierce--;
            if (projectile->pierce <= 0) {
                projectile->active = false;
            }

            if (enemy->health <= 0) {
                PickupSpawn(game, enemy->position, enemy->xpValue);
                game->player.score += enemy->scoreValue;
                game->player.kills++;
                enemy->active = false;
            }
            break;
        }
    }
}
