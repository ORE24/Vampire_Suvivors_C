#ifndef UI_H
#define UI_H

#include "game.h"
#include "platform.h"
#include "ranking.h"

void UiDrawTitle(void);
void UiDrawSetup(GameDifficulty selectedDifficulty);
void UiDrawRanking(const RankingEntry entries[MAX_RANKINGS], int count);
void UiDrawNameInput(const Game *game, const char *name, int nameLen);
void UiDrawGame(const Game *game);
PlatformSound UiSoundEvent(unsigned int flags);
void UiPlaySounds(unsigned int flags, bool enabled);

#endif
