#include "ui/story_avatar_stats_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static BOOL row_valid(const SudekiMpStoryAvatarStatsSnapshot *snapshot,
    const SudekiMpStoryAvatarStatsRow *row,uint32_t now) {
    int32_t age=(int32_t)(now-row->received_tick);
    return row->present && row->epoch==snapshot->epoch && row->revision==snapshot->revision &&
        row->spawn_generation && row->sequence && age>=0 && age<=SUDEKIMP_AVATAR_STATS_FRESH_MS &&
        isfinite(row->hp) && isfinite(row->max_hp) && isfinite(row->sp) && isfinite(row->max_sp) &&
        row->hp>=0 && row->max_hp>0 && row->hp<=row->max_hp &&
        row->sp>=0 && row->max_sp>=0 && row->sp<=row->max_sp;
}
static void name_copy(char target[32],const char source[32],unsigned player) {
    unsigned n=0;
    /* The shared atlas covers printable ASCII. Unsupported bytes receive a
     * visible replacement; an empty/malformed name gets its player label. */
    while(n<31u && source[n]) {
        unsigned char c=(unsigned char)source[n]; target[n]=(c>=32u && c<=126u)?(char)c:'?'; ++n;
    }
    target[n]=0;
    if(!n || (n==31u && source[n])) snprintf(target,32,"Player %u",player+1u);
}
BOOL SudekiMpStoryAvatarStatsViewBuild(const SudekiMpStoryAvatarStatsSnapshot *snapshot,
    uint32_t now,SudekiMpTitleExtras *out) {
    if(!out) return FALSE;
    memset(out,0,sizeof(*out)); out->overlay=TRUE;
    if(!snapshot || snapshot->local_player>=SUDEKIMP_AVATAR_STATS_PLAYERS) return FALSE;
    if(!snapshot->epoch || !snapshot->revision) return TRUE;
    for(unsigned player=0;player<SUDEKIMP_AVATAR_STATS_PLAYERS;++player) {
        const SudekiMpStoryAvatarStatsRow *row=&snapshot->rows[player];
        if(!row_valid(snapshot,row,now)) continue;
        SudekiMpOverlayAvatarCard *card=&out->avatar_cards[out->avatar_card_count++];
        card->right=944; card->y=184.f+(out->avatar_card_count-1u)*86.f;
        card->width=244; card->height=78; card->player=player; card->local=player==snapshot->local_player;
        name_copy(card->name,row->name,player); strcpy(card->avatar,"Talos");
        snprintf(card->hp,sizeof(card->hp),"HP %.0f / %.0f",(double)row->hp,(double)row->max_hp);
        snprintf(card->sp,sizeof(card->sp),"SP %.0f / %.0f",(double)row->sp,(double)row->max_sp);
        card->hp_fraction=row->hp/row->max_hp;
        card->sp_fraction=row->max_sp>0?row->sp/row->max_sp:0;
    }
    return TRUE;
}
BOOL SudekiMpStoryAvatarStatsViewRender(void *device,
    const SudekiMpStoryAvatarStatsSnapshot *snapshot,uint32_t now) {
    return SudekiMpStoryAvatarStatsViewRenderPortraits(device,snapshot,now,NULL);
}
BOOL SudekiMpStoryAvatarStatsViewRenderPortraits(void *device,
    const SudekiMpStoryAvatarStatsSnapshot *snapshot,uint32_t now,void *const portraits[4]) {
    SudekiMpTitleExtras extras; HWND window=NULL;
    if(!SudekiMpStoryAvatarStatsViewBuild(snapshot,now,&extras)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if(!extras.avatar_card_count) return TRUE;
    if(!device) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if(portraits) for(unsigned i=0;i<extras.avatar_card_count;++i)
        extras.avatar_cards[i].portrait=portraits[extras.avatar_cards[i].player];
    return SudekiMpTitleViewPrepare(device) && SudekiMpTitleViewDraw(device,1,0,0,NULL,
        SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&extras);
}
