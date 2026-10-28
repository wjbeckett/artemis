#pragma once

#include "SDL_compat.h"
#include "settings/modifiermapping.h"

// Map the key and its chord's modifier state from the same original snapshot.
// Preserve left/right sides, Shift, lock flags, and all non-modifier scancodes.
inline void remapModifiers(SDL_Keysym& keysym, const ModifierMapping& mapping)
{
    const SDL_Scancode codes[ModifierMapping::Count][2] = {
        {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL},
        {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT},
        {SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI}
    };
    const Uint16 flags[ModifierMapping::Count][2] = {
        {KMOD_LCTRL, KMOD_RCTRL}, {KMOD_LALT, KMOD_RALT}, {KMOD_LGUI, KMOD_RGUI}
    };
    const SDL_Scancode originalCode = keysym.scancode;
    const Uint16 originalModifiers = keysym.mod;
    keysym.mod &= ~(KMOD_CTRL | KMOD_ALT | KMOD_GUI);
    for (int source = 0; source < ModifierMapping::Count; source++) {
        for (int side = 0; side < 2; side++) {
            if (originalCode == codes[source][side]) {
                keysym.scancode = codes[mapping.targets[source]][side];
            }
            if (originalModifiers & flags[source][side]) {
                keysym.mod |= flags[mapping.targets[source]][side];
            }
        }
    }
}
