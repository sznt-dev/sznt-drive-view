# Manual installation

The installer does everything below for you, with a backup, and can undo it. Use this guide only if you prefer to do it by hand, or if you built the plugins yourself.

## 1. Copy the plugins

Close the game, then copy the files into the game's plugin folder (create `plugins` if it doesn't exist):

```
...\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\
    sznt-drive.dll   sznt-drive.ini
    sznt-view.dll    sznt-view.ini
```

For American Truck Simulator it's the same path inside `American Truck Simulator`. The `.ini` files are in English; translated ones (`.pt`, `.es`, `.de`) are in the `ini` folder: rename the one you want to `sznt-drive.ini` / `sznt-view.ini`.

## 2. Connect them in your profile's controls

Back up your profile's `controls.sii` first. It's in `Documents\Euro Truck Simulator 2\steam_profiles\<profile>\` (or `profiles\<profile>\` for local profiles). Open it in Notepad.

Pick two free controller slots, lines that look like ``` "device joyN ``" ```. Below we use `joy6` for Drive and `joy5` for View; replace them if those are in use.

### SZNT Drive

| Line | Change |
|---|---|
| ``` "device joy6 ``" ``` | `` "device joy6 `sdk.sznt_drive`" `` |
| `"constant c_relatsteer 1.000000"` | `"constant c_relatsteer 0.000000"` |
| `mix steering` | append ` - joy6.sz_steer?0` at the end of the expression |
| `mix aforward` | append ` + joy6.sz_throttle?0` |
| `mix abackward` | replace `semantical.abackward?0` with `semantical.abackward?0 + joy6.sz_brake?0` |
| `mix dsteerleft` / `dsteerright` / `dforward` | delete ` \| keyboard.a?0` / ` \| keyboard.d?0` / ` \| keyboard.w?0` |
| `mix dbackward` and `mix abackward` | replace `(keyboard.darrow?0 \| keyboard.s?0)` with `keyboard.darrow?0` |

### SZNT View

| Line | Change |
|---|---|
| ``` "device joy5 ``" ``` | `` "device joy5 `sdk.sznt_view`" `` |
| `mix headtron` and `mix headtrwmon` | replace `eyeposon)` with `eyeposon \| joy5.sz_on?0)` |
| `headtryaw`, `headtrwmyaw` | append ` + joy5.sz_yaw?0` |
| `headtrpitch`, `headtrwmpitc` | append ` + joy5.sz_pitch?0` |
| `headtrroll`, `headtrwmroll` | append ` + joy5.sz_roll?0` |
| `headtrx`, `headtrwmx` | append ` + joy5.sz_x?0` |
| `headtry`, `headtrwmy` | append ` + joy5.sz_y?0` |
| `headtrz`, `headtrwmz` | append ` + joy5.sz_z?0` |
| `mix camlr` | replace `* c_msens` with `* c_msens * (1 - joy5.sz_mouse?0)` |
| `mix camud` | replace `sel(c_minvert, -c_msens, c_msens)` with `sel(c_minvert, -c_msens, c_msens) * (1 - joy5.sz_mouse?0)` |
| `mix camzoom` | delete `mouse.button_middle?0 \| ` (the middle button becomes the head zoom) |

## 3. Start the game

It shows a notice about **advanced SDK features**: that's normal, click OK.

To undo everything: delete the `sznt-*.dll` files and restore your `controls.sii` backup.
