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
    input->number = 0;

    while (PlatformReadByte(&ch)) {
        if (ch == '\033') {
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
    RankingEntry rankings[MAX_RANKINGS];
    InputState input;
    int rankingCount = 0;

    GameInit(&game);
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
    printf("smoke ok: map=%dx%d hp=%d level=%d weapons=%d rankings=%d pause=ok\n",
        game.mapWidth,
        game.mapHeight,
        game.player.health,
        game.player.level,
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
    bool running = true;
    bool soundEnabled = true;
    bool scoreSaved = false;
    double previousTime;

    srand((unsigned int)time(NULL));

    if (argc > 1 && strcmp(argv[1], "--smoke-test") == 0) {
        return RunSmokeTest();
    }

    PlatformEnterTerminal();
    GameInit(&game);
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
                GameInit(&game);
                scoreSaved = false;
                screen = SCREEN_GAME;
            }

            UiDrawTitle();
        } else if (screen == SCREEN_RANKING) {
            if (input.quit) {
                running = false;
            } else if (input.back) {
                screen = SCREEN_TITLE;
            } else if (input.start || input.select) {
                GameInit(&game);
                scoreSaved = false;
                screen = SCREEN_GAME;
            }

            UiDrawRanking(rankings, rankingCount);
        } else if (screen == SCREEN_GAME) {
            if (input.quit && (game.mode == GAME_MODE_PLAYING || game.mode == GAME_MODE_PAUSED)) {
                game.mode = GAME_MODE_GAME_OVER;
                GameRequestSound(&game, SOUND_GAME_OVER);
            }

            GameUpdate(&game, &input, dt);

            if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && !scoreSaved) {
                RankingAddAndSave(&game, game.mode == GAME_MODE_VICTORY ? "WIN" : "LOSE");
                RankingLoad(rankings, &rankingCount);
                scoreSaved = true;
            }

            if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && input.restart) {
                GameInit(&game);
                scoreSaved = false;
            } else if ((game.mode == GAME_MODE_GAME_OVER || game.mode == GAME_MODE_VICTORY) && input.back) {
                screen = SCREEN_TITLE;
            }

            UiDrawGame(&game);
            UiPlaySounds(game.pendingSounds, soundEnabled);
            game.pendingSounds = 0;
        }

        PlatformSleepFrame(1.0 / 24.0);
    }

    PlatformExitTerminal();
    return 0;
}
