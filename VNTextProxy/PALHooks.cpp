#include "pch.h"

#include <string>

#include <windows.h>

#define PALTEXT_LOGGING 0

namespace PALGrabCurrentText
{
#ifdef PALTEXT_LOGGING
    static FILE* g_logFile = nullptr;
    static void dbg_log(const char* format, ...)
    {
        if (!g_logFile)
            g_logFile = _fsopen("./GrabText_hook.log", "w", _SH_DENYNO);
        if (g_logFile)
        {
            va_list args;
            va_start(args, format);
            vfprintf(g_logFile, format, args);
            fprintf(g_logFile, "\n");
            va_end(args);
            fflush(g_logFile);
        }
    }
#else
#define dbg_log(...)
#endif

    static string currentText;
    const string& get() { return currentText; }

    constexpr uintptr_t CREATE_TEXT_SPRITES_RVA = 0x0019C10;
    static void* oCreateTextSprites = nullptr;

    static void LogText(const char* text)
    {
        currentText = text;

#ifdef PALTEXT_LOGGING
        dbg_log("Text: %s", text);
#endif
    }

    // this method's calling convention is quite funky.
    extern "C" __declspec(naked) void CreateTextSprites_Hook()
    {
        __asm {
            mov     eax, [esp + 8]

            pushfd
            pushad
            push    eax // text
            call    LogText
            add     esp, 4
            popad
            popfd

            jmp     oCreateTextSprites
        }
    }

    bool Install()
    {
        uint8_t* base = (uint8_t*)GetModuleHandle(nullptr);
        oCreateTextSprites = base + CREATE_TEXT_SPRITES_RVA;

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&oCreateTextSprites, CreateTextSprites_Hook);
        LONG error = DetourTransactionCommit();

        return error == NO_ERROR;
    }
}

namespace PALVideoFix
{
    namespace
    {
        // Used by the game to switch between display modes (0:Windowed, 1:Fullscreen),
        // where wParam is the display mode to switch to.
        constexpr UINT MSG_TOGGLE_DISPLAY_MODE = WM_USER + 2;

        constexpr const char* TARGET_DLL_NAME = "./dll/PAL.dll";
        constexpr const char* TARGET_FUNCTION_NAME = "PalVideoPlay";
        constexpr uintptr_t GAME_MANAGER_POINTER_OFFSET = 0x30989F8;

#ifdef PALTEXT_LOGGING
        static FILE* g_debugLogFile = nullptr;
        static void dbg_log(const char* format, ...)
        {
            if (!g_debugLogFile)
            {
                fopen_s(&g_debugLogFile, "./VideoFix_hook_debug.log", "w");
            }
            if (g_debugLogFile)
            {
                va_list args;
                va_start(args, format);
                vfprintf(g_debugLogFile, format, args);
                fprintf(g_debugLogFile, "\n");
                va_end(args);
                fflush(g_debugLogFile);
            }
        }
#else
#define dbg_log(...)
#endif

#pragma pack(push, 1)
        struct GameManager
        {
            void* pGameDevice;
            BYTE gap4[4];
            HWND hWnd;
            BOOL isRunning;
            BYTE gap10[12];
            DWORD dword1C;
            BYTE gap20[8];
            DWORD dword28;
            BYTE gap2C[260];
            HANDLE hThread2;
            HANDLE hThread1;
            HANDLE hEvent2;
            HANDLE hEvent1;
            DWORD threadId2;
            DWORD threadId1;
            BYTE gap148[116];
            DWORD dword1BC;
            DWORD dword1C0;
            WORD word1C4;
            WORD word1C6;
            WORD word1C8;
            BYTE gap1CA[162];
            DWORD defferedWindowMode; // 0 for Windowed, 1 for Fullscreen
        };
#pragma pack(pop)

        static GameManager* g_pGameMgr = nullptr;
        static int(__cdecl* oPalVideoPlay)(const char* fileName) = nullptr;

        int __cdecl PalVideoPlay_Hook(const char* fileName)
        {
            static bool isInitialized = false;
            if (!isInitialized)
            {
                dbg_log("PalVideoPlay_Hook: First run, performing initialization...");
                HMODULE hMod = GetModuleHandleA(TARGET_DLL_NAME);
                if (hMod)
                {
                    uintptr_t moduleBase = (uintptr_t)hMod;
                    g_pGameMgr = *(GameManager**)(moduleBase + GAME_MANAGER_POINTER_OFFSET);
                    dbg_log("PalVideoPlay_Hook: Module base=0x%p, g_pGameMgr=0x%p, g_pGameMgr->hWnd=0x%p", hMod, g_pGameMgr, g_pGameMgr->hWnd);
                }
                else
                {
                    dbg_log("PalVideoPlay_Hook: ERROR - Could not get module handle for '%s'", TARGET_DLL_NAME);
                }
                isInitialized = true;
            }

            dbg_log("PalVideoPlay_Hook: Playing '%s'", fileName);

            int result = oPalVideoPlay(fileName);

            if (g_pGameMgr && g_pGameMgr->defferedWindowMode != 0)
            {
                dbg_log("PalVideoPlay_Hook: Fullscreen mode detected. Posting messages to reset display.");
                PostMessageA(g_pGameMgr->hWnd, MSG_TOGGLE_DISPLAY_MODE, 0, 0);
                PostMessageA(g_pGameMgr->hWnd, MSG_TOGGLE_DISPLAY_MODE, 1, 0);
            }
            else
            {
                dbg_log("PalVideoPlay_Hook: Windowed mode detected. No action needed.");
            }

            return result;
        }
    }

    bool Install()
    {
        dbg_log("VideoFix::Install() called.");

        LoadLibraryA("./dll/ogg.dll");
        LoadLibraryA("./dll/vorbis.dll");
        LoadLibraryA("./dll/vorbisfile.dll");
        HMODULE hMod = LoadLibraryA(TARGET_DLL_NAME);
        if (!hMod)
        {
            dbg_log("VideoFix::Install: Failed to load '%s'.", TARGET_DLL_NAME);
            return false;
        }

        oPalVideoPlay = (decltype(oPalVideoPlay))GetProcAddress(hMod, TARGET_FUNCTION_NAME);
        if (!oPalVideoPlay)
        {
            dbg_log("VideoFix::Install: Failed to find function '%s' in '%s'.", TARGET_FUNCTION_NAME, TARGET_DLL_NAME);
            return false;
        }

        dbg_log("VideoFix::Install: Found '%s' at address 0x%p.", TARGET_FUNCTION_NAME, oPalVideoPlay);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)oPalVideoPlay, PalVideoPlay_Hook);
        LONG error = DetourTransactionCommit();

        if (error == NO_ERROR)
        {
            dbg_log("VideoFix::Install: Hook for '%s' installed successfully.", TARGET_FUNCTION_NAME);
            return true;
        }

        dbg_log("VideoFix::Install: Failed to install hook, Detours error: %d", error);
        oPalVideoPlay = nullptr;
        return false;
    }
}