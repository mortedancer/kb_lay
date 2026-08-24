# kb_lay

Tiny Windows background process: **select text → press a hotkey → convert between EN/RU keyboard layouts**.

Выделите текст, нажмите **Pause** — раскладка выделенного текста переключается (ghbdtn → привет, руддщ → hello).

Idle cost is one hidden window sitting in `GetMessage` (~0% CPU, a few MB of RAM, no .NET, no service host).

## Why this is not a Windows Service

A real `services.msc` service runs in Session 0. It cannot:

- receive your desktop hotkeys
- read/write the interactive clipboard
- send Ctrl+C / Ctrl+V to the focused app

So kb_lay is a **user-session resident process** (tray icon). `--install` registers it in `HKCU\...\Run` so it starts at logon — the usual pattern for this kind of tool.

## Usage

```text
kb_lay                         run in background
kb_lay --install               copy to %LOCALAPPDATA%\kb_lay and start with Windows
kb_lay --install --hotkey=f12  same, with a custom hotkey
kb_lay --uninstall             remove startup entry and stop
kb_lay --quit                  stop the running instance
kb_lay --hotkey=ctrl+alt+q     override hotkey for this run
kb_lay --help
```

Default hotkey: **Pause**.

Examples: `pause`, `scrolllock`, `capslock`, `f12`, `ctrl+shift+q`, `ctrl+alt+r`, `win+shift+x`.

Right-click the tray icon for install / uninstall / exit.

## What it does

1. Copies the selection (`Ctrl+C`)
2. Detects Latin vs Cyrillic majority
3. Remaps characters US QWERTY ↔ Windows Russian (JCUKEN)
4. Pastes back (`Ctrl+V`)
5. Switches the foreground window to the matching input layout

The converted text stays on the clipboard.

Works in most ordinary apps (browsers, Office, messengers, editors). Does **not** work in:

- elevated windows, unless kb_lay itself is elevated (UIPI)
- some games that swallow the hotkey
- consoles, where Ctrl+C is interrupt rather than copy

Layouts: **English (US)** and **Russian**. Other layouts are out of scope on purpose.

## Build

**MinGW**

```bat
gcc -finput-charset=UTF-8 -O2 -s -mwindows -static -o kb_lay.exe src\kb_lay.c -luser32 -lshell32 -ladvapi32
gcc -finput-charset=UTF-8 -DKB_LAY_TEST -O2 -o kb_lay_test.exe src\kb_lay.c
kb_lay_test.exe
```

Or `build.bat`.

**MSVC**

```bat
cl /nologo /utf-8 /O1 /W3 src\kb_lay.c /Fe:kb_lay.exe user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS
cl /nologo /utf-8 /O1 /W3 /DKB_LAY_TEST src\kb_lay.c /Fe:kb_lay_test.exe
```

**CMake**

```bat
cmake -S . -B build
cmake --build build --config Release
```

## License

MIT
