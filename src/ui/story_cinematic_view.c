#include "ui/story_cinematic_view.h"
#include "ui/title_menu_view.h"
#include <string.h>

static BOOL ascii(const uint16_t *source,unsigned cap,char *target) {
    for(unsigned i=0;i<=cap;++i) {
        uint16_t c=source[i];
        if(c>126u || (c<32u && c && c!=10u && c!=13u)) return FALSE;
        target[i]=(char)c; if(!c) return TRUE;
    }
    return FALSE;
}
static BOOL wrap(SudekiMpTitleExtras *extras,const char *text,float size,float width) {
    unsigned cursor=0;
    while(text[cursor]) {
        if(extras->text_count>=SUDEKIMP_PANEL_TEXTS) return FALSE;
        char line[128]={0}; unsigned n=0,last_space=0;
        while(text[cursor+n] && text[cursor+n]!='\r' && text[cursor+n]!='\n') {
            if(n==126u) break;
            line[n]=text[cursor+n]; line[n+1u]=0;
            float extent=SudekiMpTitleViewTextWidth(line,sizeof(line),size);
            if(extent<0) return FALSE;
            if(extent>width) { line[n]=0; break; }
            if(line[n]==' ') last_space=n+1u;
            ++n;
        }
        if(!n && text[cursor]!='\r' && text[cursor]!='\n') return FALSE;
        BOOL broke=text[cursor+n] && text[cursor+n]!='\r' && text[cursor+n]!='\n';
        unsigned advance=n;
        if(broke && last_space) { n=last_space-1u; advance=last_space; }
        line[n]=0;
        SudekiMpPanelText *row=&extras->texts[extras->text_count++];
        row->x=60; row->width=840; row->size=size; memcpy(row->text,line,n+1u);
        cursor+=advance;
        if(text[cursor]=='\r') ++cursor;
        if(text[cursor]=='\n') ++cursor;
    }
    return TRUE;
}
BOOL SudekiMpStoryCinematicViewRender(void *device,const SudekiMpLanStoryPresentation *f) {
    SudekiMpTitleExtras extras={0}; HWND window=NULL;
    char text[SUDEKIMP_STORY_SUBTITLE_UNITS+1u]={0},name[SUDEKIMP_STORY_SPEAKER_UNITS+1u]={0};
    if(!device || !SudekiMpLanStoryPresentationValid(f)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    extras.overlay=TRUE;
    extras.overlay_letterbox=(f->flags&SUDEKIMP_STORY_LETTERBOX_PRESENT)?f->letterbox:0;
    extras.overlay_letterbox_bottom=(f->flags&SUDEKIMP_STORY_LETTERBOX_PRESENT)?f->letterbox_bottom:0;
    if(f->flags&SUDEKIMP_STORY_LINE_ACTIVE) {
        if(!ascii(f->subtitle,SUDEKIMP_STORY_SUBTITLE_UNITS,text) ||
            !ascii(f->speaker,SUDEKIMP_STORY_SPEAKER_UNITS,name)) goto unsupported;
        if(name[0] && !wrap(&extras,name,22,840)) goto unsupported;
        if(!wrap(&extras,text,22,840)) goto unsupported;
        if(extras.text_count>20u) goto unsupported;
        /* The whole current line is displayed, with measured word wrapping.
         * Native line timing remains authoritative; no local text clock. */
        float first=650.f-(float)(extras.text_count-1u)*26.f;
        for(unsigned i=0;i<extras.text_count;++i) extras.texts[i].y=first+i*26.f;
    }
    if(!extras.text_count && extras.overlay_letterbox==0 && extras.overlay_letterbox_bottom==0) return TRUE;
    if(!SudekiMpTitleViewPrepare(device) || !SudekiMpTitleViewDraw(device,1,0,0,NULL,
        SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&extras)) return FALSE;
    SetLastError(ERROR_SUCCESS); return TRUE;
unsupported:
    SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
}
