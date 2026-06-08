#ifndef RANKING_H
#define RANKING_H

#include "game_config.h"

typedef struct Game Game;

// 게임 결과: 클리어 또는 패배
typedef enum RankingResult {
    RANKING_RESULT_CLEAR, // 클리어
    RANKING_RESULT_LOSE   // 패배
} RankingResult;

// 랭킹 한 줄의 데이터
typedef struct RankingEntry {
    int score;               // 점수
    int seconds;             // 생존 시간(초)
    int kills;               // 킬 수
    int level;               // 레벨
    char result[16];         // "CLEAR" 또는 "LOSE"
    char name[MAX_NAME_LEN + 1]; // 플레이어 이름
} RankingEntry;

// scores.txt에서 랭킹을 불러옴
void RankingLoad(RankingEntry entries[MAX_RANKINGS], int *count);

// 새 기록을 추가하고 scores.txt에 저장
void RankingAddAndSave(const Game *game, const char *name, RankingResult result);

#endif
