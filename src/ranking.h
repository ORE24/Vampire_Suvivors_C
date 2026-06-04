#ifndef RANKING_H
#define RANKING_H

#include "game_config.h"

typedef struct Game Game;

typedef enum RankingResult {
    RANKING_RESULT_CLEAR = 0,
    RANKING_RESULT_LOSE
} RankingResult;

typedef struct RankingEntry {
    int score;
    int seconds;
    int kills;
    int level;
    char result[16];
    char name[MAX_NAME_LEN + 1];
} RankingEntry;

void RankingLoad(RankingEntry entries[MAX_RANKINGS], int *count);
void RankingAddAndSave(const Game *game, const char *name, RankingResult result);

#endif
