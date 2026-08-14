# Windowed play and OBS recording on Linux

Sudeki natively supports a windowed display mode through the `FullScreen`
entry in the user's UTF-16 `PlayerOptions.xml`. SudekiMP does not patch the
renderer or replace the executable to obtain a window.

The research launcher now selects windowed mode before every live run:

```bash
tools/run-wine.sh --windowed
```

The helper preserves the first untouched `PlayerOptions.xml` as
`PlayerOptions.xml.sudekimp-backup`, changes only the `FullScreen` Boolean, and
verifies the UTF-16 result. Fullscreen remains available explicitly:

```bash
tools/run-wine.sh --fullscreen
tools/configure-windowed.sh --check
```

Windowed mode keeps Sudeki represented as a normal desktop window when focus
moves to another application. Sudeki may still pause its own simulation and
audio when unfocused; this option prevents exclusive-fullscreen minimization
and does not change the game's focus policy.

## OBS Game Capture on this Linux host

This host uses a Wayland session and the Flatpak build of OBS Studio. In
addition to the normal PipeWire/XComposite sources, it has the OBSVkCapture
extension and system-wide 32-bit and 64-bit hook libraries. Existing Lutris
games enable that integration with:

```yaml
system:
  prefix_command: obs-gamecapture
```

SudekiMP applies the same prefix through:

```bash
tools/run-wine.sh --windowed --obs-gamecapture
```

The wrapper preloads the appropriate-architecture `libobs_glcapture.so` and
sets `OBS_VKCAPTURE=1` before Wine starts. Both architectures matter because
the supported Sudeki executable is PE32. Wine logs show that this host runs
Sudeki through the OpenGL rendering path handled by that capture hook.

Native Linux OBS does not include its Windows-only Game Capture implementation,
but OBSVkCapture supplies a Linux source with the same visible **Game Capture**
name. To use it:

1. Start OBS before Sudeki.
2. Add the OBSVkCapture **Game Capture** source to the desired scene.
3. Launch Sudeki through `continue-research.sh` or the command above.
4. Verify live motion and audio meters, then make a short recording test.

**Window Capture (PipeWire)** remains an available compatibility fallback, but
it is not the preferred path for this setup. The capture preload must be
present when the Wine process starts; enabling the OBS source after an ordinary
Sudeki launch cannot retrofit the hook into that already-running process.
