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

/* 무기 발사용 목표 좌표를 찾고, 없으면 발사를 생략하게 한다 */
static bool FindNearestTargetPosition(const Game *game, int range, Vec2 *position)
{
    const int enemyIndex = FindNearestEnemy(game, range);

    if (enemyIndex >= 0) {
        *position = game->enemies[enemyIndex].position;
        return true;
    }

    return false;
}

/* 비궤도 투사체 슬롯을 초기화해 무기별 발사 함수가 같은 생성 규칙을 쓰게 한다 */
static void SpawnProjectile(Game *game, const Weapon *weapon, Vec2 direction, float speed, float lifetime, int pierce, int areaHit)
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
        projectile->areaHit = areaHit;
        projectile->glyph = weapon->glyph;
        return;
    }
}

/* 빠른 투사체가 벽을 건너뛰지 않도록 이동 경로를 샘플링한다 */
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

/* ○ 원형: 가장 가까운 적 조준 1발 / Lv7 30도씩 회전 고속 발사 */
static void FireCircle(Game *game, Weapon *weapon)
{
    if (weapon->level >= 7) {
        static float circleAngle = 0.0f;
        const Vec2 dir = {cosf(circleAngle), sinf(circleAngle)};
        SpawnProjectile(game, weapon, dir, 15.0f, 2.0f, 1, 0);
        circleAngle += PI / 6.0f;
        if (circleAngle >= 2.0f * PI) circleAngle -= 2.0f * PI;
        GameRequestSound(game, SOUND_SHOOT);
        return;
    }

    {
        const int targetIndex = FindNearestEnemy(game, weapon->range);
        Vec2 direction;

        if (targetIndex < 0) return;

        direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
        direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
        SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1, 0);
        GameRequestSound(game, SOUND_SHOOT);
    }
}

/* △ 삼각형: 3발 부채꼴 / Lv.7: 5발 부채꼴 */
static void FireTriangle(Game *game, Weapon *weapon)
{
    Vec2 targetPosition;
    Vec2 direction;
    float baseAngle;
    int count = (weapon->level >= 7) ? 5 : weapon->projectileCount;
    int half = count / 2;
    int i;

    if (!FindNearestTargetPosition(game, weapon->range, &targetPosition)) {
        return;
    }

    direction.x = targetPosition.x - game->player.position.x;
    direction.y = targetPosition.y - game->player.position.y;
    direction = GameNormalize(direction);
    baseAngle = atan2f(direction.y, direction.x);

    for (i = -half; i <= half; i++) {
        const float angle = baseAngle + (float)i * 0.2f;
        SpawnProjectile(game, weapon, (Vec2){cosf(angle), sinf(angle)}, 13.0f, 1.8f, 1, 0);
    }

    GameRequestSound(game, SOUND_SHOOT);
}

/* □ 사각형: 상하좌우 4방향 / Lv.7: 8방향 전방위 */
static void FireSquare(Game *game, Weapon *weapon)
{
    static const Vec2 dirs4[4] = {
        { 1.0f,    0.0f},
        {-1.0f,    0.0f},
        { 0.0f,    1.0f},
        { 0.0f,   -1.0f}
    };
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
    int i, count;
    const Vec2 *dirs;

    if (weapon->level >= 7) {
        count = 8;
        dirs  = dirs8;
    } else {
        count = 4;
        dirs  = dirs4;
    }

    for (i = 0; i < count; i++) {
        SpawnProjectile(game, weapon, dirs[i], 13.0f, 1.8f, 1, 0);
    }

    GameRequestSound(game, SOUND_SHOOT);
}

/* ★ 별: 가장 가까운 적을 향해 발사 / Lv.7: 3발 부채꼴 */
static void FireStar(Game *game, Weapon *weapon)
{
    Vec2 targetPosition;
    Vec2 direction;
    float baseAngle;

    if (!FindNearestTargetPosition(game, weapon->range, &targetPosition)) {
        return;
    }

    direction.x = targetPosition.x - game->player.position.x;
    direction.y = targetPosition.y - game->player.position.y;
    direction = GameNormalize(direction);
    baseAngle = atan2f(direction.y, direction.x);

    if (weapon->level >= 7) {
        int s;
        for (s = -1; s <= 1; s++) {
            const float angle = baseAngle + (float)s * 0.2f;
            SpawnProjectile(game, weapon, (Vec2){cosf(angle), sinf(angle)}, 14.0f, 2.0f, 2, 0);
        }
    } else {
        SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1, 0);
    }

    GameRequestSound(game, SOUND_SHOOT);
}

/* 현재 장착 무기 발사 + 부패한 혜성 버스트 처리 */
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

    /* 버스트 연사 처리 (부패한 혜성 Lv7) */
    if (weapon->burstRemaining > 0) {
        weapon->burstTimer -= dt;
        if (weapon->burstTimer <= 0.0f) {
            Vec2 dir = weapon->burstDirection;
            float baseAngle = atan2f(dir.y, dir.x);
            for (int s = -1; s <= 1; s++) {
                float a = baseAngle + (float)s * 0.18f;
                SpawnProjectile(game, weapon, (Vec2){cosf(a), sinf(a)}, 14.0f, 2.0f, 1, 0);
            }
            GameRequestSound(game, SOUND_SHOOT);
            weapon->burstRemaining--;
            weapon->burstTimer = 0.15f;
        }
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

/* 투사체 수명, 이동, 궤도 위치, 벽 충돌을 갱신 */
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

/* 투사체와 적 충돌을 해결하고 처치 처리는 EnemyDefeat로 위임 */
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

            if (projectile->areaHit > 0) {
                /* 3x3 범위 타격 */
                EnemiesDamageInRadius(game, projectile->position, 1, projectile->damage);
                projectile->active = false;
            } else {
                enemy->health -= projectile->damage;
                projectile->pierce--;
                if (projectile->pierce <= 0) {
                    projectile->active = false;
                }

                if (enemy->health <= 0) {
                    EnemyDefeat(game, enemy);
                }
            }
            break;
        }
    }
}
