#ifndef SUDEKIMP_KEY_BINDING_H
#define SUDEKIMP_KEY_BINDING_H

#include <windows.h>

/* Parse one keyboard or mouse-button name for GetAsyncKeyState. */
BOOL SudekiMpParseInputKey(const wchar_t *text, UINT *virtual_key);

#endif
