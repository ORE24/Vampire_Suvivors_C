#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

typedef enum PlatformSound {
    PLATFORM_SOUND_NONE = 0,
    PLATFORM_SOUND_UI_MOVE,
    PLATFORM_SOUND_UI_CONFIRM,
    PLATFORM_SOUND_SHOOT,
    PLATFORM_SOUND_XP_PICKUP,
    PLATFORM_SOUND_HEAL_PICKUP,
    PLATFORM_SOUND_POWER_PICKUP,
    PLATFORM_SOUND_LEVEL_UP,
    PLATFORM_SOUND_HIT,
    PLATFORM_SOUND_GAME_OVER,
    PLATFORM_SOUND_VICTORY,
    PLATFORM_SOUND_COUNT
} PlatformSound;

void PlatformEnterTerminal(void);
void PlatformExitTerminal(void);
double PlatformNowSeconds(void);
void PlatformSleepFrame(double seconds);
bool PlatformReadByte(char *out);
void PlatformGetTerminalSize(int *columns, int *rows);
const char *PlatformSoundPath(PlatformSound sound);
void PlatformPlaySound(PlatformSound sound);

#endif
