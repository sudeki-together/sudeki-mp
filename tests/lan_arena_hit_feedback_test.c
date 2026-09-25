#include "../src/hooks/lan_arena_hit_feedback.c"
#include <stdio.h>
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); ++failures; } } while(0)
static unsigned int replay_count;
static BOOL sink_ok = TRUE;
static BOOL sink(void *context, const SudekiMpLanArenaHitFeedback *hit) {
    (void)context; (void)hit;
    if (!sink_ok) return FALSE;
    ++replay_count; return TRUE;
}

static void cursor_test(void) {
    SudekiMpLanHitCursor cursor = {0};
    SudekiMpLanArenaEnemySnapshot enemy = {0};
    enemy.feedback_generation=1;
    CHECK(SudekiMpLanHitConsume(&cursor,1,123,100,&enemy,sink,NULL));
    enemy.hit_count=1;
    enemy.hits[0]=(SudekiMpLanArenaHitFeedback){1,100,-12,100,88,0,0x2a,3};
    CHECK(SudekiMpLanHitConsume(&cursor,1,123,100,&enemy,sink,NULL));
    CHECK(replay_count==1);
    CHECK(SudekiMpLanHitConsume(&cursor,1,123,100,&enemy,sink,NULL));
    CHECK(replay_count==1);
    enemy.hit_count=3;
    enemy.hits[1]=enemy.hits[0]; enemy.hits[1].sequence=2;
    enemy.hits[2]=enemy.hits[0]; enemy.hits[2].sequence=3;
    sink_ok=FALSE;
    CHECK(!SudekiMpLanHitConsume(&cursor,1,123,100,&enemy,sink,NULL));
    CHECK(cursor.sequence==1);
    sink_ok=TRUE;
    CHECK(SudekiMpLanHitConsume(&cursor,1,123,100,&enemy,sink,NULL));
    CHECK(replay_count==3);
    enemy.hits[2].sequence=4;
    CHECK(SudekiMpLanHitConsume(&cursor,1,123,2000,&enemy,sink,NULL));
    CHECK(replay_count==3 && cursor.sequence==4); /* expired, never bursts later */
    CHECK(SudekiMpLanHitConsume(&cursor,2,123,100,&enemy,sink,NULL));
    CHECK(replay_count==3); /* reconnect baseline */
    enemy.hits[2].sequence=5;
    CHECK(SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    CHECK(replay_count==3); /* local replacement baseline */
    enemy.feedback_generation=2;
    CHECK(SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    enemy.feedback_generation=1;
    CHECK(!SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    enemy.feedback_generation=2;
    enemy.hits[2].reaction=0xff;
    CHECK(!SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    CHECK(replay_count==3); /* atomic journal validation */
    enemy.hit_count=1; enemy.hits[0].sequence=UINT32_MAX;
    cursor.sequence=UINT32_MAX-1;
    CHECK(SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    enemy.hits[0].sequence=1;
    CHECK(SudekiMpLanHitConsume(&cursor,2,124,100,&enemy,sink,NULL));
    CHECK(replay_count==5);
}

static uint8_t entity[0xb4], combat[0x74], ui[0xd0], arbiter[0x64];
static void delayed_actor_frame_test(void) {
    SudekiMpLanHitCursor cursor = {0};
    SudekiMpLanArenaEnemySnapshot confirmed = {0}, older;
    unsigned int before = replay_count;
    confirmed.feedback_generation = 10;
    CHECK(SudekiMpLanHitConsume(&cursor, 3, 321, 100, &confirmed, sink, NULL));
    older = confirmed;
    /* No admitted actor frame is required for a newly confirmed target hit.
     * Repeated render retries must not repeat the number or postpone new hits. */
    for (unsigned int i = 0; i < 8; ++i) {
        confirmed.hits[i] = (SudekiMpLanArenaHitFeedback){
            i + 1, 100 + i * 50, -12, 100, 88, 0, 0, SUDEKIMP_LAN_HIT_POPUP};
        confirmed.hit_count = (uint8_t)(i + 1);
        CHECK(SudekiMpLanHitConsume(&cursor, 3, 321, 100 + i * 50,
            &confirmed, sink, NULL));
        CHECK(replay_count == before + i + 1);
        CHECK(SudekiMpLanHitConsume(&cursor, 3, 321, 100 + i * 50,
            &confirmed, sink, NULL));
        CHECK(replay_count == before + i + 1);
    }
    /* A late interpolated/older frame cannot rewind a confirmed cursor. */
    CHECK(SudekiMpLanHitConsume(&cursor, 3, 321, 100, &older, sink, NULL));
    CHECK(cursor.sequence == 8 && replay_count == before + 8);
}
static unsigned int damage_calls, popup_calls;
static BOOL witness(SudekiMpLanHitTarget *t) {
    return SudekiMpLanHitResolveTarget(entity, 42, t);
}
static void __stdcall fake_popup(void *p, float a, float b, uint32_t c) {
    (void)p;(void)a;(void)b;(void)c; ++popup_calls;
}
static void __attribute__((thiscall)) fake_damage(void *c, void *packet) {
    (void)packet; ++damage_calls;
    *(uint32_t *)((uint8_t *)c+0x5c)=0x2cu<<9;
    *(int32_t *)(ui+0x74)=-12;
    observe_popup(ui, 100, 88, 0xffffffffu);
}
static void capture_test(void) {
    *(void **)(entity+0xa4)=combat; *(void **)(entity+0x90)=arbiter;
    *(void **)(entity+0xb0)=ui; *(void **)(combat+0x10)=entity;
    *(void **)(ui+0x10)=entity; ui[0x24]=1;
    original_damage=fake_damage; original_popup=fake_popup;
    host_witness=witness; game_thread=GetCurrentThreadId();
    observe_damage(combat,NULL);
    CHECK(damage_calls==1 && popup_calls==1 && history_count==1);
    CHECK(history[0].flags==3 && history[0].reaction==0x2c && history[0].amount==-12);
    observe_damage(combat,NULL);
    CHECK(damage_calls==2 && popup_calls==2 && history_count==2);
    CHECK(history[1].flags==1); /* unchanged pending selector is not retriggered */
    for (unsigned int i=0;i<12;++i) observe_damage(combat,NULL);
    CHECK(history_count==8 && history[7].sequence==14 && history[0].sequence==7);
    CHECK(damage_calls==14); /* observation never adds a second damage call */
    *(void **)(ui+0x10)=NULL;
    SudekiMpLanHitTarget t;
    CHECK(!witness(&t));
    host_witness=NULL; original_damage=NULL; original_popup=NULL;
}
int main(void) {
    cursor_test(); delayed_actor_frame_test(); capture_test();
    if (failures) return 1;
    puts("hit feedback tests passed"); return 0;
}
