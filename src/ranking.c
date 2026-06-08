#include "ranking.h"

#include "game.h"

#include <stdio.h>
#include <string.h>

// 랭킹 파일 이름 고정
static const char *SCORE_FILE = "scores.txt";

// RankingResult enum을 파일에 저장할 문자열로 변환
static const char *RankingResultLabel(RankingResult result)
{
    if (result == RANKING_RESULT_CLEAR) {
        return "CLEAR";
    }

    return "LOSE";
}

// scores.txt에서 현재 랭킹 표시에 필요한 최대 기록만 읽는다
void RankingLoad(RankingEntry entries[MAX_RANKINGS], int *count)
{
    FILE *file = fopen(SCORE_FILE, "r"); // 읽기 모드로 파일 열기
    int loaded = 0;

    if (count == NULL) { // 잘못된 입력 방어
        return;
    }

    *count = 0;
    if (file == NULL) { // 파일 없으면 종료 (첫 실행 시)
        return;
    }

    // 한 줄씩 읽어서 entries 배열에 저장, 최대 MAX_RANKINGS개까지
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
    *count = loaded; // 실제로 읽은 개수 반환
}

// 현재 게임 결과를 랭킹 목록에 끼워 넣고 점수 내림차순으로 저장
void RankingAddAndSave(const Game *game, const char *name, RankingResult result)
{
    RankingEntry entries[MAX_RANKINGS + 1]; // 새 기록 추가 위해 +1
    int count = 0;
    FILE *file;
    const char *resultLabel = RankingResultLabel(result); // enum → 문자열 변환

    RankingLoad(entries, &count); // 기존 랭킹 불러오기

    // 배열이 꽉 차지 않았으면 새 기록 추가
    if (count < MAX_RANKINGS + 1) {
        strncpy(entries[count].name, name, sizeof(entries[count].name) - 1);
        strncpy(entries[count].result, resultLabel, sizeof(entries[count].result) - 1);
        entries[count].score = game->player.score + (int)game->elapsed; // 점수 + 생존시간
        entries[count].seconds = (int)game->elapsed;
        entries[count].kills = game->player.kills;
        entries[count].level = game->player.level;
        count++;
    }

    // 버블 정렬: 점수 내림차순
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (entries[j].score < entries[j + 1].score) {
                RankingEntry temp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = temp;
            }
        }
    }

    // 6개면 마지막 잘라내고 5개만 유지
    if (count > MAX_RANKINGS) {
        count = MAX_RANKINGS;
    }

    file = fopen(SCORE_FILE, "w"); // 덮어쓰기 모드로 파일 열기
    if (file == NULL) {
        return;
    }

    // 랭킹 데이터를 파일에 저장
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s %s %d %d %d %d\n",
            entries[i].name[0] ? entries[i].name : "UNKNOWN", // 이름 없으면 UNKNOWN
            entries[i].result,
            entries[i].score,
            entries[i].seconds,
            entries[i].kills,
            entries[i].level);
    }

    fclose(file);
}
