#include "streaming/input/keyboardmapping.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << description << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    const SDL_Scancode codes[] = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL,
        SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI};
    const Uint16 flags[] = {KMOD_LCTRL, KMOD_RCTRL, KMOD_LALT, KMOD_RALT, KMOD_LGUI, KMOD_RGUI};
    int order[] = {0, 1, 2};
    int permutations = 0;
    do {
        const ModifierMapping mapping(order[0], order[1], order[2]);
        ModifierMapping inverse;
        for (int source = 0; source < 3; source++) inverse.targets[order[source]] = source;
        check(mapping.isValid(), "Invalid permutation");
        for (int key = 0; key < 6; key++) {
            // Check modifier press and release, including a release with no held bits.
            for (int state = 0; state < 2; state++) {
                SDL_Keysym sym = {};
                sym.scancode = codes[key];
                sym.mod = state ? flags[key] : 0;
                remapModifiers(sym, mapping);
                int target = order[key / 2] * 2 + key % 2;
                check(sym.scancode == codes[target], "Wrong left/right modifier scancode");
                check(sym.mod == (state ? flags[target] : 0), "Wrong modifier press/release state");
            }
        }
        for (unsigned int bits = 0; bits <= 0xFFFF; bits++) {
            SDL_Keysym sym = {};
            sym.scancode = SDL_SCANCODE_V;
            sym.sym = SDLK_v;
            sym.mod = static_cast<Uint16>(bits);
            remapModifiers(sym, mapping);
            check((sym.mod & ~(KMOD_CTRL | KMOD_ALT | KMOD_GUI)) ==
                  (bits & ~(KMOD_CTRL | KMOD_ALT | KMOD_GUI)), "Shift/lock/unrelated flags changed");
            for (int key = 0; key < 6; key++) {
                int target = order[key / 2] * 2 + key % 2;
                check(bool(sym.mod & flags[target]) == bool(bits & flags[key]), "Incorrect chord modifier");
            }
            check(sym.scancode == SDL_SCANCODE_V && sym.sym == SDLK_v, "Ordinary key changed");
            remapModifiers(sym, inverse);
            check(sym.mod == bits, "Inverse mapping failed");
        }
        for (int key = 0; key < SDL_NUM_SCANCODES; key++) {
            SDL_Keysym sym = {};
            sym.scancode = static_cast<SDL_Scancode>(key);
            remapModifiers(sym, mapping);
            if (std::find(std::begin(codes), std::end(codes), key) == std::end(codes)) {
                check(sym.scancode == key, "Non-modifier scancode changed");
            }
            remapModifiers(sym, inverse);
            check(sym.scancode == key, "Scancode inverse failed");
        }
        // Dropdown changes exchange assignments instead of creating aliases.
        for (int source = 0; source < 3; source++) {
            for (int target = 0; target < 3; target++) {
                ModifierMapping changed = mapping;
                changed.setTarget(source, target);
                check(changed.isValid() && changed.targets[source] == target, "Target exchange failed");
                check(!changed.setTarget(source, -1) && !changed.setTarget(source, 3), "Invalid target accepted");
            }
        }
        permutations++;
    } while (std::next_permutation(order, order + 3));
    check(permutations == 6, "Missing permutation coverage");
    check(ModifierMapping(-1, 1, 2).isIdentity(), "Negative saved target not reset");
    check(ModifierMapping(0, 3, 2).isIdentity(), "Out-of-range saved target not reset");
    check(ModifierMapping(0, 0, 2).isIdentity(), "Duplicate saved target not reset");

    const ModifierMapping preferred(0, 2, 1);
    const struct { SDL_Scancode key; Uint16 input; Uint16 expected; } chords[] = {
        {SDL_SCANCODE_C, KMOD_LCTRL, KMOD_LCTRL},
        {SDL_SCANCODE_V, KMOD_LCTRL, KMOD_LCTRL},
        {SDL_SCANCODE_V, KMOD_LALT, KMOD_LGUI},
        {SDL_SCANCODE_S, KMOD_LALT | KMOD_LSHIFT, KMOD_LGUI | KMOD_LSHIFT},
        {SDL_SCANCODE_TAB, KMOD_LGUI, KMOD_LALT},
        {SDL_SCANCODE_F4, KMOD_RGUI, KMOD_RALT},
    };
    for (const auto& chord : chords) {
        SDL_Keysym sym = {};
        sym.scancode = chord.key;
        sym.mod = chord.input;
        remapModifiers(sym, preferred);
        check(sym.scancode == chord.key && sym.mod == chord.expected, "Preferred shortcut failed");
    }
    std::cout << "All six modifier layouts, 393216 flag combinations, key releases and shortcut tests passed" << std::endl;
}
