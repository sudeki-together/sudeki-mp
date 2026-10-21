/* Production movie routing/preparation with the real MenuNative pause proof.
 * Client containment bracket, native roster and view owners remain spies. */
#include "../src/hooks/lan_story_runtime.c"
#include <assert.h>
static BOOL paused=TRUE,cast_ok=TRUE,view_ok=TRUE,borrowed=TRUE;
static BOOL loaded=TRUE,held=TRUE,dispatch_exact=TRUE,post_exact=TRUE,in_scope;
static BOOL preparation_result;
static unsigned cast_restores,view_restores,inputs,clocks,movie_calls,scope_calls;
static SudekiMpControlUpdateDispatchWitness witness={.service_post_original_exact=TRUE};
static SudekiMpLanStoryNativeRoster fixture_roster;
void StoryMovieTestMenuCreate(void);
void StoryMovieTestMenuDestroy(void);
BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *s) {
    memset(s,0,sizeof(*s));s->load_generation=1;s->start_task=1;s->on_load_task=2;
    s->start_terminal=s->start_retired=s->on_load_terminal=s->on_load_retired=1;return loaded;
}
BOOL SudekiMpLanStoryLoadGetResult(SudekiMpStoryLoadResult *s) {
    memset(s,0,sizeof(*s));s->attempt=1;s->native_called=s->files_retired=1;
    s->state=SUDEKIMP_STORY_LOAD_RETURNED;return loaded;
}
BOOL SudekiMpLanStoryMovieService(BOOL ready,BOOL (*prepare)(void)) {
    ++movie_calls;assert(prepare && !in_scope);
    preparation_result=ready && prepare();
    /* Native MoviePlay starts only after the scoped pause proof has ended. */
    assert(!in_scope);return held;
}
BOOL SudekiMpLanStoryClientRestoreViewDispatch(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientPresentation callback,void *context) {
    assert(w==&witness && callback && !context && !in_scope);++scope_calls;
    BOOL owned=FALSE;SudekiMpLanStoryScene scene={0};
    if(!dispatch_exact || !w->service_post_original_exact || !paused ||
        !SudekiMpLanPartyMenuNativeObserveOwnedPause(&owned) || !owned ||
        !SudekiMpLanPartyMenuNativeOwnsPause())return FALSE;
    in_scope=TRUE;BOOL result=callback(&fixture_roster,&scene,context);in_scope=FALSE;
    return result && post_exact;
}
BOOL SudekiMpLanStoryClientPausedRoster(SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryScene *s) {
    BOOL owned=FALSE;*r=fixture_roster;memset(s,0,sizeof(*s));
    return paused && SudekiMpLanPartyMenuNativePauseExact(&owned) && owned;
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return in_scope && dispatch_exact && r==&fixture_roster;
}
BOOL SudekiMpLanStoryLocalControlRestoreCastView(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryReplicaExact exact,void *data) {
    assert(exact==movie_roster_exact && !data && exact(r,data));++cast_restores;return cast_ok;
}
BOOL SudekiMpLanStoryReplicaRetainsView(void) {return borrowed;}
BOOL SudekiMpLanStoryReplicaRestoreView(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryReplicaExact exact,void *data) {
    assert(exact==movie_roster_exact && !data && exact(r,data));++view_restores;return view_ok;
}
void SudekiMpLanStoryInputClear(void) {++inputs;}
void SudekiMpLanStoryEffectsResetClock(void) {++clocks;}
static void run(void) {
    assert(service_story_movie(&witness,TRUE) && !in_scope && !movie_prepare_witness);
}
int main(void) {
    StoryMovieTestMenuCreate();
    BOOL owned=FALSE;
    assert(!SudekiMpLanPartyMenuNativePauseExact(&owned));
    assert(SudekiMpLanPartyMenuNativeObserveOwnedPause(&owned) && owned);
    input_attempted=client_input_ready=TRUE;host_cinematic_view=host_room_view=TRUE;
    history_count=world_history_count=dialogue_count=3;presentation_sample.valid=effects_ready=TRUE;
    assert(!prepare_story_movie() && !scope_calls); /* No retained dispatch permission. */
    paused=FALSE;run();assert(!preparation_result && !cast_restores);paused=TRUE;
    dispatch_exact=FALSE;run();assert(!preparation_result && !cast_restores);dispatch_exact=TRUE;
    witness.service_post_original_exact=FALSE;run();assert(!preparation_result && !cast_restores);witness.service_post_original_exact=TRUE;
    cast_ok=FALSE;run();assert(!preparation_result && cast_restores==1 && !view_restores);cast_ok=TRUE;
    view_ok=FALSE;run();assert(!preparation_result && view_restores==1 && host_room_view);view_ok=TRUE;
    run();assert(preparation_result && cast_restores==3 && view_restores==2 && clocks==1);
    assert(!client_input_ready && !host_room_view && !host_cinematic_view && !history_count &&
        !world_history_count && !dialogue_count && !presentation_sample.valid && !effects_ready);
    post_exact=FALSE;run();assert(!preparation_result);post_exact=TRUE;
    unsigned before=scope_calls;loaded=FALSE;run();assert(!preparation_result && scope_calls==before);loaded=TRUE;
    assert(service_story_movie(&witness,FALSE) && !preparation_result && scope_calls==before);
    movie_prepare_witness=&witness;before=movie_calls;
    assert(!service_story_movie(&witness,TRUE) && movie_calls==before);movie_prepare_witness=NULL;
    assert(!prepare_story_movie());
    StoryMovieTestMenuDestroy();
    puts("StoryMovieRuntimeTest: PASS (scoped controller preparation, real pause proof, containment/view spies)");return 0;
}
