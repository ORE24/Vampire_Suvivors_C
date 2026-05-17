#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCORE_FILE = "scores.txt";

static void GenerateUpgrades(Game *game)
{
    game->upgrades[0] = (UpgradeOption){
        "Magic Bolt",
        "Bolt +1 damage, faster shots",
        WEAPON_MAGIC_BOLT,
        0
    };
    game->upgrades[1] = (UpgradeOption){
        "Holy Aura",
        "Aura +1 range, stronger pulse",
        WEAPON_HOLY_AURA,
        1
    };
    game->upgrades[2] = (UpgradeOption){
        "Survivor",
        "+2 max HP, heal 2, wider XP pull",
        WEAPON_COUNT,
        2
    };
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

char GameMapTile(const Game *game, int x, int y)
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

bool GameMapIsBlocked(const Game *game, int x, int y)
{
    const char tile = GameMapTile(game, x, y);
    return tile == '#' || tile == 'T';
}

void GameRequestSound(Game *game, unsigned int flags)
{
    game->pendingSounds |= flags;
}

void GameInit(Game *game)
{
    memset(game, 0, sizeof(*game));

    game->mapWidth = DEFAULT_MAP_WIDTH;
    game->mapHeight = DEFAULT_MAP_HEIGHT;
    game->mode = GAME_MODE_PLAYING;
    game->player.position = (Vec2){game->mapWidth / 2.0f, game->mapHeight / 2.0f};
    game->player.maxHealth = 12;
    game->player.health = 12;
    game->player.level = 1;
    game->player.xp = 0;
    game->player.xpToNextLevel = 6;
    game->player.score = 0;
    game->player.kills = 0;
    game->player.moveCooldown = 0.0f;
    game->player.invulnerableTimer = 0.0f;
    game->player.magnetRange = 4;

    game->weapons[WEAPON_MAGIC_BOLT] = (Weapon){
        WEAPON_MAGIC_BOLT,
        1,
        1,
        1,
        18,
        0.62f,
        0.25f,
        '*'
    };
    game->weapons[WEAPON_HOLY_AURA] = (Weapon){
        WEAPON_HOLY_AURA,
        1,
        1,
        0,
        2,
        1.45f,
        0.80f,
        'o'
    };

    game->elapsed = 0.0f;
    game->spawnTimer = 0.0f;
    game->spawnInterval = 1.2f;
    game->auraPulseTimer = 0.0f;
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
    if (upgrade->weapon == WEAPON_MAGIC_BOLT) {
        Weapon *weapon = &game->weapons[WEAPON_MAGIC_BOLT];
        weapon->level++;
        weapon->damage++;
        if (weapon->cooldown > 0.20f) {
            weapon->cooldown -= 0.05f;
        }
        if (weapon->level == 3 || weapon->level == 5 || weapon->level == 8) {
            weapon->projectileCount++;
        }
    } else if (upgrade->weapon == WEAPON_HOLY_AURA) {
        Weapon *weapon = &game->weapons[WEAPON_HOLY_AURA];
        weapon->level++;
        weapon->damage++;
        if (weapon->range < 7) {
            weapon->range++;
        }
        if (weapon->cooldown > 0.55f) {
            weapon->cooldown -= 0.08f;
        }
    } else {
        game->player.maxHealth += 2;
        game->player.health += 2;
        game->player.magnetRange++;
        if (game->player.health > game->player.maxHealth) {
            game->player.health = game->player.maxHealth;
        }
    }

    game->mode = GAME_MODE_PLAYING;
}

void GameUpdate(Game *game, const InputState *input, float dt)
{
    if (game->mode == GAME_MODE_GAME_OVER || game->mode == GAME_MODE_VICTORY) {
        return;
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
        if (input->left || input->up) {
            game->selectedUpgrade = (game->selectedUpgrade + UPGRADE_CHOICES - 1) % UPGRADE_CHOICES;
        }
        if (input->right || input->down) {
            game->selectedUpgrade = (game->selectedUpgrade + 1) % UPGRADE_CHOICES;
        }
        if (input->number >= 1 && input->number <= 3) {
            GameApplyUpgrade(game, input->number - 1);
        } else if (input->select) {
            GameApplyUpgrade(game, game->selectedUpgrade);
        }
        return;
    }

    game->elapsed += dt;
    if (game->elapsed >= SURVIVAL_SECONDS) {
        game->mode = GAME_MODE_VICTORY;
        game->player.score += 1000;
        GameRequestSound(game, SOUND_VICTORY);
        return;
    }

    game->spawnInterval = 1.15f - (float)game->elapsed * 0.0014f;
    if (game->spawnInterval < 0.30f) {
        game->spawnInterval = 0.30f;
    }

    PlayerUpdate(game, input, dt);

    game->spawnTimer += dt;
    while (game->spawnTimer >= game->spawnInterval) {
        EnemiesSpawnWave(game);
        game->spawnTimer -= game->spawnInterval;
    }

    WeaponsUpdate(game, dt);
    ProjectilesUpdate(game, dt);
    EnemiesUpdate(game, dt);
    CombatResolve(game);
    PickupsUpdate(game, dt);

    if (game->auraPulseTimer > 0.0f) {
        game->auraPulseTimer -= dt;
        if (game->auraPulseTimer < 0.0f) {
            game->auraPulseTimer = 0.0f;
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
           fscanf(file, "%15s %d %d %d %d",
               entries[loaded].result,
               &entries[loaded].score,
               &entries[loaded].seconds,
               &entries[loaded].kills,
               &entries[loaded].level) == 5) {
        loaded++;
    }

    fclose(file);
    *count = loaded;
}

void RankingAddAndSave(const Game *game, const char *result)
{
    RankingEntry entries[MAX_RANKINGS + 1];
    int count = 0;
    FILE *file;
    int insertIndex;

    RankingLoad(entries, &count);

    if (count < MAX_RANKINGS + 1) {
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
        fprintf(file, "%s %d %d %d %d\n",
            entries[i].result,
            entries[i].score,
            entries[i].seconds,
            entries[i].kills,
            entries[i].level);
    }

    fclose(file);
}
