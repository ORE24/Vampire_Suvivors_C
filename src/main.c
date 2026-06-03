#include "game.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void ReadInput(InputState *input)
{
    char ch;

    memset(input, 0, sizeof(*input));

    while (PlatformReadByte(&ch)) {
        if (ch == '\033') { /* ESC: 화살표 또는 Esc 처리 */
            char next;
            char code;

            input->escape = true;
            input->pauseToggle = true;
            if (PlatformReadByte(&next) && next == '[' && PlatformReadByte(&code)) {
                input->escape = false;
                input->pauseToggle = false;
                if (code == 'A') {
                    input->up = true;
                } else if (code == 'B') {
                    input->down = true;
                } else if (code == 'C') {
                    input->right = true;
                } else if (code == 'D') {
                    input->left = true;
                }
            }
            continue;
        }

        /* 이름 입력용 raw 문자 저장 (알파벳/숫자/-/_) */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            input->typedChar = ch;
        }

        ch = (char)tolower((unsigned char)ch);
        if (ch == 'w') {
            input->up = true;
        } else if (ch == 's') {
            input->down = true;
            input->start = true;
        } else if (ch == 'a') {
            input->left = true;
        } else if (ch == 'd') {
            input->right = true;
        } else if (ch == '\r' || ch == '\n' || ch == ' ') {
            input->select = true;
        } else if (ch >= '1' && ch <= '3') {
            input->number = ch - '0';
            input->select = true;
        } else if (ch == 'r') {
            input->ranking = true;
            input->restart = true;
        } else if (ch == 'q') {
            input->quit = true;
        } else if (ch == 'b') {
            input->back = true;
        } else if (ch == 'm') {
            input->muteToggle = true;
        }
    }
}

static bool GameIsFinished(const Game *game)
{
    return game->mode == GAME_MODE_GAME_OVER || game->mode == GAME_MODE_VICTORY;
}

static int RunSmokeTest(void)
{
    Game game;
    Game hardGame;
    Game projectileGame;
    RankingEntry rankings[MAX_RANKINGS];
    InputState input;
    int rankingCount = 0;

    GameInit(&game, DIFFICULTY_EASY);
    GameInit(&hardGame, DIFFICULTY_HARD);

    if (WEAPON_COUNT != 4 ||
        game.player.maxHealth <= hardGame.player.maxHealth ||
        game.spawnStartInterval <= hardGame.spawnStartInterval ||
        game.highEnemyStart <= hardGame.highEnemyStart) {
        fprintf(stderr, "smoke failed: difficulty or weapon setup is invalid\n");
        return 1;
    }

    if (UiSoundEvent(SOUND_UI_MOVE) == UiSoundEvent(SOUND_SHOOT) ||
        UiSoundEvent(SOUND_UI_CONFIRM) == UiSoundEvent(SOUND_XP_PICKUP) ||
        UiSoundEvent(SOUND_SHOOT) == UiSoundEvent(SOUND_XP_PICKUP)) {
        fprintf(stderr, "smoke failed: selection, shooting, and xp sounds are not distinct\n");
        return 1;
    }

    game.nextMiniEventTime = 999.0f;
    memset(&input, 0, sizeof(input));
    GameUpdate(&game, &input, 179.0f);
    if (game.mapPhase != MAP_PHASE_CRYPT ||
        game.bossPhase != BOSS_PHASE_NONE ||
        game.bossStatus != BOSS_STATUS_NONE) {
        fprintf(stderr, "smoke failed: map or boss changed before 3 minutes\n");
        return 1;
    }
    GameUpdate(&game, &input, 120.0f);
    if (game.mapPhase != MAP_PHASE_GRAVEYARD ||
        game.bossPhase != BOSS_PHASE_NONE ||
        game.bossStatus != BOSS_STATUS_NONE) {
        fprintf(stderr, "smoke failed: graveyard survival did not run before 5-minute boss\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.nextMiniEventTime = 999.0f;
    game.elapsed = 179.8f;
    {
        const int blockedX = game.mapWidth / 2 - 20;
        const int blockedY = 4;

        game.player.position = (Vec2){(float)blockedX, (float)blockedY};
        game.enemies[0] = (Enemy){true, ENEMY_ONE_HP, {(float)blockedX, (float)blockedY}, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
        game.pickups[0] = (Pickup){true, PICKUP_HEAL_PACK, {(float)blockedX, (float)blockedY}, 0, 0.0f};
        game.projectiles[0] = (Projectile){true, {(float)blockedX, (float)blockedY}, {1.0f, 0.0f}, 1, 1.0f, 1, '*'};

        GameUpdate(&game, &input, 0.30f);
        if (game.mapPhase != MAP_PHASE_GRAVEYARD ||
            game.graveyardWarningTimer <= 0.0f ||
            game.graveyardSpawnInterval <= 0.0f ||
            (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
            fprintf(stderr, "smoke failed: graveyard did not activate at 3 minutes\n");
            return 1;
        }
        if (GameMapIsBlocked(&game, GameRound(game.player.position.x), GameRound(game.player.position.y)) ||
            GameMapIsBlocked(&game, GameRound(game.enemies[0].position.x), GameRound(game.enemies[0].position.y)) ||
            GameMapIsBlocked(&game, GameRound(game.pickups[0].position.x), GameRound(game.pickups[0].position.y)) ||
            game.projectiles[0].active) {
            fprintf(stderr, "smoke failed: graveyard transition did not free actors or remove blocked projectile\n");
            return 1;
        }

        game.pendingSounds = 0;
        GameUpdate(&game, &input, 0.10f);
        if (game.mapPhase != MAP_PHASE_GRAVEYARD ||
            (game.pendingSounds & SOUND_POWER_PICKUP) != 0u) {
            fprintf(stderr, "smoke failed: graveyard transition repeated\n");
            return 1;
        }

        GameUpdate(&game, &input, 5.0f);
        if (game.graveyardWarningTimer > 0.0f) {
            fprintf(stderr, "smoke failed: graveyard warning did not expire\n");
            return 1;
        }

        game.elapsed = 299.8f;
        game.pendingSounds = 0;
        GameUpdate(&game, &input, 0.30f);
        if (game.mapPhase != MAP_PHASE_BOSS_ARENA ||
            game.graveyardSpawnInterval != 0.0f ||
            (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
            fprintf(stderr, "smoke failed: boss arena did not activate at 5 minutes\n");
            return 1;
        }
        if (GameMapIsBlocked(&game, blockedX, blockedY)) {
            fprintf(stderr, "smoke failed: boss arena did not return to the starting map layout\n");
            return 1;
        }
    }

    if (game.bossPhase != BOSS_PHASE_ONE ||
        game.bossStatus != BOSS_STATUS_SHIELDED ||
        game.bossPuzzle != BOSS_PUZZLE_SEQUENCE ||
        game.bossCount != 1) {
        fprintf(stderr, "smoke failed: boss fight did not start in boss arena\n");
        return 1;
    }

    {
        const int protectedHp = game.bosses[0].health;
        game.projectiles[0] = (Projectile){
            true,
            game.bosses[0].position,
            {0.0f, 0.0f},
            999,
            1.0f,
            1,
            '*'
        };
        CombatResolve(&game);
        if (game.bosses[0].health != protectedHp) {
            fprintf(stderr, "smoke failed: shielded boss took projectile damage\n");
            return 1;
        }
    }

    for (int step = 0; step < BOSS_SEQUENCE_LEN; step++) {
        const int altarIndex = game.bossSequence[step];
        game.player.position = game.bossAltars[altarIndex];
        GameUpdate(&game, &input, 0.02f);
    }
    if (game.bossStatus != BOSS_STATUS_VULNERABLE ||
        game.bossPuzzle != BOSS_PUZZLE_NONE ||
        game.phaseOneDamageWindow != 1) {
        fprintf(stderr, "smoke failed: sequence puzzle did not open first damage window\n");
        return 1;
    }

    {
        const int halfHp = game.bosses[0].maxHealth / 2;
        game.projectiles[0] = (Projectile){
            true,
            game.bosses[0].position,
            {0.0f, 0.0f},
            game.bosses[0].maxHealth,
            1.0f,
            1,
            '*'
        };
        CombatResolve(&game);
        if (game.bosses[0].health != halfHp ||
            game.bossStatus != BOSS_STATUS_SHIELDED ||
            game.bossPuzzle != BOSS_PUZZLE_ORB) {
            fprintf(stderr, "smoke failed: first damage window did not stop at 50%% and start orb puzzle\n");
            return 1;
        }
    }

    for (int carry = 0; carry < 3; carry++) {
        game.player.position = game.bossOrbPosition;
        GameUpdate(&game, &input, 0.02f);
        if (!game.bossOrbCarried) {
            fprintf(stderr, "smoke failed: orb was not picked up\n");
            return 1;
        }
        game.player.position = game.bossCentralAltar;
        GameUpdate(&game, &input, 0.02f);
    }
    if (game.bossStatus != BOSS_STATUS_VULNERABLE ||
        game.phaseOneDamageWindow != 2 ||
        game.bossOrbDeliveries != 3) {
        fprintf(stderr, "smoke failed: three orb deliveries did not open second damage window\n");
        return 1;
    }

    game.projectiles[0] = (Projectile){
        true,
        game.bosses[0].position,
        {0.0f, 0.0f},
        game.bosses[0].maxHealth,
        1.0f,
        1,
        '*'
    };
    CombatResolve(&game);
    if (game.bossPhase != BOSS_PHASE_TWO ||
        game.bossStatus != BOSS_STATUS_CLONES ||
        game.bossPuzzle != BOSS_PUZZLE_SEAL ||
        game.bossCount != 4) {
        fprintf(stderr, "smoke failed: phase one defeat did not create four phase two clones\n");
        return 1;
    }

    {
        const int previousTrueBoss = game.trueBossIndex;
        GameUpdate(&game, &input, 15.10f);
        if (game.trueBossIndex == previousTrueBoss ||
            game.bossTrueSwapTimer <= 0.0f) {
            fprintf(stderr, "smoke failed: true boss did not swap after 15 seconds\n");
            return 1;
        }
    }

    while (game.bossStatus == BOSS_STATUS_CLONES) {
        const int targetAltar = (game.trueBossIndex + 1) % BOSS_ALTAR_COUNT;
        game.player.position = game.bossSealAltars[targetAltar];
        GameUpdate(&game, &input, 0.02f);
        if (game.bossSealCount > 0 && game.bossStatus == BOSS_STATUS_CLONES) {
            game.player.position = (Vec2){game.mapWidth / 2.0f, game.mapHeight / 2.0f};
            GameUpdate(&game, &input, 0.02f);
        }
        if (game.bossSealCount > 3) {
            fprintf(stderr, "smoke failed: seal count overflowed\n");
            return 1;
        }
    }
    if (game.bossStatus != BOSS_STATUS_FINAL_DAMAGE ||
        game.bossCount != 1) {
        fprintf(stderr, "smoke failed: three seals did not start final damage window\n");
        return 1;
    }

    game.projectiles[0] = (Projectile){
        true,
        game.bosses[0].position,
        {0.0f, 0.0f},
        game.bosses[0].maxHealth,
        1.0f,
        1,
        '*'
    };
    CombatResolve(&game);
    if (game.mode != GAME_MODE_VICTORY ||
        game.bossStatus != BOSS_STATUS_DEFEATED) {
        fprintf(stderr, "smoke failed: final boss defeat did not trigger victory\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.nextMiniEventTime = 999.0f;
    game.elapsed = 299.9f;
    game.mapPhase = MAP_PHASE_GRAVEYARD;
    GameUpdate(&game, &input, 0.2f);
    {
        const int initialY = GameRound(game.player.position.y);
        game.bossForbiddenKey = 'W';
        game.bossForbiddenTimer = 6.0f;
        game.player.moveCooldown = 0.0f;
        memset(&input, 0, sizeof(input));
        input.up = true;
        PlayerUpdate(&game, &input, 0.10f);
        if (GameRound(game.player.position.y) != initialY) {
            fprintf(stderr, "smoke failed: forbidden W input was not ignored\n");
            return 1;
        }
    }

    {
        const int initialHp = game.player.health;
        game.bossShadowActive = true;
        game.bossShadowPosition = (Vec2){game.player.position.x + 5.0f, game.player.position.y};
        game.bossShadowTimer = 10.0f;
        game.bossShadowDamageCooldown = 0.0f;
        game.enemies[0] = (Enemy){
            true,
            ENEMY_ONE_HP,
            game.bossShadowPosition,
            1,
            1,
            1,
            2,
            10,
            99.0f,
            99.0f,
            'b'
        };
        memset(&input, 0, sizeof(input));
        GameUpdate(&game, &input, 0.10f);
        if (game.player.health != initialHp - 1 ||
            game.bossShadowDamageCooldown <= 0.0f) {
            fprintf(stderr, "smoke failed: shadow damage did not apply\n");
            return 1;
        }
        GameUpdate(&game, &input, 0.30f);
        if (game.player.health != initialHp - 1) {
            fprintf(stderr, "smoke failed: shadow damage cooldown did not prevent repeated damage\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    EnemiesSpawnGraveyardWave(&game);
    {
        int enemiesBeforeGraveyard = 0;
        int enemiesAfterGraveyard = 0;

        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (game.enemies[i].active) {
                enemiesBeforeGraveyard++;
            }
        }
        if (enemiesBeforeGraveyard != 0) {
            fprintf(stderr, "smoke failed: graveyard spawn ran before graveyard phase\n");
            return 1;
        }

        game.mapPhase = MAP_PHASE_GRAVEYARD;
        EnemiesSpawnGraveyardWave(&game);
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (game.enemies[i].active) {
                int nearGrave = 0;
                for (int grave = 0; grave < 4; grave++) {
                    int graveX;
                    int graveY;
                    GameGraveyardSpawnPoint(&game, grave, &graveX, &graveY);
                    if (abs(GameRound(game.enemies[i].position.x) - graveX) <= 3 &&
                        abs(GameRound(game.enemies[i].position.y) - graveY) <= 3) {
                        nearGrave = 1;
                    }
                }
                if (!nearGrave) {
                    fprintf(stderr, "smoke failed: graveyard enemy did not spawn near a grave\n");
                    return 1;
                }
                enemiesAfterGraveyard++;
            }
        }
        if (enemiesAfterGraveyard < 4) {
            fprintf(stderr, "smoke failed: four graveyard spawns did not produce enemies\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.nextMiniEventTime = 0.5f;
    memset(&input, 0, sizeof(input));
    GameUpdate(&game, &input, 0.60f);
    if (game.activeMiniEvent == MINI_EVENT_NONE ||
        game.nextMiniEventTime < 30.0f ||
        game.miniEventMessageTimer <= 0.0f ||
        (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: mini event did not trigger on schedule\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    EnemiesSpawnBatStorm(&game, 12);
    {
        int batCount = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (game.enemies[i].active && game.enemies[i].type == ENEMY_ONE_HP) {
                batCount++;
            }
        }
        if (batCount < 8) {
            fprintf(stderr, "smoke failed: bat storm did not spawn enough bats\n");
            return 1;
        }
    }

    game.enemies[0] = (Enemy){true, ENEMY_ONE_HP, {game.player.position.x + 6.0f, game.player.position.y}, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
    WeaponsUpdate(&game, 1.20f);

    {
        int activeProjectiles = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game.projectiles[i].active) {
                activeProjectiles++;
            }
        }
        if (activeProjectiles < 1) {
            fprintf(stderr, "smoke failed: weapon projectiles did not spawn\n");
            return 1;
        }
        if ((game.pendingSounds & SOUND_SHOOT) == 0u) {
            fprintf(stderr, "smoke failed: shooting sound was not requested\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    PickupSpawn(&game, game.player.position, 3);
    PickupsUpdate(&game, 0.10f);
    if (game.player.xp != 3 || (game.pendingSounds & SOUND_XP_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: xp pickup did not collect or sound\n");
        return 1;
    }

    game.player.health = game.player.maxHealth - 5;
    game.pendingSounds = 0;
    PickupSpawnTyped(&game, game.player.position, PICKUP_HEAL_PACK, 0);
    PickupsUpdate(&game, 0.10f);
    if (game.player.health <= game.player.maxHealth - 5 ||
        (game.pendingSounds & SOUND_HEAL_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: heal pack pickup did not heal or sound\n");
        return 1;
    }

    game.pendingSounds = 0;
    game.player.health = game.player.maxHealth - 6;
    PickupApplyGamblerDiceEffect(&game, 0);
    if (game.player.health <= game.player.maxHealth - 6 ||
        (game.pendingSounds & SOUND_HEAL_PICKUP) == 0u ||
        game.diceMessageTimer <= 0.0f ||
        strstr(game.diceMessage, "HP 회복") == NULL) {
        fprintf(stderr, "smoke failed: gambler dice heal effect did not apply\n");
        return 1;
    }
    GameUpdate(&game, &input, 5.1f);
    if (game.diceMessageTimer > 0.0f || game.diceMessage[0] != '\0') {
        fprintf(stderr, "smoke failed: gambler dice message did not expire\n");
        return 1;
    }

    game.pendingSounds = 0;
    {
        Weapon *weapon = &game.weapons[game.activeWeapon];
        const int previousLevel = weapon->level;
        const int previousDamage = weapon->damage;
        PickupApplyGamblerDiceEffect(&game, 2);
        if ((weapon->level <= previousLevel && previousLevel < 3) ||
            weapon->damage <= previousDamage ||
            (game.pendingSounds & SOUND_LEVEL_UP) == 0u ||
            strstr(game.diceMessage, "좋은 무기") == NULL) {
            fprintf(stderr, "smoke failed: gambler dice weapon effect did not upgrade\n");
            return 1;
        }
    }

    game.pendingSounds = 0;
    PickupSpawnTyped(&game, game.player.position, PICKUP_FRENZY_MAGAZINE, 0);
    PickupsUpdate(&game, 0.10f);
    if (game.frenzyTimer <= 0.0f || (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: frenzy magazine did not activate\n");
        return 1;
    }

    {
        int beforeProjectiles = 0;
        int afterProjectiles = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game.projectiles[i].active) {
                beforeProjectiles++;
            }
        }
        WeaponsUpdate(&game, 0.12f);
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game.projectiles[i].active) {
                afterProjectiles++;
            }
        }
        if (afterProjectiles <= beforeProjectiles) {
            fprintf(stderr, "smoke failed: frenzy magazine did not create bullet storm\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    PickupSpawnTyped(&game, game.player.position, PICKUP_PINATA_SKULL, 0);
    PickupsUpdate(&game, 0.10f);
    {
        int rewardPickups = 0;
        int rewardXpValue = 0;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (game.pickups[i].active) {
                rewardPickups++;
                if (game.pickups[i].type == PICKUP_XP) {
                    rewardXpValue += game.pickups[i].value;
                }
            }
        }
        if (rewardPickups < 8 ||
            rewardXpValue <= 0 ||
            (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
            fprintf(stderr, "smoke failed: treasure chest did not scatter xp rewards\n");
            return 1;
        }
    }

    GameInit(&game, DIFFICULTY_EASY);
    PickupSpawn(&game, (Vec2){game.player.position.x + 3.0f, game.player.position.y}, 2);
    PickupSpawn(&game, (Vec2){game.player.position.x + 4.0f, game.player.position.y}, 4);
    PickupSpawnTyped(&game, game.player.position, PICKUP_VACUUM, 0);
    PickupsUpdate(&game, 0.10f);
    if (game.player.xp != 6 ||
        (game.pendingSounds & SOUND_POWER_PICKUP) == 0u ||
        (game.pendingSounds & SOUND_XP_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: vacuum pickup did not gather xp or sound\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    game.enemies[0] = (Enemy){true, ENEMY_ONE_HP, {game.player.position.x + 2.0f, game.player.position.y}, 1, 1, 1, 2, 10, 0.0f, 0.30f, 'b'};
    PickupSpawnTyped(&game, game.player.position, PICKUP_ROSARY, 0);
    PickupsUpdate(&game, 0.10f);
    if (game.enemies[0].active || game.player.kills != 1 ||
        (game.pendingSounds & SOUND_POWER_PICKUP) == 0u) {
        fprintf(stderr, "smoke failed: rosary pickup did not clear enemies or sound\n");
        return 1;
    }

    GameInit(&game, DIFFICULTY_EASY);
    for (int i = 0; i < MAX_PICKUPS; i++) {
        PickupSpawn(&game, (Vec2){1.0f + (float)(i % 10), 1.0f + (float)(i / 10)}, 1);
    }
    PickupSpawn(&game, (Vec2){20.0f, 20.0f}, 7);
    {
        int totalXpValue = 0;
        int activePickups = 0;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (game.pickups[i].active) {
                activePickups++;
                if (game.pickups[i].type == PICKUP_XP) {
                    totalXpValue += game.pickups[i].value;
                }
            }
        }
        if (activePickups != MAX_PICKUPS || totalXpValue != MAX_PICKUPS + 7) {
            fprintf(stderr, "smoke failed: full pickup pool did not preserve xp value\n");
            return 1;
        }
    }
    PickupSpawnTyped(&game, (Vec2){22.0f, 20.0f}, PICKUP_HEAL_PACK, 0);
    {
        bool foundHealPack = false;
        int totalXpValue = 0;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (game.pickups[i].active && game.pickups[i].type == PICKUP_HEAL_PACK) {
                foundHealPack = true;
            }
            if (game.pickups[i].active && game.pickups[i].type == PICKUP_XP) {
                totalXpValue += game.pickups[i].value;
            }
        }
        if (!foundHealPack || totalXpValue != MAX_PICKUPS + 7) {
            fprintf(stderr, "smoke failed: full pickup pool did not preserve bonus pickup and xp value\n");
            return 1;
        }
    }
    for (int i = 0; i < MAX_PICKUPS; i++) {
        game.pickups[i] = (Pickup){true, PICKUP_HEAL_PACK, {1.0f + (float)(i % 10), 1.0f + (float)(i / 10)}, 0, 0.0f};
    }
    PickupSpawnTyped(&game, (Vec2){23.0f, 20.0f}, PICKUP_ROSARY, 0);
    PickupSpawn(&game, (Vec2){24.0f, 20.0f}, 5);
    {
        bool foundRosary = false;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (game.pickups[i].active && game.pickups[i].type == PICKUP_ROSARY) {
                foundRosary = true;
                break;
            }
        }
        if (!foundRosary || game.player.xp != 5 || game.player.score != 5) {
            fprintf(stderr, "smoke failed: full bonus pickup pool did not preserve high-priority bonus or xp value\n");
            return 1;
        }
    }
    GameInit(&game, DIFFICULTY_EASY);
    game.pickups[0] = (Pickup){true, PICKUP_XP, {1.0f, 1.0f}, 9, 0.0f};
    for (int i = 1; i < MAX_PICKUPS; i++) {
        game.pickups[i] = (Pickup){true, PICKUP_HEAL_PACK, {1.0f + (float)(i % 10), 1.0f + (float)(i / 10)}, 0, 0.0f};
    }
    PickupSpawnTyped(&game, (Vec2){25.0f, 20.0f}, PICKUP_ROSARY, 0);
    {
        bool foundRosary = false;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            if (game.pickups[i].active && game.pickups[i].type == PICKUP_ROSARY) {
                foundRosary = true;
                break;
            }
        }
        if (!foundRosary || game.player.xp != 9 || game.player.score != 9) {
            fprintf(stderr, "smoke failed: final xp pickup replacement did not preserve xp value\n");
            return 1;
        }
    }

    GameInit(&projectileGame, DIFFICULTY_EASY);
    {
        const int wallX = projectileGame.mapWidth * 28 / 100;
        const int wallY = projectileGame.mapHeight * 32 / 100 + 1;

        if (!GameMapIsBlocked(&projectileGame, wallX, wallY) ||
            GameMapIsBlocked(&projectileGame, wallX - 1, wallY) ||
            GameMapIsBlocked(&projectileGame, wallX + 2, wallY)) {
            fprintf(stderr, "smoke failed: projectile wall test map setup is invalid\n");
            return 1;
        }

        projectileGame.projectiles[0] = (Projectile){
            true,
            {(float)(wallX - 1), (float)wallY},
            {32.0f, 0.0f},
            1,
            1.0f,
            1,
            '*'
        };
        ProjectilesUpdate(&projectileGame, 0.10f);
        if (projectileGame.projectiles[0].active) {
            fprintf(stderr, "smoke failed: projectile crossed a wall\n");
            return 1;
        }
    }

    memset(&input, 0, sizeof(input));
    input.pauseToggle = true;
    GameUpdate(&game, &input, 1.0f);

    if (game.mode != GAME_MODE_PAUSED || game.elapsed != 0.0f) {
        fprintf(stderr, "smoke failed: pause did not stop gameplay\n");
        return 1;
    }

    memset(&input, 0, sizeof(input));
    GameUpdate(&game, &input, 1.0f);

    if (game.mode != GAME_MODE_PAUSED || game.elapsed != 0.0f) {
        fprintf(stderr, "smoke failed: paused game advanced\n");
        return 1;
    }

    input.pauseToggle = true;
    GameUpdate(&game, &input, 1.0f);

    if (game.mode != GAME_MODE_PLAYING || game.elapsed != 0.0f) {
        fprintf(stderr, "smoke failed: resume did not restore gameplay\n");
        return 1;
    }

    RankingLoad(rankings, &rankingCount);
    printf("smoke ok: map=%dx%d hp=%d difficulty=%s weapons=%d rankings=%d pause=ok\n",
        game.mapWidth,
        game.mapHeight,
        game.player.health,
        GameDifficultyName(game.difficulty),
        WEAPON_COUNT,
        rankingCount);
    return 0;
}

int main(int argc, char **argv)
{
    AppScreen screen = SCREEN_TITLE;
    Game game;
    RankingEntry rankings[MAX_RANKINGS];
    int rankingCount = 0;
    GameDifficulty selectedDifficulty = DIFFICULTY_EASY;
    bool running = true;
    bool soundEnabled = true;
    bool scoreSaved = false;
    unsigned int appPendingSounds = 0;
    double previousTime;
    char playerName[MAX_NAME_LEN + 1] = {0};
    int playerNameLen = 0;

    srand((unsigned int)time(NULL));

    if (argc > 1 && strcmp(argv[1], "--smoke-test") == 0) {
        return RunSmokeTest();
    }

    PlatformEnterTerminal();
    GameInit(&game, selectedDifficulty);
    RankingLoad(rankings, &rankingCount);
    previousTime = PlatformNowSeconds();

    while (running) {
        InputState input;
        const double currentTime = PlatformNowSeconds();
        float dt = (float)(currentTime - previousTime);
        previousTime = currentTime;

        if (dt > 0.12f) {
            dt = 0.12f;
        }

        ReadInput(&input);

        if (input.muteToggle) {
            soundEnabled = !soundEnabled;
            if (soundEnabled) {
                appPendingSounds |= SOUND_UI_CONFIRM;
            }
        }

        if (screen == SCREEN_TITLE) {
            if (input.quit || input.back || input.escape) {
                running = false;
            } else if (input.ranking) {
                RankingLoad(rankings, &rankingCount);
                screen = SCREEN_RANKING;
                appPendingSounds |= SOUND_UI_CONFIRM;
            } else if (input.start || input.select) {
                screen = SCREEN_SETUP;
                appPendingSounds |= SOUND_UI_CONFIRM;
            }

            UiDrawTitle();
        } else if (screen == SCREEN_SETUP) {
            const GameDifficulty previousDifficulty = selectedDifficulty;

            if (input.quit) {
                running = false;
            } else if (input.back || input.escape) {
                screen = SCREEN_TITLE;
                appPendingSounds |= SOUND_UI_CONFIRM;
            } else if (input.number == 1 || input.left || input.up) {
                selectedDifficulty = DIFFICULTY_EASY;
            } else if (input.number == 2 || input.right || input.down) {
                selectedDifficulty = DIFFICULTY_HARD;
            }
            if (selectedDifficulty != previousDifficulty) {
                appPendingSounds |= SOUND_UI_MOVE;
            }

            if ((input.select && input.number == 0) || input.number == 1 || input.number == 2) {
                GameInit(&game, selectedDifficulty);
                scoreSaved = false;
                screen = SCREEN_GAME;
                appPendingSounds |= SOUND_UI_CONFIRM;
            }

            if (screen == SCREEN_SETUP) {
                UiDrawSetup(selectedDifficulty);
            }
        } else if (screen == SCREEN_RANKING) {
            if (input.quit) {
                running = false;
            } else if (input.back || input.escape) {
                screen = SCREEN_TITLE;
                appPendingSounds |= SOUND_UI_CONFIRM;
            } else if (input.start || input.select) {
                screen = SCREEN_SETUP;
                appPendingSounds |= SOUND_UI_CONFIRM;
            }

            UiDrawRanking(rankings, rankingCount);
        } else if (screen == SCREEN_GAME) {
            if (input.quit && (game.mode == GAME_MODE_PLAYING || game.mode == GAME_MODE_PAUSED)) {
                game.mode = GAME_MODE_GAME_OVER;
                GameRequestSound(&game, SOUND_GAME_OVER);
            }

            GameUpdate(&game, &input, dt);

            if (GameIsFinished(&game)) {
                if (input.restart) {
                    GameInit(&game, selectedDifficulty);
                    scoreSaved = false;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                } else if (input.back || input.escape) {
                    screen = SCREEN_TITLE;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                } else if (!scoreSaved) {
                    /* 이름 입력 화면으로 전환 */
                    memset(playerName, 0, sizeof(playerName));
                    playerNameLen = 0;
                    screen = SCREEN_NAME_INPUT;
                }
            }

            UiDrawGame(&game);
            UiPlaySounds(game.pendingSounds, soundEnabled);
            game.pendingSounds = 0;
        } else if (screen == SCREEN_NAME_INPUT) {
            /* Esc/Q → 이름 없이 저장 (skip) */
            if (input.escape || input.quit) {
                RankingAddAndSave(&game, "NONAME",
                    game.mode == GAME_MODE_VICTORY ? "WIN" : "LOSE");
                RankingLoad(rankings, &rankingCount);
                scoreSaved = true;
                screen = SCREEN_RANKING;
                appPendingSounds |= SOUND_UI_CONFIRM;
            } else {
                /* 이름 입력: 문자 추가 */
                if (input.typedChar != 0 && playerNameLen < MAX_NAME_LEN) {
                    playerName[playerNameLen++] = input.typedChar;
                    playerName[playerNameLen] = '\0';
                }
                /* 백스페이스/B키로 한 글자 삭제 */
                if (input.back && playerNameLen > 0) {
                    playerName[--playerNameLen] = '\0';
                }
                /* Enter → 저장 후 랭킹 화면 */
                if (input.select) {
                    if (playerNameLen == 0) {
                        strncpy(playerName, "NONAME", MAX_NAME_LEN);
                        playerNameLen = 6;
                    }
                    RankingAddAndSave(&game, playerName,
                        game.mode == GAME_MODE_VICTORY ? "WIN" : "LOSE");
                    RankingLoad(rankings, &rankingCount);
                    scoreSaved = true;
                    screen = SCREEN_RANKING;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                }
            }
            UiDrawNameInput(&game, playerName, playerNameLen);
        }

        UiPlaySounds(appPendingSounds, soundEnabled);
        appPendingSounds = 0;
        PlatformSleepFrame(1.0 / 24.0);
    }

    PlatformExitTerminal();
    return 0;
}
