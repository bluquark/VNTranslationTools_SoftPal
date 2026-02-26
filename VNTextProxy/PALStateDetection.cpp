#include "pch.h"
#include "PALStateDetection.h"
#include "Util/Logger.h"

// Defined in PALHooks.cpp
namespace PALGrabCurrentText {
    extern void* (__cdecl* oPalTaskGetData)(void*);
    extern int textOffset;
}

#define dbg_log(...) proxy_log(LogCategory::HOOKS, __VA_ARGS__)

bool isChoice = false;
bool isSaveScreen = false;
const unsigned char* spriteText = nullptr;
const unsigned char* fontBeginText = nullptr;

namespace PALStateDetection
{
    // Generic __cdecl function type with 8 int-sized params.
    // On x86 __cdecl, the caller manages the stack, so declaring more params
    // than the real function expects is safe: the extra values are pushed and
    // popped by our code, and the callee ignores them. This lets us capture
    // up to 8 args from any PAL function without knowing its exact signature.
    typedef int (__cdecl* PalFunc)(int, int, int, int, int, int, int, int);

    //-----------------------------------------------------------------------
    // Hook macro: generates a trampoline pointer and hook function for each
    // PAL export. The hook logs all 8 potential args, calls the original via
    // the Detours trampoline, and logs the return value.
    //-----------------------------------------------------------------------
    #define PAL_HOOK(name) \
        static PalFunc o_##name = nullptr; \
        static int __cdecl name##_Hook(int a1, int a2, int a3, int a4, \
                                        int a5, int a6, int a7, int a8) \
        { \
            dbg_log("[PAL_STATE] " #name "(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)", \
                    a1, a2, a3, a4, a5, a6, a7, a8); \
            int ret = o_##name(a1, a2, a3, a4, a5, a6, a7, a8); \
            dbg_log("[PAL_STATE] " #name " -> 0x%x (%d)", ret, ret); \
            return ret; \
        }


    //===================================================================
    // Task management - tracks game flow states (dialogue, choice, save)
    //===================================================================
    PAL_HOOK(PalTaskCreate)
    PAL_HOOK(PalTaskFree)
    PAL_HOOK(PalTaskChangeState)
    PAL_HOOK(PalTaskChangeNext)
    PAL_HOOK(PalTaskChangeNextImm)
    PAL_HOOK(PalTaskGetState)
    PAL_HOOK(PalTaskSetMessage)
    PAL_HOOK(PalTaskGetMessage)
    PAL_HOOK(PalTaskReProcess)
    PAL_HOOK(PalTaskReset)
    PAL_HOOK(PalDefaultCreate)

    //===================================================================
    // Button system - strong choice screen indicator
    // Buttons are the UI elements for player choices.
    //===================================================================
    PAL_HOOK(PalButtonCreate)
    PAL_HOOK(PalButtonCreateEx)
    PAL_HOOK(PalButtonCtrl)
    PAL_HOOK(PalButtonEntry)
    PAL_HOOK(PalButtonEntryEx)
    PAL_HOOK(PalButtonDelete)
    PAL_HOOK(PalButtonRelease)

    PAL_HOOK(PalButtonGetReactionEx)
    PAL_HOOK(PalButtonSetPos)
    PAL_HOOK(PalButtonSetReaction)

    //===================================================================
    // Font/text rendering - context for what mode text is being drawn in
    // NOTE: PalFontSetType is NOT hooked here (already hooked by
    //       PALFontTypeOverride in PALHooks.cpp).
    //===================================================================

    // PalFontBegin: captures a1 as the current rendering text pointer.
    // a1 reliably identifies 3 contexts:
    //   - taskData + textOffset: body text (use directly)
    //   - within taskData before textOffset: name struct (no text tracking)
    //   - anything else: continuation/sprite text (skip leading null bytes)
    static PalFunc o_PalFontBegin = nullptr;

    static int __cdecl PalFontBegin_Hook(int a1, int a2, int a3, int a4,
                                          int a5, int a6, int a7, int a8)
    {
        dbg_log("[PAL_STATE] PalFontBegin(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)",
                a1, a2, a3, a4, a5, a6, a7, a8);

        // Classify a1 to set fontBeginText for this rendering pass
        fontBeginText = nullptr;

        __try
        {
            if (PALGrabCurrentText::oPalTaskGetData)
            {
                void* taskData = PALGrabCurrentText::oPalTaskGetData(nullptr);
                if (taskData)
                {
                    int td = (int)taskData;
                    int textOff = PALGrabCurrentText::textOffset;

                    if (a1 == td + textOff)
                    {
                        // Body text - use a1 directly
                        fontBeginText = (const unsigned char*)a1;
                        dbg_log("[FONT_BEGIN] Body text at a1=0x%08x", a1);
                    }
                    else if (a1 >= td && a1 < td + textOff)
                    {
                        // Within taskData struct before text - name rendering, no text tracking
                        fontBeginText = nullptr;
                        dbg_log("[FONT_BEGIN] Name struct at a1=0x%08x (taskData+0x%x)", a1, a1 - td);
                    }
                    else
                    {
                        // Continuation text or sprite text - skip leading null bytes
                        const unsigned char* p = (const unsigned char*)a1;
                        if (!IsBadReadPtr(p, 16))
                        {
                            int skip = 0;
                            while (skip < 16 && p[skip] == 0) skip++;
                            if (skip < 16)
                            {
                                fontBeginText = p + skip;
                                dbg_log("[FONT_BEGIN] Continuation/sprite text at a1=0x%08x, skip=%d bytes", a1, skip);
                            }
                            else
                            {
                                dbg_log("[FONT_BEGIN] All nulls at a1=0x%08x", a1);
                            }
                        }
                        else
                        {
                            dbg_log("[FONT_BEGIN] Unreadable at a1=0x%08x", a1);
                        }
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            fontBeginText = nullptr;
            dbg_log("[FONT_BEGIN] Exception classifying a1=0x%08x", a1);
        }

        int ret = o_PalFontBegin(a1, a2, a3, a4, a5, a6, a7, a8);
        dbg_log("[PAL_STATE] PalFontBegin -> 0x%x (%d)", ret, ret);
        return ret;
    }

    // PalFontEnd: clears isChoice/isSaveScreen/spriteText (sprite text rendering is finished)
    static PalFunc o_PalFontEnd = nullptr;
    static int __cdecl PalFontEnd_Hook(int a1, int a2, int a3, int a4,
                                        int a5, int a6, int a7, int a8)
    {
        dbg_log("[PAL_STATE] PalFontEnd(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)",
                a1, a2, a3, a4, a5, a6, a7, a8);
        int ret = o_PalFontEnd(a1, a2, a3, a4, a5, a6, a7, a8);
        dbg_log("[PAL_STATE] PalFontEnd -> 0x%x (%d), clearing isChoice/isSaveScreen/fontBeginText", ret, ret);
        isChoice = false;
        isSaveScreen = false;
        spriteText = nullptr;
        fontBeginText = nullptr;
        return ret;
    }
    PAL_HOOK(PalFontDrawText)
    PAL_HOOK(PalFontGetColor)
    PAL_HOOK(PalFontSetColor)
    PAL_HOOK(PalFontGetEffect)
    PAL_HOOK(PalFontSetEffect)
    PAL_HOOK(PalFontGetFontSize)
    PAL_HOOK(PalFontSetFontSize)
    PAL_HOOK(PalFontGetType)
    PAL_HOOK(PalFontGetSize)
    PAL_HOOK(PalFontLoad)
    PAL_HOOK(PalFontUnload)
    PAL_HOOK(PalExFontLoad)
    PAL_HOOK(PalExFontUnload)

    //===================================================================
    // Sprite text creation - text rendered as sprites (UI text, choices)
    //===================================================================

    // Helper: safely read the sprite text string from PalSpriteCreateText/Ex's a2 arg.
    // Returns nullptr if the pointer is unreadable.
    static const char* SafeReadSpriteText(int ptr)
    {
        __try
        {
            if (IsBadReadPtr((void*)ptr, 4))
                return nullptr;
            const char* s = (const char*)ptr;
            // Quick sanity: first byte should be printable or high SJIS
            if ((unsigned char)s[0] < 0x20 && s[0] != 0)
                return nullptr;
            return s;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    // PalSpriteCreateText: sets isChoice (choice button labels)
    // a2 is a direct pointer to the choice/UI text string (pipe-delimited words).
    static PalFunc o_PalSpriteCreateText = nullptr;
    static int __cdecl PalSpriteCreateText_Hook(int a1, int a2, int a3, int a4,
                                                 int a5, int a6, int a7, int a8)
    {
        isChoice = true;
        const char* text = SafeReadSpriteText(a2);
        spriteText = (const unsigned char*)text;
        dbg_log("[PAL_STATE] PalSpriteCreateText(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) [isChoice=1]",
                a1, a2, a3, a4, a5, a6, a7, a8);
        if (text)
            dbg_log("[CHOICE_TEXT] \"%s\"", text);

        int ret = o_PalSpriteCreateText(a1, a2, a3, a4, a5, a6, a7, a8);
        dbg_log("[PAL_STATE] PalSpriteCreateText -> 0x%x (%d)", ret, ret);
        return ret;
    }

    // PalSpriteCreateTextEx: sets isSaveScreen (save screen text)
    // a2 is a direct pointer to the save screen text string (pipe-delimited words).
    // May start with a SJIS high byte (e.g. 0xab for opening quote).
    static PalFunc o_PalSpriteCreateTextEx = nullptr;
    static int __cdecl PalSpriteCreateTextEx_Hook(int a1, int a2, int a3, int a4,
                                                   int a5, int a6, int a7, int a8)
    {
        isSaveScreen = true;
        const char* text = SafeReadSpriteText(a2);
        spriteText = (const unsigned char*)text;
        dbg_log("[PAL_STATE] PalSpriteCreateTextEx(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) [isSaveScreen=1]",
                a1, a2, a3, a4, a5, a6, a7, a8);
        if (text)
            dbg_log("[SAVESCREEN_TEXT] \"%s\"", text);

        int ret = o_PalSpriteCreateTextEx(a1, a2, a3, a4, a5, a6, a7, a8);
        dbg_log("[PAL_STATE] PalSpriteCreateTextEx -> 0x%x (%d)", ret, ret);
        return ret;
    }
    PAL_HOOK(PalSpritePaint)

    //===================================================================
    // Thumbnail creation - strong save screen indicator
    // Save screens need a thumbnail of the current scene.
    //===================================================================
    PAL_HOOK(PalThumbnailCreate)
    PAL_HOOK(PalThumbnailCreateMosaic)

    //===================================================================
    // Screen effects - transitions may bracket mode changes
    //===================================================================
    PAL_HOOK(PalEffect)
    PAL_HOOK(PalEffectEx)

    PAL_HOOK(PalEffectEnable)
    PAL_HOOK(PalEffectEnableIs)

    //===================================================================
    // Hook table for table-driven installation
    //===================================================================
    struct HookEntry {
        const char* exportName;    // Name for GetProcAddress
        PalFunc* pOriginal;        // Where to store the trampoline pointer
        PalFunc hookFunc;          // Our hook function
    };

    #define HOOK_ENTRY(name) { #name, &o_##name, name##_Hook }

    static HookEntry g_hooks[] = {
        // --- Task management ---
        HOOK_ENTRY(PalTaskCreate),
        HOOK_ENTRY(PalTaskFree),
        HOOK_ENTRY(PalTaskChangeState),
        HOOK_ENTRY(PalTaskChangeNext),
        HOOK_ENTRY(PalTaskChangeNextImm),
        HOOK_ENTRY(PalTaskGetState),
        HOOK_ENTRY(PalTaskSetMessage),
        HOOK_ENTRY(PalTaskGetMessage),
        HOOK_ENTRY(PalTaskReProcess),
        HOOK_ENTRY(PalTaskReset),
        HOOK_ENTRY(PalDefaultCreate),

        // --- Button system ---
        HOOK_ENTRY(PalButtonCreate),
        HOOK_ENTRY(PalButtonCreateEx),
        HOOK_ENTRY(PalButtonCtrl),
        HOOK_ENTRY(PalButtonEntry),
        HOOK_ENTRY(PalButtonEntryEx),
        HOOK_ENTRY(PalButtonDelete),
        HOOK_ENTRY(PalButtonRelease),

        HOOK_ENTRY(PalButtonGetReactionEx),
        HOOK_ENTRY(PalButtonSetPos),
        HOOK_ENTRY(PalButtonSetReaction),

        // --- Font/text rendering ---
        HOOK_ENTRY(PalFontBegin),
        HOOK_ENTRY(PalFontEnd),
        HOOK_ENTRY(PalFontDrawText),
        HOOK_ENTRY(PalFontGetColor),
        HOOK_ENTRY(PalFontSetColor),
        HOOK_ENTRY(PalFontGetEffect),
        HOOK_ENTRY(PalFontSetEffect),
        HOOK_ENTRY(PalFontGetFontSize),
        HOOK_ENTRY(PalFontSetFontSize),
        HOOK_ENTRY(PalFontGetType),
        HOOK_ENTRY(PalFontGetSize),
        HOOK_ENTRY(PalFontLoad),
        HOOK_ENTRY(PalFontUnload),
        HOOK_ENTRY(PalExFontLoad),
        HOOK_ENTRY(PalExFontUnload),

        // --- Sprite text ---
        HOOK_ENTRY(PalSpriteCreateText),
        HOOK_ENTRY(PalSpriteCreateTextEx),
        HOOK_ENTRY(PalSpritePaint),

        // --- Thumbnails (save screen) ---
        HOOK_ENTRY(PalThumbnailCreate),
        HOOK_ENTRY(PalThumbnailCreateMosaic),

        // --- Effects ---
        HOOK_ENTRY(PalEffect),
        HOOK_ENTRY(PalEffectEx),

        HOOK_ENTRY(PalEffectEnable),
        HOOK_ENTRY(PalEffectEnableIs),
    };

    //===================================================================
    // Installation
    //===================================================================
    bool Install(HMODULE hPalDll)
    {
        dbg_log("[PAL_STATE] Installing PAL state detection hooks...");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        int hookCount = 0;
        int totalCount = sizeof(g_hooks) / sizeof(g_hooks[0]);

        for (int i = 0; i < totalCount; i++)
        {
            HookEntry& entry = g_hooks[i];
            FARPROC proc = GetProcAddress(hPalDll, entry.exportName);
            if (!proc)
            {
                // Not all functions exist in all PAL.dll versions (FH vs Yureaka)
                continue;
            }

            *entry.pOriginal = (PalFunc)proc;
            DetourAttach(&(PVOID&)*entry.pOriginal, (PVOID)entry.hookFunc);
            dbg_log("[PAL_STATE]   Hooked %s at 0x%p", entry.exportName, proc);
            hookCount++;
        }

        LONG err = DetourTransactionCommit();
        dbg_log("[PAL_STATE] Installed %d/%d hooks (result=%d)", hookCount, totalCount, err);
        return err == NO_ERROR;
    }

    //===================================================================
    // Passive getter helpers
    // These call the original PAL function via the Detours trampoline,
    // bypassing our hook logging. Protected with SEH since we don't know
    // exact parameter requirements for all functions.
    //===================================================================
    static int SafeCallPalFunc(PalFunc func)
    {
        if (!func)
            return -1;
        __try
        {
            return func(0, 0, 0, 0, 0, 0, 0, 0);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -1;
        }
    }

    int CallPalTaskGetState()       { return SafeCallPalFunc(o_PalTaskGetState); }
    int CallPalFontGetType()        { return SafeCallPalFunc(o_PalFontGetType); }

    int CallPalEffectEnableIs()     { return SafeCallPalFunc(o_PalEffectEnableIs); }
}
