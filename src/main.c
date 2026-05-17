#include "game.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static struct termios originalTermios;
static bool terminalReady = false;

static void TerminalExit(void)
{
    if (!terminalReady) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    printf("\033[?25h\033[?1049l\033[0m");
    fflush(stdout);
    terminalReady = false;
}

static void TerminalEnter(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &originalTermios) != 0) {
        fprintf(stderr, "Failed to read terminal settings.\n");
        exit(1);
    }

    raw = originalTermios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        fprintf(stderr, "Failed to enter raw terminal mode.\n");
        exit(1);
    }

    terminalReady = true;
    atexit(TerminalExit);
    printf("\033[?1049h\033[?25l\033[2J\033[H");
    fflush(stdout);
}

static double NowSeconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void SleepFrame(double seconds)
{
    struct timespec ts;

    if (seconds <= 0.0) {
        return;
    }

    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

static bool ReadByte(char *out)
{
    fd_set set;
    struct timeval timeout;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout) <= 0) {
        return false;
    }

    return read(STDIN_FILENO, out, 1) == 1;
}

static void ReadInput(InputState *input)
{
    char ch;

    memset(input, 0, sizeof(*input));
    input->number = 0;

    while (ReadByte(&ch)) {
        if (ch == '\033') {
            char next;
            char code;

            input->back = true;
            if (ReadByte(&next) && next == '[' && ReadByte(&code)) {
                input->back = false;
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
        GameInit(&game);
        RankingLoad(rankings, &rankingCount);
        printf("smoke ok: hp=%d level=%d weapons=%d rankings=%d\n",
            game.player.health,
            game.player.level,
            WEAPON_COUNT,
            rankingCount);
        return 0;
    }

    TerminalEnter();
    GameInit(&game);
    RankingLoad(rankings, &rankingCount);
    previousTime = NowSeconds();

    while (running) {
        InputState input;
        const double currentTime = NowSeconds();
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
            if (input.quit && game.mode == GAME_MODE_PLAYING) {
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

        SleepFrame(1.0 / 24.0);
    }

    TerminalExit();
    return 0;
}
