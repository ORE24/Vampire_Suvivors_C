#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

#include <conio.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE inputHandle;
static HANDLE outputHandle;
static DWORD originalInputMode;
static DWORD originalOutputMode;
static bool inputModeReady = false;
static bool outputModeReady = false;
static bool terminalReady = false;
static char pendingBytes[4];
static int pendingIndex = 0;
static int pendingCount = 0;

static bool TranslateWindowsKey(int key, char *out)
{
    char code = '\0';

    if (key == 72) {
        code = 'A';
    } else if (key == 80) {
        code = 'B';
    } else if (key == 77) {
        code = 'C';
    } else if (key == 75) {
        code = 'D';
    }

    if (code == '\0') {
        return false;
    }

    pendingBytes[0] = '\033';
    pendingBytes[1] = '[';
    pendingBytes[2] = code;
    pendingIndex = 1;
    pendingCount = 3;
    *out = pendingBytes[0];
    return true;
}

void PlatformEnterTerminal(void)
{
    inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (inputHandle != INVALID_HANDLE_VALUE &&
        GetConsoleMode(inputHandle, &originalInputMode)) {
        DWORD rawMode = originalInputMode;
        rawMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
        SetConsoleMode(inputHandle, rawMode);
        inputModeReady = true;
    }

    if (outputHandle != INVALID_HANDLE_VALUE &&
        GetConsoleMode(outputHandle, &originalOutputMode)) {
        DWORD outputMode = originalOutputMode;
        outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(outputHandle, outputMode);
        outputModeReady = true;
    }

    terminalReady = true;
    atexit(PlatformExitTerminal);
    printf("\033[?1049h\033[?25l\033[2J\033[H");
    fflush(stdout);
}

void PlatformExitTerminal(void)
{
    if (!terminalReady) {
        return;
    }

    if (inputModeReady) {
        SetConsoleMode(inputHandle, originalInputMode);
    }
    if (outputModeReady) {
        SetConsoleMode(outputHandle, originalOutputMode);
    }

    printf("\033[?25h\033[?1049l\033[0m");
    fflush(stdout);
    terminalReady = false;
}

double PlatformNowSeconds(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

void PlatformSleepFrame(double seconds)
{
    if (seconds <= 0.0) {
        return;
    }

    Sleep((DWORD)(seconds * 1000.0));
}

bool PlatformReadByte(char *out)
{
    int ch;

    if (pendingIndex < pendingCount) {
        *out = pendingBytes[pendingIndex++];
        if (pendingIndex >= pendingCount) {
            pendingIndex = 0;
            pendingCount = 0;
        }
        return true;
    }

    if (!_kbhit()) {
        return false;
    }

    ch = _getch();
    if (ch == 0 || ch == 224) {
        return TranslateWindowsKey(_getch(), out);
    }

    *out = (char)ch;
    return true;
}

#else

#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static struct termios originalTermios;
static bool terminalReady = false;

void PlatformEnterTerminal(void)
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
    atexit(PlatformExitTerminal);
    printf("\033[?1049h\033[?25l\033[2J\033[H");
    fflush(stdout);
}

void PlatformExitTerminal(void)
{
    if (!terminalReady) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    printf("\033[?25h\033[?1049l\033[0m");
    fflush(stdout);
    terminalReady = false;
}

double PlatformNowSeconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void PlatformSleepFrame(double seconds)
{
    struct timespec ts;

    if (seconds <= 0.0) {
        return;
    }

    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

bool PlatformReadByte(char *out)
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

#endif
