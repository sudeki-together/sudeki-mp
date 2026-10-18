#ifndef SUDEKIMP_LAN_STORY_SHOT_CUES_H
#define SUDEKIMP_LAN_STORY_SHOT_CUES_H
#include <windows.h>
#include <stdint.h>

/* Client, read-only: the ranged-shot cue names from this game's OWN data
 * (no protocol change; both games run the identical build). No native call,
 * no write. Names are checked against missiles.xsb by the executor.
 * - Party shot: the weapon item record (item+0x120 travel, +0x124 fire,
 *   string-table indices, -1 = none), as native CMissile init 0x186E10
 *   chooses them.
 * - World missile (Kurian Shooter): this client's own native copy of the
 *   shooting monster near the missile's first position, whose missile
 *   manager owns a MissileData with that VFX kind; its weapon default
 *   strings (+0x3BC travel, +0x3DC fire). All agreeing candidates or silence.
 * An empty name means that cue is not played. */
typedef struct SudekiMpStoryShotCueNames { char travel[100], fire[100]; } SudekiMpStoryShotCueNames;
enum { SUDEKIMP_SHOT_CUES_OK=0, SUDEKIMP_SHOT_CUES_SILENT=1, SUDEKIMP_SHOT_CUES_DEFER=2 };
BOOL SudekiMpStoryShotCuesImageMatches(HMODULE image);
unsigned SudekiMpStoryShotCuesForItem(HMODULE image,unsigned character,unsigned item,
    SudekiMpStoryShotCueNames *out);
unsigned SudekiMpStoryShotCuesForWorldMissile(HMODULE image,unsigned vfx_kind,const float first_position[3],
    SudekiMpStoryShotCueNames *out);
#endif
