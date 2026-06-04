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
        projectile->orbit = false;
        projectile->orbitAngle = 0.0f;
        projectile->orbitRadius = 0.0f;
        projectile->orbitHitCooldown = 0.0f;
        projectile->glyph = weapon->glyph;
        return;
    }
}

static void SpawnOrbitProjectile(Game *game, const Weapon *weapon, float angle, float radius)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &game->projectiles[i];
        if (p->active) continue;
        p->active           = true;
        p->orbit            = true;
        p->orbitAngle       = angle;
        p->orbitRadius      = radius;
        p->orbitHitCooldown = 0.0f;
        p->position.x       = game->player.position.x + cosf(angle) * radius;
        p->position.y       = game->player.position.y + sinf(angle) * radius;
        p->velocity         = (Vec2){0.0f, 0.0f};
        p->damage           = weapon->damage;
        p->lifetime         = 999.0f;
        p->pierce           = 999;
        p->areaHit          = 0;
        p->glyph            = weapon->glyph;
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

/* 피의 고리: Lv1~2 조준 1발 / Lv3~6 궤도 1링 / Lv7 궤도 2링 */
static void FireCircle(Game *game, Weapon *weapon)
{
    if (weapon->level >= 3) {
        static const float RADII[2] = {5.0f, 8.5f};
        const int ringCount  = (weapon->level >= 7) ? 2 : 1;
        const int targetCount = (int)(5.0f * game->player.attackSpeedMult + 0.5f);
        bool needRespawn = false;

        /* 각 링별로 궤도탄 수 확인 */
        for (int r = 0; r < ringCount; r++) {
            int count = 0;
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                Projectile *p = &game->projectiles[i];
                if (p->active && p->orbit && fabsf(p->orbitRadius - RADII[r]) < 0.1f)
                    count++;
            }
            if (count < targetCount) { needRespawn = true; break; }
        }

        if (needRespawn) {
            /* 기존 궤도탄 전부 제거 후 재배치 */
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                if (game->projectiles[i].active && game->projectiles[i].orbit)
                    game->projectiles[i].active = false;
            }
            for (int r = 0; r < ringCount; r++) {
                /* 2링일 때 외링은 절반 위상 오프셋으로 엇갈리게 배치 */
                float offset = (r == 1) ? PI / (float)targetCount : 0.0f;
                for (int j = 0; j < targetCount; j++) {
                    float angle = offset + (float)j * (2.0f * PI / (float)targetCount);
                    SpawnOrbitProjectile(game, weapon, angle, RADII[r]);
                }
            }
            GameRequestSound(game, SOUND_SHOOT);
        }
        return;
    }

    /* Lv1~2: 가장 가까운 적에게 조준 1발 */
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

/* 혼돈의 살점: 부채꼴 3발 (Lv7: 5발) */
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

/* 십자 저주: 4방향 발사 (Lv7: 4방향 각 2발) */
static void FireSquare(Game *game, Weapon *weapon)
{
    static const Vec2 dirs[4] = {
        { 1.0f,  0.0f},
        {-1.0f,  0.0f},
        { 0.0f,  1.0f},
        { 0.0f, -1.0f}
    };
    int i;

    if (weapon->level >= 7) {
        /* 만랩: 각 방향마다 약간 벌어진 2발씩 */
        static const float spread = 0.15f;
        for (i = 0; i < 4; i++) {
            float ax = dirs[i].x - dirs[i].y * spread;
            float ay = dirs[i].y + dirs[i].x * spread;
            float bx = dirs[i].x + dirs[i].y * spread;
            float by = dirs[i].y - dirs[i].x * spread;
            SpawnProjectile(game, weapon, (Vec2){ax, ay}, 13.0f, 1.8f, 1, 0);
            SpawnProjectile(game, weapon, (Vec2){bx, by}, 13.0f, 1.8f, 1, 0);
        }
    } else {
        for (i = 0; i < 4; i++) {
            SpawnProjectile(game, weapon, dirs[i], 13.0f, 1.8f, 1, 0);
        }
    }

    GameRequestSound(game, SOUND_SHOOT);
}

/* 부패한 혜성: 강타 1발 (Lv7: 3발씩 3연사 버스트) */
static void FireStar(Game *game, Weapon *weapon)
{
    Vec2 targetPosition;
    Vec2 direction;

    if (!FindNearestTargetPosition(game, weapon->range, &targetPosition)) {
        return;
    }

    direction.x = targetPosition.x - game->player.position.x;
    direction.y = targetPosition.y - game->player.position.y;

    if (weapon->level >= 7) {
        /* 1차 발사 (3발 부채꼴), 이후 2연 버스트 예약 */
        float baseAngle = atan2f(direction.y, direction.x);
        for (int s = -1; s <= 1; s++) {
            float a = baseAngle + (float)s * 0.18f;
            SpawnProjectile(game, weapon, (Vec2){cosf(a), sinf(a)}, 14.0f, 2.0f, 1, 0);
        }
        weapon->burstRemaining = 2;
        weapon->burstTimer     = 0.15f;
        weapon->burstDirection = GameNormalize(direction);
    } else {
        SpawnProjectile(game, weapon, direction, 13.0f, 1.8f, 1, 0);
    }

    GameRequestSound(game, SOUND_SHOOT);
}

/* 현재 장착 무기 발사 + 부패한 혜성 버스트 처리 */
void WeaponsUpdate(Game *game, float dt)
{
    /* 피의 고리가 아닌 다른 무기로 전환되면 궤도탄 소멸 */
    if (game->activeWeapon != WEAPON_MAGIC_BOLT) {
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game->projectiles[i].active && game->projectiles[i].orbit)
                game->projectiles[i].active = false;
        }
    }

    Weapon *weapon = &game->weapons[game->activeWeapon];
    /* 공격속도 배율 적용: 쿨타임을 배율로 나눔 */
    const float effectiveCooldown = (game->player.attackSpeedMult > 0.0f)
        ? weapon->cooldown / game->player.attackSpeedMult
        : weapon->cooldown;

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

        if (projectile->orbit) {
            projectile->orbitAngle += 2.2f * dt;
            projectile->position.x = game->player.position.x + cosf(projectile->orbitAngle) * projectile->orbitRadius;
            projectile->position.y = game->player.position.y + sinf(projectile->orbitAngle) * projectile->orbitRadius;
            if (projectile->orbitHitCooldown > 0.0f)
                projectile->orbitHitCooldown -= dt;
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

        /* 궤도탄: 히트 쿨타임 중이면 충돌 검사 생략 */
        if (projectile->orbit && projectile->orbitHitCooldown > 0.0f) {
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

            if (projectile->orbit) {
                /* 궤도탄: 소멸 없이 히트 쿨타임만 부여 (공격속도 반영) */
                const float atkMult = game->player.attackSpeedMult > 0.0f ? game->player.attackSpeedMult : 1.0f;
                enemy->health -= projectile->damage;
                projectile->orbitHitCooldown = 0.5f / atkMult;
                if (enemy->health <= 0) {
                    EnemyDefeat(game, enemy);
                }
            } else if (projectile->areaHit > 0) {
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
