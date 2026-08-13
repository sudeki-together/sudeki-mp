#ifndef SUDEKIMP_LOG_H
#define SUDEKIMP_LOG_H

#include <windows.h>

BOOL SudekiMpLogOpenBesideGame(const wchar_t *game_path);
void SudekiMpLogWrite(const char *message);
void SudekiMpLogFormat(const char *format, ...);
void SudekiMpLogClose(void);

#endif
