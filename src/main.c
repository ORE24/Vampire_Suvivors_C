#include "game.h"
#include "platform.h"
#include "ranking.h"
#include "smoke_test.h"
#include "ui.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum AppScreen {
    SCREEN_TITLE = 0,
    SCREEN_CONTROLS,
    SCREEN_SETUP,
    SCREEN_RANKING,
    SCREEN_GAME,
    SCREEN_NAME_INPUT
} AppScreen;

/* 화면 상태를 배경음악 정책으로 변환하는 main 전용 라우터 */
static PlatformMusic AppMusicForScreen(AppScreen screen)
{
    if (screen == SCREEN_TITLE || screen == SCREEN_CONTROLS || screen == SCREEN_SETUP) {
        return PLATFORM_MUSIC_MENU;
    }
    if (screen == SCREEN_GAME) {
        return PLATFORM_MUSIC_GAME;
    }
    return PLATFORM_MUSIC_NONE;
}

/* 플랫폼 입력 바이트를 게임/메뉴에서 쓰는 한 프레임 입력 상태로 정규화 */
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

        /* 백스페이스/DEL: 이름 입력 한 글자 삭제 (#6) */
        if (ch == '\b' || (unsigned char)ch == 127) {
            input->back = true;
            continue;
        }

        /* 이름 입력용 raw 문자 저장 (알파벳/숫자/-/_) */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '=') {
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

static RankingResult RankingResultForGame(const Game *game)
{
    return game->mode == GAME_MODE_VICTORY ? RANKING_RESULT_CLEAR : RANKING_RESULT_LOSE;
}

/* 앱 화면 전환, 게임 업데이트, 렌더링, 사운드 재생을 묶는 최상위 루프 */
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
                screen = SCREEN_CONTROLS;
                appPendingSounds |= SOUND_UI_CONFIRM;
            }

            UiDrawTitle();
        } else if (screen == SCREEN_CONTROLS) {
            if (input.quit) {
                running = false;
            } else if (input.back || input.escape) {
                screen = SCREEN_TITLE;
                appPendingSounds |= SOUND_UI_CONFIRM;
            } else if (input.start || input.select) {
                screen = SCREEN_SETUP;
                appPendingSounds |= SOUND_UI_CONFIRM;
            }

            UiDrawControls();
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
                UiClearScreen();
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
                if (input.back || input.escape) {
                    screen = SCREEN_TITLE;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                } else if ((input.select || input.start || input.ranking || input.restart) && !scoreSaved) {
                    memset(playerName, 0, sizeof(playerName));
                    playerNameLen = 0;
                    screen = SCREEN_NAME_INPUT;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                } else if ((input.select || input.start || input.ranking || input.restart) && scoreSaved) {
                    RankingLoad(rankings, &rankingCount);
                    screen = SCREEN_RANKING;
                    appPendingSounds |= SOUND_UI_CONFIRM;
                }
            }

            UiDrawGame(&game);
            UiPlaySounds(game.pendingSounds, soundEnabled);
            game.pendingSounds = 0;
        } else if (screen == SCREEN_NAME_INPUT) {
            /* Esc/Q: 이름 없이 저장 */
            if (input.escape || input.quit) {
                RankingAddAndSave(&game, "NONAME", RankingResultForGame(&game));
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
                    RankingAddAndSave(&game, playerName, RankingResultForGame(&game));
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
        PlatformPlayMusic(AppMusicForScreen(screen), soundEnabled);
        PlatformSleepFrame(1.0 / 24.0);
    }

    PlatformExitTerminal();
    return 0;
}
