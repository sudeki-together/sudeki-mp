#ifndef SUDEKIMP_STORY_AWAY_BANNER_H
#define SUDEKIMP_STORY_AWAY_BANNER_H
#include "network/lan_story_step_away.h"
typedef struct SudekiMpStoryAwayLayout { float x,y,width,size; } SudekiMpStoryAwayLayout;
/* pixels is the atlas-measured width at 20 * min(W/960,H/720). */
int SudekiMpStoryAwayBannerLayout(unsigned width,unsigned height,float pixels,SudekiMpStoryAwayLayout *);
int SudekiMpStoryAwayBannerRender(void *image,void *device,SudekiMpStoryAwayPhase,unsigned character);
#endif
