# kb_lay

Tiny Windows tray app: **select mistyped text → double-tap Ctrl → remap EN ↔ RU**.

Выделите текст, нажмите **Ctrl, Ctrl** — как будто набрали на другой раскладке (`ghbdtn` → `привет`, `руддщ` → `hello`).

One hidden window in `GetMessage`: ~0% CPU idle, a few MB RAM, ~150 KB exe, no .NET.

This is a **user-session** process (tray icon), not a `services.msc` service. Session-0 services cannot see desktop hotkeys, the clipboard, or the focused app. `--install` puts it in `HKCU\...\Run` so it starts at logon.

## Use

1. Select the text that was typed in the wrong layout.
2. Double-tap **Ctrl** (either side, two taps within 500 ms).
3. The selection is replaced. The previous clipboard is restored.
4. By default the focused window also switches to that language so you can keep typing.

`Ctrl+C` / `Ctrl+V` are not treated as taps.

| Typed (wrong layout) | Result |
|---|---|
| `ghbdtn` | `привет` |
| `руддщ` | `hello` |
| `Ghbdtn` | `Привет` |

Layouts: **English (US)** QWERTY ↔ **Russian** JCUKEN. Nothing else.

## Install

```text
kb_lay                         run in the tray
kb_lay --install               copy to %LOCALAPPDATA%\kb_lay and start with Windows
kb_lay --uninstall             remove startup and stop
kb_lay --quit                  stop the running instance
kb_lay --help
```

Right-click the tray icon: **Switch layout after convert**, install, uninstall, exit.

A GitHub Actions artifact (`kb_lay.exe`) is built on every push to `main`.

## Options

```text
kb_lay --hotkey=ctrl+ctrl      default: double-tap Ctrl
kb_lay --switch-layout         after paste, switch input layout (default)
kb_lay --no-switch-layout      convert text only
```

`--install` stores the current hotkey and layout-switch flag in the Run key. Changing the tray checkbox updates that key if kb_lay is already installed.

### Hotkeys

| `--hotkey=` | Behavior |
|---|---|
| `ctrl+ctrl` | double-tap Ctrl, either side (default) |
| `lctrl+lctrl` | left Ctrl only |
| `rctrl+rctrl` | right Ctrl only |
| `alt+alt` | double-tap Alt |
| `shift+shift` | double-tap Shift |
| `caps` | tap Caps Lock (does not toggle caps) |
| `pause`, `scrolllock`, `f12` | single key via `RegisterHotKey` |
| `ctrl+shift+q`, `ctrl+alt+r`, `win+shift+x` | ordinary combos |

## How it works

1. Snapshot the clipboard (all HGLOBAL formats).
2. Copy the selection (`WM_COPY`, then spaced Ctrl+C if needed — Chrome ignores a 4-key `SendInput` burst).
3. Count Latin vs Cyrillic; remap through the US ↔ Russian key map.
4. Paste (`WM_PASTE` or spaced Ctrl+V).
5. If layout-switch is on, activate `00000409` (EN) or `00000419` (RU) on the focused window.
6. Restore the clipboard snapshot.

Native edit controls use `WM_COPY` / `WM_PASTE`. Browsers and most other apps use injected Ctrl+C / Ctrl+V.

Does **not** work in:

- elevated windows, unless kb_lay is elevated (UIPI)
- some games that swallow input
- consoles, where Ctrl+C is interrupt rather than copy

## Build

`build.bat` (MinGW `gcc` + `windres`, or MSVC `cl`).

**MinGW**

```bat
windres -I res res\kb_lay.rc -O coff -o kb_lay_res.o
gcc -finput-charset=UTF-8 -O2 -s -mwindows -static -o kb_lay.exe src\kb_lay.c kb_lay_res.o -luser32 -lshell32 -ladvapi32
gcc -finput-charset=UTF-8 -DKB_LAY_TEST -O2 -o kb_lay_test.exe src\kb_lay.c
kb_lay_test.exe
```

**MSVC**

```bat
cl /nologo /utf-8 /O1 /W3 src\kb_lay.c res\kb_lay.rc /Fe:kb_lay.exe user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS
cl /nologo /utf-8 /O1 /W3 /DKB_LAY_TEST src\kb_lay.c /Fe:kb_lay_test.exe
```

**CMake**

```bat
cmake -S . -B build
cmake --build build --config Release
```

`kb_lay.exe --selftest` creates a hidden EDIT, converts `ghbdtn` → `привет`, and checks the clipboard was restored.

Tray icon: `tools/make_icon.ps1` writes `res/kb_lay.ico` (ImageMagick + GDI+).

## License

MIT
