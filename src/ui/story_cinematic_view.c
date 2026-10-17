#include "ui/story_cinematic_view.h"
#include "ui/title_menu_view.h"
#include "ui/story_markup.h"
#include <string.h>

/* Measured word wrap of one decoded line. Each wrapped row is emitted as
 * consecutive texts of one colour (the game's <colour> spans), each run
 * placed after the measured width of the row before it. row[] keeps the
 * row index of every text for the vertical layout. */
static BOOL wrap(SudekiMpTitleExtras *extras,unsigned *rows,unsigned row_of[],const char *text,
    const uint32_t *colour,uint32_t plain,float size,float width) {
    unsigned cursor=0;
    while(text[cursor]) {
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
        for(unsigned run=0;run<n || (!n && !run);) {
            if(extras->text_count>=SUDEKIMP_PANEL_TEXTS) return FALSE;
            uint32_t c=colour?colour[cursor+run]:0u; unsigned stop=run;
            while(stop<n && (colour?colour[cursor+stop]:0u)==c) ++stop;
            float left=0;
            if(run) {
                char prefix[128]; memcpy(prefix,line,run); prefix[run]=0;
                left=SudekiMpTitleViewTextWidth(prefix,sizeof(prefix),size);
                if(left<0 || left>width) return FALSE;
            }
            SudekiMpPanelText *t=&extras->texts[extras->text_count];
            row_of[extras->text_count++]=*rows;
            t->x=60.f; t->indent=left; t->width=width; t->size=size; t->colour=c?c:plain;
            memcpy(t->text,line+run,stop-run); t->text[stop-run]=0;
            if(!n) break;
            run=stop;
        }
        ++*rows;
        cursor+=advance;
        if(text[cursor]=='\r') ++cursor;
        if(text[cursor]=='\n') ++cursor;
    }
    return TRUE;
}
/* The same line without colours, if the coloured runs do not fit. */
static BOOL layout(SudekiMpTitleExtras *extras,unsigned *rows,unsigned row_of[],const char *name,
    const char *text,const uint32_t *colour) {
    extras->text_count=0; *rows=0;
    /* Native speaker colour 0xFFFFA0 (CSpeech builds <colour='ffffa0'>name</colour> : text). */
    if(name[0] && !wrap(extras,rows,row_of,name,NULL,0xffffa0u,22,840)) return FALSE;
    return wrap(extras,rows,row_of,text,colour,0u,22,840);
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
        static uint32_t colours[SUDEKIMP_STORY_SUBTITLE_UNITS+1u];
        unsigned rows=0,row_of[SUDEKIMP_PANEL_TEXTS];
        if(!SudekiMpStoryMarkupDecode(f->subtitle,SUDEKIMP_STORY_SUBTITLE_UNITS+1u,text,colours,sizeof(text)) ||
            !SudekiMpStoryMarkupDecode(f->speaker,SUDEKIMP_STORY_SPEAKER_UNITS+1u,name,NULL,sizeof(name))) goto unsupported;
        if(!layout(&extras,&rows,row_of,name,text,colours) && !layout(&extras,&rows,row_of,name,text,NULL)) goto unsupported;
        if(rows>20u) goto unsupported;
        /* The whole current line is displayed, with measured word wrapping.
         * Native line timing remains authoritative; no local text clock. */
        float first=650.f-(float)(rows-1u)*26.f;
        for(unsigned i=0;i<extras.text_count;++i) extras.texts[i].y=first+row_of[i]*26.f;
    }
    if(!extras.text_count && extras.overlay_letterbox==0 && extras.overlay_letterbox_bottom==0) return TRUE;
    if(!SudekiMpTitleViewPrepare(device) || !SudekiMpTitleViewDraw(device,1,0,0,NULL,
        SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&extras)) return FALSE;
    SetLastError(ERROR_SUCCESS); return TRUE;
unsupported:
    SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
}
