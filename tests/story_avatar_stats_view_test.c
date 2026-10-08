/* Plain UI snapshot fixture. No actor/native HUD/D3D object is read or
 * modified; the renderer submission is captured to verify visible cards. */
#include "ui/story_avatar_stats_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned preparations,draws;
static SudekiMpTitleExtras submitted;
BOOL SudekiMpTitleViewPrepare(void *device) { assert(device); ++preparations; return TRUE; }
BOOL SudekiMpTitleViewDraw(void *device,unsigned count,unsigned selected,unsigned mask,
    const SudekiMpTitleLabel *labels,SudekiMpTitleButtonState state,double seconds,
    float opacity,HWND *window,const SudekiMpTitleExtras *extras) {
    assert(device && count==1 && !selected && !mask && !labels && window);
    assert(state==SUDEKIMP_TITLE_BUTTON_REST && seconds==0 && opacity==1);
    assert(extras && extras->overlay && !extras->panel && !extras->text_count);
    submitted=*extras; ++draws; return TRUE;
}
static SudekiMpStoryAvatarStatsSnapshot fixture(uint32_t now) {
    SudekiMpStoryAvatarStatsSnapshot s={.epoch=7,.revision=9,.local_player=1};
    for(unsigned p=0;p<4;++p) {
        SudekiMpStoryAvatarStatsRow *r=&s.rows[p];
        r->present=TRUE; r->epoch=s.epoch; r->revision=s.revision;
        r->spawn_generation=10+p; r->sequence=30+p; r->received_tick=now;
        snprintf(r->name,sizeof(r->name),"Player %u",p+1u);
        r->hp=25.f*(p+1u); r->max_hp=100; r->sp=10.f*p; r->max_sp=40;
    }
    return s;
}
static void four_cards_and_render(void) {
    SudekiMpStoryAvatarStatsSnapshot s=fixture(1000); SudekiMpTitleExtras view;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view));
    assert(view.avatar_card_count==4 && view.overlay && !view.panel && !view.text_count);
    for(unsigned p=0;p<4;++p) {
        const SudekiMpOverlayAvatarCard *c=&view.avatar_cards[p];
        assert(!strcmp(c->name,s.rows[p].name) && !strcmp(c->avatar,"Talos"));
        assert(c->player==p && !c->portrait);
        assert(c->local==(p==1) && fabsf(c->hp_fraction-.25f*(p+1))<.00001f);
        assert(fabsf(c->sp_fraction-.25f*p)<.00001f);
        assert(c->right==944 && c->width==244 && c->height==78);
        if(p) assert(c->y>=view.avatar_cards[p-1].y+view.avatar_cards[p-1].height);
    }
    assert(!strcmp(view.avatar_cards[0].hp,"HP 25 / 100"));
    assert(!strcmp(view.avatar_cards[3].sp,"SP 30 / 40"));
    assert(SudekiMpStoryAvatarStatsViewRender((void *)1,&s,1000));
    assert(draws==1 && preparations==1 && submitted.avatar_card_count==4);
    assert(!s.rows[0].name[8] && s.rows[0].hp==25); /* Input snapshot is unchanged. */
}
static void stale_and_absent_omitted(void) {
    SudekiMpStoryAvatarStatsSnapshot s=fixture(1000); SudekiMpTitleExtras view;
    s.rows[0].received_tick=749; --s.rows[1].revision; s.rows[2].present=FALSE;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view));
    assert(view.avatar_card_count==1 && !strcmp(view.avatar_cards[0].name,"Player 4"));
    assert(view.avatar_cards[0].y==184 && !view.avatar_cards[0].local);
    /* Fresh generation starts with a fresh snapshot; no card state leaks
     * from the replaced actor or an earlier render. */
    ++s.rows[3].spawn_generation; s.rows[3].sequence=1; s.rows[3].hp=0;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view));
    assert(view.avatar_card_count==1 && !strcmp(view.avatar_cards[0].hp,"HP 0 / 100"));
    s.rows[3].present=FALSE;
    assert(SudekiMpStoryAvatarStatsViewRender(NULL,&s,1000));
    assert(draws==1 && preparations==1);
    s=fixture(1000); s.rows[0].received_tick=750;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && view.avatar_card_count==4);
    s.rows[0].received_tick=1001;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && view.avatar_card_count==3);
    s=fixture(10); s.rows[0].received_tick=UINT32_MAX-20u;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,10,&view) && view.avatar_card_count==4);
}
static void malformed_and_zero_sp(void) {
    SudekiMpStoryAvatarStatsSnapshot s=fixture(1000); SudekiMpTitleExtras view;
    s.rows[0].hp=NAN; s.rows[1].sp=-1; s.rows[2].max_hp=INFINITY;
    s.rows[3].sp=s.rows[3].max_sp=0;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && view.avatar_card_count==1);
    assert(view.avatar_cards[0].sp_fraction==0 && !strcmp(view.avatar_cards[0].sp,"SP 0 / 0"));
    s=fixture(1000); s.rows[0].spawn_generation=0; s.rows[1].sequence=0;
    s.rows[2].hp=101; s.rows[3].max_sp=0;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && !view.avatar_card_count);
    s=fixture(1000); s.rows[0].name[0]=0; memset(s.rows[1].name,'x',32);
    strcpy(s.rows[2].name,"A\001B");
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view));
    assert(!strcmp(view.avatar_cards[0].name,"Player 1"));
    assert(!strcmp(view.avatar_cards[1].name,"Player 2"));
    assert(!strcmp(view.avatar_cards[2].name,"A?B"));
    s.local_player=4;
    assert(!SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && !view.avatar_card_count);
    assert(!SudekiMpStoryAvatarStatsViewBuild(NULL,1000,&view));
    assert(!SudekiMpStoryAvatarStatsViewBuild(&s,1000,NULL));
    s=fixture(1000); s.epoch=0;
    assert(SudekiMpStoryAvatarStatsViewBuild(&s,1000,&view) && !view.avatar_card_count);
}
static void portrait_borrow_is_per_paint_and_player(void) {
    SudekiMpStoryAvatarStatsSnapshot s=fixture(1000);
    void *portraits[4]={(void *)10,(void *)11,(void *)12,(void *)13};
    s.rows[0].present=FALSE; s.rows[2].received_tick=700;
    assert(SudekiMpStoryAvatarStatsViewRenderPortraits((void *)1,&s,1000,portraits));
    assert(submitted.avatar_card_count==2);
    assert(submitted.avatar_cards[0].player==1 && submitted.avatar_cards[0].portrait==(void *)11);
    assert(submitted.avatar_cards[1].player==3 && submitted.avatar_cards[1].portrait==(void *)13);
    /* A following paint without a lease cannot inherit the earlier texture. */
    assert(SudekiMpStoryAvatarStatsViewRender((void *)1,&s,1000));
    assert(submitted.avatar_card_count==2 && !submitted.avatar_cards[0].portrait && !submitted.avatar_cards[1].portrait);
}
int main(void) {
    four_cards_and_render(); stale_and_absent_omitted(); malformed_and_zero_sp();
    portrait_borrow_is_per_paint_and_player();
    puts("StoryAvatarStatsViewTest PASS: four independent cards, local marker, stale/absent/malformed omission, zero SP, no retained UI state");
    return 0;
}
