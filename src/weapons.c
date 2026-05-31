#include "game.h"

#include <math.h>

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

/* ○ 원형: 가장 가까운 적에게 1발 조준 발사 (Dmg20, 쿨1.0s) */
static void FireCircle(Game *game, Weapon *weapon)
{
    const int targetIndex = FindNearestEnemy(game, weapon->range);
    Vec2 direction;

    if (targetIndex < 0) {
        return;
    }

    direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
    direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
    SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1);
    GameRequestSound(game, SOUND_ATTACK);
}

/* △ 삼각형: 가장 가까운 적 방향으로 3발 부채꼴 발사 (Dmg8 x3, 쿨0.4s) */
static void FireTriangle(Game *game, Weapon *weapon)
{
    const int targetIndex = FindNearestEnemy(game, weapon->range);
    Vec2 direction;
    float baseAngle;
    int i;

    if (targetIndex < 0) {
        return;
    }

    direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
    direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
    direction = GameNormalize(direction);
    baseAngle = atan2f(direction.y, direction.x);

    /* 0, -0.2rad, +0.2rad 세 방향 */
    for (i = -1; i <= 1; i++) {
        const float angle = baseAngle + (float)i * 0.2f;
        SpawnProjectile(game, weapon, (Vec2){cosf(angle), sinf(angle)}, 13.0f, 1.8f, 1);
    }

    GameRequestSound(game, SOUND_ATTACK);
}

/* □ 사각형: 상하좌우 4방향 동시 발사 (Dmg15 x4, 쿨1.0s) */
static void FireSquare(Game *game, Weapon *weapon)
{
    static const Vec2 dirs[4] = {
        { 1.0f,  0.0f},  /* 오른쪽 */
        {-1.0f,  0.0f},  /* 왼쪽   */
        { 0.0f,  1.0f},  /* 아래   */
        { 0.0f, -1.0f}   /* 위     */
    };
    int i;

    for (i = 0; i < 4; i++) {
        SpawnProjectile(game, weapon, dirs[i], 13.0f, 1.8f, 1);
    }

    GameRequestSound(game, SOUND_ATTACK);
}

/* ★ 별: 가장 가까운 적에게 강타 1발 (Dmg40, 쿨2.0s) */
static void FireStar(Game *game, Weapon *weapon)
{
    const int targetIndex = FindNearestEnemy(game, weapon->range);
    Vec2 direction;

    if (targetIndex < 0) {
        return;
    }

    direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
    direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
    SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1);
    GameRequestSound(game, SOUND_ATTACK);
}

/* 현재 장착 무기만 발사 (PPT: 무기는 항상 1개) */
void WeaponsUpdate(Game *game, float dt)
{
    Weapon *weapon = &game->weapons[game->activeWeapon];
    /* 공격속도 배율 적용: 쿨타임을 배율로 나눔 */
    const float effectiveCooldown = (game->player.attackSpeedMult > 0.0f)
        ? weapon->cooldown / game->player.attackSpeedMult
        : weapon->cooldown;

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
