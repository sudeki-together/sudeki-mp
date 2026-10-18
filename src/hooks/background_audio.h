#ifndef SUDEKIMP_BACKGROUND_AUDIO_H
#define SUDEKIMP_BACKGROUND_AUDIO_H
#include <windows.h>

/* [SudekiMP] BackgroundAudio (test aid, default off): keep a game audible
 * when its window is not focused, so two games on one machine can be heard
 * (and muted per stream) at the same time. CONFIRMED_STATIC, supported image:
 * - The window procedure's WM_KILLFOCUS branch (0x28D737) pauses every XACT
 *   category, stdcall 0x2853B0(engine, 0xFF, 1), and pauses any playing Bink
 *   movie, BinkPause(bink, 1); WM_SETFOCUS undoes both. The pause argument
 *   becomes 0 (0x28D762, 0x28D778), so focus loss leaves sound running.
 * - XACT creates its secondary DirectSound buffers at three exact sites with
 *   DSBUFFERDESC flags 0x100A0 and without DSBCAPS_GLOBALFOCUS (0x282303 /
 *   0x2824CB / 0x282981); 0x8000 is set in each immediate.
 * The window policy's patches in the same branch are separate bytes. Install
 * before XACT starts; restored in reverse. */
BOOL SudekiMpInstallBackgroundAudio(HMODULE game_module);
BOOL SudekiMpUninstallBackgroundAudio(void);
BOOL SudekiMpBackgroundAudioInstalled(void);
#endif
