#ifndef SUDEKIMP_STORY_MARKUP_H
#define SUDEKIMP_STORY_MARKUP_H
#include <windows.h>
#include <stdint.h>
/* The game's speech text carries its own rich-text tags (native vocabulary
 * img, br, alpha, colour and their closing forms; CONFIRMED_STATIC .rdata
 * strings near "< EMPTY >"; e.g. <colour='00FFFF'>Shadani-Mo</colour>).
 * Decode one UTF-16 line into printable ASCII with a colour per character
 * (0xRRGGBB, 0 = default): colour spans nest (depth 8), <br> is a line
 * break, alpha/img tags are dropped, anything that is not a known tag stays
 * literal. FALSE for a unit the overlay cannot draw (non-ASCII, controls
 * other than CR/LF) or a line longer than `capacity`-1. Pure. */
BOOL SudekiMpStoryMarkupDecode(const uint16_t *source,unsigned source_units,
    char *text,uint32_t *colour,unsigned capacity);
#endif
