# macOS modifier mapping tests

From the repository root on Linux or macOS, with SDL2 development headers:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iapp $(pkg-config --cflags sdl2) \
  tests/input/keyboardmapping.cpp -o /tmp/artemis-keyboardmapping-test
/tmp/artemis-keyboardmapping-test
```

The test covers all six layouts, all 65,536 modifier flag patterns for each
layout, every SDL scancode, left/right modifier presses and releases, and the
Windows shortcut preset. Shift, lock flags and ordinary keys stay unchanged.
It also verifies exchanging occupied targets and rejecting malformed mappings.

The preferences test uses Qt Core, QML and Test development packages:

```sh
mkdir -p build/modifierpreferences
cd build/modifierpreferences
qmake6 ../../tests/input/modifierpreferences.pro
make
./modifierpreferences-test
```

It exercises the real preference implementation with temporary INI settings:
defaults, presets, QML-facing property writes and change notifications, saved
layouts, migration from `swapcmdctrl`, and invalid saved values. Only the
Wayland environment query is stubbed; no desktop or host is required.

## Live macOS streaming checklist

These checks require an actual Mac client and Windows/Linux host:

1. Restore physical Control/Option/Command in macOS Modifier Keys. Disable any
   third-party remaps for Artemis during this test. macOS remapping happens
   before Artemis receives input and cannot be undone by this setting.
2. Check the three selectors default to Ctrl, Alt, Windows/Super. Try all three
   preset buttons and individual selectors; selecting an occupied target must
   exchange the assignments and update both selectors. Restart Artemis and
   confirm the layout persists. Changes take effect on the next stream.
3. Choose Windows shortcut layout and Capture system keyboard shortcuts: always.
   Start a stream. Check Control+C/V, Option+V, Option+Shift+S, Command+Tab and
   Command+F4 against the Windows host. Releasing Command must finish Alt+Tab.
   On Linux, use the desktop's bindings or a key-event viewer.
4. Test both left and right modifiers and chords with several modifiers held.
   Artemis's existing Control+Option+Shift shortcuts must still work.
5. Hold a mapped modifier while switching focus, toggling fullscreen, or
   ungrabbing. Return to the stream and check no host modifier remains held.
6. With capture off, or fullscreen-only capture in windowed mode, the original
   mapping must be used. Other Mac apps retain their existing mappings.
7. Restore Default and reconnect to verify the original behavior. On Windows
   and Linux clients, the settings must be hidden and input must be unchanged
   even if non-default modifier preferences exist.
