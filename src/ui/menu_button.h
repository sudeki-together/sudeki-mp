#ifndef SUDEKIMP_MENU_BUTTON_H
#define SUDEKIMP_MENU_BUTTON_H
#include <stdint.h>
unsigned int SudekiMpButtonCoverage(int x, int y, int left, int top,
    int right, int bottom, int radius);
/* ARGB8 surface; height of each button is 30, all bounds in pixels. */
void SudekiMpDrawMenuButton(uint32_t *pixels, int pitch, int width, int height,
    int left, int top, int right, int bottom, int highlighted);
#endif
