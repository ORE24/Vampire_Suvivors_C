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

	memset(input, 0, sizeof(*input)); //InputState ����ü�� ��� ����� 0���� �ʱ�ȭ
    // input->number = 0; <- **�ʿ����. ����� �� �ּ� ó��.**

	while (PlatformReadByte(&ch)) { // ch�� Ű���忡�� �Էµ� ���� ����, �Է��� ������ false ��ȯ�Ͽ� ���� ����
        if (ch == '\033') { /* ESC: 화살표 또는 Esc 처리 */
            char next;
            char code;

            input->back = true;
            input->pauseToggle = true;
            if (PlatformReadByte(&next) && next == '[' && PlatformReadByte(&code)) {
                input->back = false;
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
        }

        if (screen == SCREEN_TITLE) {
            if (input.quit || input.back) {
                running = false;
            } else if (input.ranking) {
                RankingLoad(rankings, &rankingCount);
                screen = SCREEN_RANKING;
            } else if (input.start || input.select) {
                screen = SCREEN_SETUP;
            }

            UiDrawTitle();
        } else if (screen == SCREEN_SETUP) {
            if (input.quit) {
                running = false;
            } else if (input.back) {
                screen = SCREEN_TITLE;
            } else if (input.number == 1 || input.left || input.up) {
                selectedDifficulty = DIFFICULTY_EASY;
            } else if (input.number == 2 || input.right || input.down) {
                selectedDifficulty = DIFFICULTY_HARD;
            }

            if ((input.select && input.number == 0) || input.number == 1 || input.number == 2) {
                GameInit(&game, selectedDifficulty);
                scoreSaved = false;
                screen = SCREEN_GAME;
            }

            if (screen == SCREEN_SETUP) {
                UiDrawSetup(selectedDifficulty);
            }
        } else if (screen == SCREEN_RANKING) {
            if (input.quit) {
                running = false;
            } else if (input.back) {
                screen = SCREEN_TITLE;
            } else if (input.start || input.select) {
                screen = SCREEN_SETUP;
            }

            UiDrawRanking(rankings, rankingCount);
        } else if (screen == SCREEN_GAME) {
            if (input.quit && (game.mode == GAME_MODE_PLAYING || game.mode == GAME_MODE_PAUSED)) {
                game.mode = GAME_MODE_GAME_OVER;
                GameRequestSound(&game, SOUND_GAME_OVER);
            }

            GameUpdate(&game, &input, dt);

            if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && !scoreSaved) {
                /* 이름 입력 화면으로 전환 */
                memset(playerName, 0, sizeof(playerName));
                playerNameLen = 0;
                screen = SCREEN_NAME_INPUT;
            }

            if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && input.restart) {
                GameInit(&game, selectedDifficulty);
                scoreSaved = false;
            } else if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && input.back) {
                screen = SCREEN_TITLE;
            }

            UiDrawGame(&game);
            UiPlaySounds(game.pendingSounds, soundEnabled);
            game.pendingSounds = 0;
        } else if (screen == SCREEN_NAME_INPUT) {
            /* 이름 입력: 문자 추가 */
            if (input.typedChar != 0 && playerNameLen < MAX_NAME_LEN) {
                playerName[playerNameLen++] = input.typedChar;
                playerName[playerNameLen] = '\0';
            }
            /* B키로 한 글자 삭제 */
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
            }
            /* Q/Esc → 이름 없이 저장 */
            if (input.quit) {
                RankingAddAndSave(&game, "NONAME",
                    game.mode == GAME_MODE_VICTORY ? "WIN" : "LOSE");
                RankingLoad(rankings, &rankingCount);
                scoreSaved = true;
                screen = SCREEN_RANKING;
            }
            UiDrawNameInput(&game, playerName, playerNameLen);
        }

		PlatformSleepFrame(1.0 / 24.0); // �ʴ� 24 ���������� ���� ���� ����
    }

    PlatformExitTerminal();
    return 0;
}
