#ifndef SUDEKIMP_LOG_H
#define SUDEKIMP_LOG_H

#include <windows.h>

BOOL SudekiMpLogOpenBesideGame(const wchar_t *game_path);
void SudekiMpLogWrite(const char *message);
void SudekiMpLogFormat(const char *format, ...);
void SudekiMpLogClose(void);
/* Research diagnostics switch: bounded, log-only probes (clip/pose/input
 * transitions, phase profiles, hitch detector). Default on; the loader sets it
 * from [StoryAreas] ResearchDiagnostics. */
void SudekiMpLogSetResearch(BOOL enabled);
BOOL SudekiMpLogResearchEnabled(void);

#endif
