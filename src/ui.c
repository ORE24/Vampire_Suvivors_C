#include "game.h"

#include <stdio.h>
#include <string.h>

typedef enum CellColor {
    CELL_DIM = 0,
    CELL_WALL,
    CELL_PLAYER,
    CELL_ENEMY_LOW,
    CELL_ENEMY_MID,
    CELL_ENEMY_HIGH,
    CELL_PROJECTILE,
    CELL_XP,
    CELL_AURA,
    CELL_TEXT
} CellColor;

typedef struct Cell {
    char glyph;
    CellColor color;
} Cell;

static const char *ColorCode(CellColor color)
{
    switch (color) {
        case CELL_DIM:
            return "\033[2;37m";
        case CELL_WALL:
            return "\033[1;37m";
        case CELL_PLAYER:
            return "\033[1;36m";
        case CELL_ENEMY_LOW:
            return "\033[1;31m";
        case CELL_ENEMY_MID:
            return "\033[38;5;208m";
        case CELL_ENEMY_HIGH:
            return "\033[1;35m";
        case CELL_PROJECTILE:
            return "\033[1;33m";
        case CELL_XP:
            return "\033[1;32m";
        case CELL_AURA:
            return "\033[1;34m";
        case CELL_TEXT:
            return "\033[1;37m";
    }

    return "\033[0m";
}

static void BeginFrame(void)
{
    printf("\033[H");
}

static void BeginCleanFrame(void)
{
    printf("\033[H\033[2J");
}

static void EndFrame(void)
{
    printf("\033[0m\033[J");
    fflush(stdout);
}

static void DrawBar(const char *label, int current, int max, int width, const char *barColor)
{
    int filled;

    if (max <= 0) {
        max = 1;
    }

    filled = current * width / max;
    filled = GameClampInt(filled, 0, width);

    printf("%s%s\033[0m [", barColor, label);
    fputs(barColor, stdout);
    for (int i = 0; i < width; i++) {
        if (i == filled) {
            fputs("\033[2;37m", stdout);
        }
        putchar(i < filled ? '=' : ' ');
    }
    printf("\033[0m] %d/%d", current, max);
}

static void PutCell(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int x, int y, char glyph, CellColor color)
{
    if (x < 0 || x >= game->mapWidth || y < 0 || y >= game->mapHeight) {
        return;
    }

    grid[y][x].glyph = glyph;
    grid[y][x].color = color;
}

static void BuildGrid(const Game *game, Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH])
{
    for (int y = 0; y < game->mapHeight; y++) {
        for (int x = 0; x < game->mapWidth; x++) {
            const char tile = GameMapTile(game, x, y);
            grid[y][x].glyph = tile == '.' ? ' ' : tile;
            grid[y][x].color = tile == '.' ? CELL_DIM : CELL_WALL;
        }
    }

    if (game->auraPulseTimer > 0.0f) {
        const int centerX = GameRound(game->player.position.x);
        const int centerY = GameRound(game->player.position.y);
        const int radius = game->weapons[WEAPON_HOLY_AURA].range;
        const int radiusSquared = radius * radius;

        for (int y = centerY - radius; y <= centerY + radius; y++) {
            for (int x = centerX - radius; x <= centerX + radius; x++) {
                const int dx = x - centerX;
                const int dy = y - centerY;
                if (dx * dx + dy * dy <= radiusSquared && !GameMapIsBlocked(game, x, y)) {
                    PutCell(game, grid, x, y, '~', CELL_AURA);
                }
            }
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        const Pickup *pickup = &game->pickups[i];
        if (pickup->active) {
            PutCell(game, grid, GameRound(pickup->position.x), GameRound(pickup->position.y), '+', CELL_XP);
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *projectile = &game->projectiles[i];
        if (projectile->active) {
            PutCell(game, grid, GameRound(projectile->position.x), GameRound(projectile->position.y), projectile->glyph, CELL_PROJECTILE);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *enemy = &game->enemies[i];
        CellColor color = CELL_ENEMY_LOW;

        if (!enemy->active) {
            continue;
        }

        if (enemy->type == ENEMY_THREE_HP) {
            color = CELL_ENEMY_MID;
        } else if (enemy->type == ENEMY_FORTY_HP) {
            color = CELL_ENEMY_HIGH;
        }

        PutCell(game, grid, GameRound(enemy->position.x), GameRound(enemy->position.y), enemy->glyph, color);
    }

    if (game->player.invulnerableTimer <= 0.0f || ((int)(game->player.invulnerableTimer * 12.0f) % 2) == 0) {
        PutCell(game, grid, GameRound(game->player.position.x), GameRound(game->player.position.y), '@', CELL_PLAYER);
    }
}

static void DrawGridLine(const Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH], int y, int width)
{
    CellColor currentColor = CELL_TEXT;
    bool hasColor = false;

    for (int x = 0; x < width; x++) {
        if (!hasColor || grid[y][x].color != currentColor) {
            currentColor = grid[y][x].color;
            fputs(ColorCode(currentColor), stdout);
            hasColor = true;
        }
        putchar(grid[y][x].glyph);
    }

    printf("\033[0m\n");
}

void UiDrawTitle(void)
{
    BeginCleanFrame();
    printf("\033[1;36m");
    printf("======================================================================\n");
    printf("                    TERMINAL SURVIVORS: CRYPT MVP                    \n");
    printf("======================================================================\n");
    printf("\033[0m\n");
    printf("A C terminal survival game. No raylib, no graphics window.\n\n");
    printf("\033[1;37mGoal\033[0m\n");
    printf("  Survive for 10 minutes. If HP reaches 0, the run ends.\n\n");
    printf("\033[1;37mControls\033[0m\n");
    printf("  WASD / Arrow keys : move\n");
    printf("  1, 2, 3 or Enter  : choose level-up upgrade\n");
    printf("  1 / 2             : choose Easy / Hard on setup\n");
    printf("  R                 : ranking on this screen, restart after run\n");
    printf("  Esc               : pause/resume during a run\n");
    printf("  M                 : toggle terminal bell sound\n");
    printf("  Q                 : quit or end current run\n");
    printf("  Esc on title      : quit\n\n");
    printf("\033[1;33mPress S or Enter to open setup. Press R for rankings.\033[0m\n");
    EndFrame();
}

void UiDrawSetup(GameDifficulty selectedDifficulty)
{
    BeginCleanFrame();
    printf("\033[1;36m============================== SETUP ================================\033[0m\n\n");
    printf("Choose difficulty before starting the run.\n\n");
    printf("%s[1] Easy\033[0m  HP 14, slower waves, late vampires\n",
        selectedDifficulty == DIFFICULTY_EASY ? "\033[1;32m> " : "  ");
    printf("%s[2] Hard\033[0m  HP 10, faster waves, early vampires\n\n",
        selectedDifficulty == DIFFICULTY_HARD ? "\033[1;31m> " : "  ");
    printf("Current: \033[1;33m%s\033[0m\n\n", GameDifficultyName(selectedDifficulty));
    printf("Use arrow keys or 1/2 to change. Enter starts. B/Esc returns.\n");
    EndFrame();
}

void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count)
{
    BeginCleanFrame();
    printf("\033[1;33m============================== RANKING ==============================\033[0m\n\n");

    if (count == 0) {
        printf("No recorded runs yet.\n");
    } else {
        printf(" #  Result  Score   Time   Kills  Level\n");
        printf(" -- ------- ------- ------ ------ -----\n");
        for (int i = 0; i < count; i++) {
            printf(" %d  %-6s  %6d  %02d:%02d  %5d  %5d\n",
                i + 1,
                entries[i].result,
                entries[i].score,
                entries[i].seconds / 60,
                entries[i].seconds % 60,
                entries[i].kills,
                entries[i].level);
        }
    }

    printf("\nPress B/Esc to go back, S/Enter for setup, Q to quit.\n");
    EndFrame();
}

void UiDrawGame(const Game *game)
{
    Cell grid[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
    const int seconds = (int)game->elapsed;
    const int remaining = (int)SURVIVAL_SECONDS - seconds;

    BuildGrid(game, grid);
    BeginFrame();

    printf("\033[1;36mTerminal Survivors\033[0m  ");
    printf("Mode %s  ", GameDifficultyName(game->difficulty));
    printf("Time %02d:%02d / 10:00  ", seconds / 60, seconds % 60);
    printf("Remain %02d:%02d  ", remaining / 60, remaining % 60);
    printf("Kills %d  Score %d\n", game->player.kills, game->player.score);

    DrawBar("HP", game->player.health, game->player.maxHealth, 18, "\033[1;31m");
    printf("   ");
    DrawBar("XP", game->player.xp, game->player.xpToNextLevel, 18, "\033[1;32m");
    printf("   LV %d\n", game->player.level);

    printf("Weapons: Bolt Lv%d Dmg%d x%d CD%.2f | Aura Lv%d Dmg%d R%d CD%.2f\n",
        game->weapons[WEAPON_MAGIC_BOLT].level,
        game->weapons[WEAPON_MAGIC_BOLT].damage,
        game->weapons[WEAPON_MAGIC_BOLT].projectileCount,
        game->weapons[WEAPON_MAGIC_BOLT].cooldown,
        game->weapons[WEAPON_HOLY_AURA].level,
        game->weapons[WEAPON_HOLY_AURA].damage,
        game->weapons[WEAPON_HOLY_AURA].range,
        game->weapons[WEAPON_HOLY_AURA].cooldown);
    printf("         Lance Lv%d Dmg%d P%d CD%.2f | Star Lv%d Dmg%d x%d CD%.2f\n",
        game->weapons[WEAPON_PIERCING_LANCE].level,
        game->weapons[WEAPON_PIERCING_LANCE].damage,
        game->weapons[WEAPON_PIERCING_LANCE].projectileCount,
        game->weapons[WEAPON_PIERCING_LANCE].cooldown,
        game->weapons[WEAPON_STAR_BURST].level,
        game->weapons[WEAPON_STAR_BURST].damage,
        game->weapons[WEAPON_STAR_BURST].projectileCount,
        game->weapons[WEAPON_STAR_BURST].cooldown);
    printf("Legend: \033[1;36m@\033[0m you  \033[1;31mb\033[0m fast  \033[38;5;208mG\033[0m guard  \033[1;35mV\033[0m vampire  \033[1;33m*\033[0m bolt  \033[1;33m|\033[0m lance  \033[1;33mx\033[0m star  \033[1;32m+\033[0m XP\n\n");

    for (int y = 0; y < game->mapHeight; y++) {
        DrawGridLine(grid, y, game->mapWidth);
    }

    if (game->mode == GAME_MODE_LEVEL_UP) {
        printf("\n\033[1;33mLEVEL UP - choose one upgrade\033[0m\n");
        for (int i = 0; i < UPGRADE_CHOICES; i++) {
            printf("%s[%d] %s - %s\033[0m\n",
                i == game->selectedUpgrade ? "\033[1;36m> " : "  ",
                i + 1,
                game->upgrades[i].name,
                game->upgrades[i].description);
        }
    } else if (game->mode == GAME_MODE_PAUSED) {
        printf("\n\033[1;33mPAUSED\033[0m  Press Esc to resume. Q ends the run.\n");
    } else if (game->mode == GAME_MODE_GAME_OVER) {
        printf("\n\033[1;31mGAME OVER\033[0m  HP reached 0. Press R to restart or B/Esc for title.\n");
    } else if (game->mode == GAME_MODE_VICTORY) {
        printf("\n\033[1;32mVICTORY\033[0m  You survived 10 minutes. Press R to restart or B/Esc for title.\n");
    } else {
        printf("\nMove to dodge. Attacks are automatic. Esc pauses. Q ends the run. M toggles sound.\n");
    }

    EndFrame();
}

void UiPlaySounds(unsigned int flags, bool enabled)
{
    int count = 0;

    if (!enabled || flags == 0u) {
        return;
    }

    if ((flags & SOUND_ATTACK) != 0u) {
        count += 1;
    }
    if ((flags & SOUND_XP) != 0u) {
        count += 2;
    }
    if ((flags & SOUND_LEVEL_UP) != 0u) {
        count += 3;
    }
    if ((flags & SOUND_HIT) != 0u) {
        count += 2;
    }
    if ((flags & SOUND_GAME_OVER) != 0u || (flags & SOUND_VICTORY) != 0u) {
        count += 4;
    }

    if (count > 5) {
        count = 5;
    }

    for (int i = 0; i < count; i++) {
        putchar('\a');
    }
    fflush(stdout);
}
