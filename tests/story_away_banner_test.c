#include "../src/ui/story_away_banner.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    const unsigned sizes[][2]={{640,480},{960,720},{1280,720},{1920,1080},{2560,1080},{1080,1920},{3840,2160}};
    SudekiMpStoryAwayLayout b={NAN,NAN,NAN,NAN};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);++i) {
        float scale=fminf(sizes[i][0]/960.f,sizes[i][1]/720.f),width=430.f*scale;
        assert(SudekiMpStoryAwayBannerLayout(sizes[i][0],sizes[i][1],width,&b));
        assert(fabsf(b.x+b.width*.5f-480.f)<.001f && b.x>0 && b.x+b.width<960);
        assert(b.y>=b.size && b.y+b.size<100); /* top bar; confirm starts 286, subtitles at bottom */
    }
    assert(SudekiMpStoryAwayBannerLayout(64,64,10,&b));
    assert(!SudekiMpStoryAwayBannerLayout(63,720,10,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,63,10,&b));
    assert(!SudekiMpStoryAwayBannerLayout(0,720,40,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,0,40,&b));
    assert(!SudekiMpStoryAwayBannerLayout(16385,720,40,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,16385,40,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,720,NAN,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,720,0,&b));
    assert(!SudekiMpStoryAwayBannerLayout(960,720,960,&b));
    puts("story away banner layout passed"); return 0;
}
