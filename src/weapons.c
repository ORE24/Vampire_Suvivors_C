#include "game.h"

#include <math.h>

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

static void SpawnProjectile(Game *game, Vec2 direction)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *projectile = &game->projectiles[i];
        if (projectile->active) {
            continue;
        }

        direction = GameNormalize(direction);
        projectile->active = true;
        projectile->position = game->player.position;
        projectile->velocity = GameScale(direction, 13.0f);
        projectile->damage = game->weapons[WEAPON_MAGIC_BOLT].damage;
        projectile->lifetime = 1.8f;
        projectile->glyph = game->weapons[WEAPON_MAGIC_BOLT].glyph;
        return;
    }
}

static void FireMagicBolt(Game *game, Weapon *weapon)
{
    const int targetIndex = FindNearestEnemy(game, weapon->range);
    Vec2 direction;
    float baseAngle;
    float spread;

    if (targetIndex < 0) {
        return;
    }

    direction.x = game->enemies[targetIndex].position.x - game->player.position.x;
    direction.y = game->enemies[targetIndex].position.y - game->player.position.y;
    direction = GameNormalize(direction);
    baseAngle = atan2f(direction.y, direction.x);
    spread = 0.24f;

    for (int i = 0; i < weapon->projectileCount; i++) {
        const float offset = ((float)i - ((float)weapon->projectileCount - 1.0f) * 0.5f) * spread;
        const float angle = baseAngle + offset;
        SpawnProjectile(game, (Vec2){cosf(angle), sinf(angle)});
    }

    GameRequestSound(game, SOUND_ATTACK);
}

static void PulseHolyAura(Game *game, Weapon *weapon)
{
    EnemiesDamageInRadius(game, game->player.position, weapon->range, weapon->damage);
    game->auraPulseTimer = 0.22f;
    GameRequestSound(game, SOUND_ATTACK);
}

void WeaponsUpdate(Game *game, float dt)
{
    Weapon *bolt = &game->weapons[WEAPON_MAGIC_BOLT];
    Weapon *aura = &game->weapons[WEAPON_HOLY_AURA];

    bolt->timer -= dt;
    if (bolt->timer <= 0.0f) {
        FireMagicBolt(game, bolt);
        bolt->timer += bolt->cooldown;
    }

    aura->timer -= dt;
    if (aura->timer <= 0.0f) {
        PulseHolyAura(game, aura);
        aura->timer += aura->cooldown;
    }
}

void ProjectilesUpdate(Game *game, float dt)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *projectile = &game->projectiles[i];
        int x;
        int y;

        if (!projectile->active) {
            continue;
        }

        projectile->position = GameAdd(projectile->position, GameScale(projectile->velocity, dt));
        projectile->lifetime -= dt;
        x = GameRound(projectile->position.x);
        y = GameRound(projectile->position.y);

        if (projectile->lifetime <= 0.0f || GameMapIsBlocked(game, x, y)) {
            projectile->active = false;
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
            projectile->active = false;

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
