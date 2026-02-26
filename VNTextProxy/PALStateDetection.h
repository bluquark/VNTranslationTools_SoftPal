#pragma once

#include <windows.h>

// Set by PalSpriteCreateText hook, cleared by PalFontEnd hook.
extern bool isChoice;

// Set by PalSpriteCreateTextEx hook, cleared by PalFontEnd hook.
extern bool isSaveScreen;

// The current sprite text string (choice button or save screen text).
// Set by PalSpriteCreateText/Ex hooks, cleared by PalFontEnd.
// Points into PAL engine memory; valid only while isChoice/isSaveScreen is true.
extern const unsigned char* spriteText;

namespace PALStateDetection {
    bool Install(HMODULE hPalDll);

    // Passive getters that can be called proactively to query PAL engine state.
    // These call the original PAL.dll functions via trampolines (bypassing hook logging).
    // Returns -1 if the function is not available in the current PAL.dll version.
    int CallPalTaskGetState();
    int CallPalFontGetType();
    int CallPalEffectEnableIs();
}
