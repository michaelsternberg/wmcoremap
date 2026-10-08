# Changelog

All notable changes to wmcoremap are listed here.

## Unreleased

## [0.1] (2026-10-04)

- First version: per-core CPU heatmap in a 64x64 Window Maker dockapp,
  in the wmtop LED style. The grid scales from 1 to 1024 CPUs.
- Colour ramp in wmsysmon's LED colours: dark grey-green (#283C38) at
  idle, green (#00EB00) under moderate load, yellow, orange and red near
  100%.
- -size small|medium|large: 4x4 (wmsysmon's LED shape), 6 or 9 pixel LEDs,
  shrinking automatically when the CPUs do not fit. Offline CPUs are
  drawn as hollow rings.
- Total CPU % and CPU temperature (hwmon / thermal zone auto-detection,
  -sensor to override) in the bottom row; -c / -f for Celsius / Fahrenheit,
  right click switches at runtime.
- Screens, cycled with left click or the wheel: CPU heatmap, total load
  history, temperature history, and GPU busy % / VRAM on amdgpu.
- Options: -interval, -screen, -fg, -bg, -square, -fake N for testing
  layouts.

[0.1]: https://github.com/michaelsternberg/wmcoremap/releases/tag/v0.1
