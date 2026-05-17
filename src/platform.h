#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

void PlatformEnterTerminal(void);
void PlatformExitTerminal(void);
double PlatformNowSeconds(void);
void PlatformSleepFrame(double seconds);
bool PlatformReadByte(char *out);
void PlatformGetTerminalSize(int *columns, int *rows);

#endif
