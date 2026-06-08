#include "smoke_test.h"

#include "game.h"
#include "platform.h"
#include "ranking.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

int RunSmokeTest(void)
{
    Game game;
    Game hardGame;
    RankingEntry rankings[MAX_RANKINGS];
    InputState input;
    int rankingCount = 0;

    memset(&input, 0, sizeof(input));
    GameInit(&game, DIFFICULTY_EASY);
    GameInit(&hardGame, DIFFICULTY_HARD);

    if (WEAPON_COUNT != 4 ||
        game.player.maxHealth <= hardGame.player.maxHealth ||
        game.spawnStartInterval <= hardGame.spawnStartInterval ||
        game.highEnemyStart <= hardGame.highEnemyStart) {
        fprintf(stderr, "smoke failed: difficulty or weapon setup is invalid\n");
        return 1;
    }

    if (UiSoundEvent(SOUND_SHOOT) != PLATFORM_SOUND_SHOOT ||
        UiSoundEvent(SOUND_XP_PICKUP) != PLATFORM_SOUND_XP_PICKUP ||
        UiSoundEvent(SOUND_LEVEL_UP) != PLATFORM_SOUND_LEVEL_UP ||
        UiSoundEvent(SOUND_SHOOT) == UiSoundEvent(SOUND_XP_PICKUP) ||
        UiSoundEvent(SOUND_XP_PICKUP) == UiSoundEvent(SOUND_LEVEL_UP)) {
        fprintf(stderr, "smoke failed: shoot, xp, and reward sounds are not distinct\n");
        return 1;
    }

    if (PlatformMusicPath(PLATFORM_MUSIC_MENU)[0] == '\0' ||
        PlatformMusicPath(PLATFORM_MUSIC_GAME)[0] == '\0') {
        fprintf(stderr, "smoke failed: music assets are invalid\n");
        return 1;
    }

    game.nextMiniEventTime = 999.0f;
    GameUpdate(&game, &input, 180.1f);
    if (game.mode != GAME_MODE_PLAYING ||
        game.elapsed < 180.0f ||
        (game.pendingSounds & SOUND_POWER_PICKUP) != 0u) {
        fprintf(stderr, "smoke failed: 3-minute map transition legacy behavior is still active\n");
        return 1;
    }

    game.elapsed = SURVIVAL_SECONDS - 0.1f;
    game.pendingSounds = 0;
    GameUpdate(&game, &input, 0.2f);
    if (game.mode != GAME_MODE_VICTORY ||
        game.elapsed != SURVIVAL_SECONDS ||
        (game.pendingSounds & SOUND_VICTORY) == 0u) {
        fprintf(stderr, "smoke failed: 5-minute survival did not trigger GAME CLEAR\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.nextMiniEventTime = 0.5f;
    GameUpdate(&game, &input, 0.6f);
    if (game.activeMiniEvent != MINI_EVENT_BAT_STORM ||
        game.miniEventMessageTimer <= 0.0f ||
        (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: bat event did not trigger\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    PickupSpawn(&game, game.player.position, 3);
    PickupsUpdate(&game, 0.1f);
    if (game.player.xp != 3 || (game.pendingSounds & SOUND_XP_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: xp pickup did not collect or sound\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    {
        const int previousLevel = game.player.level;
        PickupSpawnTyped(&game, game.player.position, PICKUP_TREASURE_CHEST, 0);
        PickupsUpdate(&game, 0.1f);
        if (game.mode != GAME_MODE_LEVEL_UP ||
            game.player.level != previousLevel ||
            (game.pendingSounds & SOUND_LEVEL_UP) == 0u) {
            fprintf(stderr, "smoke failed: treasure chest did not open reward choices\n");
            return 1;
        }
        memset(&input, 0, sizeof(input));
        input.select = true;
        GameUpdate(&game, &input, 0.1f);
        if (game.mode != GAME_MODE_PLAYING) {
            fprintf(stderr, "smoke failed: reward choice did not return to play\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.enemies[0] = (Enemy){true, ENEMY_BAT, game.player.position, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
    EnemyDefeat(&game, &game.enemies[0]);
    {
        int xpCount = 0;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (!game.pickups[i].active) {
                continue;
            }
            if (game.pickups[i].type == PICKUP_XP) {
                xpCount++;
            } else if (game.pickups[i].type != PICKUP_TREASURE_CHEST) {
                fprintf(stderr, "smoke failed: unexpected pickup type spawned\n");
                return 1;
            }
        }
        if (xpCount < 1 || game.player.kills != 1) {
            fprintf(stderr, "smoke failed: enemy defeat did not spawn xp or count kill\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.player.health = game.player.maxHealth - 1;
    game.upgrades[0] = (UpgradeOption){"회복의 붕대", "smoke", WEAPON_COUNT, 6};
    GameApplyUpgrade(&game, 0);
    if (game.player.hpRecoveryTimer != HP_RECOVERY_SECONDS) {
        fprintf(stderr, "smoke failed: bandage reward did not start its active timer\n");
        return 1;
    }
    for (int i = 0; i < 20; i++) {
        Enemy enemy = {true, ENEMY_BAT, game.player.position, 1, 1, 1, 0, 0, 0.0f, 0.30f, 'b'};
        EnemyDefeat(&game, &enemy);
    }
    if (game.player.health != game.player.maxHealth ||
        game.player.bandageKillCount != 0) {
        fprintf(stderr, "smoke failed: bandage reward did not heal after 20 kills\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.enemies[0] = (Enemy){true, ENEMY_BAT, {game.player.position.x + 6.0f, game.player.position.y}, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
    WeaponsUpdate(&game, 1.2f);
    {
        int activeProjectiles = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game.projectiles[i].active) {
                activeProjectiles++;
            }
        }
        if (activeProjectiles < 1 || (game.pendingSounds & SOUND_SHOOT) == 0u) {
            fprintf(stderr, "smoke failed: weapon did not fire or request shoot sound\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    memset(&input, 0, sizeof(input));
    input.pauseToggle = true;
    GameUpdate(&game, &input, 1.0f);
    if (game.mode != GAME_MODE_PAUSED || game.elapsed != 0.0f) {
        fprintf(stderr, "smoke failed: pause did not stop gameplay\n");
        return 1;
    }
    memset(&input, 0, sizeof(input));
    input.pauseToggle = true;
    GameUpdate(&game, &input, 1.0f);
    if (game.mode != GAME_MODE_PLAYING || game.elapsed != 0.0f) {
        fprintf(stderr, "smoke failed: resume did not restore gameplay\n");
        return 1;
    }

    RankingLoad(rankings, &rankingCount);
    printf("smoke ok: clear=%ds map=single pickups=xp,chest event=bat music=menu/game bandage=20kills rankings=%d\n",
        (int)SURVIVAL_SECONDS,
        rankingCount);
    return 0;
}
