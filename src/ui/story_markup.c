#include "ui/story_markup.h"

static int lower(uint16_t c) { return c>='A' && c<='Z'?c+('a'-'A'):c; }
static BOOL word(const uint16_t *s,unsigned n,unsigned at,const char *name,unsigned *end) {
    unsigned i=0;
    for(;name[i];++i) if(at+i>=n || lower(s[at+i])!=name[i]) return FALSE;
    uint16_t next=at+i<n?s[at+i]:0;
    if(next!='>' && next!='=' && next!=' ' && next!='/') return FALSE;
    *end=at+i; return TRUE;
}
static int hex(uint16_t c) {
    return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;
}
BOOL SudekiMpStoryMarkupDecode(const uint16_t *s,unsigned units,char *text,uint32_t *colour,unsigned capacity) {
    uint32_t stack[8]; unsigned depth=0,out=0,n=0;
    if(!s || !text || !capacity) return FALSE;
    while(n<units && s[n]) ++n;
    if(n==units) return FALSE;                       /* not terminated */
    for(unsigned i=0;i<n;) {
        uint16_t c=s[i];
        if(c>126u || (c<32u && c!=10u && c!=13u)) return FALSE;
        if(c=='<') {
            unsigned at=i+1u,end=0,close=0; BOOL closing=at<n && s[at]=='/';
            if(closing) ++at;
            for(unsigned k=at;k<n && k<i+96u;++k) if(s[k]=='>') { close=k; break; }
            const char *names[]={"colour","alpha","br","img"}; int tag=-1;
            for(int t=0;close && t<4 && tag<0;++t) if(word(s,n,at,names[t],&end) && end<=close) tag=t;
            if(tag>=0) {
                if(tag==0 && !closing) {
                    unsigned k=end; while(k<close && (s[k]==' ' || s[k]=='=')) ++k;
                    BOOL has_value=k<close && s[end]!='>';
                    while(k<close && (s[k]=='\'' || s[k]=='"' || s[k]==' ')) ++k;
                    uint32_t value=0; unsigned digits=0;
                    while(k<close && digits<8u && hex(s[k])>=0) { value=(value<<4)|(uint32_t)hex(s[k]); ++k; ++digits; }
                    if(has_value) {
                        /* A value that is not hex opens a default-coloured span. */
                        value&=0xffffffu; if(digits && !value) value=0x010101u;   /* 0 means default */
                        if(depth<8u) stack[depth++]=value; else stack[7]=value;
                    } else if(depth) --depth;                          /* a bare <colour> closes */
                } else if(tag==0 && closing) { if(depth) --depth; }
                else if(tag==2 && !closing) {
                    if(out+1u>=capacity) return FALSE;
                    if(colour) colour[out]=0;
                    text[out++]='\n';
                }
                i=close+1u; continue;
            }
        }
        if(out+1u>=capacity) return FALSE;
        if(colour) colour[out]=depth?stack[depth-1u]:0u;
        text[out++]=(char)c; ++i;
    }
    text[out]=0; if(colour) colour[out]=0;
    return TRUE;
}
