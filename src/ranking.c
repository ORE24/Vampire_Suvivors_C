#include "ranking.h"

#include "game.h"

#include <stdio.h>
#include <string.h>

static const char *SCORE_FILE = "scores.txt";

/* 저장 포맷은 문자열이지만 호출부는 RankingResult enum만 다루게 한다 */
static const char *RankingResultLabel(RankingResult result)
{
    if (result == RANKING_RESULT_CLEAR) {
        return "CLEAR";
    }

    return "LOSE";
}

/* scores.txt에서 현재 랭킹 표시에 필요한 최대 기록만 읽는다 */
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
           fscanf(file, "%15s %15s %d %d %d %d",
               entries[loaded].name,
               entries[loaded].result,
               &entries[loaded].score,
               &entries[loaded].seconds,
               &entries[loaded].kills,
               &entries[loaded].level) == 6) {
        loaded++;
    }

    fclose(file);
    *count = loaded;
}

/* 현재 게임 결과를 랭킹 목록에 끼워 넣고 점수 내림차순으로 저장 */
void RankingAddAndSave(const Game *game, const char *name, RankingResult result)
{
    RankingEntry entries[MAX_RANKINGS + 1];
    int count = 0;
    FILE *file;
    int insertIndex;
    const char *resultLabel = RankingResultLabel(result);

    RankingLoad(entries, &count);

    if (count < MAX_RANKINGS + 1) {
        strncpy(entries[count].name, name, sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        strncpy(entries[count].result, resultLabel, sizeof(entries[count].result) - 1);
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
        fprintf(file, "%s %s %d %d %d %d\n",
            entries[i].name[0] ? entries[i].name : "NONAME",
            entries[i].result,
            entries[i].score,
            entries[i].seconds,
            entries[i].kills,
            entries[i].level);
    }

    fclose(file);
}
