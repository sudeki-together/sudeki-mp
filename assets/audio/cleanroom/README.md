# Cleanroom audio

This folder documents original SudekiMP-only sound cues used by the cleanroom.
It must not contain audio extracted from Sudeki or `SOLData.baf`.

`despawn-cue.md` describes the current procedural despawn sound. The DLL builds
the small PCM wave in memory, so the repository does not need to carry an
opaque binary asset and the cue can be changed without touching the game data.
