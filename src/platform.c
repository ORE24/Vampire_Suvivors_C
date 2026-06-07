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
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

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
static PlatformMusic currentMusic = PLATFORM_MUSIC_NONE;
static bool musicOpen = false;

static const char *SOUND_PATHS[PLATFORM_SOUND_COUNT] = {
    "",
    "assets\\audio\\ui_move.wav",
    "assets\\audio\\ui_confirm.wav",
    "assets\\audio\\shoot.wav",
    "assets\\audio\\xp_pickup.wav",
    "assets\\audio\\heal_pickup.wav",
    "assets\\audio\\power_pickup.wav",
    "assets\\audio\\level_up.wav",
    "assets\\audio\\hit.wav",
    "assets\\audio\\game_over.wav",
    "assets\\audio\\victory.wav"
};

static const char *MUSIC_PATHS[PLATFORM_MUSIC_COUNT] = {
    "",
    "assets\\audio\\menu_music.mp3",
    "assets\\audio\\game_music.mp3"
};

/* MCI로 열린 현재 배경음악 alias를 정리 */
static void PlatformStopMusic(void)
{
    if (musicOpen) {
        (void)mciSendStringA("stop vsc_music", NULL, 0, NULL);
        (void)mciSendStringA("close vsc_music", NULL, 0, NULL);
        musicOpen = false;
    }
    currentMusic = PLATFORM_MUSIC_NONE;
}

/* Windows 확장키 코드를 ANSI 방향키 시퀀스로 바꿔 상위 입력 로직을 단순화 */
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

/* 게임 시작 시 콘솔을 UTF-8, raw input, ANSI 출력 모드로 전환 */
void PlatformEnterTerminal(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* 프레임 전체를 버퍼링했다가 fflush 한 번에 출력 → 화면 흔들림 제거 */
    static char frameBuffer[256 * 1024];
    setvbuf(stdout, frameBuffer, _IOFBF, sizeof(frameBuffer));

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

/* 종료 시 콘솔 모드와 화면 상태를 원래대로 복구 */
void PlatformExitTerminal(void)
{
    if (!terminalReady) {
        PlatformStopMusic();
        return;
    }

    PlatformStopMusic();
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

/* 비차단 입력을 한 바이트씩 제공하고 방향키는 ESC 시퀀스로 내보낸다 */
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

const char *PlatformSoundPath(PlatformSound sound)
{
    if (sound <= PLATFORM_SOUND_NONE || sound >= PLATFORM_SOUND_COUNT) {
        return "";
    }

    return SOUND_PATHS[sound];
}

const char *PlatformMusicPath(PlatformMusic music)
{
    if (music <= PLATFORM_MUSIC_NONE || music >= PLATFORM_MUSIC_COUNT) {
        return "";
    }

    return MUSIC_PATHS[music];
}

/* 짧은 효과음은 WinMM PlaySound로 비동기 재생 */
void PlatformPlaySound(PlatformSound sound)
{
    const char *path = PlatformSoundPath(sound);

    if (path[0] == '\0') {
        return;
    }

    (void)PlaySoundA(path, NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
}

/* 화면별 배경음악을 MCI alias 하나로 반복 재생하며 중복 재시작을 막는다 */
void PlatformPlayMusic(PlatformMusic music, bool enabled)
{
    char command[512];
    const char *path;

    if (!enabled || music == PLATFORM_MUSIC_NONE) {
        PlatformStopMusic();
        return;
    }

    if (musicOpen && currentMusic == music) {
        return;
    }

    PlatformStopMusic();
    path = PlatformMusicPath(music);
    if (path[0] == '\0') {
        return;
    }

    (void)snprintf(command, sizeof(command),
        "open \"%s\" type mpegvideo alias vsc_music",
        path);
    if (mciSendStringA(command, NULL, 0, NULL) != 0) {
        return;
    }

    musicOpen = true;
    currentMusic = music;
    (void)mciSendStringA("play vsc_music repeat", NULL, 0, NULL);
}

#endif
