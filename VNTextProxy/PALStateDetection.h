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

// The current rendering text, captured from PalFontBegin's a1 argument.
// Set by PalFontBegin hook, cleared by PalFontEnd hook.
// For body text: points to taskData + textOffset (the full dialogue line).
// For name rendering: nullptr (a1 is a struct, not text).
// For continuation text (second half of split lines): points past leading null bytes.
extern const unsigned char* fontBeginText;

namespace PALStateDetection {
    bool Install(HMODULE hPalDll);

    // Passive getters that can be called proactively to query PAL engine state.
    // These call the original PAL.dll functions via trampolines (bypassing hook logging).
    // Returns -1 if the function is not available in the current PAL.dll version.
    int CallPalTaskGetState();
    int CallPalFontGetType();
    int CallPalEffectEnableIs();
}
