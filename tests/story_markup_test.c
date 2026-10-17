/* The game's speech markup in the client's subtitle overlay. Pure. */
#include "ui/story_markup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t in[1100]; static char out[1100]; static uint32_t col[1100];
static void u16(const char *s) { unsigned i=0; for(;s[i];++i) in[i]=(uint8_t)s[i]; in[i]=0; }
static BOOL decode(const char *s) { u16(s); return SudekiMpStoryMarkupDecode(in,1025,out,col,1025); }
static void span(const char *word,uint32_t colour) {
    const char *at=strstr(out,word); assert(at);
    for(unsigned i=0;word[i];++i) assert(col[(at-out)+i]==colour);
}
int main(void) {
    assert(decode("Glad to be alive that's why. I am a refugee from <colour='00FFFF'>Shadani-Mo</colour>. There has been too much bloodshed there."));
    assert(!strcmp(out,"Glad to be alive that's why. I am a refugee from Shadani-Mo. There has been too much bloodshed there."));
    span("Shadani-Mo",0x00ffff); span("Glad",0); span(". There",0);
    assert(decode("<colour='ff7777>failed</colour> ok") && !strcmp(out,"failed ok")); span("failed",0xff7777); span(" ok",0);
    assert(decode("<colour='000ffff>Block</colour>") && !strcmp(out,"Block")); span("Block",0x00ffff);
    assert(decode("a <colour=4E7AF9>Emerald Panacea</colour>") && !strcmp(out,"a Emerald Panacea")); span("Emerald Panacea",0x4e7af9);
    assert(decode("<colour='F0DC96''>Buki :</colour> hi") && !strcmp(out,"Buki : hi")); span("Buki :",0xf0dc96);
    assert(decode("<colour='112233'>x<colour>y") && !strcmp(out,"xy")); span("x",0x112233); span("y",0);   /* bare closes */
    assert(decode("<colour='112233'>a<colour='445566'>b</colour>c</colour>d") && !strcmp(out,"abcd"));
    span("a",0x112233); span("b",0x445566); span("c",0x112233); span("d",0);
    assert(decode("<colour='000000'>k</colour>") && col[0]==0x010101);            /* black is not "default" */
    assert(decode("<colour='00ffff'>A <colour='zz'>B</colour> C</colour>") && !strcmp(out,"A B C"));
    span("A",0x00ffff); span("B",0); span(" C",0x00ffff);                          /* non-hex opens a default span */
    assert(decode("one<br>two<br/>three</br>") && !strcmp(out,"one\ntwo\nthree"));
    assert(decode("Press <img filename='SUI_Button_A' width='32' height='24' shift_y='6'></img> now") && !strcmp(out,"Press  now"));
    assert(decode("<alpha='80'>x</alpha>") && !strcmp(out,"x"));
    assert(decode("<unreadable signature>") && !strcmp(out,"<unreadable signature>"));
    assert(decode("a < b") && !strcmp(out,"a < b"));
    assert(decode("<colour='00ffff' no close") && !strcmp(out,"<colour='00ffff' no close"));
    assert(decode("</colour>stray") && !strcmp(out,"stray"));
    in[0]='a'; in[1]=0xe9; in[2]=0; assert(!SudekiMpStoryMarkupDecode(in,1025,out,col,1025));    /* non-ASCII */
    in[0]='a'; in[1]=7; in[2]=0;    assert(!SudekiMpStoryMarkupDecode(in,1025,out,col,1025));    /* control */
    for(unsigned i=0;i<1024;++i) in[i]='x';
    in[1024]=0;
    assert(SudekiMpStoryMarkupDecode(in,1025,out,col,1025) && strlen(out)==1024);
    assert(!SudekiMpStoryMarkupDecode(in,1025,out,col,1024));                               /* no room */
    in[1024]='x'; assert(!SudekiMpStoryMarkupDecode(in,1025,out,col,1025));                  /* unterminated */
    u16("plain"); assert(SudekiMpStoryMarkupDecode(in,1025,out,NULL,1025) && !strcmp(out,"plain"));
    puts("StoryMarkupTest: PASS (colour spans, nesting, malformed forms, br/img/alpha, literals, limits)");
    return 0;
}
