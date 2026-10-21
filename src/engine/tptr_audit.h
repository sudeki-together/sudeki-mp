#ifndef SUDEKIMP_TPTR_AUDIT_H
#define SUDEKIMP_TPTR_AUDIT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { SUDEKIMP_TPTR_AUDIT_LIMIT=64, SUDEKIMP_TPTR_STACK=1, SUDEKIMP_TPTR_LINK=2,
    SUDEKIMP_TPTR_CYCLE=4, SUDEKIMP_TPTR_BOUND=8, SUDEKIMP_TPTR_UNREADABLE=16 };
typedef struct SudekiMpTPtrNode { uintptr_t object,previous,next; } SudekiMpTPtrNode;
typedef struct SudekiMpTPtrWalk {
    unsigned flags,count; uintptr_t nodes[SUDEKIMP_TPTR_AUDIT_LIMIT];
} SudekiMpTPtrWalk;
typedef int (*SudekiMpTPtrRead)(void *,uintptr_t,void *,size_t);
/* Read-only, bounded, current-stack diagnostic. Zero flags is not lifetime proof. */
static inline SudekiMpTPtrWalk SudekiMpTPtrAuditWalk(uintptr_t object,uintptr_t head,
    uintptr_t own,uintptr_t stack_limit,uintptr_t stack_base,SudekiMpTPtrRead read,void *context) {
    SudekiMpTPtrWalk out={0}; uintptr_t previous=0,node=head;
    while(node) {
        for(unsigned i=0;i<out.count;++i) if(out.nodes[i]==node) {out.flags|=SUDEKIMP_TPTR_CYCLE;return out;}
        if(out.count==SUDEKIMP_TPTR_AUDIT_LIMIT) {out.flags|=SUDEKIMP_TPTR_BOUND;return out;}
        out.nodes[out.count++]=node;
        if(node!=own && node>=stack_limit && node<stack_base) out.flags|=SUDEKIMP_TPTR_STACK;
        SudekiMpTPtrNode copy;
        if(!read || !read(context,node,&copy,sizeof(copy))) {out.flags|=SUDEKIMP_TPTR_UNREADABLE;return out;}
        if(copy.object!=object || copy.previous!=previous) out.flags|=SUDEKIMP_TPTR_LINK;
        previous=node;node=copy.next;
    }
    return out;
}
typedef struct SudekiMpTPtrCapture {
    uintptr_t at[SUDEKIMP_TPTR_AUDIT_LIMIT];
    uint8_t bytes[SUDEKIMP_TPTR_AUDIT_LIMIT][32];
    int readable[SUDEKIMP_TPTR_AUDIT_LIMIT];
} SudekiMpTPtrCapture;
/* Caller owns storage; native audit uses only its CAS winner's static buffer. */
static inline void SudekiMpTPtrAuditCapture(const SudekiMpTPtrWalk *walk,
    SudekiMpTPtrCapture *out,SudekiMpTPtrRead read,void *context) {
    for(unsigned i=0;i<walk->count;++i) {
        out->at[i]=walk->nodes[i]>=8u?walk->nodes[i]-8u:0;
        out->readable[i]=read && read(context,out->at[i],out->bytes[i],sizeof(out->bytes[i]));
    }
}
#endif
