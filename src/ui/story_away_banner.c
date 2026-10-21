#include "ui/story_away_banner.h"
#include "ui/title_menu_view.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
int SudekiMpStoryAwayBannerLayout(unsigned w,unsigned h,float pixels,SudekiMpStoryAwayLayout *out) {
    if(!out || w<64u || h<64u || w>16384u || h>16384u || !isfinite(pixels) || pixels<=0 || pixels>w-8.f) return 0;
    *out=(SudekiMpStoryAwayLayout){(w-pixels)*480.f/w,24.f,pixels*960.f/w,20.f};
    return 1;
}
static int readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && a+n>=a && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
/* Existing MenuNative owner supplies its already-validated image on the game
 * thread. These are the same screen globals used by name tags. No D3D
 * surface reference is acquired. Top text leaves the subtitle/roster bottom
 * and the native confirm box in the middle free. Draw after letterboxing. */
int SudekiMpStoryAwayBannerRenderMessage(void *image,void *device,SudekiMpStoryAwayPhase phase,unsigned c,const char *message) {
    if(phase==SUDEKIMP_STORY_AWAY_NONE) return 1;
    uint8_t *base=image;
    if(!device || !base || !readable(base+0x34ec9c,8u)) return 0;
    unsigned w=*(unsigned *)(base+0x34ec9c),h=*(unsigned *)(base+0x34eca0);
    if(w<64u || h<64u || w>16384u || h>16384u || !SudekiMpTitleViewPrepare(device)) return 0;
    SudekiMpTitleExtras extras={0}; HWND window=NULL;
    extras.overlay=TRUE; extras.text_count=1;
    SudekiMpPanelText *t=&extras.texts[0]; SudekiMpStoryAwayLayout box;
    if(message) snprintf(t->text,sizeof(t->text),"%s",message);
    else SudekiMpStoryAwayBanner(phase,c,t->text,sizeof(t->text));
    float width=SudekiMpTitleViewTextWidth(t->text,sizeof(t->text),20.f*fminf(w/960.f,h/720.f));
    if(!SudekiMpStoryAwayBannerLayout(w,h,width,&box)) return 0;
    t->x=box.x; t->y=box.y; t->width=box.width; t->size=box.size; t->colour=0xffffa0;
    return SudekiMpTitleViewDraw(device,1,0,0,NULL,SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&extras);
}

int SudekiMpStoryAwayBannerRender(void *image,void *device,SudekiMpStoryAwayPhase phase,unsigned c) {
    return SudekiMpStoryAwayBannerRenderMessage(image,device,phase,c,NULL);
}
