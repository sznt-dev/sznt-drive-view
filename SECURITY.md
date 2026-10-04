# Security & transparency

SZNT Drive & View are input plugins for a game. We think you should know exactly what they do on your PC, so here it is, all of it.

## What runs, and when

| Component | When it runs | What it does |
|---|---|---|
| `sznt-drive.dll` | Loaded by the game at start-up | Reads the state of 4 keys (W A S D by default) with `GetAsyncKeyState`, **only while the game window is in front**, and sends three analog values (steering, throttle, brake) to the game through the official SCS Input SDK. Reads game telemetry (speed, trailers, cargo mass, wheel surfaces, wipers). |
| `sznt-view.dll` | Loaded by the game at start-up | Installs a low-level mouse hook (`WH_MOUSE_LL`) that **only reads mouse movement**, never blocks or alters it, and only uses it while the game is in front and the interior camera is active. Reads the middle button and the camera keys (1-9). Sends head position/rotation to the game's head-tracking channels. Reads game telemetry (speed, cab motion, wheels). |
| `SZNT-*-Setup.exe` | Only when you run it | Installs, updates or removes the plugins (details below). |

Both plugins also read the game's own `game.log.txt`, only to notice when the third-party *TM Real Walk* plugin puts you on foot, so they can step aside.

## Network

- **The plugins never access the network.** There is no telemetry, analytics, crash reporting or auto-update inside the game.
- **The installer** sends a single HTTPS `GET` to `api.github.com/repos/sznt-dev/sznt-drive-view/releases/latest` to read the latest version number. Nothing about you or your computer is sent. If you are offline, nothing happens.

## Files and settings the installer touches

| Where | What |
|---|---|
| `<game>\bin\win_x64\plugins\` | `sznt-drive.dll/.ini`, `sznt-view.dll/.ini`. An older version (`tm-*.dll`) is renamed to `.old`. |
| `Documents\<game>\profiles\*\controls.sii` and `steam_profiles\*\controls.sii` | Adds the bindings for the virtual devices and removes W A S D from the truck's digital controls. A backup `controls.sii.sznt-backup` is created the first time. |
| `%LOCALAPPDATA%\Programs\SZNT\` | A copy of the installer, used by *Settings → Apps* to uninstall. |
| `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SZNT.Drive` / `SZNT.View` | The uninstall entries. |
| `HKCU\Software\SZNT` | The language you picked. |
| Start menu | A shortcut to the installer. |

No services, no scheduled tasks, no drivers, nothing that starts with Windows. Administrator rights are only requested if Windows blocks writing to the game folder.

Uninstalling deletes the DLLs, keeps your settings as `.ini.bak`, and restores each `controls.sii` exactly as it was (this is covered by the automated tests).

## Verifying a download

Every release lists the SHA-256 of each file. In PowerShell:

```powershell
Get-FileHash .\SZNT-Setup-v1.0.0-beta.2.exe -Algorithm SHA256
```

The installer also checks the SHA-256 of every file it carries before writing it; a damaged installer refuses to install.

You can always [build everything from source](README.md#-build-it-yourself). Every commit is built and tested on GitHub Actions.

## Antivirus warnings

New, unsigned programs that read keyboard or mouse input are sometimes flagged by heuristic engines. That is a guess based on behavior, not a detection of known malware. If you see one, compare the hash with the release, or build from source.

## Reporting a problem

If you find a security issue, please report it privately through GitHub (**Security → Report a vulnerability**) or via [mods.sznt.dev](https://mods.sznt.dev). Please don't open a public issue for security problems.
