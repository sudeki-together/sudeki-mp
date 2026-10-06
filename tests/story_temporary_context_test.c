/* Synthetic read-only diagnostic fixture, NOT native placement or gameplay.
 * Exercise the real observer helpers without installing hooks or granting a
 * native roster witness. Exact native field interpretation has separate static
 * evidence; no retail code is executed by this test. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../src/hooks/lan_story_observer.c"

static char last_log[4096];
static unsigned log_count;
void SudekiMpLogFormat(const char *format,...) {
    va_list args; va_start(args,format);
    int n=vsnprintf(last_log,sizeof(last_log),format,args);
    va_end(args); assert(n>=0 && (size_t)n<sizeof(last_log)); ++log_count;
}
void SudekiMpLogWrite(const char *s) { (void)s; assert(!"unexpected log path"); }
BOOL SudekiMpCleanroomEngineWorldReady(void) { assert(!"native call"); return FALSE; }
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor a) {
    (void)a; assert(!"native call"); return NULL;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *w) { (void)w; assert(!"roster call"); return FALSE; }
BOOL SudekiMpLanStorySceneValid(const SudekiMpLanStoryScene *s) {
    (void)s; assert(!"scene call"); return FALSE;
}
BOOL SudekiMpLanStorySceneSame(const SudekiMpLanStoryScene *a,const SudekiMpLanStoryScene *b) {
    (void)a; (void)b; assert(!"scene call"); return FALSE;
}
BOOL SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,
    size_t n,const void *r) {
    (void)h; (void)p; (void)e; (void)n; (void)r; assert(!"hook install"); return FALSE;
}
BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *h) {
    (void)h; assert(!"hook restore"); return FALSE;
}

static uint8_t world[0x3a0],table[2*0x54];
static void fixture(void) {
    memset(world,0,sizeof(world)); memset(table,0,sizeof(table));
    *(void **)(base+0x408d10u)=world;
    *(void **)(world+0x50u)=table; *(unsigned *)(world+0x54u)=2;
    *(void **)(world+0xcu)=table; *(void **)(world+0x14u)=table+0x54u;
    *(const char **)(table+0x24u)="brightwater";
    *(const char **)(table+0x54u+0x24u)="church";
    *(uint32_t *)(table+0x34u)=3; *(uint32_t *)(table+0x54u+0x34u)=1;
    const uint32_t values[6]={0x3f800000u,0x40000000u,0xc0400000u,0,0x3f800000u,0};
    memcpy(world+0x28u,values,sizeof(values));
    *(uint16_t *)(world+0x40u)=0x1234u;
    /* Adjacent bytes must not be read as part of the 16-bit sector. */
    world[0x42u]=0xab; world[0x43u]=0xcd;
    native_thread=GetCurrentThreadId(); foreign_thread=FALSE;
    observed.epoch=7; log_count=0;
}
static void copied_sector_and_read_only(void) {
    fixture(); uint8_t before[sizeof(world)],descriptors[sizeof(table)];
    memcpy(before,world,sizeof(world)); memcpy(descriptors,table,sizeof(table));
    TemporaryWorldCopy copy=temporary_world_copy(world);
    assert(copy.exact && copy.context_valid && copy.sector==0x1234u);
    assert(!strcmp(copy.current,"brightwater") && !strcmp(copy.destination,"church"));
    assert(!memcmp(copy.return_bits,world+0x28u,sizeof(copy.return_bits)));
    assert(!memcmp(world,before,sizeof(world)) && !memcmp(table,descriptors,sizeof(table)));
    *(uint16_t *)(world+0x40u)=0xffffu;
    copy=temporary_world_copy(world);
    /* Preserve the native sentinel. Diagnostics never invent a floor/sector. */
    assert(copy.exact && copy.sector==0xffffu);
    *(uint32_t *)(world+0x28u)=0x7fc00000u;
    copy=temporary_world_copy(world); assert(copy.exact && !copy.context_valid);
}
static void entry_after_exit_before(void) {
    fixture(); TemporaryJournal j={0}; j.serial=1; j.kind=TEMP;
    j.before=temporary_world_copy(world); j.before.sector=0x0102u;
    temporary_journal_end(&j,world);
    assert(log_count==3 && strstr(last_log,"phase=after_entry") &&
        strstr(last_log,"sector=4660 ") && !strstr(last_log,"camera="));
    assert(strstr(last_log,"completion=unobserved policy=observation_only"));
    assert(strlen(last_log)>=2 && last_log[strlen(last_log)-2]=='\r' &&
        last_log[strlen(last_log)-1]=='\n');
    j.kind=EXIT; j.before.state=4;
    uint8_t before[sizeof(world)]; memcpy(before,world,sizeof(world));
    temporary_journal_end(&j,world);
    assert(log_count==6 && strstr(last_log,"phase=before_exit") && strstr(last_log,"sector=258 "));
    assert(!memcmp(world,before,sizeof(world)));
    j.serial=0; temporary_journal_end(&j,world); assert(log_count==6);
}
static void unknown_is_not_completion(void) {
    fixture();
    assert(!temporary_world_copy(world+4).exact);
    *(void **)(world+0xcu)=table+1; assert(!temporary_world_copy(world).exact);
    fixture(); *(unsigned *)(world+0x54u)=4097; assert(!temporary_world_copy(world).exact);
    fixture(); *(void **)(world+0x50u)=NULL; assert(!temporary_world_copy(world).exact);
    fixture(); TemporaryJournal j={0}; j.serial=1; j.kind=TEMP;
    foreign_thread=TRUE; temporary_journal_end(&j,world);
    assert(strstr(last_log,"captured=0 finite=0") && strstr(last_log,"completion=unobserved"));
    foreign_thread=FALSE;
}
int main(void) {
    base=VirtualAlloc(NULL,0x410000u,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(base);
    copied_sector_and_read_only(); entry_after_exit_before(); unknown_is_not_completion();
    assert(VirtualFree(base,0,MEM_RELEASE)); base=NULL;
    puts("StoryTemporaryContextTest: PASS (synthetic diagnostics only)"); return 0;
}
