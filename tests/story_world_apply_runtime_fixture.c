/* Real runtime failure routing; the callback executes real WorldApplyPrepared
 * in the capacity fixture. No native game callbacks execute here. */
#define SudekiMpLanStoryWorldFrameMatches fixture_frame_matches
#define SudekiMpLanStoryWorldRecruiting fixture_recruiting
#define SudekiMpLanStoryWorldFinishRecruitment fixture_finish_recruitment
#define SudekiMpLanStoryWorldPrepare fixture_world_prepare
#define SudekiMpLanStoryWorldApplyPrepared fixture_world_apply
#define SudekiMpLanStoryWorldCancelPrepared fixture_world_cancel
#define SudekiMpLogFormat fixture_runtime_log
#define SudekiMpLogWrite fixture_runtime_write
#include "../src/hooks/lan_story_runtime.c"
#include <assert.h>
#include <stdarg.h>
static void (*apply_failure)(void);
static unsigned world_refusals,view_restores;
static DWORD apply_error;
BOOL fixture_frame_matches(const SudekiMpLanStoryWorldFrame *w,const SudekiMpLanStoryFrame *p) {(void)w;(void)p;return TRUE;}
BOOL fixture_recruiting(void) {return FALSE;}
BOOL fixture_finish_recruitment(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryWorldFrame *f,uint32_t e,SudekiMpLanStoryReplicaExact x,void *c) {(void)r;(void)f;(void)e;(void)x;(void)c;assert(0);return FALSE;}
BOOL fixture_world_prepare(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryWorldFrame *f,SudekiMpLanStoryReplicaExact x,void *c,BOOL v) {(void)r;(void)f;(void)x;(void)c;(void)v;return TRUE;}
BOOL fixture_world_apply(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryWorldFrame *f,SudekiMpLanStoryReplicaExact x,void *c) {(void)r;(void)f;(void)x;(void)c;apply_failure();apply_error=GetLastError();return FALSE;}
BOOL fixture_world_cancel(void) {assert(0);return FALSE;}
BOOL SudekiMpLanStoryReplicaPrepareEquipment(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryFrame *f,SudekiMpLanStoryReplicaExact x,void *c) {(void)r;(void)f;(void)x;(void)c;return TRUE;}
BOOL SudekiMpLanStoryReplicaApply(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryFrame *f,BOOL v,SudekiMpLanStoryReplicaExact x,void *c) {(void)r;(void)f;(void)v;(void)x;(void)c;return TRUE;}
BOOL SudekiMpLanStoryReplicaRestoreView(const SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryReplicaExact x,void *c) {(void)r;(void)x;(void)c;++view_restores;return TRUE;}
void fixture_runtime_write(const char *s) {(void)s;assert(0);}
void fixture_runtime_log(const char *format,...) {
    char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    if(strstr(text,"event=present_refused")) {++world_refusals;assert(strstr(text,"step=world_apply error=1237"));}
}
void __attribute__((externally_visible)) test_runtime_world_apply_failure(void (*callback)(void)) {
    apply_failure=callback;world_refusals=view_restores=0;
    client_local_selected=TRUE;host_cinematic_view=TRUE;
    SudekiMpLanStoryNativeRoster r={0};SudekiMpLanStoryScene s={.phase=SUDEKIMP_LAN_STORY_READY};
    SudekiMpLanStoryFrame p={0};SudekiMpLanStoryWorldFrame w={0};StoryPresentation f={.party=&p,.world=&w};
    assert(!apply_presented_frame(&r,&s,&f));
    assert(apply_error==ERROR_RETRY && world_refusals==1 && view_restores==1 && !host_cinematic_view);
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {(void)r;assert(0);return FALSE;}
