#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

#if !defined(_WIN32)
#error "This project supports Windows only. Build it with Visual Studio 2022 on Windows."
#elif !defined(_MSC_VER) || defined(__clang__)
#error "This project supports the MSVC compiler from Visual Studio 2022 only."
#else

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

    printf("\033[0m\033[2J\033[H\033[?25h\033[?1049l");
    fflush(stdout);

    if (inputModeReady) {
        SetConsoleMode(inputHandle, originalInputMode);
    }
    if (outputModeReady) {
        SetConsoleMode(outputHandle, originalOutputMode);
    }

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

void PlatformGetTerminalSize(int *columns, int *rows)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (columns != NULL) {
        *columns = 100;
    }
    if (rows != NULL) {
        *rows = 36;
    }

    if (outputHandle == NULL || outputHandle == INVALID_HANDLE_VALUE) {
        outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (outputHandle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(outputHandle, &info)) {
        if (columns != NULL) {
            *columns = info.srWindow.Right - info.srWindow.Left + 1;
        }
        if (rows != NULL) {
            *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        }
    }
}

#endif
