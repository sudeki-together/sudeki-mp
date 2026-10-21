#ifndef SUDEKIMP_STORY_MOVIE_H
#define SUDEKIMP_STORY_MOVIE_H
#include <stdint.h>
#include <stddef.h>
/* Additive, host-authored presentation only. No filename or native pointer
 * crosses the wire. FMA04A is the only admitted installed asset. */
enum { SUDEKIMP_STORY_MOVIE_FMA04A=1, SUDEKIMP_STORY_MOVIE_WIRE=20,
    SUDEKIMP_STORY_MOVIE_FRESH_MS=1000, SUDEKIMP_STORY_MOVIE_LATE_MS=1500,
    SUDEKIMP_STORY_MOVIE_LIMIT_MS=600000 };
typedef struct SudekiMpStoryMovie {
    uint32_t serial,epoch,revision,elapsed_ms;
    uint8_t movie,active;
} SudekiMpStoryMovie;
static inline int SudekiMpStoryMovieValid(const SudekiMpStoryMovie *v) {
    return v && v->serial && v->epoch && v->revision &&
        v->movie==SUDEKIMP_STORY_MOVIE_FMA04A && v->active<=1 &&
        v->elapsed_ms<=SUDEKIMP_STORY_MOVIE_LIMIT_MS;
}
static inline int SudekiMpStoryMovieSame(const SudekiMpStoryMovie *a,const SudekiMpStoryMovie *b) {
    return a && b && a->serial==b->serial && a->epoch==b->epoch &&
        a->revision==b->revision && a->movie==b->movie;
}
static inline int SudekiMpStoryMovieAdvances(const SudekiMpStoryMovie *a,const SudekiMpStoryMovie *b) {
    if(!SudekiMpStoryMovieValid(b)) return 0;
    if(!a->serial) return 1;
    if(a->serial!=b->serial) return (int32_t)(b->serial-a->serial)>0;
    return SudekiMpStoryMovieSame(a,b) && !(b->active && !a->active) &&
        b->elapsed_ms>=a->elapsed_ms;
}
static inline int SudekiMpStoryMovieEncode(const SudekiMpStoryMovie *v,uint8_t *b,size_t n) {
    if(!b || n!=SUDEKIMP_STORY_MOVIE_WIRE || !SudekiMpStoryMovieValid(v)) return 0;
    b[0]=1;b[1]=v->movie;b[2]=v->active;b[3]=0;
    uint32_t a[]={v->serial,v->epoch,v->revision,v->elapsed_ms};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) b[4+4*i+j]=(uint8_t)(a[i]>>(8*j));
    return 1;
}
static inline int SudekiMpStoryMovieDecode(const uint8_t *b,size_t n,SudekiMpStoryMovie *v) {
    if(!b || !v || n!=SUDEKIMP_STORY_MOVIE_WIRE || b[0]!=1 || b[3]) return 0;
    uint32_t a[4]={0};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) a[i]|=(uint32_t)b[4+4*i+j]<<(8*j);
    SudekiMpStoryMovie next={a[0],a[1],a[2],a[3],b[1],b[2]};
    if(!SudekiMpStoryMovieValid(&next)) return 0;
    *v=next;return 1;
}
#endif
