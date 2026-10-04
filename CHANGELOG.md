# Changelog

## 1.0.0-beta.2

### SZNT View
- **Calmer head in turns and on banked roads.** The steady-gaze reflex was too strong: on a tilted road the cab leaned and the view stayed level, like a camera on a gimbal. It now compensates only a small part of the tilt, more slowly, and follows sustained road camber, the way a driver feels it from an air-suspended seat.
  - steady gaze 50-60% → 15%, reaction 0.08 s → 0.25 s, limit 4° → 1.5°
  - cab inertia 0.7 → 0.3, head tilt in turns 3.0° → 2.0°
  - in a typical turn the head now moves about half as much relative to the cab.

### Installer
- New look, matching mods.sznt.dev: same fonts, colors and logo, light and dark theme following Windows.
- Short in-game clips of each mod instead of drawings.
- The installer with both mods is now `SZNT-Drive-View-Setup.exe`.
- Files inside the installer are compressed (smaller download), and checking whether the game is open no longer lists running processes, it just looks for the game window.
- Shorter, plainer wording in all four languages.

## 1.0.0-beta.1

First public release.

### SZNT Drive
- Analog steering, throttle and brake on the keyboard (W A S D): weighted steering wheel with inertia and self-centering, speed-dependent lock.
- Progressive pedals with "foot memory" between gear changes, floor it (double tap W) and emergency braking (double tap S).
- **Dynamic weight:** tractor + coupled trailers + job cargo mass. Heavy load is the reference; empty rigs feel lighter, heavy haul heavier.
- **Grip:** on ice, snow, mud, grass, gravel and wet roads the wheel goes light and self-centers less.

### SZNT View
- Virtual head tracker: mouse look with weight and inertia, head zoom on the middle button.
- **Steady gaze:** the neck compensates part of the cab's pitch and roll.
- **Body inertia tied to the cab:** cab tilt and sway are felt in the seat, on top of the chassis acceleration.
- Subtle head movement on surface changes (front axle, then rear; one side only on the shoulder). No camera shake.
- Breathing, natural sway, looking into turns.
- Interior camera detected from the camera keys; works with or without TM Real Walk.

### Installer
- Three editions: SZNT Drive, SZNT View, and both together.
- Language selection (English, Português, Español, Deutsch).
- Finds the games in every Steam library, sets up every profile's controls (with backup) using free controller slots, keeps your settings on update and migrates the previous TM Handling / TM Head setup.
- Verifies the SHA-256 of every embedded file. Uninstall restores the controls exactly.
