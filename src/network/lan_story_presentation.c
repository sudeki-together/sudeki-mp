#include "network/lan_story_presentation.h"
#include <math.h>
#include <string.h>

_Static_assert(SUDEKIMP_STORY_PRESENTATION_CHUNK_MAX_SIZE+28u<=1468u,"presentation datagram bound");
_Static_assert(SUDEKIMP_STORY_PRESENTATION_MAX_SIZE<=3u*1120u,"presentation fragment bound");
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) { return get16(p)|((uint32_t)get16(p+2)<<16); }
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) { put16(p,(uint16_t)v); put16(p+2,(uint16_t)(v>>16)); }
static uint32_t digest(const uint8_t *p,size_t n) {
    uint32_t h=2166136261u;
    for(size_t i=0;i<n;++i) { h^=p[i]; h*=16777619u; }
    return h;
}
static BOOL text_length(const uint16_t *s,unsigned cap,unsigned *length) {
    if(!s || !length) return FALSE;
    for(unsigned i=0;i<=cap;++i) {
        uint16_t c=s[i];
        if(!c) { *length=i; return TRUE; }
        if(i==cap || (c<32u && c!=9u && c!=10u && c!=13u) || c==0xfffeu || c==0xffffu)
            return FALSE;
        if(c>=0xd800u && c<=0xdbffu) {
            if(i+1u>=cap || s[i+1u]<0xdc00u || s[i+1u]>0xdfffu) return FALSE;
            ++i;
        } else if(c>=0xdc00u && c<=0xdfffu) return FALSE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryPresentationCueValid(const char *s) {
    if(!s || !s[0]) return FALSE;
    for(unsigned i=0;i<=SUDEKIMP_STORY_CUE_BYTES;++i) {
        unsigned char c=(unsigned char)s[i];
        if(!c) return TRUE;
        /* Native PlayWav uses a filename stem. No dots, separators, drive
         * designators, wildcards or path syntax may cross this boundary. */
        if(i==SUDEKIMP_STORY_CUE_BYTES || !((c>='a' && c<='z') ||
            (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-')) return FALSE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryPresentationValid(const SudekiMpLanStoryPresentation *f) {
    unsigned text=0,name=0;
    if(!f || !f->epoch || !f->revision || !f->sequence || (f->flags&~31u) ||
        !text_length(f->subtitle,SUDEKIMP_STORY_SUBTITLE_UNITS,&text) ||
        !text_length(f->speaker,SUDEKIMP_STORY_SPEAKER_UNITS,&name) ||
        !isfinite(f->letterbox) || f->letterbox<0 || f->letterbox>.4f ||
        !isfinite(f->letterbox_bottom) || f->letterbox_bottom<0 || f->letterbox_bottom>.4f ||
        (!(f->flags&SUDEKIMP_STORY_LETTERBOX_PRESENT) && (f->letterbox!=0 || f->letterbox_bottom!=0)) ||
        f->elapsed_ms>3600000u || f->duration_ms>3600000u) return FALSE;
    if((f->flags&SUDEKIMP_STORY_DIALOGUE_ACTIVE) && !f->conversation_serial) return FALSE;
    if(f->flags&SUDEKIMP_STORY_LINE_ACTIVE) {
        if(!f->conversation_serial || !f->line_serial || !text) return FALSE;
    } else if(text || name || f->cue[0] || f->elapsed_ms || f->duration_ms ||
        (f->flags&(SUDEKIMP_STORY_SPEAKER_PRESENT|SUDEKIMP_STORY_CUE_PRESENT))) return FALSE;
    if(!!name!=!!(f->flags&SUDEKIMP_STORY_SPEAKER_PRESENT)) return FALSE;
    if(f->flags&SUDEKIMP_STORY_CUE_PRESENT) {
        if(!SudekiMpLanStoryPresentationCueValid(f->cue)) return FALSE;
    } else if(f->cue[0]) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanStoryPresentationMatches(const SudekiMpLanStoryPresentation *f,
    const SudekiMpLanStoryFrame *p) {
    return SudekiMpLanStoryPresentationValid(f) && SudekiMpLanStoryFrameValid(p) &&
        f->epoch==p->epoch && f->revision==p->revision && f->host_tick==p->host_tick &&
        f->sequence==p->sequence;
}
BOOL SudekiMpLanStoryPresentationEncode(const SudekiMpLanStoryPresentation *f,
    uint8_t *p,size_t capacity,size_t *written) {
    if(written) *written=0;
    if(!p || !written || !SudekiMpLanStoryPresentationValid(f)) return FALSE;
    unsigned nt=0,ns=0,nc=(unsigned)strlen(f->cue);
    if(!text_length(f->subtitle,SUDEKIMP_STORY_SUBTITLE_UNITS,&nt) ||
        !text_length(f->speaker,SUDEKIMP_STORY_SPEAKER_UNITS,&ns)) return FALSE;
    size_t size=SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE+2u*nt+2u*ns+nc;
    if(capacity<size) return FALSE;
    memset(p,0,size); memcpy(p,"SPP\2",4u);
    put32(p+4,f->epoch); put32(p+8,f->revision); put32(p+12,f->host_tick);
    put32(p+16,f->sequence); put32(p+20,f->conversation_serial); put32(p+24,f->line_serial);
    put32(p+28,f->elapsed_ms); put32(p+32,f->duration_ms); p[36]=f->flags;
    put16(p+40,(uint16_t)nt); put16(p+42,(uint16_t)ns); put16(p+44,(uint16_t)nc);
    uint32_t bits; memcpy(&bits,&f->letterbox,4u); put32(p+48,bits);
    memcpy(&bits,&f->letterbox_bottom,4u); put32(p+52,bits);
    unsigned cursor=SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE;
    for(unsigned i=0;i<nt;++i,cursor+=2u) put16(p+cursor,f->subtitle[i]);
    for(unsigned i=0;i<ns;++i,cursor+=2u) put16(p+cursor,f->speaker[i]);
    memcpy(p+cursor,f->cue,nc); *written=size; return TRUE;
}
BOOL SudekiMpLanStoryPresentationDecode(const uint8_t *p,size_t size,
    SudekiMpLanStoryPresentation *out) {
    SudekiMpLanStoryPresentation f={0};
    if(!p || !out || size<SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE || size>SUDEKIMP_STORY_PRESENTATION_MAX_SIZE ||
        memcmp(p,"SPP\2",4u) || p[37] || p[38] || p[39] || p[46] || p[47]) return FALSE;
    unsigned nt=get16(p+40),ns=get16(p+42),nc=get16(p+44);
    if(nt>SUDEKIMP_STORY_SUBTITLE_UNITS || ns>SUDEKIMP_STORY_SPEAKER_UNITS ||
        nc>SUDEKIMP_STORY_CUE_BYTES || size!=SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE+2u*nt+2u*ns+nc) return FALSE;
    f.epoch=get32(p+4); f.revision=get32(p+8); f.host_tick=get32(p+12); f.sequence=get32(p+16);
    f.conversation_serial=get32(p+20); f.line_serial=get32(p+24);
    f.elapsed_ms=get32(p+28); f.duration_ms=get32(p+32); f.flags=p[36];
    uint32_t bits=get32(p+48); memcpy(&f.letterbox,&bits,4u);
    bits=get32(p+52); memcpy(&f.letterbox_bottom,&bits,4u);
    unsigned cursor=SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE;
    for(unsigned i=0;i<nt;++i,cursor+=2u) { f.subtitle[i]=get16(p+cursor); if(!f.subtitle[i]) return FALSE; }
    for(unsigned i=0;i<ns;++i,cursor+=2u) { f.speaker[i]=get16(p+cursor); if(!f.speaker[i]) return FALSE; }
    for(unsigned i=0;i<nc;++i) { f.cue[i]=(char)p[cursor+i]; if(!f.cue[i]) return FALSE; }
    if(!SudekiMpLanStoryPresentationValid(&f)) return FALSE;
    *out=f; return TRUE;
}
static unsigned fragment_count(unsigned size) { return (size+1119u)/1120u; }
static unsigned fragment_size(unsigned size,unsigned index) {
    unsigned left=size-index*1120u; return left>1120u?1120u:left;
}
unsigned SudekiMpLanStoryPresentationChunkCount(const SudekiMpLanStoryPresentation *f) {
    uint8_t bytes[SUDEKIMP_STORY_PRESENTATION_MAX_SIZE]; size_t size=0;
    return SudekiMpLanStoryPresentationEncode(f,bytes,sizeof(bytes),&size)?fragment_count((unsigned)size):0;
}
BOOL SudekiMpLanStoryPresentationChunkEncode(const SudekiMpLanStoryPresentation *f,
    unsigned index,uint8_t *p,size_t capacity,size_t *written) {
    uint8_t payload[SUDEKIMP_STORY_PRESENTATION_MAX_SIZE]; size_t total=0;
    if(written) *written=0;
    if(!p || !written || !SudekiMpLanStoryPresentationEncode(f,payload,sizeof(payload),&total) ||
        index>=fragment_count((unsigned)total)) return FALSE;
    unsigned n=fragment_size((unsigned)total,index);
    if(capacity<36u+n) return FALSE;
    memset(p,0,36u); memcpy(p,"SPC\2",4u);
    put32(p+4,f->epoch); put32(p+8,f->revision); put32(p+12,f->host_tick); put32(p+16,f->sequence);
    put32(p+20,digest(payload,total)); put16(p+24,(uint16_t)total);
    put16(p+26,(uint16_t)(index*1120u)); put16(p+28,(uint16_t)n);
    p[30]=(uint8_t)index; p[31]=(uint8_t)fragment_count((unsigned)total);
    memcpy(p+36,payload+index*1120u,n); *written=36u+n; return TRUE;
}
static BOOL chunk_valid(const SudekiMpLanStoryPresentationChunk *c) {
    return c && c->epoch && c->revision && c->sequence && c->total_size>=SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE &&
        c->total_size<=SUDEKIMP_STORY_PRESENTATION_MAX_SIZE &&
        c->chunks==fragment_count(c->total_size) && c->index<c->chunks &&
        c->offset==c->index*1120u && c->size==fragment_size(c->total_size,c->index);
}
BOOL SudekiMpLanStoryPresentationChunkDecode(const uint8_t *p,size_t size,
    SudekiMpLanStoryPresentationChunk *out) {
    SudekiMpLanStoryPresentationChunk c={0};
    if(!p || !out || size<36u || size>SUDEKIMP_STORY_PRESENTATION_CHUNK_MAX_SIZE ||
        memcmp(p,"SPC\2",4u) || p[32] || p[33] || p[34] || p[35]) return FALSE;
    c.epoch=get32(p+4); c.revision=get32(p+8); c.host_tick=get32(p+12); c.sequence=get32(p+16);
    c.digest=get32(p+20); c.total_size=get16(p+24); c.offset=get16(p+26); c.size=get16(p+28);
    c.index=p[30]; c.chunks=p[31];
    if(!chunk_valid(&c) || size!=36u+c.size) return FALSE;
    memcpy(c.bytes,p+36,c.size); *out=c; return TRUE;
}
BOOL SudekiMpLanStoryPresentationAssemble(const SudekiMpLanStoryPresentationChunk *chunks,
    unsigned count,SudekiMpLanStoryPresentation *out) {
    uint8_t bytes[SUDEKIMP_STORY_PRESENTATION_MAX_SIZE]={0}; unsigned mask=0;
    if(!chunks || !out || !count || count>SUDEKIMP_STORY_PRESENTATION_MAX_CHUNKS ||
        !chunk_valid(chunks) || count!=chunks->chunks) return FALSE;
    for(unsigned i=0;i<count;++i) {
        const SudekiMpLanStoryPresentationChunk *c=chunks+i;
        if(!chunk_valid(c) || c->epoch!=chunks->epoch || c->revision!=chunks->revision ||
            c->sequence!=chunks->sequence || c->host_tick!=chunks->host_tick ||
            c->digest!=chunks->digest || c->total_size!=chunks->total_size ||
            c->chunks!=count || (mask&(1u<<c->index))) return FALSE;
        mask|=1u<<c->index; memcpy(bytes+c->offset,c->bytes,c->size);
    }
    SudekiMpLanStoryPresentation f;
    if(mask!=(1u<<count)-1u || digest(bytes,chunks->total_size)!=chunks->digest ||
        !SudekiMpLanStoryPresentationDecode(bytes,chunks->total_size,&f) ||
        f.epoch!=chunks->epoch || f.revision!=chunks->revision ||
        f.sequence!=chunks->sequence || f.host_tick!=chunks->host_tick) return FALSE;
    *out=f; return TRUE;
}
