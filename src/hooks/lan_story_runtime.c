#include "hooks/lan_story_runtime.h"
#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_objects.h"
#include "hooks/lan_story_area_membership.h"
#include "hooks/lan_story_loot_trace.h"
#include "hooks/lan_story_snapshot.h"
#include "hooks/lan_story_shots.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "hooks/lan_story_load.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_story_client.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_replica.h"
#include "hooks/lan_story_world.h"
#include "hooks/lan_story_effects.h"
#include "hooks/lan_story_realtime.h"
#include "hooks/lan_story_quick_menu.h"
#include "hooks/lan_story_render.h"
#include "hooks/lan_story_host_control.h"
#include "hooks/lan_story_control.h"
#include "hooks/lan_story_cast.h"
#include "hooks/lan_story_recruit.h"
#include "hooks/lan_story_local_control.h"
#include "hooks/lan_story_activity.h"
#include "hooks/lan_story_cinematic.h"
#include "ui/story_cinematic_view.h"
#include "hooks/lan_story_menu.h"
#include "hooks/lan_party_menu_native.h"
#include "hooks/lobby_gameplay.h"
#include "hooks/lan_story_split.h"
#include "engine/log.h"
#include <string.h>

static SudekiMpLanPartySession *session;
static SudekiMpControlUpdateObserverGate gate;
static HANDLE worker,stop_worker;
static volatile LONG stopping,observer_removed;
static BOOL registered,observer_attempted;
static BOOL shots_attempted;
static BOOL aim_pose_attempted;
static unsigned char owner;
static unsigned local_seat;
static DWORD last_trace,last_publish;
static uint32_t last_remote_revision;
static SudekiMpLanStoryCapture capture;
static DWORD last_frame_attempt,last_frame_trace;
static uint32_t captured_frames,received_frames;
static unsigned last_capture_result;
static int phases[4];
static BOOL saved_profile,client_attempted,menu_attempted,input_attempted,replica_attempted;
static BOOL world_attempted,effects_attempted,render_attempted,presentation_attempted;
static BOOL realtime_attempted;
static BOOL quick_menu_attempted;
static BOOL cast_attempted;
static BOOL host_attempted,menu_initialized,menu_lobby_known,menu_scene_known,host_binding_ready;
static BOOL control_attempted,recruit_attempted,local_control_attempted,activity_attempted;
static BOOL split_attempted;
static uint8_t traced_foreign;
static const char *client_pending_trace;
static unsigned client_pending_traces;
static const char *control_block_trace[4];
static unsigned control_block_traces;
static volatile LONG runtime_ready,client_exit_prepared,client_drained,host_drained;
static uint8_t *game_base;
static SudekiMpLobbyStatus menu_lobby;
static SudekiMpLanStoryScene menu_scene;
static uint64_t story_lobby_tickets[4];
static DWORD runtime_thread,last_present_trace,last_received_frame;
enum { STORY_HISTORY_CAPACITY=8 };
static SudekiMpLanStoryFrame history[STORY_HISTORY_CAPACITY];
static uint32_t history_receipts[STORY_HISTORY_CAPACITY];
static SudekiMpLanStoryWorldFrame world_history[STORY_HISTORY_CAPACITY];
static uint32_t world_receipts[STORY_HISTORY_CAPACITY];
static unsigned world_history_count;
static unsigned history_count,last_present_result;
static unsigned presentation_buffer_ms=66u;
static SudekiMpLanPartyLease history_lease;
static SudekiMpLanStoryNativeRoster client_seed;
/* One read-only baseline per runtime lifetime. Never rebuild from surviving
 * objects, from a reconnect, or from a roster/scene revision. This is not yet
 * input authority or an authenticated cross-peer catalog. */
static SudekiMpLanStoryObjectSnapshot initial_objects;
static SudekiMpLanStoryScene initial_object_scene;
static BOOL initial_objects_attempted,initial_objects_known;
/* Read-only area evidence; no native ownership or gameplay admission depends
 * on this diagnostic cache. At most one attempt/second, 64 changed records.
 * Do not retain a roster or native pointer between controller dispatches. */
static struct {
    DWORD attempted_at;
    BOOL attempted,known,exact;
    unsigned records;
    SudekiMpLanStoryAreaMembership last;
} area_observation;
static BOOL loot_trace_attempted;
static uint32_t presented_epoch,presented_generations[4];
static uint32_t presented_revision;
static BOOL presentation_changed;
static SudekiMpLanStoryScene host_before_recruit;
/* Retained only from a complete exact native capture. The seed is historical
 * reconstruction data; only a later fresh paired frame can admit input. */
static SudekiMpLanStoryCatchup host_seed,host_catchup[4],client_catchup;
static SudekiMpLanStoryScene host_live_scene;
static DWORD host_live_receipt,client_catchup_ack_at;
static BOOL client_catchup_seeded;
static unsigned loaded_party_mask;
static SudekiMpLanPartyPresence story_presence,story_published;
static BOOL story_policy_initialized,story_key_focused,story_key_down;
static uint32_t story_next_request,story_pending_request;
static unsigned story_swap_traces;
static BOOL publish_story_ownership(void);
static BOOL story_swap_blocks(unsigned player);

static unsigned client_catchup_trace;
static void observe_area_membership(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,DWORD now) {
    if(area_observation.records>=64u ||
        (area_observation.attempted && now-area_observation.attempted_at<1000u)) return;
    area_observation.attempted=TRUE; area_observation.attempted_at=now;
    SudekiMpLanStoryNativeRoster roster;
    SudekiMpLanStoryAreaMembership sample={0};
    BOOL exact=scene && scene->phase==SUDEKIMP_LAN_STORY_READY &&
        SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) &&
        SudekiMpLanStoryAreaMembershipObserve((HMODULE)game_base,w,&roster,&sample);
    /* Walking between sectors in the SAME region is not a new area record.
     * Keep full sector in the sampled log, but compare area-level state. */
    SudekiMpLanStoryAreaMembership key=sample;
    for(unsigned c=0;c<4u;++c) {
        key.members[c].sector&=0xff00u;
        key.members[c].movement_sector&=0xff00u;
        /* Keep unknown/mismatched regions visible without consuming the
         * bounded journal on ordinary native movement flag/sector changes. */
        memset(key.members[c].movement_flags,0,sizeof(key.members[c].movement_flags));
    }
    if(area_observation.known && area_observation.exact==exact &&
        (!exact || !memcmp(&key,&area_observation.last,sizeof(key)))) return;
    area_observation.known=TRUE; area_observation.exact=exact;
    area_observation.last=key; ++area_observation.records;
    if(!exact) {
        SudekiMpLogWrite("story_areas event=membership_unknown policy=read_only no_completion_inferred=1\r\n");
        return;
    }
    for(unsigned c=0;c<4u;++c) if(sample.available_mask&(1u<<c)) {
        const SudekiMpLanStoryAreaMember *m=&sample.members[c];
        SudekiMpLogFormat("story_areas event=membership character=%u epoch=%lu revision=%lu region=%u sector=%u name=%s current=%u native_state=%lu flags=%u pause_refs=%u data=%u pending=%u navigation=%u collision=%u registration=%u enabled=%u mask=%lu movement_present=%u movement_sector=%u movement_sector_valid=%u movement_region_matches=%u movement_flags=%u,%u policy=read_only playable=unproven\r\n",
            c,(unsigned long)sample.epoch,(unsigned long)sample.revision,m->sector>>8,m->sector,m->name,
            m->is_current,(unsigned long)m->native_state,m->zone_flags,m->pause_refs,
            m->data_present,m->data_pending,m->navigation_present,m->collision_present,
            m->collision_registration,m->collision_enabled,(unsigned long)m->collision_mask,
            m->movement_present,m->movement_sector,m->movement_sector_valid,
            m->movement_region_matches,m->movement_flags[0],m->movement_flags[1]);
    }
}
static SudekiMpLanStoryRecruitment host_recruit,client_recruit;
static BOOL client_recruit_committed;
static BOOL client_switch_prepared,client_local_selected,client_input_ready;
static SudekiMpLanStoryControlFence client_control_fence;
static SudekiMpLanPartyLease client_control_connection;
static uint32_t client_input_sequence,client_input_sent_at;
static SudekiMpLanStoryActionRequest client_action;
static SudekiMpLanPartyLease client_action_connection;
static uint32_t client_action_serial,client_action_sent_at;
static BOOL client_action_pending;
static unsigned client_control_trace_state,client_control_trace_count;
static unsigned host_control_hold_traces;
static struct {
    SudekiMpLanPartyLease connection,native_key;
    SudekiMpLanStoryControlFence fence;
    BOOL acquired,draining,ready,input_held;
    uint32_t transaction,last_input_at;
    SudekiMpLanStoryMovement movement;
} host_control[4];
static SudekiMpLanStoryPresentation dialogue_history[STORY_HISTORY_CAPACITY],visible_dialogue;
static uint32_t dialogue_receipts[STORY_HISTORY_CAPACITY],visible_dialogue_receipt;
static unsigned dialogue_count;
static BOOL visible_dialogue_valid;
typedef struct StoryCinematicTrace {
    uint32_t epoch,line;
    unsigned flags,count;
    DWORD error;
    BOOL known,ok;
} StoryCinematicTrace;
static StoryCinematicTrace audio_trace,overlay_trace;
static void trace_cinematic(StoryCinematicTrace *trace,const char *stage,
    const SudekiMpLanStoryPresentation *f,BOOL ok,DWORD error) {
    if(trace->count>=64u || (trace->known && trace->epoch==f->epoch &&
        trace->line==f->line_serial && trace->flags==f->flags && trace->ok==ok &&
        trace->error==error)) return;
    trace->known=TRUE; trace->epoch=f->epoch; trace->line=f->line_serial;
    trace->flags=f->flags; trace->ok=ok; trace->error=error; ++trace->count;
    SudekiMpLogFormat("story_cinematic event=%s epoch=%lu line=%lu flags=%u elapsed_ms=%lu ok=%u error=%lu bars=%.4f,%.4f\r\n",
        stage,(unsigned long)f->epoch,(unsigned long)f->line_serial,(unsigned)f->flags,
        (unsigned long)f->elapsed_ms,(unsigned)ok,(unsigned long)error,
        (double)f->letterbox,(double)f->letterbox_bottom);
}
static struct {
    BOOL valid;
    SudekiMpLanPartyLease lease;
    SudekiMpLanStoryScene scene;
    uint32_t sequence,host_tick,receipt;
} presentation_sample;
/* This witness belongs to the last complete actor/world/view presentation.
 * Cosmetic particle advancement may decline independently. It must not clear
 * a valid movement witness; controller admission rechecks native ownership,
 * scene/generation and receipt freshness on its own game-thread seam. */
static BOOL effects_ready;
typedef struct StoryTiming {
    uint64_t total_us;
    uint32_t count,max_us,last_us;
} StoryTiming;
static LARGE_INTEGER timing_frequency;
static StoryTiming capture_timing,present_timing,effects_timing;
static StoryTiming world_preflight_timing,party_apply_timing,world_apply_timing;
static StoryTiming controller_timing,controller_gap_timing,cast_service_timing,control_service_timing;
static LARGE_INTEGER previous_controller_stamp;
static LARGE_INTEGER timing_begin(void) {
    LARGE_INTEGER stamp={0};
    if(timing_frequency.QuadPart>0) (void)QueryPerformanceCounter(&stamp);
    return stamp;
}
static void timing_end(StoryTiming *timing,LARGE_INTEGER start) {
    LARGE_INTEGER end;
    if(!start.QuadPart || timing_frequency.QuadPart<=0 ||
        !QueryPerformanceCounter(&end) || end.QuadPart<start.QuadPart) return;
    uint64_t us=(uint64_t)(end.QuadPart-start.QuadPart)*UINT64_C(1000000)/
        (uint64_t)timing_frequency.QuadPart;
    if(us>UINT32_MAX || timing->count==UINT32_MAX) return;
    timing->total_us+=us; ++timing->count; timing->last_us=(uint32_t)us;
    if(us>timing->max_us) timing->max_us=(uint32_t)us;
}
static unsigned long timing_mean(const StoryTiming *timing) {
    return timing->count?(unsigned long)(timing->total_us/timing->count):0ul;
}
typedef struct StoryPresentation {
    const SudekiMpLanStoryFrame *party;
    const SudekiMpLanStoryWorldFrame *world;
    BOOL resources_waiting;
    const SudekiMpLanStoryControlState *control;
    const SudekiMpLanPartyLease *connection;
    const SudekiMpLanStoryPresentation *dialogue;
} StoryPresentation;
static BOOL presentation_fresh(uint32_t now,uint32_t receipt);
static BOOL service_quick_menu(const SudekiMpLanStoryNativeRoster *,const SudekiMpLanStoryScene *,void *);

static BOOL load_finished(void) {
    SudekiMpLanStoryTaskTraceStatus tasks;
    SudekiMpStoryLoadResult load;
    return SudekiMpLanStoryTaskTraceGetStatus(&tasks) && !tasks.unknown &&
        tasks.load_generation && tasks.start_task && tasks.on_load_task &&
        tasks.start_terminal && tasks.start_retired && tasks.on_load_terminal && tasks.on_load_retired &&
        SudekiMpLanStoryLoadGetResult(&load) && load.attempt && load.native_called &&
        load.files_retired && load.state==SUDEKIMP_STORY_LOAD_RETURNED && !load.result;
}
static BOOL input_closed(void *controller) {
    return controller==client_seed.controller && client_seed.leader_character<4u &&
        SudekiMpLanStoryInputExact(controller,client_seed.actors[client_seed.leader_character]);
}
static void capture_initial_objects(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    if(initial_objects_attempted || scene->phase!=SUDEKIMP_LAN_STORY_READY || !load_finished()) return;
    initial_objects_attempted=TRUE;
    SudekiMpLanStoryNativeRoster roster;
    initial_objects_known=SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) &&
        SudekiMpLanStoryObjectsObserve((HMODULE)game_base,w,&roster,&initial_objects);
    if(initial_objects_known) initial_object_scene=*scene;
    SudekiMpLogFormat("story_objects event=initial_snapshot result=%s objects=%lu epoch=%lu policy=read_only_no_authority\r\n",
        initial_objects_known?"captured":"refused",(unsigned long)initial_objects.count,
        (unsigned long)scene->epoch);
    if(initial_objects_known && !local_seat) {
        /* Install only after the read-only baseline and on its exact native
         * thread, outside every action callback. This enables observation,
         * not client interaction or account mutation. */
        loot_trace_attempted=TRUE;
        BOOL ok=SudekiMpLanStoryLootTraceInstall((HMODULE)game_base) &&
            SudekiMpLanStoryLootTraceBind(w,&roster);
        SudekiMpLogFormat("story_loot_trace event=install ok=%u policy=observe_only\r\n",(unsigned)ok);
    }
}
static BOOL replica_exact(const SudekiMpLanStoryNativeRoster *roster,void *unused) {
    (void)unused;
    return SudekiMpLanStoryClientRosterExact(roster);
}
static BOOL cleanup_exact(const SudekiMpLanStoryNativeRoster *roster,void *unused) {
    (void)unused;
    return SudekiMpLanStoryClientCleanupRosterExact(roster);
}
static BOOL audio_present_exact(void *context) {
    return SudekiMpLanStoryClientRosterExact(context);
}
static BOOL audio_cleanup_exact(void *context) {
    return SudekiMpLanStoryClientCleanupRosterExact(context);
}
static BOOL stop_stale_audio(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)scene; (void)unused;
    return SudekiMpLanStoryCinematicAudioStop(audio_present_exact,(void *)roster);
}
static BOOL apply_catchup_seed(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *context) {
    const SudekiMpLanStoryCatchup *seed=context;
    if(!SudekiMpLanStoryCatchupValid(seed) || scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        roster->available_mask!=seed->seed_scene.available_mask ||
        scene->available_mask!=seed->seed_scene.available_mask || strcmp(scene->world,seed->seed_scene.world) ||
        strcmp(scene->temporary,seed->seed_scene.temporary) ||
        SudekiMpLanStoryWorldRecruiting() || client_recruit_committed ||
        client_local_selected || client_switch_prepared) return FALSE;
    if(!SudekiMpLanStoryWorldPrepare(roster,&seed->world,replica_exact,NULL,FALSE)) return FALSE;
    /* Bootstrap never borrows the old camera, submits audio, or publishes
     * runtime_ready. A fresh live frame supplies the current spectator view. */
    if(!SudekiMpLanStoryReplicaApply(roster,&seed->party,FALSE,replica_exact,NULL)) {
        (void)SudekiMpLanStoryWorldCancelPrepared(); return FALSE;
    }
    return SudekiMpLanStoryWorldApplyPrepared(roster,&seed->world,replica_exact,NULL);
}
static void trace_catchup(unsigned stage,const char *reason) {
    if(client_catchup_trace==stage) return;
    client_catchup_trace=stage;
    SudekiMpLogFormat("lan_story event=catchup_client stage=%u reason=%s transaction=%lu target_epoch=%lu target_revision=%lu input=closed\r\n",
        stage,reason,(unsigned long)client_catchup.transaction,
        (unsigned long)client_catchup.target.epoch,(unsigned long)client_catchup.target.revision);
}
static BOOL service_catchup_seed(const SudekiMpLanPartyPeerStatus *peer,
    const SudekiMpLanStoryScene *remote,const SudekiMpLanStoryScene *native,uint32_t now) {
    if(!client_catchup.transaction &&
        !SudekiMpLanPartyGetStoryCatchup(session,&peer->lease,now,&client_catchup)) return FALSE;
    if(!menu_lobby_known || !menu_lobby.running || menu_lobby.local_slot!=local_seat ||
        menu_lobby.admission[local_seat].sequence!=client_catchup.transaction ||
        menu_lobby.admission[local_seat].ticket!=story_lobby_tickets[local_seat]) return TRUE;
    if(client_catchup.target.revision!=remote->revision ||
        !SudekiMpLanStorySceneSame(&client_catchup.target,remote)) return FALSE;
    if(client_catchup_seeded || client_recruit_committed) return FALSE;
    if(!client_catchup_trace) trace_catchup(1u,"validated_snapshot");
    /* A late spectator joining before recruitment already uses the ordinary
     * live pair. Only the closed 4 -> 12 route needs a historical seed. */
    if(!client_catchup.recruitment.transaction) {
        if(native->available_mask!=client_catchup.seed_scene.available_mask) return TRUE;
        client_catchup_seeded=TRUE; return FALSE;
    }
    if(presented_epoch || native->available_mask!=4u ||
        strcmp(native->world,remote->world) || strcmp(native->temporary,remote->temporary)) return FALSE;
    if(!SudekiMpLanStoryClientPresent(apply_catchup_seed,&client_catchup)) {
        trace_catchup(2u,"native_seed_preflight_pending"); return TRUE;
    }
    presented_epoch=client_catchup.seed_scene.epoch;
    presented_revision=client_catchup.seed_scene.revision;
    for(unsigned c=0;c<4u;++c) presented_generations[c]=client_catchup.party.actors[c].generation;
    client_catchup_seeded=TRUE;
    trace_catchup(3u,"native_seed_bound_waiting_recruitment");
    return TRUE;
}
static BOOL apply_presented_frame(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *context) {
    StoryPresentation *frame=context;
    if(scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        !SudekiMpLanStoryWorldFrameMatches(frame->world,frame->party)) return FALSE;
    if(SudekiMpLanStoryWorldRecruiting() &&
        (!client_recruit_committed || frame->party->epoch!=client_recruit.after_epoch ||
         frame->party->revision<client_recruit.after_revision ||
         frame->party->actors[3].generation!=client_recruit.actor_generation ||
         !SudekiMpLanStoryWorldFinishRecruitment(roster,frame->world,client_recruit.after_epoch,
             replica_exact,NULL))) return FALSE;
    LARGE_INTEGER started=timing_begin();
    BOOL prepared=SudekiMpLanStoryReplicaPrepareEquipment(roster,frame->party,replica_exact,NULL) &&
        SudekiMpLanStoryWorldPrepare(roster,frame->world,replica_exact,NULL,
        client_switch_prepared || client_local_selected);
    DWORD prepare_error=GetLastError();
    timing_end(&world_preflight_timing,started);
    if(!prepared) {
        frame->resources_waiting=prepare_error==ERROR_IO_PENDING;
        return FALSE;
    }
    started=timing_begin();
    BOOL party_applied=SudekiMpLanStoryReplicaApply(roster,frame->party,
        !client_switch_prepared && !client_local_selected,replica_exact,NULL);
    timing_end(&party_apply_timing,started);
    if(!party_applied) {
        (void)SudekiMpLanStoryWorldCancelPrepared(); return FALSE;
    }
    started=timing_begin();
    BOOL world_applied=SudekiMpLanStoryWorldApplyPrepared(roster,frame->world,replica_exact,NULL);
    timing_end(&world_apply_timing,started);
    if(world_applied && client_local_selected) {
        float yaw=0,pitch=0;
        if(client_input_ready && !SudekiMpLanStoryMenuCapturesInput() &&
            !SudekiMpLanStoryQuickMenuCapturesInput() &&
            !SudekiMpLanStoryInputOrbit(roster->controller,roster->actors[client_control_fence.character],
                client_control_fence.transaction,&yaw,&pitch)) {
            client_input_ready=FALSE; SudekiMpLanStoryInputClear();
            yaw=pitch=0;
        }
        if(!SudekiMpLanStoryLocalControlPresent(roster,replica_exact,NULL,yaw,pitch)) return FALSE;
    }
    if(world_applied) {
        if(aim_pose_attempted) SudekiMpLanAimActors(roster->actors[3],NULL);
        if(frame->dialogue) {
            BOOL ok=SudekiMpLanStoryCinematicAudioPresent(frame->dialogue,audio_present_exact,(void *)roster);
            DWORD error=ok?ERROR_SUCCESS:GetLastError();
            trace_cinematic(&audio_trace,"audio",frame->dialogue,ok,error);
        }
        else if(!visible_dialogue_valid || !presentation_fresh(GetTickCount(),visible_dialogue_receipt))
            (void)SudekiMpLanStoryCinematicAudioStop(audio_present_exact,(void *)roster);
    }
    if(world_applied && frame->control && frame->connection &&
        !client_local_selected && !SudekiMpLanStoryWorldRecruiting()) {
        if(!client_switch_prepared) {
            SudekiMpLanStoryView seed; float anchor[3];
            if(!SudekiMpLanStoryReplicaViewSeed(roster,replica_exact,NULL,&seed,anchor) ||
                !SudekiMpLanStoryLocalControlSeedView(&seed,anchor)) return FALSE;
            if(!SudekiMpLanStoryReplicaRestoreView(roster,replica_exact,NULL)) return FALSE;
        }
        /* A fresh host offer can replace PREPARE without an intervening
         * observed revocation. The recruited world/actor is already proved
         * by this paired frame. Retain its seeded view and refresh only the
         * plain transport fence; never borrow a second spectator view. */
        client_control_fence=frame->control->fence;
        client_control_connection=*frame->connection;
        client_switch_prepared=TRUE;
    }
    return world_applied;
}
static BOOL presentation_fresh(uint32_t now,uint32_t receipt) {
    int32_t age=(int32_t)(now-receipt); return age>=-16 && age<=250;
}
static uint32_t older_receipt(uint32_t a,uint32_t b) {
    return (int32_t)(a-b)<=0?a:b;
}
static uint32_t later_receipt(uint32_t a,uint32_t b) {
    return (int32_t)(a-b)>=0?a:b;
}
static unsigned interpolation_delay(const unsigned *party_indices,
    const unsigned *world_indices,unsigned count,uint32_t now) {
    /* A complete pair arriving at a steady <=46ms cadence fits a50ms
     * interpolation window, including4ms of headroom. Keep the established
     * 66ms window for startup, missing sequences or uneven delivery. These
     * are already validated plain history copies, never native owners. */
    if(count<4u) return 66u;
    unsigned first=count>6u?count-6u:0u;
    for(unsigned n=first+1u;n<count;++n) {
        unsigned a=party_indices[n-1u],b=party_indices[n];
        uint32_t received_a=later_receipt(history_receipts[a],world_receipts[world_indices[n-1u]]);
        uint32_t received_b=later_receipt(history_receipts[b],world_receipts[world_indices[n]]);
        int32_t host_step=(int32_t)(history[b].host_tick-history[a].host_tick);
        int32_t received_step=(int32_t)(received_b-received_a);
        uint32_t skew_a=received_a-older_receipt(history_receipts[a],world_receipts[world_indices[n-1u]]);
        uint32_t skew_b=received_b-older_receipt(history_receipts[b],world_receipts[world_indices[n]]);
        if((uint32_t)(history[b].sequence-history[a].sequence)!=1u ||
            host_step<16 || host_step>46 || received_step<16 || received_step>46 ||
            received_step-host_step < -8 || received_step-host_step > 8 ||
            skew_a>8u || skew_b>8u ||
            (unsigned)(host_step>received_step?host_step:received_step)+
                (skew_a>skew_b?skew_a:skew_b)+4u>50u) return 66u;
    }
    unsigned last=count-1u;
    uint32_t complete=later_receipt(history_receipts[party_indices[last]],
        world_receipts[world_indices[last]]);
    int32_t age=(int32_t)(now-complete);
    return age>=0 && age<=50?50u:66u;
}
/* Client: the host scene's area for this player's own character must equal
 * the client's frozen native area; other characters may be elsewhere. */
static BOOL client_area_matches(const SudekiMpLanStoryScene *native,const SudekiMpLanStoryScene *remote) {
    unsigned c=session?SudekiMpLanPartyLocalCharacter(session):4u;
    if(c<4u && remote->phase==SUDEKIMP_LAN_STORY_READY && (remote->available_mask&(1u<<c)))
        return SudekiMpLanStorySceneCharacterAreaMatches(remote,c,native->world,native->temporary);
    return !strcmp(native->world,remote->world) && !strcmp(native->temporary,remote->temporary);
}
static uint8_t client_foreign_characters(const SudekiMpLanStoryScene *remote) {
    unsigned c=session?SudekiMpLanPartyLocalCharacter(session):4u; uint8_t mask=0;
    if(c>=4u) return 0;
    for(unsigned o=0;o<4u;++o) if((remote->available_mask&(1u<<o)) && !SudekiMpLanStorySceneSameArea(remote,c,o))
        mask|=(uint8_t)(1u<<o);
    return mask;
}
static BOOL current_presentation(const SudekiMpLanStoryScene *native,SudekiMpLanStoryScene *remote) {
    SudekiMpLanPartyPeerStatus peer;
    uint32_t now=GetTickCount();
    if(!effects_ready || !presentation_sample.valid || presentation_changed || !session || !saved_profile || !local_seat ||
        InterlockedCompareExchange(&stopping,0,0) ||
        !presentation_fresh(now,presentation_sample.receipt) ||
        !SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
        peer.lease.token!=presentation_sample.lease.token ||
        peer.lease.generation!=presentation_sample.lease.generation ||
        peer.lease.seat!=presentation_sample.lease.seat ||
        !SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,remote) ||
        remote->revision!=presentation_sample.scene.revision ||
        !SudekiMpLanStorySceneSame(&presentation_sample.scene,remote) ||
        remote->phase!=SUDEKIMP_LAN_STORY_READY || native->phase!=SUDEKIMP_LAN_STORY_READY ||
        !client_area_matches(native,remote) ||
        native->available_mask!=remote->available_mask) return FALSE;
    return TRUE;
}
static BOOL advance_effects(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *unused) {
    (void)unused; SudekiMpLanStoryScene remote;
    if(!current_presentation(native,&remote)) return FALSE;
    return SudekiMpLanStoryEffectsAdvance(roster,remote.epoch,remote.revision,
        presentation_sample.sequence,presentation_sample.host_tick,presentation_sample.receipt,replica_exact,NULL);
}
typedef struct StoryAimObservation { void *actor;float direction[3]; } StoryAimObservation;
static BOOL observe_story_aim(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *context) {
    StoryAimObservation *observation=context;SudekiMpLanStoryScene remote;
    return current_presentation(native,&remote) &&
        SudekiMpLanStoryWorldAimDirection(roster,observation->actor,replica_exact,NULL,observation->direction);
}
static BOOL story_aim_witness(void *actor,BOOL projectile,float direction[3]) {
    if(projectile || !aim_pose_attempted || !runtime_thread ||
        GetCurrentThreadId()!=runtime_thread || !effects_ready || !presentation_sample.valid) return FALSE;
    StoryAimObservation observation={.actor=actor};
    if(!SudekiMpLanStoryClientEffectsPresent(observe_story_aim,&observation,
        SudekiMpLanAimPoseWitness,NULL)) return FALSE;
    memcpy(direction,observation.direction,sizeof(observation.direction));return TRUE;
}
static void present(void);
static void render_present(void *unused) { (void)unused; present(); }
static void invalidate_presentation(void) {
    presentation_sample.valid=effects_ready=FALSE; visible_dialogue_valid=FALSE;
    client_input_ready=FALSE;
    if(input_attempted) SudekiMpLanStoryInputClear();
    SudekiMpLanStoryEffectsResetClock();
    InterlockedExchange(&runtime_ready,0);
}
static void render_dispatch(void *unused) {
    (void)unused;
    if(!session || !saved_profile || !local_seat || !runtime_thread ||
        runtime_thread!=GetCurrentThreadId()) return;
    presentation_attempted=TRUE;
    if(InterlockedCompareExchange(&stopping,0,0) ||
        !SudekiMpLanStoryClientRenderDispatch(render_present,NULL,
            SudekiMpLanStoryRenderWitness,NULL)) invalidate_presentation();
}
static void effects_dispatch(void *unused) {
    (void)unused;
    LARGE_INTEGER start=timing_begin();
    BOOL attempted=effects_attempted && effects_ready && presentation_sample.valid;
    if(!effects_attempted || !effects_ready || !presentation_sample.valid ||
        !SudekiMpLanStoryClientEffectsPresent(advance_effects,NULL,
            SudekiMpLanStoryEffectsWitness,NULL)) {
        effects_ready=FALSE;
        SudekiMpLanStoryEffectsResetClock();
    }
    if(attempted) timing_end(&effects_timing,start);
}
static int world_for_party(const SudekiMpLanStoryFrame *party,uint32_t now) {
    for(unsigned i=0;i<world_history_count;++i)
        if(presentation_fresh(now,world_receipts[i]) &&
            SudekiMpLanStoryWorldFrameMatches(&world_history[i],party)) return (int)i;
    return -1;
}
static int dialogue_for_party(const SudekiMpLanStoryFrame *party,uint32_t now) {
    for(unsigned i=0;i<dialogue_count;++i)
        if(presentation_fresh(now,dialogue_receipts[i]) &&
            SudekiMpLanStoryPresentationMatches(&dialogue_history[i],party)) return (int)i;
    return -1;
}
static void refresh_menu(void) {
    BOOL scene_current=menu_scene_known;
    if(scene_current && !local_seat) {
        int32_t age=(int32_t)(GetTickCount()-menu_scene.observed_tick);
        scene_current=age>=0 && age<=250;
    }
    SudekiMpLobbyStatus display=menu_lobby;
    SudekiMpLanPartyPresence presence;
    if(SudekiMpLanPartyGetPresence(session,&presence)) for(unsigned p=0;p<4u;++p) {
        display.members[p].character=presence.ownership.assignment.character[p];
        display.members[p].locked=display.members[p].character<4u;
        display.members[p].reserved=!!(presence.ownership.assignment.humans&(1u<<p));
    }
    if(menu_initialized && menu_lobby_known)
        SudekiMpLanStoryMenuRefresh(&display,
            scene_current?menu_scene.leader_seat:SUDEKIMP_LAN_STORY_NO_SEAT,
            scene_current?menu_scene.available_mask:0u,
            scene_current && menu_scene.phase==SUDEKIMP_LAN_STORY_READY,
            !local_seat && host_binding_ready && !InterlockedCompareExchange(&stopping,0,0),
            local_seat && InterlockedCompareExchange(&runtime_ready,0,0) &&
                !InterlockedCompareExchange(&stopping,0,0),
            local_seat && client_input_ready && !InterlockedCompareExchange(&stopping,0,0));
}
static BOOL menu_admission(void) { return saved_profile && session; }
static void menu_toggle(void) {
    if(InterlockedCompareExchange(&stopping,0,0)) return;
    refresh_menu();
    SudekiMpLanStoryMenuToggle();
}
static BOOL retire_client(void) {
    SudekiMpLanStoryClientReport report;
    SudekiMpLanStoryInputClear(); client_input_ready=FALSE;
    if(quick_menu_attempted && SudekiMpLanStoryQuickMenuCapturesInput() &&
        !SudekiMpLanStoryClientPresent(service_quick_menu,NULL)) return FALSE;
    if(SudekiMpLanStoryClientRecruiting()) {
        SudekiMpLanStoryRecruitReport recruitment;
        (void)SudekiMpLanStoryClientRecruitService(&recruitment);
        return FALSE; /* Native async completion is committed on its normal dispatch. */
    }
    if(SudekiMpLanStoryCinematicAudioRetains()) {
        SudekiMpLanStoryNativeRoster roster; SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene) ||
            !SudekiMpLanStoryCinematicAudioStop(audio_cleanup_exact,&roster)) return FALSE;
    }
    if(SudekiMpLanStoryReplicaRetainsView()) {
        SudekiMpLanStoryNativeRoster roster; SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene) ||
            !SudekiMpLanStoryReplicaRestoreView(&roster,cleanup_exact,NULL)) return FALSE;
    }
    return SudekiMpLanStoryClientPrepareExit(&report);
}
static void present(void) {
    if(!session || !saved_profile || !local_seat || !runtime_thread ||
        GetCurrentThreadId()!=runtime_thread) return;
    if(InterlockedCompareExchange(&stopping,0,0)) {
        invalidate_presentation(); return;
    }
    SudekiMpLanStoryClientReport report={0};
    /* This dispatcher owns presentation only. Pause acquisition and shutdown
     * preparation remain on the original MenuNative callback. */
    BOOL contained=SudekiMpLanStoryClientService(&report);
    DWORD now=GetTickCount();
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanStoryScene remote,native;
    SudekiMpLanStoryNativeRoster roster;
    unsigned result=0,buffer_ms=66u;
    if(!contained || !SudekiMpLanStoryClientPausedRoster(&roster,&native)) goto finish;
    if(!SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
        !SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote)) {
        history_count=world_history_count=0; goto finish;
    }
    menu_scene=remote; menu_scene_known=TRUE;
    if(history_lease.token!=peer.lease.token || history_lease.generation!=peer.lease.generation ||
        history_lease.seat!=peer.lease.seat) {
        if(presented_epoch) presentation_changed=TRUE;
        history_count=world_history_count=0; history_lease=peer.lease;
    }
    if(service_catchup_seed(&peer,&remote,&native,now)) { result=8u; goto finish; }
    if(presented_epoch && remote.epoch!=presented_epoch) presentation_changed=TRUE;
    if(presentation_changed) { result=5; goto finish; }
    if(remote.phase!=SUDEKIMP_LAN_STORY_READY || !client_area_matches(&native,&remote) ||
        native.available_mask!=remote.available_mask) {
        history_count=world_history_count=0; result=1; goto finish;
    }
    SudekiMpLanStoryFrame next; uint32_t receipt;
    for(unsigned n=0;n<8u && SudekiMpLanPartyPopStoryFrameReceived(session,&peer.lease,now,&next,&receipt);++n) {
        int32_t age=(int32_t)(now-receipt);
        if(age < -16 || age>250) continue;
        if(presented_epoch) {
            if(next.epoch!=presented_epoch) presentation_changed=TRUE;
            for(unsigned c=0;c<4u;++c)
                if(next.actors[c].generation!=presented_generations[c]) presentation_changed=TRUE;
            if(presentation_changed) { history_count=0; result=5; goto finish; }
        }
        if(history_count && (next.epoch!=history[history_count-1u].epoch ||
            next.revision!=history[history_count-1u].revision)) history_count=0;
        if(history_count==STORY_HISTORY_CAPACITY) {
            memmove(history,history+1,(STORY_HISTORY_CAPACITY-1u)*sizeof(*history));
            memmove(history_receipts,history_receipts+1,
                (STORY_HISTORY_CAPACITY-1u)*sizeof(*history_receipts));
            --history_count;
        }
        history_receipts[history_count]=receipt;
        history[history_count++]=next; last_received_frame=receipt; ++received_frames;
    }
    SudekiMpLanStoryWorldFrame world_next;
    for(unsigned n=0;n<STORY_HISTORY_CAPACITY &&
        SudekiMpLanPartyPopStoryWorld(session,&peer.lease,now,&world_next,&receipt);++n) {
        if(!presentation_fresh(now,receipt)) continue;
        if(world_history_count && (world_next.epoch!=world_history[world_history_count-1u].epoch ||
            world_next.revision!=world_history[world_history_count-1u].revision)) world_history_count=0;
        if(world_history_count==STORY_HISTORY_CAPACITY) {
            memmove(world_history,world_history+1,(STORY_HISTORY_CAPACITY-1u)*sizeof(*world_history));
            memmove(world_receipts,world_receipts+1,(STORY_HISTORY_CAPACITY-1u)*sizeof(*world_receipts));
            --world_history_count;
        }
        world_receipts[world_history_count]=receipt; world_history[world_history_count++]=world_next;
    }
    SudekiMpLanStoryPresentation dialogue;
    for(unsigned n=0;n<STORY_HISTORY_CAPACITY &&
        SudekiMpLanPartyPopStoryPresentation(session,&peer.lease,now,&dialogue,&receipt);++n) {
        if(!presentation_fresh(now,receipt)) continue;
        if(dialogue_count && (dialogue.epoch!=dialogue_history[dialogue_count-1u].epoch ||
            dialogue.revision!=dialogue_history[dialogue_count-1u].revision)) dialogue_count=0;
        if(dialogue_count==STORY_HISTORY_CAPACITY) {
            memmove(dialogue_history,dialogue_history+1,(STORY_HISTORY_CAPACITY-1u)*sizeof(*dialogue_history));
            memmove(dialogue_receipts,dialogue_receipts+1,(STORY_HISTORY_CAPACITY-1u)*sizeof(*dialogue_receipts));
            --dialogue_count;
        }
        dialogue_history[dialogue_count]=dialogue; dialogue_receipts[dialogue_count++]=receipt;
    }
    /* Present one complete party/NPC observation. Missing datagrams cannot
     * attach an old NPC batch to a newer party frame or refresh its age. */
    unsigned matched[STORY_HISTORY_CAPACITY],matched_world[STORY_HISTORY_CAPACITY],matches=0;
    for(unsigned i=0;i<history_count;++i) {
        if(!presentation_fresh(now,history_receipts[i]) ||
            !SudekiMpLanStoryFrameMatchesScene(&history[i],&remote)) continue;
        int world=world_for_party(&history[i],now);
        if(world<0) continue;
        matched[matches]=i; matched_world[matches++]=(unsigned)world;
    }
    if(!matches) { result=6; goto finish; }
    unsigned newest=matched[matches-1u];
    unsigned dialogue_party_index=newest;
    SudekiMpLanStoryFrame sample=history[newest];
    unsigned world_index=matched_world[matches-1u];
    SudekiMpLanStoryWorldFrame world_sample=world_history[world_index];
    uint32_t sample_receipt=older_receipt(history_receipts[newest],world_receipts[world_index]);
    int32_t received_age=(int32_t)(now-history_receipts[newest]);
    if(matches>1u) {
        BOOL same_timeline=presentation_sample.valid &&
            presentation_sample.lease.token==peer.lease.token &&
            presentation_sample.lease.generation==peer.lease.generation &&
            presentation_sample.lease.seat==peer.lease.seat &&
            presentation_sample.scene.revision==remote.revision &&
            SudekiMpLanStorySceneSame(&presentation_sample.scene,&remote) &&
            presentation_fresh(now,presentation_sample.receipt);
        buffer_ms=interpolation_delay(matched,matched_world,matches,now);
        /* Approach the shorter delay one millisecond per successful frame,
         * so a healthy stream cannot suddenly jump16ms ahead. */
        if(buffer_ms==50u && (!same_timeline || presentation_buffer_ms>50u))
            buffer_ms=same_timeline?presentation_buffer_ms-1u:66u;
        uint32_t tick=sample.host_tick+(received_age>0?(uint32_t)received_age:0u)-buffer_ms;
        /* Growing the buffer must not rewind an already displayed pose.
         * Retain only its plain timestamp for the same fresh scene/lease;
         * all actor/world/input validation below still runs in full. */
        if(same_timeline &&
            (int32_t)(presentation_sample.host_tick-tick)>0 &&
            (int32_t)(sample.host_tick-presentation_sample.host_tick)>=0)
            tick=presentation_sample.host_tick;
        if((int32_t)(tick-history[matched[0]].host_tick)<0) {
            dialogue_party_index=matched[0];
            sample=history[matched[0]]; world_sample=world_history[matched_world[0]];
            sample_receipt=older_receipt(history_receipts[matched[0]],
                world_receipts[matched_world[0]]);
        } else for(unsigned n=1;n<matches;++n) {
            unsigned lower=matched[n-1u],upper=matched[n];
            if((int32_t)(tick-history[upper].host_tick)>=0) continue;
            if(!SudekiMpLanStoryFrameInterpolate(&history[lower],&history[upper],tick,&sample) ||
                !SudekiMpLanStoryWorldFrameInterpolate(&world_history[matched_world[n-1u]],
                    &world_history[matched_world[n]],tick,&world_sample)) goto finish;
            dialogue_party_index=lower;
            sample_receipt=older_receipt(older_receipt(history_receipts[lower],history_receipts[upper]),
                older_receipt(world_receipts[matched_world[n-1u]],
                    world_receipts[matched_world[n]]));
            break;
        }
    }
    /* An established independent camera does not consume the host camera.
     * Its own fresh native owner/geometry proof still runs in LocalControlPresent.
     * Spectators and initial camera handoff continue to require a host view. */
    if(!SudekiMpLanStoryViewUsable(&sample.view,client_local_selected)) { result=2; goto finish; }
    /* The first presentation binds the remote native actors to this exact
     * loaded local world. A replacement requires a fresh load/admission;
     * resetting interpolation history alone cannot grant a new native lease. */
    if(!presented_epoch) {
        presented_epoch=sample.epoch;
        for(unsigned c=0;c<4u;++c) presented_generations[c]=sample.actors[c].generation;
    }
    SudekiMpLanStoryControlState control;
    unsigned chosen=SudekiMpLanPartyLocalCharacter(session);
    BOOL control_offered=local_control_attempted && chosen<4u &&
        (sample.available_mask&(1u<<chosen)) &&
        SudekiMpLanPartyGetStoryControl(session,&peer.lease,now,&control) &&
        (control.phase==SUDEKIMP_STORY_CONTROL_PREPARE || control.phase==SUDEKIMP_STORY_CONTROL_READY) &&
        SudekiMpLanStoryControlMatchesScene(&control.fence,&remote) &&
        control.fence.character==chosen &&
        control.fence.actor_generation==sample.actors[chosen].generation;
    int dialogue_index=dialogue_for_party(&history[dialogue_party_index],now);
    StoryPresentation presented={&sample,&world_sample,FALSE,
        control_offered?&control:NULL,control_offered?&peer.lease:NULL,
        dialogue_index>=0?&dialogue_history[dialogue_index]:NULL};
    LARGE_INTEGER present_start=timing_begin();
    {
        uint8_t foreign=client_foreign_characters(&remote);
        SudekiMpLanStoryReplicaSetForeignCharacters(foreign);
        SudekiMpLanStoryWorldSetForeignCharacters(foreign);
        if(foreign!=traced_foreign) {
            traced_foreign=foreign;
            SudekiMpLogFormat("lan_story event=client_area foreign=%u inside=%u temporary=%s epoch=%lu revision=%lu\r\n",
                foreign,remote.inside_mask,remote.temporary,(unsigned long)remote.epoch,(unsigned long)remote.revision);
        }
    }
    result=SudekiMpLanStoryClientPresent(apply_presented_frame,&presented)?4u:3u;
    if(result==3u && presented.resources_waiting && SudekiMpLanStoryClientService(&report)) result=7u;
    timing_end(&present_timing,present_start);
    {
        /* Research hitch detector: a slow present or a long gap between two
         * presents (native side stalling) with the phase costs of that frame. */
        static LARGE_INTEGER previous_present; static unsigned hitch_logs;
        LARGE_INTEGER end_stamp=timing_begin(); uint64_t gap_us=0;
        if(previous_present.QuadPart && timing_frequency.QuadPart>0 && end_stamp.QuadPart>previous_present.QuadPart)
            gap_us=(uint64_t)(end_stamp.QuadPart-previous_present.QuadPart)*UINT64_C(1000000)/(uint64_t)timing_frequency.QuadPart;
        previous_present=end_stamp;
        if((present_timing.last_us>50000u || gap_us>90000u) && hitch_logs<200u) {
            ++hitch_logs;
            SudekiMpLogFormat("lan_story event=hitch gap_us=%lu present_us=%lu preflight_us=%lu party_us=%lu world_us=%lu effects_last_us=%lu result=%u received=%lu\r\n",
                (unsigned long)gap_us,(unsigned long)present_timing.last_us,(unsigned long)world_preflight_timing.last_us,
                (unsigned long)party_apply_timing.last_us,(unsigned long)world_apply_timing.last_us,
                (unsigned long)effects_timing.last_us,result,(unsigned long)received_frames);
        }
    }
    if(result==4u) {
        presentation_buffer_ms=buffer_ms;
        presented_revision=sample.revision;
        /* Discrete line state belongs to an actual received observation.
         * Interpolation changes host_tick but cannot invent a speech event. */
        visible_dialogue_valid=dialogue_index>=0;
        if(visible_dialogue_valid) {
            visible_dialogue=dialogue_history[dialogue_index];
            visible_dialogue_receipt=dialogue_receipts[dialogue_index];
        }
        presentation_sample.lease=peer.lease; presentation_sample.scene=remote;
        presentation_sample.sequence=sample.sequence; presentation_sample.host_tick=sample.host_tick;
        presentation_sample.receipt=sample_receipt; presentation_sample.valid=effects_ready=TRUE;
        if(client_catchup.transaction && client_catchup.target.revision==remote.revision &&
            SudekiMpLanStorySceneSame(&client_catchup.target,&remote) &&
            (!menu_lobby_known || menu_lobby.admission[local_seat].phase!=SUDEKIMP_LOBBY_ADMISSION_COMPLETE) &&
            (!client_catchup.recruitment.transaction || (client_recruit_committed &&
             !SudekiMpLanStoryWorldRecruiting() &&
             sample.actors[3].generation==client_catchup.recruitment.actor_generation)) &&
            (!client_catchup_ack_at || (uint32_t)(now-client_catchup_ack_at)>=100u) &&
            SudekiMpLanPartyAcknowledgeStoryCatchup(session,&peer.lease,
                client_catchup.transaction,sample.sequence)) {
            client_catchup_ack_at=now; trace_catchup(4u,"fresh_world_presented_ack_sent");
        }
    }
finish:
    if(result!=4u) { presentation_buffer_ms=66u; presentation_sample.valid=effects_ready=FALSE; visible_dialogue_valid=FALSE; SudekiMpLanStoryEffectsResetClock(); }
    InterlockedExchange(&runtime_ready,result==4u);
    if(result!=last_present_result || !last_present_trace || now-last_present_trace>=1000u) {
        const char *reason=result==8u?"catchup_seed_pending":result==7u?"waiting_for_world_resources":result==6u?"waiting_for_matching_party_world":
            result==5u?"native_presentation_changed":result==4u?"presented":
            result==3u?"native_presentation_refused":result==2u?"view_unavailable":
            result==1u?"scene_not_matched":"not_ready";
        SudekiMpLogFormat("lan_story event=spectator state=%u reason=%s pause=%u roster=%u scheduler=%u entities=%u received=%lu replica_applied=%u party_buffered=%u world_buffered=%u apply_samples=%lu apply_mean_us=%lu apply_max_us=%lu effects_samples=%lu effects_mean_us=%lu effects_max_us=%lu input_admitted=%u local_selected=%u control_phase=%u buffer_ms=%u\r\n",
            result,reason,report.pause_owned,report.roster_exact,report.scheduler_unchanged,report.entities_unchanged,
            (unsigned long)received_frames,result==4u,history_count,world_history_count,
            (unsigned long)present_timing.count,timing_mean(&present_timing),(unsigned long)present_timing.max_us,
            (unsigned long)effects_timing.count,timing_mean(&effects_timing),(unsigned long)effects_timing.max_us,
            client_input_ready?1u:0u,client_local_selected?1u:0u,client_control_trace_state,buffer_ms);
        if(present_timing.count) SudekiMpLogFormat(
            "lan_story event=presentation_cost preflight_samples=%lu preflight_mean_us=%lu preflight_max_us=%lu party_samples=%lu party_mean_us=%lu party_max_us=%lu world_samples=%lu world_mean_us=%lu world_max_us=%lu\r\n",
            (unsigned long)world_preflight_timing.count,timing_mean(&world_preflight_timing),
            (unsigned long)world_preflight_timing.max_us,(unsigned long)party_apply_timing.count,
            timing_mean(&party_apply_timing),(unsigned long)party_apply_timing.max_us,
            (unsigned long)world_apply_timing.count,timing_mean(&world_apply_timing),
            (unsigned long)world_apply_timing.max_us);
        memset(&present_timing,0,sizeof(present_timing));
        memset(&effects_timing,0,sizeof(effects_timing));
        memset(&world_preflight_timing,0,sizeof(world_preflight_timing));
        memset(&party_apply_timing,0,sizeof(party_apply_timing));
        memset(&world_apply_timing,0,sizeof(world_apply_timing));
        last_present_result=result; last_present_trace=now;
    }
}
static BOOL same_connection(const SudekiMpLanPartyLease *a,const SudekiMpLanPartyLease *b) {
    return a->token && a->generation && a->token==b->token &&
        a->generation==b->generation && a->seat==b->seat;
}
static void observe_recruitment(const SudekiMpLanStoryScene *scene,const SudekiMpLanStoryFrame *frame,
    void *controller,const SudekiMpControlUpdateDispatchWitness *w) {
    if(scene->available_mask==4u && scene->leader_seat==2u) {
        host_before_recruit=*scene; return;
    }
    /* The journal proves creation once. Later unrelated tasks may replace
     * its last-entry observation. Retain that proof only for this exact scene
     * and actor generation; a fresh native frame renews its publication. */
    if(host_recruit.transaction) {
        if(host_binding_ready && SudekiMpLanStoryRecruitmentMatchesScene(&host_recruit,scene) &&
            host_recruit.actor_generation==frame->actors[3].generation) {
            host_recruit.observed_tick=frame->host_tick;
            (void)SudekiMpLanPartyPublishStoryRecruitment(session,&host_recruit);
        }
        return;
    }
    SudekiMpLanStoryRecruitmentStatus recruitment; SudekiMpLanStorySpawnObservation spawn;
    SudekiMpLanStoryNativeRoster roster;
    if(scene->available_mask!=12u || scene->leader_seat!=2u || !host_binding_ready ||
        !host_before_recruit.epoch || strcmp(scene->world,host_before_recruit.world) ||
        strcmp(scene->temporary,host_before_recruit.temporary) ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) ||
        !SudekiMpLanStoryTaskTraceGetRecruitmentStatus(&recruitment) || recruitment.unknown ||
        !recruitment.terminal || !recruitment.retired || recruitment.hash!=0x4fb91e97u ||
        !recruitment.party_add_exact || recruitment.party_add_count!=1u ||
        recruitment.party_add_before!=2u || recruitment.party_add_after!=2u ||
        recruitment.added_actor!=roster.actors[3] ||
        !SudekiMpLanStoryTaskTraceGetSpawnObservation(&spawn) || spawn.unknown ||
        spawn.load_generation!=recruitment.load_generation || spawn.task!=recruitment.task ||
        !spawn.construction_exact || !spawn.completion_returned || !spawn.group_member_exact ||
        !spawn.party_add_matches || !spawn.job_destructor_returned || !spawn.job_storage_released ||
        spawn.actor!=roster.actors[3] || !spawn.actor_resource.exact || !spawn.placement_resource.exact ||
        !spawn.actor_resource.text_known || !spawn.placement_resource.text_known ||
        (spawn.actor_resource.encoded_kind&0x1fffu)!=0xf81u || spawn.actor_resource.identifier!=0x8557d453u ||
        strcmp(spawn.actor_resource.text,"PC_AILISH") ||
        (spawn.placement_resource.encoded_kind&0x1fffu)!=0xfb5u || spawn.placement_resource.identifier!=0x3d608dc8u ||
        strcmp(spawn.placement_resource.text,"TSA_AILISH_LIGHTHOUSE")) return;
    if(!host_recruit.transaction) {
        host_recruit=(SudekiMpLanStoryRecruitment){1u,1u,host_before_recruit.epoch,
            host_before_recruit.revision,scene->epoch,scene->revision,frame->actors[3].generation,frame->host_tick};
    }
    if(SudekiMpLanStoryRecruitmentMatchesScene(&host_recruit,scene) &&
        host_recruit.actor_generation==frame->actors[3].generation) {
        host_recruit.observed_tick=GetTickCount();
        (void)SudekiMpLanPartyPublishStoryRecruitment(session,&host_recruit);
    }
}
static BOOL drain_story_controls(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    BOOL complete=TRUE;
    SudekiMpLanStoryNativeRoster roster; memset(&roster,0,sizeof(roster));
    BOOL known=SudekiMpLanStoryObserverRoster(controller,w,scene,&roster);
    if(activity_attempted && SudekiMpLanStoryActivityRetains() &&
        (!known || !SudekiMpLanStoryActivityService(w,&roster,0u))) return FALSE;
    for(unsigned p=1;p<4u;++p) if(host_control[p].native_key.token) {
        host_control[p].draining=TRUE;
        host_control[p].ready=FALSE;
        (void)SudekiMpLanPartyRevokeStoryControl(session,&host_control[p].connection);
        if(!known || !SudekiMpLanStoryControlDrain(w,&roster,&host_control[p].native_key)) {
            complete=FALSE; continue;
        }
        uint32_t transaction=host_control[p].transaction;
        memset(&host_control[p],0,sizeof(host_control[p])); host_control[p].transaction=transaction;
    }
    return complete && !SudekiMpLanStoryControlRetains();
}
static BOOL drain_story_casts(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    if(!cast_attempted) return TRUE;
    SudekiMpLanStoryCastRequestStop();
    SudekiMpLanStoryNativeRoster roster;
    if(SudekiMpLanStoryCastRetains() &&
        (!SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) ||
         !SudekiMpLanStoryCastService(w,&roster))) return FALSE;
    if(!SudekiMpLanStoryCastUninstall()) return FALSE;
    cast_attempted=FALSE; return TRUE;
}
/* The pure ownership policy reserves character identities. Story actor leases
 * and the existing PREPARE/ACK/READY exchange independently prove native
 * ownership and the client's presented camera. Never infer either from this
 * table or from a lobby lock. All calls here are on the native thread. */
static BOOL publish_story_ownership(void) {
    if(local_seat || !story_policy_initialized) return FALSE;
    BOOL changed=!story_published.sequence ||
        memcmp(&story_published.ownership,&story_presence.ownership,sizeof(story_presence.ownership)) ||
        memcmp(story_published.ack_request,story_presence.ack_request,sizeof(story_presence.ack_request)) ||
        memcmp(story_published.ack_result,story_presence.ack_result,sizeof(story_presence.ack_result));
    if(changed) {
        if(story_presence.sequence==UINT32_MAX) return FALSE;
        ++story_presence.sequence;
    }
    story_presence.observed_tick=GetTickCount();
    if(!SudekiMpLanPartyPublishPresence(session,&story_presence)) return FALSE;
    if(changed && story_swap_traces<96u) {
        ++story_swap_traces;
        const SudekiMpPartyOwnership *o=&story_presence.ownership;
        SudekiMpLogFormat("lan_story_swap event=ownership revision=%lu phase=%u player=%u target=%u characters=%u,%u,%u,%u available=%u connected=%u controlling=%u\r\n",
            (unsigned long)o->assignment.revision,o->phase,o->player,o->target,
            o->assignment.character[0],o->assignment.character[1],o->assignment.character[2],
            o->assignment.character[3],o->assignment.available,o->connected,o->controlling);
    }
    story_published=story_presence; return TRUE;
}
static BOOL story_swap_blocks(unsigned player) {
    const SudekiMpPartyOwnership *o=&story_presence.ownership;
    return story_policy_initialized && o->phase==SUDEKIMP_PARTY_SWAP_HANDOFF &&
        (!o->player || o->player==player);
}
static void story_control_confirmed(unsigned p) {
    SudekiMpPartyOwnership *o=&story_presence.ownership;
    unsigned c=o->assignment.character[p];
    if(c>=4u || !(o->connected&(1u<<p))) return;
    uint32_t world=o->assignment.world;
    if(o->control[p]==SUDEKIMP_PARTY_CONTROL_AI) {
        uint32_t generation=o->assignment.generation[c];
        if(!SudekiMpPartyControlAcquireBegin(o,p,world,o->assignment.revision,generation)) return;
        if(!SudekiMpPartyControlAcquireCommit(o,p,world,generation)) return;
    }
    if(o->control[p]!=SUDEKIMP_PARTY_CONTROL_WAIT_ACK) return;
    if(o->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && o->player==p)
        (void)SudekiMpPartySwapAcknowledge(o,p,o->request,world,o->assignment.revision);
    else (void)SudekiMpPartyControlAcknowledge(o,p,world,o->assignment.revision,
        o->assignment.generation[c]);
}
static void story_control_released(unsigned p) {
    SudekiMpPartyOwnership *o=&story_presence.ownership;
    if(o->phase!=SUDEKIMP_PARTY_SWAP_IDLE && o->player==p) return;
    unsigned c=o->assignment.character[p];
    if(c>=4u) return;
    if(o->control[p]==SUDEKIMP_PARTY_CONTROL_HUMAN || o->control[p]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK)
        o->control[p]=SUDEKIMP_PARTY_CONTROL_DRAINING;
    if(o->control[p]==SUDEKIMP_PARTY_CONTROL_DRAINING)
        (void)SudekiMpPartyControlDrainComplete(o,p,o->assignment.world,o->assignment.generation[c]);
}
static void service_story_ownership(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    if(!story_policy_initialized || local_seat) return;
    SudekiMpPartyOwnership *o=&story_presence.ownership;
    SudekiMpLanStoryNativeRoster roster;
    if(!SudekiMpLanStoryObserverRoster(controller,w,scene,&roster)) return;
    if(o->phase==SUDEKIMP_PARTY_SWAP_IDLE && o->assignment.available!=roster.available_mask &&
        o->assignment.revision<UINT32_MAX) {
        SudekiMpPartyOwnership candidate=*o;
        candidate.assignment.available=roster.available_mask; ++candidate.assignment.revision;
        if(SudekiMpPartyOwnershipValid(&candidate)) *o=candidate;
    }
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer={0};
        if(!SudekiMpLanPartyPeerStatusGet(session,p,&peer)) continue;
        BOOL live=peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed;
        if(live && !(o->connected&(1u<<p))) (void)SudekiMpPartyOwnershipConnect(o,p);
        /* A launch ticket that has not connected yet is not a departure. */
        if(!live && (o->connected&(1u<<p))) (void)SudekiMpPartyOwnershipDisconnect(o,p);
        if(host_control[p].ready && !host_control[p].draining && !story_swap_blocks(p))
            story_control_confirmed(p);
        else if(!host_control[p].native_key.token) story_control_released(p);
    }
    if(host_binding_ready && !story_swap_blocks(0u)) story_control_confirmed(0u);
    SudekiMpLanPartyCommand command;
    for(unsigned n=0;n<4u && SudekiMpLanPartyTakeCommand(session,&command);++n) {
        unsigned p=command.player,c=o->assignment.character[p];
        SudekiMpLanPartyPeerStatus peer={0};
        BOOL sender=!p?(!command.lease.token && !command.lease.generation):
            SudekiMpLanPartyPeerStatusGet(session,p,&peer) && peer.transport_confirmed &&
            peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && same_connection(&peer.lease,&command.lease);
        SudekiMpPartySwapResult result=SUDEKIMP_PARTY_SWAP_UNAUTHORIZED;
        if(sender && command.kind==SUDEKIMP_LAN_PARTY_COMMAND_SWAP && (o->connected&(1u<<p))) {
            result=SUDEKIMP_PARTY_SWAP_STALE;
            if(command.generation==(c<4u?o->assignment.generation[c]:0u)) {
                BOOL admitting=FALSE;
                if(menu_lobby_known) for(unsigned i=1;i<4u;++i)
                    if(menu_lobby.admission[i].phase>SUDEKIMP_LOBBY_ADMISSION_NONE &&
                        menu_lobby.admission[i].phase<SUDEKIMP_LOBBY_ADMISSION_COMPLETE) admitting=TRUE;
                BOOL busy=admitting || SudekiMpLanStoryMenuCapturesInput() || !host_binding_ready ||
                    !SudekiMpLanStoryHostControlCanSwap(controller,w,scene) ||
                    (p && (!host_control[p].ready && (scene->available_mask&(1u<<c))));
                result=SudekiMpPartySwapRequest(o,p,command.target,command.request,
                    command.world,command.revision,0u,busy);
            }
        }
        if(command.request>story_presence.ack_request[p]) {
            story_presence.ack_request[p]=command.request; story_presence.ack_result[p]=(uint8_t)result;
        }
        if(story_swap_traces<96u) SudekiMpLogFormat("lan_story_swap event=request player=%u from=%u target=%u request=%lu result=%u\r\n",
            p,c,command.target,(unsigned long)command.request,(unsigned)result);
    }
    if(o->phase==SUDEKIMP_PARTY_SWAP_RESERVED) {
        if(!(o->connected&(1u<<o->player)))
            (void)SudekiMpPartySwapCancel(o,o->player,o->request);
        else (void)SudekiMpPartySwapBegin(o,o->player,o->request);
    }
    if(o->phase==SUDEKIMP_PARTY_SWAP_HANDOFF) {
        unsigned p=o->player;
        if(!p) {
            /* Native Next rotates controller/camera/HUD and AI defaults. It
             * may only run with every remote override positively drained. */
            if(drain_story_casts(controller,w,scene) && drain_story_controls(controller,w,scene)) {
                if(host_binding_ready && scene->leader_seat==o->target) {
                    if(SudekiMpPartySwapCommit(o,0u,o->request)) story_control_confirmed(0u);
                } else (void)SudekiMpLanStoryHostControlSelect(controller,w,scene,o->target);
            }
        } else if(!host_control[p].native_key.token) {
            (void)SudekiMpPartySwapCommitReleased(o,p,o->request);
        } else host_control[p].draining=TRUE;
    }
    /* A disconnect during a transfer still finishes native release before
     * clearing either claim. It can never recycle a quarantined actor. */
    if(o->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && o->player &&
        !(o->connected&(1u<<o->player)) && !host_control[o->player].native_key.token)
        (void)SudekiMpPartyOwnershipLeaveDrained(o,o->player);
    (void)publish_story_ownership();
}
static void poll_story_swap(void) {
    if(!saved_profile || !session || !runtime_thread || runtime_thread!=GetCurrentThreadId()) return;
    HWND window=GetForegroundWindow(); DWORD pid=0;
    BOOL focused=window && GetWindowThreadProcessId(window,&pid)==runtime_thread && pid==GetCurrentProcessId();
    BOOL down=(GetAsyncKeyState(VK_F1)&0x8000)!=0;
    if(!focused || !story_key_focused) { story_key_down=down; story_key_focused=focused; return; }
    BOOL pressed=down && !story_key_down; story_key_down=down;
    SudekiMpLanPartyPresence state;
    if(!SudekiMpLanPartyGetPresence(session,&state)) {
        if(pressed) SudekiMpLanStoryMenuNotice("Waiting for the host before switching.");
        return;
    }
    if(story_pending_request && state.ack_request[local_seat]>=story_pending_request) {
        unsigned result=state.ack_result[local_seat]; story_pending_request=0;
        SudekiMpLanStoryMenuNotice(result==SUDEKIMP_PARTY_SWAP_OK?"Switching character...":
            result==SUDEKIMP_PARTY_SWAP_OCCUPIED?"That character is already claimed.":
            result==SUDEKIMP_PARTY_SWAP_UNAVAILABLE?"That character is not in the party yet.":
            result==SUDEKIMP_PARTY_SWAP_STALE?"Party changed; press F1 again.":
            "Wait until combat, dialogue and other transfers have finished.");
    }
    if(!pressed || SudekiMpLanStoryMenuCapturesInput()) return;
    const SudekiMpPartyOwnership *o=&state.ownership;
    if(story_pending_request || o->phase!=SUDEKIMP_PARTY_SWAP_IDLE) {
        SudekiMpLanStoryMenuNotice("A character transfer is still pending."); return;
    }
    unsigned current=o->assignment.character[local_seat],target=4u;
    /* Follow the familiar Tal/Ailish/Buki/Elco cycle; skip absent or reserved
     * actors. Character indices on the wire remain canonical SMP4 indices. */
    static const unsigned order[4]={2u,3u,0u,1u};
    unsigned start=3u;
    for(unsigned i=0;i<4u;++i) if(order[i]==current) start=i;
    for(unsigned i=1;i<=4u;++i) {
        unsigned c=order[(start+i)%4u];
        if(c!=current && (o->assignment.available&(1u<<c)) &&
            SudekiMpPartyCharacterOwner(&o->assignment,c)==4u) { target=c; break; }
    }
    if(target>=4u) { SudekiMpLanStoryMenuNotice("No unclaimed party character is available."); return; }
    uint32_t floor=state.ack_request[local_seat];
    if(floor<o->last_request[local_seat]) floor=o->last_request[local_seat];
    if(story_next_request<floor) story_next_request=floor;
    if(story_next_request==UINT32_MAX) return;
    SudekiMpLanPartyCommand command={.request=++story_next_request,
        .world=o->assignment.world,.revision=o->assignment.revision,
        .generation=current<4u?o->assignment.generation[current]:0u,
        .kind=SUDEKIMP_LAN_PARTY_COMMAND_SWAP,.player=(uint8_t)local_seat,.target=(uint8_t)target};
    if(SudekiMpLanPartyQueueCommand(session,&command)) {
        story_pending_request=command.request; SudekiMpLanStoryInputClear();
        SudekiMpLanStoryMenuNotice("Requesting character switch...");
    } else SudekiMpLanStoryMenuNotice("The previous request is still pending.");
}
static unsigned host_input_mask(void) {
    unsigned mask=0;
    for(unsigned p=1;p<4u;++p)
        if(host_control[p].ready && !host_control[p].draining && !host_control[p].input_held) mask|=1u<<p;
    return mask;
}
static void trace_control_block(unsigned p,const char *reason) {
    if(p>=4u || control_block_trace[p]==reason) return;
    control_block_trace[p]=reason;
    if(control_block_traces>=96u) return;
    ++control_block_traces;
    SudekiMpLogFormat("lan_story_control event=host_control_blocked player=%u reason=%s\r\n",p,reason?reason:"none");
}
static void service_story_controls(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    if(!control_attempted) return;
    SudekiMpLanStoryNativeRoster roster; memset(&roster,0,sizeof(roster));
    BOOL known=SudekiMpLanStoryObserverRoster(controller,w,scene,&roster);
    uint32_t now=GetTickCount();
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer={0};
        BOOL connected=SudekiMpLanPartyPeerStatusGet(session,p,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed;
        unsigned character=SudekiMpLanPartyPlayerCharacter(session,p);
        connected=connected && character<4u;
        if(host_control[p].native_key.token && !known && !host_control[p].draining &&
            split_attempted && SudekiMpLanStorySplitActive()) {
            /* Host lead's split-area transition: the exterior player's actor is
             * unchanged. Hold input until the roster is exact again. */
            trace_control_block(p,"split_transition_hold"); continue;
        }
        if(host_control[p].native_key.token) {
            const char *drain_reason=story_swap_blocks(p)?"character_swap":
                !connected?"peer_not_observing":
                character!=host_control[p].native_key.seat?"character_changed":
                !same_connection(&peer.lease,&host_control[p].connection)?"connection_changed":
                !known?"roster_unknown":
                !SudekiMpLanStoryControlMatchesScene(&host_control[p].fence,scene)?"scene_changed":
                !SudekiMpLanStoryHostControlBound(controller,w,scene)?"host_binding_changed":NULL;
            if(drain_reason && !host_control[p].draining) {
                SudekiMpLogFormat("lan_story_control event=host_control_drain player=%u character=%u transaction=%lu reason=%s\r\n",
                    p,host_control[p].native_key.seat,
                    (unsigned long)host_control[p].fence.transaction,drain_reason);
                host_control[p].draining=TRUE;
            }
            if(host_control[p].draining) {
                host_control[p].ready=FALSE;
                (void)SudekiMpLanPartyRevokeStoryControl(session,&host_control[p].connection);
                if(activity_attempted && SudekiMpLanStoryActivityRetains() &&
                    (!known || !SudekiMpLanStoryActivityService(w,&roster,0u))) {
                    trace_control_block(p,!known?"drain_roster_unknown":"drain_activity_retained"); continue;
                }
                if(known && SudekiMpLanStoryControlDrain(w,&roster,&host_control[p].native_key)) {
                    uint32_t transaction=host_control[p].transaction;
                    memset(&host_control[p],0,sizeof(host_control[p])); host_control[p].transaction=transaction;
                    trace_control_block(p,"drained");
                } else trace_control_block(p,!known?"drain_roster_unknown":"drain_native_refused");
                continue;
            }
        } else {
            if(connected) trace_control_block(p,story_swap_blocks(p)?"offer_swap":!known?"offer_roster_unknown":
                !host_binding_ready?"offer_host_binding":character==roster.leader_character?"offer_is_leader":
                !(roster.available_mask&(1u<<character))?"offer_unavailable":
                capture.epoch!=scene->epoch || capture.revision!=scene->revision?"offer_capture_scene":
                capture.actors[character]!=roster.actors[character] || !capture.generation[character]?"offer_capture_actor":
                !presentation_fresh(now,capture.last_tick)?"offer_capture_stale":
                !SudekiMpLanPartyStoryCatchupComplete(session,&peer.lease)?"offer_catchup":"offer_attempt");
            if(story_swap_blocks(p) || !connected || !known || !host_binding_ready || character==roster.leader_character ||
                !(roster.available_mask&(1u<<character)) ||
                capture.epoch!=scene->epoch || capture.revision!=scene->revision ||
                capture.actors[character]!=roster.actors[character] || !capture.generation[character] ||
                !presentation_fresh(now,capture.last_tick) ||
                !SudekiMpLanPartyStoryCatchupComplete(session,&peer.lease) ||
                host_control[p].transaction==UINT32_MAX ||
                !SudekiMpLanStoryControlBegin(w,&roster)) continue;
            SudekiMpLanPartyLease key;
            if(!SudekiMpLanStoryControlNextLease(w,&peer.lease,character,&key)) continue;
            host_control[p].connection=peer.lease; host_control[p].native_key=key;
            host_control[p].fence=(SudekiMpLanStoryControlFence){scene->epoch,scene->revision,
                ++host_control[p].transaction,capture.generation[character],(uint8_t)p,(uint8_t)character};
            if(!SudekiMpLanStoryControlAcquire(w,&roster,&key)) {
                if(SudekiMpLanStoryControlRetainsKey(&key)) host_control[p].draining=TRUE;
                else {
                    /* A preflight refusal owns no native reference. Do not
                     * wait forever for a release of a lease never acquired. */
                    uint32_t transaction=host_control[p].transaction;
                    memset(&host_control[p],0,sizeof(host_control[p]));
                    host_control[p].transaction=transaction;
                }
                continue;
            }
            host_control[p].acquired=TRUE;
        }
        if(!host_control[p].acquired || !SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key)) {
            host_control[p].draining=TRUE; continue;
        }
        SudekiMpLanStoryControlState state={host_control[p].fence,now,
            SUDEKIMP_STORY_CONTROL_PREPARE};
        if(SudekiMpLanPartyStoryControlAcknowledged(session,&peer.lease,&state.fence))
            state.phase=SUDEKIMP_STORY_CONTROL_READY;
        BOOL published=SudekiMpLanPartyPublishStoryControl(session,&peer.lease,&state);
        host_control[p].ready=published && state.phase==SUDEKIMP_STORY_CONTROL_READY;
        if(!host_control[p].ready) host_control[p].last_input_at=0;
        SudekiMpLanStoryMovement input; uint32_t receipt;
        if(host_control[p].ready &&
            SudekiMpLanPartyTakeStoryMovement(session,&peer.lease,now,&input,&receipt)) {
            host_control[p].movement=input; host_control[p].last_input_at=receipt;
        }
        float x=0,z=0;
        /* The receipt stamp comes from the transport worker's clock and can be
         * a few milliseconds newer than this tick's `now`; a negative age is
         * fresh, not a wrapped-around stale value (live: one zeroed tick per
         * such packet showed as an idle pop between run cycles). */
        int32_t input_age=(int32_t)(now-host_control[p].last_input_at);
        BOOL fresh=host_control[p].last_input_at && input_age<=100;
        BOOL fence_ok=SudekiMpLanStoryControlFenceSame(&host_control[p].movement.fence,&state.fence);
        if(host_binding_ready && host_control[p].ready && fresh && fence_ok) {
            x=host_control[p].movement.world_x; z=host_control[p].movement.world_z;
        }
        {
            /* Bounded research diagnostic: applied-movement transitions and why. */
            static BOOL was_moving[4]; static unsigned move_logs;
            BOOL moving=(x!=0.0f || z!=0.0f);
            if(moving!=was_moving[p] && move_logs<400u) {
                ++move_logs; was_moving[p]=moving;
                SudekiMpLogFormat("lan_story_control event=host_input ms=%lu player=%u moving=%u binding=%u ready=%u fresh=%u age_ms=%ld fence=%u packet=%.2f,%.2f\r\n",
                    (unsigned long)GetTickCount(),p,moving,host_binding_ready,host_control[p].ready,fresh,
                    host_control[p].last_input_at?(long)input_age:0l,fence_ok,
                    (double)host_control[p].movement.world_x,(double)host_control[p].movement.world_z);
            }
        }
        /* A native telescope/dialogue may filter host input while both
         * actor bindings remain exact. Keep the companion lease and stop
         * movement; resuming requires a fresh packet after the filter clears. */
        if(!host_binding_ready) host_control[p].last_input_at=0;
        BOOL native_held=FALSE;
        if(!SudekiMpLanStoryControlMove(w,&roster,&host_control[p].native_key,x,z,&native_held)) {
            host_control[p].last_input_at=0;
            (void)SudekiMpLanPartyRevokeStoryControl(session,&peer.lease);
            host_control[p].draining=TRUE;
        } else {
            BOOL held=!host_binding_ready || native_held;
            if(held) host_control[p].last_input_at=0;
            if(held!=host_control[p].input_held && host_control_hold_traces<64u) {
                ++host_control_hold_traces;
                SudekiMpLogFormat("lan_story_control event=host_input_hold player=%u transaction=%lu held=%u reason=%s ownership=retained\r\n",
                    p,(unsigned long)state.fence.transaction,held?1u:0u,
                    !host_binding_ready?"host_input_filtered":native_held?"native_body_or_world_busy":"ready");
            }
            host_control[p].input_held=held;
        }
    }
    if(activity_attempted && known) {
        unsigned mask=0u;
        for(unsigned p=1;p<4u;++p)
            if(host_control[p].ready && host_control[p].acquired && !host_control[p].draining &&
                SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key))
                mask|=1u<<host_control[p].native_key.seat;
        {
            static unsigned traced_mask=99u,traces;
            if(mask!=traced_mask && traces<48u) {
                ++traces; traced_mask=mask;
                unsigned p1=1u;
                SudekiMpLogFormat("lan_story_activity event=mask value=%u p1_ready=%u p1_acquired=%u p1_draining=%u p1_exact=%u\r\n",
                    mask,host_control[p1].ready,host_control[p1].acquired,host_control[p1].draining,
                    host_control[p1].native_key.token?SudekiMpLanStoryControlExact(w,&roster,&host_control[p1].native_key):0);
            }
        }
        (void)SudekiMpLanStoryActivityService(w,&roster,mask);
        /* Remote players currently holding their characters may stay in the
         * exterior when the host's lead uses a temporary door. */
        if(split_attempted) SudekiMpLanStorySplitSetEligibility(
            scene->phase==SUDEKIMP_LAN_STORY_READY && !scene->temporary[0],(uint8_t)mask);
    } else if(split_attempted) SudekiMpLanStorySplitSetEligibility(FALSE,0u);
    if(known && host_binding_ready && story_policy_initialized &&
        story_presence.ownership.phase==SUDEKIMP_PARTY_SWAP_IDLE) {
        /* A local handoff retires namespaces before native rotation. Only
         * rebuild after that rotation and a new settled control exchange. */
        unsigned remote_ready=0;
        for(unsigned p=1;p<4u;++p) if(host_control[p].ready && !host_control[p].draining)
            remote_ready|=1u<<p;
        if(remote_ready && !cast_attempted) {
            cast_attempted=TRUE;
            if(!SudekiMpLanStoryCastInstall((HMODULE)game_base))
                SudekiMpLogWrite("story_cast event=install_refused admission=closed\r\n");
        }
        if(remote_ready && cast_attempted && !SudekiMpLanStoryMenuCapturesInput())
            (void)SudekiMpLanStoryCastTryBind(w,&roster);
    }
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanStoryActionRequest request;
        if(!host_control[p].native_key.token ||
            !SudekiMpLanPartyTakeStoryAction(session,&host_control[p].connection,GetTickCount(),&request)) continue;
        unsigned outcome=SUDEKIMP_STORY_ACTION_UNAVAILABLE;
        if(known && host_control[p].ready && !host_control[p].draining && !story_swap_blocks(p) &&
            !SudekiMpLanStoryMenuCapturesInput() &&
            (request.kind!=SUDEKIMP_STORY_ACTION_MELEE || host_binding_ready) &&
            SudekiMpLanStoryControlFenceSame(&request.fence,&host_control[p].fence) &&
            SudekiMpLanStoryHostControlBound(controller,w,scene) &&
            SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key)) {
            if(request.kind==SUDEKIMP_STORY_ACTION_SKILL)
                outcome=SudekiMpLanStoryCastSubmit(w,&roster,&host_control[p].native_key,request.slot);
            else if(request.kind==SUDEKIMP_STORY_ACTION_MELEE)
                outcome=SudekiMpLanStoryControlMelee(w,&roster,&host_control[p].native_key,request.slot);
        }
        /* Take consumed this ID before entering native code. Even an unknown
         * native outcome is terminal for this request: retransmission only
         * resends the retained result, never repeats Use or melee submission. */
        SudekiMpLanStoryActionResult result={request.fence,request.request,GetTickCount(),
            request.kind,request.slot,(uint8_t)outcome};
        (void)SudekiMpLanPartyPublishStoryActionResult(session,&host_control[p].connection,&result);
        SudekiMpLogFormat("story_cast event=request_result player=%u character=%u request=%lu kind=%u slot=%u outcome=%u\r\n",
            p,request.fence.character,(unsigned long)request.request,request.kind,request.slot,outcome);
    }
}
static void service_client_recruitment(void) {
    if(!recruit_attempted || !session || !local_seat || client_recruit_committed) return;
    if(SudekiMpLanStoryClientRecruiting()) {
        SudekiMpLanStoryRecruitReport report;
        (void)SudekiMpLanStoryClientRecruitService(&report);
        return;
    }
    SudekiMpLanPartyPeerStatus peer; SudekiMpLanStoryScene remote,native;
    SudekiMpLanStoryNativeRoster roster; SudekiMpLanStoryRecruitment recruitment;
    SudekiMpLanStoryTaskTraceStatus tasks; uint32_t now=GetTickCount();
    if(!SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
        !SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) ||
        !SudekiMpLanPartyGetStoryRecruitment(session,&peer.lease,now,&recruitment) ||
        !SudekiMpLanStoryRecruitmentMatchesScene(&recruitment,&remote) ||
        !presented_epoch || recruitment.before_epoch!=presented_epoch ||
        recruitment.before_revision!=presented_revision ||
        !SudekiMpLanStoryClientPausedRoster(&roster,&native) || native.available_mask!=4u ||
        strcmp(native.world,remote.world) || strcmp(native.temporary,remote.temporary) ||
        !SudekiMpLanStoryTaskTraceGetStatus(&tasks) || tasks.unknown) return;
    SudekiMpLanStoryRecruitRequest request={recruitment.route,recruitment.transaction,tasks.load_generation,
        native.epoch,native.revision,recruitment.after_epoch,recruitment.after_revision,
        recruitment.actor_generation,recruitment.observed_tick};
    if(SudekiMpLanStoryCinematicAudioRetains() &&
        !SudekiMpLanStoryCinematicAudioStop(audio_cleanup_exact,&roster)) return;
    client_recruit=recruitment;
    (void)SudekiMpLanStoryClientRecruitBegin(&request,recruitment.before_epoch);
}

static void trace_client_control(unsigned phase,const char *reason,uint32_t transaction) {
    if(client_control_trace_count && phase==client_control_trace_state) return;
    client_control_trace_state=phase; /* periodic status stays current after the detail cap */
    if(client_control_trace_count>=64u) return;
    ++client_control_trace_count;
    SudekiMpLogFormat("lan_story_control event=client_phase player=%u phase=%u transaction=%lu reason=%s local_character=%u input_ready=%u\r\n",
        local_seat,phase,(unsigned long)transaction,reason,
        client_seed.leader_character,client_input_ready?1u:0u);
}
static BOOL queue_client_action(void *actor,unsigned kind,unsigned slot) {
    unsigned character=client_control_fence.character;
    if(!session || !local_seat || !client_input_ready || character>=4u ||
        actor!=client_seed.actors[character] || client_action_pending || client_action_serial==UINT32_MAX ||
        InterlockedCompareExchange(&stopping,0,0) || !presentation_sample.valid ||
        !presentation_fresh(GetTickCount(),presentation_sample.receipt) ||
        !same_connection(&client_control_connection,&presentation_sample.lease)) return FALSE;
    if(kind>UINT8_MAX || slot>UINT8_MAX) return FALSE;
    SudekiMpLanStoryActionRequest request={client_control_fence,client_action_serial+1u,
        presentation_sample.sequence,(uint8_t)kind,(uint8_t)slot};
    if(!SudekiMpLanStoryActionRequestValid(&request)) return FALSE;
    client_action=request; ++client_action_serial;
    client_action_connection=client_control_connection;
    client_action_pending=TRUE; client_action_sent_at=0;
    return TRUE; /* Plain local outbox only. Never client-side CSkill::Use. */
}
static BOOL queue_client_skill(void *actor,unsigned slot) {
    return queue_client_action(actor,SUDEKIMP_STORY_ACTION_SKILL,slot);
}
static void service_client_action(const SudekiMpLanPartyLease *connection,
    const SudekiMpLanStoryControlFence *fence,uint32_t now) {
    if(!client_action_pending) return;
    if(!connection || !fence || !same_connection(connection,&client_action_connection) ||
        !SudekiMpLanStoryControlFenceSame(fence,&client_action.fence)) {
        /* Never migrate an uncertain old submission into a new character,
         * scene or reconnect. The host retains its own native action journal. */
        client_action_pending=FALSE; return;
    }
    SudekiMpLanStoryActionResult result;
    if(SudekiMpLanPartyGetStoryActionResult(session,connection,&result) &&
        SudekiMpLanStoryActionResultMatches(&result,&client_action)) {
        SudekiMpLogFormat("story_skill event=result player=%u character=%u request=%lu kind=%u slot=%u outcome=%u\r\n",
            local_seat,fence->character,(unsigned long)result.request,result.kind,result.slot,result.outcome);
        if(result.kind==SUDEKIMP_STORY_ACTION_SKILL && result.outcome!=SUDEKIMP_STORY_ACTION_STARTED)
            SudekiMpLanStoryMenuNotice(result.outcome==SUDEKIMP_STORY_ACTION_NO_SP?
                "Not enough SP for that skill.":result.outcome==SUDEKIMP_STORY_ACTION_BUSY?
                "Your character is busy; try the skill again when ready.":
                result.outcome==SUDEKIMP_STORY_ACTION_EXPIRED?
                "The skill request expired; reopen Q and try again.":
                "That skill is not available right now.");
        client_action_pending=FALSE; return;
    }
    if(!client_action_sent_at || (uint32_t)(now-client_action_sent_at)>=100u) {
        /* A send error is uncertain delivery. Retry this SAME request ID and
         * acknowledged frame; transport dedup owns at-most-once execution. */
        (void)SudekiMpLanPartySendStoryAction(session,connection,&client_action);
        client_action_sent_at=now;
    }
}
static BOOL service_quick_menu(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)unused;
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanStoryControlState control;
    SudekiMpLanStoryScene remote;
    uint32_t now=GetTickCount();
    /* UI lifetime is a current connection/character lease, not successful
     * world-material cloning on this render frame. Opening Q can itself load
     * resources. Retain its exact local owner through that temporary stall;
     * neither new opens nor skill requests gain stale-frame authority. */
    BOOL owned=session && local_seat && client_local_selected &&
        !InterlockedCompareExchange(&stopping,0,0) && !SudekiMpLanStoryMenuCapturesInput() &&
        r->leader_character==client_control_fence.character &&
        SudekiMpLanPartyLocalCharacter(session)==r->leader_character &&
        SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
        peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && same_connection(&peer.lease,&client_control_connection) &&
        SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) &&
        SudekiMpLanPartyGetStoryControl(session,&peer.lease,now,&control) &&
        control.phase==SUDEKIMP_STORY_CONTROL_READY &&
        SudekiMpLanStoryControlFenceSame(&control.fence,&client_control_fence) &&
        SudekiMpLanStoryControlMatchesScene(&control.fence,&remote) &&
        scene->epoch==r->epoch && client_area_matches(scene,&remote);
    BOOL admitted=owned && client_input_ready && presentation_sample.valid &&
        presentation_fresh(now,presentation_sample.receipt);
    BOOL toggle=owned && SudekiMpLanStoryInputTakeQuickMenu(r->controller,
        r->actors[r->leader_character],client_control_fence.transaction);
    return SudekiMpLanStoryQuickMenuService(r,client_control_fence.transaction,owned,admitted,toggle);
}
static void service_client_control(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryScene *native) {
    if(!local_control_attempted) return;
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanStoryScene remote;
    SudekiMpLanStoryControlState state;
    uint32_t now=GetTickCount();
    unsigned chosen=SudekiMpLanPartyLocalCharacter(session);
    BOOL offered=chosen<4u &&
        SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
        peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
        SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) &&
        SudekiMpLanPartyGetStoryControl(session,&peer.lease,now,&state) &&
        (state.phase==SUDEKIMP_STORY_CONTROL_PREPARE || state.phase==SUDEKIMP_STORY_CONTROL_READY) &&
        SudekiMpLanStoryControlMatchesScene(&state.fence,&remote) &&
        state.fence.character==chosen && presented_epoch==state.fence.epoch &&
        presented_generations[chosen]==state.fence.actor_generation;
    client_input_ready=FALSE;
    if(!offered) {
        SudekiMpLanStoryInputClear();
        service_client_action(NULL,NULL,now);
        /* An offer gap closes input, but does not undo the completed view
         * restore/seed transaction. The same recruited world may receive a
         * later offer; its plain fence is refreshed by presentation. */
        trace_client_control(0u,"waiting_for_host_offer",0u);
        return;
    }
    if(client_local_selected && client_seed.leader_character!=chosen) client_local_selected=FALSE;
    if(!client_local_selected) {
        if(!client_switch_prepared || !same_connection(&peer.lease,&client_control_connection) ||
            !SudekiMpLanStoryControlFenceSame(&state.fence,&client_control_fence)) {
            SudekiMpLanStoryInputClear();
            trace_client_control(1u,"waiting_for_recruited_view",state.fence.transaction); return;
        }
        SudekiMpLanStoryLocalControlReport report={0};
        (void)SudekiMpLanStoryClientSelectCharacter(controller,w,native,chosen,&report);
        if(report.coherent && SudekiMpLanStoryObserverSample(controller,w,native) &&
            SudekiMpLanStoryObserverRoster(controller,w,native,&client_seed))
            client_local_selected=report.bound && client_seed.leader_character==chosen;
        if(!client_local_selected) {
            trace_client_control(2u,report.reason?report.reason:"native_selection_pending",state.fence.transaction);
            return;
        }
    }
    SudekiMpLanStoryNativeRoster roster;
    const char *pending=!SudekiMpLanStoryObserverRoster(controller,w,native,&roster)?"pending_roster":
        !SudekiMpLanStoryLocalControlReady(controller,w,&roster,native)?"pending_local_control":
        !presentation_sample.valid?"pending_presentation_invalid":
        !presentation_fresh(now,presentation_sample.receipt)?"pending_presentation_stale":
        presentation_sample.scene.epoch!=state.fence.epoch?"pending_epoch":
        presentation_sample.scene.revision<state.fence.revision?"pending_revision":
        presented_generations[chosen]!=state.fence.actor_generation?"pending_generation":
        !same_connection(&presentation_sample.lease,&peer.lease)?"pending_connection":
        !SudekiMpLanStoryInputArm(controller,roster.actors[chosen],state.fence.transaction)?"pending_input_arm":NULL;
    if(pending) {
        SudekiMpLanStoryInputClear();
        if(pending!=client_pending_trace && client_pending_traces<64u) {
            ++client_pending_traces; client_pending_trace=pending;
            SudekiMpLogFormat("lan_story_control event=client_pending reason=%s sample_epoch=%lu sample_revision=%lu fence_epoch=%lu fence_revision=%lu generation=%lu fence_generation=%lu\r\n",
                pending,(unsigned long)presentation_sample.scene.epoch,(unsigned long)presentation_sample.scene.revision,
                (unsigned long)state.fence.epoch,(unsigned long)state.fence.revision,
                (unsigned long)presented_generations[chosen<4u?chosen:0u],(unsigned long)state.fence.actor_generation);
        }
        trace_client_control(3u,"local_view_or_presented_frame_pending",state.fence.transaction); return;
    }
    if(!same_connection(&peer.lease,&client_control_connection) ||
        !SudekiMpLanStoryControlFenceSame(&client_control_fence,&state.fence)) {
        client_control_connection=peer.lease; client_control_fence=state.fence;
        client_input_sequence=client_input_sent_at=0;
    }
    if(state.phase==SUDEKIMP_STORY_CONTROL_PREPARE) {
        if(SudekiMpLanPartyAcknowledgeStoryControl(session,&peer.lease,&state.fence))
            trace_client_control(4u,"native_binding_acknowledged",state.fence.transaction);
        return;
    }
    service_client_action(&peer.lease,&state.fence,now);
    float local_x=0,local_z=0,x=0,z=0;
    if(SudekiMpLanStoryMenuCapturesInput()) SudekiMpLanStoryInputClear();
    else if(SudekiMpLanStoryQuickMenuCapturesInput()) SudekiMpLanStoryInputMuteMovement();
    else if(!SudekiMpLanStoryInputSample(controller,roster.actors[chosen],state.fence.transaction,&local_x,&local_z) ||
        !SudekiMpLanStoryLocalControlDirection(controller,w,&roster,native,local_x,local_z,&x,&z)) {
        static unsigned sample_fail_logs;
        if(sample_fail_logs<200u) { ++sample_fail_logs; SudekiMpLogFormat("lan_story_control event=client_input_sample_failed ms=%lu\r\n",(unsigned long)GetTickCount()); }
        SudekiMpLanStoryInputClear(); return;
    }
    {
        /* Bounded research diagnostic: moving/stopped transitions of the sent input. */
        static BOOL was_moving; static unsigned move_logs;
        BOOL moving=(x!=0.0f || z!=0.0f);
        if(moving!=was_moving && move_logs<400u) {
            ++move_logs; was_moving=moving;
            SudekiMpLogFormat("lan_story_control event=client_input ms=%lu moving=%u local=%.2f,%.2f world=%.2f,%.2f\r\n",
                (unsigned long)GetTickCount(),moving,(double)local_x,(double)local_z,(double)x,(double)z);
        }
    }
    client_input_ready=TRUE;
    if(!SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput() &&
        !client_action_pending) {
        unsigned melee=SudekiMpLanStoryInputTakeMelee(controller,roster.actors[chosen],state.fence.transaction);
        if(melee && queue_client_action(roster.actors[chosen],SUDEKIMP_STORY_ACTION_MELEE,melee))
            service_client_action(&peer.lease,&state.fence,now);
    }
    trace_client_control(5u,"host_confirmed_movement",state.fence.transaction);
    if(client_input_sequence==UINT32_MAX) { client_input_ready=FALSE; SudekiMpLanStoryInputClear(); return; }
    /* Send on every client frame (~21 ms). A 33 ms gate on a ~21 ms frame
     * sent every second frame (42-63 ms), which periodically outran the
     * host's input hold and showed as an idle pose between run cycles. */
    if(client_input_sent_at && (uint32_t)(now-client_input_sent_at)<15u) return;
    SudekiMpLanStoryMovement input={state.fence,++client_input_sequence,presentation_sample.sequence,x,z};
    if(SudekiMpLanPartySendStoryMovement(session,&peer.lease,&input)) client_input_sent_at=now;
}

static void menu_frame(void) {
    if(!session || !saved_profile || !runtime_thread ||
        GetCurrentThreadId()!=runtime_thread) return;
    if(local_seat) {
        SudekiMpLanPartyPeerStatus peer;
        menu_scene_known=SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
            SudekiMpLanPartyGetStoryScene(session,&peer.lease,GetTickCount(),&menu_scene);
        if(InterlockedCompareExchange(&stopping,0,0)) {
            invalidate_presentation();
            if(retire_client()) InterlockedExchange(&client_exit_prepared,1);
        } else {
            service_client_recruitment();
            SudekiMpLanStoryClientReport report={0};
            BOOL contained=SudekiMpLanStoryClientRetains()?
                SudekiMpLanStoryClientService(&report):SudekiMpLanStoryClientAcquire(&report);
            if(!contained || !presentation_attempted) invalidate_presentation();
            if(contained && quick_menu_attempted)
                (void)SudekiMpLanStoryClientPresent(service_quick_menu,NULL);
            if(contained && SudekiMpLanStoryCinematicAudioRetains() &&
                (!visible_dialogue_valid || !presentation_fresh(GetTickCount(),visible_dialogue_receipt)))
                (void)SudekiMpLanStoryClientPresent(stop_stale_audio,NULL);
        }
        presentation_attempted=FALSE;
    }
    refresh_menu();
    void **slot=(void **)(game_base+0x3c31dcu);
    MEMORY_BASIC_INFORMATION m;
    if(VirtualQuery(slot,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        (uintptr_t)slot+sizeof(*slot)<=(uintptr_t)m.BaseAddress+m.RegionSize)
    {
        if(local_seat && visible_dialogue_valid && presentation_fresh(GetTickCount(),visible_dialogue_receipt)) {
            BOOL ok=SudekiMpStoryCinematicViewRender(*slot,&visible_dialogue);
            DWORD error=ok?ERROR_SUCCESS:GetLastError();
            trace_cinematic(&overlay_trace,"overlay",&visible_dialogue,ok,error);
        }
        (void)SudekiMpLanStoryMenuRender(*slot);
    }
}
BOOL SudekiMpLanStoryRuntimeReady(void) {
    return saved_profile && session && !InterlockedCompareExchange(&stopping,0,0) &&
        runtime_thread==GetCurrentThreadId() && InterlockedCompareExchange(&runtime_ready,0,0)!=0;
}
static BOOL host_catchup_available(uint32_t now) {
    if((story_policy_initialized && story_presence.ownership.phase!=SUDEKIMP_PARTY_SWAP_IDLE) ||
        !host_binding_ready || !presentation_fresh(now,host_live_receipt) ||
        !host_seed.transaction || host_live_scene.phase!=SUDEKIMP_LAN_STORY_READY ||
        strcmp(host_seed.seed_scene.world,host_live_scene.world) ||
        strcmp(host_seed.seed_scene.temporary,host_live_scene.temporary)) return FALSE;
    if(!host_seed.recruitment.transaction && host_live_scene.available_mask==loaded_party_mask)
        return host_live_scene.epoch==host_seed.seed_scene.epoch &&
            host_live_scene.revision==host_seed.seed_scene.revision;
    return SudekiMpLanStoryRecruitmentMatchesScene(&host_recruit,&host_live_scene) &&
        host_recruit.before_epoch==host_seed.seed_scene.epoch &&
        host_recruit.before_revision==host_seed.seed_scene.revision;
}
static void service_story_admission(SudekiMpLobby *lobby,const SudekiMpLobbyStatus *status,
    unsigned player,const SudekiMpLanPartyPeerStatus *peer) {
    const SudekiMpLobbyMember *member=&status->members[player];
    const SudekiMpLobbyAdmission *a=&status->admission[player];
    if(a->phase==SUDEKIMP_LOBBY_ADMISSION_COMPLETE) return;
    if(a->phase==SUDEKIMP_LOBBY_ADMISSION_FAILED) {
        /* A timed-out load may still own native tasks on the client. TCP
         * departure requests its normal cancel/drain; never recycle its ticket. */
        (void)SudekiMpLobbyDisconnectMember(lobby,player); return;
    }
    if(!member->locked || member->character>=4u) return;
    uint32_t now=GetTickCount();
    if(a->phase==SUDEKIMP_LOBBY_ADMISSION_NONE) {
        if(peer->phase==SUDEKIMP_LAN_PARTY_FREE && !story_lobby_tickets[player] &&
            !host_control[player].native_key.token && host_catchup_available(now))
            (void)SudekiMpLobbyHostAdmit(lobby,player);
        return;
    }
    if(!a->ticket || !a->sequence) return;
    if(!story_lobby_tickets[player]) {
        if(peer->phase!=SUDEKIMP_LAN_PARTY_FREE || host_control[player].native_key.token ||
            !host_catchup_available(now) || !story_policy_initialized) return;
        SudekiMpPartyOwnership *o=&story_presence.ownership;
        if(o->assignment.character[player]!=member->character &&
            !SudekiMpPartyOwnershipReserveStory(o,0u,player,member->character,
                o->assignment.world,o->assignment.revision)) return;
        if(!publish_story_ownership() ||
            !SudekiMpLanPartyRegisterStoryAdmission(session,player,member->character,a->ticket)) return;
        story_lobby_tickets[player]=a->ticket;
        SudekiMpLogFormat("lan_story event=catchup_admitted player=%u character=%u transaction=%lu\r\n",
            player,member->character,(unsigned long)a->sequence);
    }
    if(peer->phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !peer->transport_confirmed) return;
    if(!SudekiMpLanPartyStoryCatchupComplete(session,&peer->lease)) {
        SudekiMpLanStoryCatchup *snapshot=&host_catchup[player];
        if(!snapshot->transaction) {
            if(!host_catchup_available(now)) return;
            *snapshot=host_seed; snapshot->transaction=a->sequence; snapshot->target=host_live_scene;
            if(host_seed.seed_scene.available_mask!=host_live_scene.available_mask)
                snapshot->recruitment=host_recruit;
            if(!SudekiMpLanStoryCatchupValid(snapshot)) {
                memset(snapshot,0,sizeof(*snapshot)); return;
            }
        }
        if(menu_scene_known && (snapshot->target.epoch!=menu_scene.epoch ||
            snapshot->target.revision!=menu_scene.revision ||
            !SudekiMpLanStorySceneSame(&snapshot->target,&menu_scene))) {
            (void)SudekiMpLobbyHostAdmissionComplete(lobby,player,a->sequence,a->ticket,FALSE);
            SudekiMpLogFormat("lan_story event=catchup_cancelled player=%u transaction=%lu reason=host_scene_changed\r\n",
                player,(unsigned long)a->sequence);
            (void)SudekiMpLanPartyDisconnect(session,&peer->lease); return;
        }
        (void)SudekiMpLanPartyPublishStoryCatchup(session,&peer->lease,snapshot);
        return;
    }
    /* A present character additionally completes the established native
     * PREPARE/ACK/READY exchange. Unavailable characters remain spectators. */
    if(a->phase==SUDEKIMP_LOBBY_ADMISSION_LOADED && menu_scene_known &&
        (!(menu_scene.available_mask&(1u<<member->character)) ||
         (host_control[player].ready && host_control[player].fence.character==member->character)) &&
        SudekiMpLobbyHostAdmissionComplete(lobby,player,a->sequence,a->ticket,TRUE))
        SudekiMpLogFormat("lan_story event=catchup_complete player=%u character=%u transaction=%lu input_ready=%u\r\n",
            player,member->character,(unsigned long)a->sequence,host_control[player].ready?1u:0u);
}
void SudekiMpLanStoryRuntimeLobbyService(SudekiMpLobby *lobby) {
    SudekiMpLobbyStatus status;
    if(!session || !saved_profile || !lobby || GetCurrentThreadId()!=runtime_thread) return;
    SudekiMpLobbyStatusGet(lobby,&status);
    menu_lobby=status; menu_lobby_known=TRUE;
    if(SudekiMpLanStoryMenuLeaveRequested()) {
        if(SudekiMpLobbyGameplayCancel()) SudekiMpLobbyLeave(lobby);
        return;
    }
    if(local_seat) {
        if(status.phase==SUDEKIMP_LOBBY_ERROR || status.phase==SUDEKIMP_LOBBY_IDLE)
            InterlockedExchange(&runtime_ready,0);
        return;
    }
    if(status.phase!=SUDEKIMP_LOBBY_HOSTING || !status.running) return;
    if(story_policy_initialized) {
        unsigned admitted=1u;
        for(unsigned p=1;p<4u;++p) if(story_lobby_tickets[p]) admitted|=1u<<p;
        (void)SudekiMpLobbyReflectAssignments(lobby,story_presence.ownership.assignment.character,admitted);
    }
    /* TCP departure closes the gameplay endpoint first. Any recruited actor
     * lease drains on the verified native dispatch before transport release. */
    for(unsigned player=1;player<4u;++player) {
        SudekiMpLanPartyPeerStatus peer;
        if(!SudekiMpLanPartyPeerStatusGet(session,player,&peer)) continue;
        const SudekiMpLobbyMember *member=&status.members[player];
        /* UDP timeout/END may arrive before TCP departure. Its ticket was
         * already revoked under the transport lock; close the matching lobby
         * membership as well instead of leaving a connected ghost player. */
        if(member->present && story_lobby_tickets[player] &&
            !SudekiMpLanPartyAdmissionMatches(session,player,story_lobby_tickets[player])) {
            (void)SudekiMpLobbyDisconnectMember(lobby,player);
            continue;
        }
        if(member->present) {
            service_story_admission(lobby,&status,player,&peer); continue;
        }
        if(peer.phase==SUDEKIMP_LAN_PARTY_PENDING || peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING) {
            (void)SudekiMpLanPartyDisconnect(session,&peer.lease);
            continue;
        }
        /* FREE alone is insufficient: the controller seam must have positively
         * drained the native lease before clearing this owner record. A
         * quarantine or partial release keeps the reservation unavailable. */
        if(peer.phase!=SUDEKIMP_LAN_PARTY_FREE || host_control[player].native_key.token ||
            host_control[player].acquired || host_control[player].draining) continue;
        if(story_policy_initialized) {
            SudekiMpPartyOwnership *o=&story_presence.ownership;
            if(o->connected&(1u<<player)) (void)SudekiMpPartyOwnershipDisconnect(o,player);
            story_control_released(player);
            if(o->assignment.humans&(1u<<player)) {
                if(!SudekiMpPartyOwnershipReleaseDeparted(o,player) || !publish_story_ownership()) continue;
            }
        }
        if(story_lobby_tickets[player]) {
            if(!SudekiMpLanPartyRevokeAdmission(session,player,story_lobby_tickets[player])) continue;
            story_lobby_tickets[player]=0;
            memset(&host_catchup[player],0,sizeof(host_catchup[player]));
        }
        if(member->reserved && SudekiMpLobbyReleaseReservation(lobby,player))
            SudekiMpLogFormat("lan_story event=departure_released player=%u character=%u native_cleanup=complete policy=available_ai\r\n",
                player,member->character);
    }
}
static BOOL retain_module(void) {
    HMODULE retained;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&owner,&retained);
    SetLastError(ERROR_BUSY);
    return FALSE;
}
unsigned SudekiMpLanStoryRuntimePort(void) {
    return session?SudekiMpLanPartyPort(session):0u;
}

static DWORD WINAPI poll_network(void *unused) {
    (void)unused;
    while(WaitForSingleObject(stop_worker,4u)==WAIT_TIMEOUT)
        SudekiMpLanPartyPoll(session,GetTickCount());
    return 0;
}
static void service(void *controller,void *data,
    const SudekiMpControlUpdateDispatchWitness *w) {
    (void)data;
    LARGE_INTEGER controller_start={0};
    if(!SudekiMpControlUpdateObserverGateTryEnter(&gate)) return;
    if(!w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) goto done;
    if(!runtime_thread) runtime_thread=GetCurrentThreadId();
    if(runtime_thread!=GetCurrentThreadId()) goto done;
    if(saved_profile && !local_seat) {
        controller_start=timing_begin();
        timing_end(&controller_gap_timing,previous_controller_stamp);
        previous_controller_stamp=controller_start;
    }
    if(InterlockedCompareExchange(&stopping,0,0)) {
        if(menu_initialized) {
            SudekiMpLanStoryScene tools_scene;
            BOOL tools_known=SudekiMpLanStoryObserverSample(controller,w,&tools_scene);
            if(!SudekiMpLanStoryMenuDrainToolsOnDispatch(controller,w,
                    tools_known?&tools_scene:NULL)) goto done;
        }
        if(local_seat && saved_profile && SudekiMpLanStoryClientRecruiting()) {
            SudekiMpLanStoryScene pending;
            if(SudekiMpLanStoryObserverSample(controller,w,&pending) &&
                SudekiMpLanStoryClientRecruitCommit(controller,w,&pending)) {
                client_recruit_committed=TRUE;
                (void)SudekiMpLanStoryObserverRoster(controller,w,&pending,&client_seed);
            }
        }
        if(host_attempted && !InterlockedCompareExchange(&host_drained,0,0)) {
            SudekiMpLanStoryScene draining_scene;
            if(!SudekiMpLanStoryObserverSample(controller,w,&draining_scene) ||
                !drain_story_casts(controller,w,&draining_scene) ||
                (control_attempted && !drain_story_controls(controller,w,&draining_scene)) ||
                !SudekiMpLanStoryHostControlDrain(controller,w,&draining_scene)) goto done;
            InterlockedExchange(&host_drained,1);
        }
        if(split_attempted && SudekiMpLanStorySplitUninstall()) split_attempted=FALSE;
        if((!client_attempted || InterlockedCompareExchange(&client_drained,0,0)) && !split_attempted &&
            SudekiMpLanStoryObserverUninstall()) InterlockedExchange(&observer_removed,1);
        goto done;
    }
    DWORD now=GetTickCount();
    SudekiMpLanStoryScene native;
    BOOL known=SudekiMpLanStoryObserverSample(controller,w,&native);
    now=GetTickCount(); /* Sample's observation timestamp precedes this frame. */
    if(saved_profile && !local_seat) observe_area_membership(controller,w,known?&native:NULL,now);
    if(saved_profile) {
        if(known) capture_initial_objects(controller,w,&native);
        if(local_seat) {
            if(known && SudekiMpLanStoryClientRecruiting() &&
                SudekiMpLanStoryClientRecruitCommit(controller,w,&native)) {
                client_recruit_committed=TRUE; presentation_changed=FALSE;
                presented_epoch=presented_revision=0; history_count=world_history_count=dialogue_count=0;
                memset(presented_generations,0,sizeof(presented_generations));
            }
            if(SudekiMpLanStoryInputObserve(controller,w) && known && load_finished() &&
                SudekiMpLanStoryObserverRoster(controller,w,&native,&client_seed))
                (void)SudekiMpLanStoryClientObserve(controller,w,&native);
        } else {
            BOOL loaded=known && native.phase==SUDEKIMP_LAN_STORY_READY && load_finished();
            if(loaded) (void)SudekiMpLanStoryHostControlService(controller,w,&native,
                SudekiMpLanStoryMenuCapturesInput());
            /* A native rotation invalidates the earlier roster. Publish only
             * a new observation after the host adapter returns. */
            if(loaded) known=SudekiMpLanStoryObserverSample(controller,w,&native);
            host_binding_ready=loaded && known &&
                SudekiMpLanStoryHostControlReady(controller,w,&native);
            /* Load acknowledgement is independent of the transient native
             * control binding shown in the session menu. */
            InterlockedExchange(&runtime_ready,loaded && known &&
                native.phase==SUDEKIMP_LAN_STORY_READY);
            menu_scene_known=known;
            if(known) menu_scene=native;
        }
    }
    if(saved_profile && !local_seat && menu_initialized)
        SudekiMpLanStoryMenuServiceTools(controller,w,known?&native:NULL,
            known && host_binding_ready && story_policy_initialized &&
            story_presence.ownership.phase==SUDEKIMP_PARTY_SWAP_IDLE);
    if(saved_profile && known) poll_story_swap();
    if(!local_seat && saved_profile && known) {
        SudekiMpLanStoryNativeRoster cast_roster;
        LARGE_INTEGER cast_start=timing_begin();
        if(cast_attempted && SudekiMpLanStoryObserverRoster(controller,w,&native,&cast_roster))
            (void)SudekiMpLanStoryCastService(w,&cast_roster);
        timing_end(&cast_service_timing,cast_start);
        LARGE_INTEGER control_start=timing_begin();
        service_story_ownership(controller,w,&native);
        service_story_controls(controller,w,&native);
        timing_end(&control_service_timing,control_start);
    }
    if(local_seat && saved_profile && known) service_client_control(controller,w,&native);
    now=GetTickCount(); /* Host control may have required a fresh observation. */
    if(shots_attempted && (!known || native.phase!=SUDEKIMP_LAN_STORY_READY))
        SudekiMpLanStoryShotsCloseAdmission();
    if(!local_seat && known && (!last_publish || now-last_publish>=50u)) {
        if(SudekiMpLanPartyPublishStoryScene(session,&native)) last_publish=now;
    }
    /* Capture on every controller tick (~21 ms). A 33 ms gate on a ~21 ms
     * tick captured every second tick (42-50 ms steps), which sat at the edge
     * of the client's 50 ms interpolation window and produced periodic holds
     * and 50<->66 ms delay flips. */
    if(!local_seat && known && (!last_frame_attempt || now-last_frame_attempt>=15u)) {
        SudekiMpLanStoryFrame frame;
        last_frame_attempt=now;
        LARGE_INTEGER capture_start=timing_begin();
        unsigned result=SudekiMpLanStoryCaptureMovement(&capture,session,controller,w,&native,saved_profile,now,&frame)?1u:0u;
        if(result && shots_attempted && !SudekiMpLanStoryShotsCapture(controller,w,&native,&frame)) result=0;
        unsigned published_frame=0,world_captured=0,world_count=0,world_sent=0;
        if(result) {
            ++captured_frames;
            SudekiMpLanStoryWorldFrame world;
            if(saved_profile) {
                SudekiMpLanStoryWorldSetExcludedZone(split_attempted?SudekiMpLanStorySplitTemporaryData():NULL);
                SudekiMpLanStoryWorldSetSplitAllowlist(split_attempted && SudekiMpLanStorySplitActive());
                world_captured=SudekiMpLanStoryWorldCapture(session,controller,w,&native,&frame,&world)?1u:0u;
                if(world_captured) world_count=world.count;
            }
            published_frame=SudekiMpLanPartySendStoryFrame(session,&frame)?1u:0u;
            if(saved_profile) {
                SudekiMpLanStoryPresentation presentation;
                if(SudekiMpLanStoryCapturePresentation(&capture,controller,w,&native,&frame,&presentation))
                    (void)SudekiMpLanPartySendStoryPresentation(session,&presentation);
                observe_recruitment(&native,&frame,controller,w);
            }
            /* A failed send to one endpoint must not suppress the matching
             * world batch for other endpoints. Transport independently checks
             * that this party sequence was admitted by the host. */
            if(world_captured) world_sent=SudekiMpLanPartySendStoryWorld(session,&world)?1u:0u;
            if(saved_profile && world_captured && frame.view.valid &&
                SudekiMpLanStoryWorldFrameMatches(&world,&frame)) {
                host_live_scene=native; host_live_receipt=GetTickCount();
                if(!loaded_party_mask) loaded_party_mask=native.available_mask;
                if(native.available_mask==loaded_party_mask && !host_recruit.transaction) {
                    host_seed=(SudekiMpLanStoryCatchup){.transaction=1u,
                        .target=native,.seed_scene=native,.party=frame,.world=world};
                    if(!SudekiMpLanStoryCatchupValid(&host_seed)) memset(&host_seed,0,sizeof(host_seed));
                }
            }
        }
        timing_end(&capture_timing,capture_start);
        if(result!=last_capture_result || !last_frame_trace || now-last_frame_trace>=1000u) {
            SudekiMpLogFormat("lan_story event=host_frame epoch=%lu revision=%lu available=%u captured=%u transport_accepted=%u count=%lu world_captured=%u entities=%u world_sent=%u capture_samples=%lu capture_mean_us=%lu capture_max_us=%lu input_player_mask=%u\r\n",
                (unsigned long)native.epoch,(unsigned long)native.revision,native.available_mask,
                result,published_frame,(unsigned long)captured_frames,world_captured,world_count,world_sent,
                (unsigned long)capture_timing.count,timing_mean(&capture_timing),(unsigned long)capture_timing.max_us,
                host_input_mask());
            memset(&capture_timing,0,sizeof(capture_timing));
            if(saved_profile) {
                SudekiMpLogFormat("story_perf event=controller samples=%lu gap_mean_us=%lu gap_max_us=%lu service_mean_us=%lu service_max_us=%lu cast_mean_us=%lu cast_max_us=%lu control_mean_us=%lu control_max_us=%lu\r\n",
                    (unsigned long)controller_timing.count,timing_mean(&controller_gap_timing),
                    (unsigned long)controller_gap_timing.max_us,timing_mean(&controller_timing),
                    (unsigned long)controller_timing.max_us,timing_mean(&cast_service_timing),
                    (unsigned long)cast_service_timing.max_us,timing_mean(&control_service_timing),
                    (unsigned long)control_service_timing.max_us);
                memset(&controller_timing,0,sizeof(controller_timing));
                memset(&controller_gap_timing,0,sizeof(controller_gap_timing));
                memset(&cast_service_timing,0,sizeof(cast_service_timing));
                memset(&control_service_timing,0,sizeof(control_service_timing));
            }
            last_frame_trace=now; last_capture_result=result;
        }
    }
    for(unsigned seat=1;seat<4u;++seat) {
        if(local_seat && local_seat!=seat) continue;
        SudekiMpLanPartyPeerStatus peer;
        if(!SudekiMpLanPartyPeerStatusGet(session,seat,&peer)) continue;
        if(phases[seat]!=(int)peer.phase) {
            phases[seat]=(int)peer.phase;
            SudekiMpLogFormat("lan_story event=connection seat=%u phase=%u reason=%u policy=scene_observation_only\r\n",
                seat,peer.phase,peer.failure);
        }
        /* Metadata-only observers have no native lease. Saved-story peers
         * reach this point only after the controller seam clears theirs. */
        if(!local_seat && peer.phase==SUDEKIMP_LAN_PARTY_DRAINING &&
            !host_control[seat].native_key.token)
            (void)SudekiMpLanPartyReleaseDrained(session,&peer.lease);
        if(local_seat && !saved_profile) {
            SudekiMpLanStoryScene remote;
            SudekiMpLanStoryFrame frame;
            BOOL received=FALSE;
            for(unsigned n=0;n<8u && SudekiMpLanPartyPopStoryFrame(session,&peer.lease,now,&frame);++n) {
                ++received_frames; received=TRUE;
            }
            if(received && (!last_frame_trace || now-last_frame_trace>=1000u)) {
                SudekiMpLogFormat("lan_story event=received_frame epoch=%lu revision=%lu available=%u sequence=%lu count=%lu replica_applied=0\r\n",
                    (unsigned long)frame.epoch,(unsigned long)frame.revision,frame.available_mask,
                    (unsigned long)frame.sequence,(unsigned long)received_frames);
                last_frame_trace=now;
            }
            if(SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) &&
                (last_remote_revision!=remote.revision || !last_trace || now-last_trace>=1000u)) {
                BOOL same_area=known && native.phase==SUDEKIMP_LAN_STORY_READY &&
                    remote.phase==SUDEKIMP_LAN_STORY_READY &&
                    !strcmp(native.world,remote.world) && !strcmp(native.temporary,remote.temporary);
                unsigned character=SudekiMpLanPartyLocalCharacter(session);
                unsigned view=character<4u?SudekiMpLanStoryViewSeat(&remote,character,
                    SUDEKIMP_LAN_STORY_NO_SEAT):remote.leader_seat;
                SudekiMpLogFormat("lan_story event=host_scene epoch=%lu revision=%lu phase=%u available=%u lead=%u desired_view=%u same_area=%u world=%s temporary=%s input_admitted=%u replica_applied=%u\r\n",
                    (unsigned long)remote.epoch,(unsigned long)remote.revision,
                    remote.phase,remote.available_mask,remote.leader_seat,view,same_area,
                    remote.world,remote.temporary,client_input_ready?1u:0u,presentation_sample.valid?1u:0u);
                last_trace=now; last_remote_revision=remote.revision;
            }
        }
    }
done:
    timing_end(&controller_timing,controller_start);
    SudekiMpControlUpdateObserverGateLeave(&gate);
}
BOOL SudekiMpInstallLanStoryRuntime(HMODULE module,const SudekiMpLanPartyConfig *config) {
    if(!module || !config || !config->story_observation || config->story_observation>2u || session || observer_attempted) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    session=SudekiMpLanPartyCreate(config);
    if(!session) return FALSE;
    local_seat=config->local_seat; last_trace=last_publish=last_remote_revision=0;
    saved_profile=config->story_observation==2u; runtime_thread=0;
    story_policy_initialized=story_key_focused=story_key_down=FALSE;
    story_next_request=story_pending_request=0; story_swap_traces=0;
    memset(&story_presence,0,sizeof(story_presence)); memset(&story_published,0,sizeof(story_published));
    timing_frequency.QuadPart=0; (void)QueryPerformanceFrequency(&timing_frequency);
    previous_controller_stamp.QuadPart=0;
    memset(&controller_timing,0,sizeof(controller_timing));
    memset(&controller_gap_timing,0,sizeof(controller_gap_timing));
    memset(&cast_service_timing,0,sizeof(cast_service_timing));
    memset(&control_service_timing,0,sizeof(control_service_timing));
    memset(&capture_timing,0,sizeof(capture_timing)); memset(&present_timing,0,sizeof(present_timing));
    memset(&effects_timing,0,sizeof(effects_timing));
    game_base=(uint8_t *)module;
    menu_lobby_known=menu_scene_known=host_binding_ready=FALSE;
    presentation_attempted=FALSE;
    memset(&menu_lobby,0,sizeof(menu_lobby)); memset(&menu_scene,0,sizeof(menu_scene));
    memcpy(story_lobby_tickets,config->lobby_nonce,sizeof(story_lobby_tickets));
    InterlockedExchange(&runtime_ready,0); InterlockedExchange(&client_drained,0);
    InterlockedExchange(&client_exit_prepared,0);
    InterlockedExchange(&host_drained,0);
    history_count=world_history_count=last_present_result=0; last_present_trace=last_received_frame=0;
    memset(&world_preflight_timing,0,sizeof(world_preflight_timing));
    memset(&party_apply_timing,0,sizeof(party_apply_timing));
    memset(&world_apply_timing,0,sizeof(world_apply_timing));
    memset(history_receipts,0,sizeof(history_receipts)); memset(world_receipts,0,sizeof(world_receipts));
    presented_epoch=presented_revision=0; presentation_changed=FALSE;
    memset(&host_before_recruit,0,sizeof(host_before_recruit));
    memset(&host_seed,0,sizeof(host_seed)); memset(host_catchup,0,sizeof(host_catchup));
    memset(&host_live_scene,0,sizeof(host_live_scene)); host_live_receipt=0; loaded_party_mask=0;
    memset(&client_catchup,0,sizeof(client_catchup)); client_catchup_seeded=FALSE;
    client_catchup_ack_at=0; client_catchup_trace=0;
    memset(&host_recruit,0,sizeof(host_recruit)); memset(&client_recruit,0,sizeof(client_recruit));
    memset(host_control,0,sizeof(host_control)); client_recruit_committed=FALSE;
    client_switch_prepared=client_local_selected=client_input_ready=FALSE;
    memset(&client_control_fence,0,sizeof(client_control_fence));
    memset(&client_control_connection,0,sizeof(client_control_connection));
    client_input_sequence=client_input_sent_at=0;
    memset(&client_action,0,sizeof(client_action));
    memset(&client_action_connection,0,sizeof(client_action_connection));
    client_action_serial=client_action_sent_at=0; client_action_pending=FALSE;
    client_control_trace_count=client_control_trace_state=0;
    host_control_hold_traces=0;
    dialogue_count=0; visible_dialogue_valid=FALSE; visible_dialogue_receipt=0;
    memset(&audio_trace,0,sizeof(audio_trace)); memset(&overlay_trace,0,sizeof(overlay_trace));
    memset(presented_generations,0,sizeof(presented_generations));
    memset(&history_lease,0,sizeof(history_lease)); memset(&client_seed,0,sizeof(client_seed));
    memset(&initial_objects,0,sizeof(initial_objects)); memset(&initial_object_scene,0,sizeof(initial_object_scene));
    initial_objects_attempted=initial_objects_known=FALSE;
    memset(&area_observation,0,sizeof(area_observation));
    loot_trace_attempted=FALSE;
    memset(&presentation_sample,0,sizeof(presentation_sample));
    effects_ready=FALSE;
    last_frame_attempt=last_frame_trace=captured_frames=received_frames=0;
    last_capture_result=2u; SudekiMpLanStoryCaptureReset(&capture);
    for(unsigned i=0;i<4u;++i) phases[i]=-1;
    InterlockedExchange(&stopping,0); InterlockedExchange(&observer_removed,0);
    if(saved_profile && !local_seat) {
        SudekiMpPartyAssignment assignment={.world=1u,.revision=1u};
        for(unsigned p=0;p<4u;++p) {
            assignment.generation[p]=1u; assignment.character[p]=4u;
            if((config->reserved_mask&(1u<<p)) && config->character[p]<4u) {
                assignment.humans|=(uint8_t)(1u<<p); assignment.character[p]=config->character[p];
            }
        }
        story_policy_initialized=SudekiMpPartyOwnershipInitializePresence(&story_presence.ownership,
            &assignment,1u,0u);
        if(!story_policy_initialized || !publish_story_ownership()) goto fail;
    }
    observer_attempted=TRUE;
    if(!SudekiMpLanStoryObserverInstall(module)) goto fail;
    if(saved_profile) {
        realtime_attempted=TRUE;
        if(!SudekiMpLanStoryRealtimeInstall(module)) goto fail;
        world_attempted=TRUE;
        if(!SudekiMpInitializeLanStoryWorld(module)) goto fail;
        if(!SudekiMpLanStoryMenuInitialize(module,local_seat)) goto fail;
        menu_initialized=TRUE;
        if(!local_seat) {
            host_attempted=TRUE;
            if(!SudekiMpLanStoryHostControlInstall(module,config->character[0])) goto fail;
            control_attempted=TRUE;
            if(!SudekiMpLanStoryControlInstall(module,config->character[0])) goto fail;
            activity_attempted=TRUE;
            if(!SudekiMpLanStoryActivityInitialize(module)) goto fail;
            split_attempted=TRUE;
            if(!SudekiMpLanStorySplitInstall(module)) goto fail;
            shots_attempted=TRUE;
            if(!SudekiMpLanStoryShotsInstall(module)) goto fail;
        }
    }
    if(saved_profile && local_seat) {
        input_attempted=TRUE;
        if(!SudekiMpLanStoryInputInstall(module,local_seat)) goto fail;
        quick_menu_attempted=TRUE;
        if(!SudekiMpLanStoryQuickMenuInstall(module,queue_client_skill)) goto fail;
        client_attempted=TRUE;
        if(!SudekiMpLanStoryClientInstall(module,input_closed)) goto fail;
        replica_attempted=TRUE;
        if(!SudekiMpInitializeLanStoryReplica(module)) goto fail;
        recruit_attempted=TRUE;
        if(!SudekiMpLanStoryRecruitInstall(module)) goto fail;
        local_control_attempted=TRUE;
        if(!SudekiMpLanStoryLocalControlInstall(module)) goto fail;
        aim_pose_attempted=TRUE;
        if(!SudekiMpLanAimPoseOnlyInstall(module,story_aim_witness)) goto fail;
    }
    if(saved_profile) {
        menu_attempted=TRUE;
        if(!SudekiMpLanPartyMenuNativeInstall(module,menu_admission,menu_toggle,menu_frame)) goto fail;
    }
    if(saved_profile && local_seat) {
        render_attempted=TRUE;
        if(!SudekiMpLanStoryRenderInstall(module,render_dispatch,NULL)) goto fail;
        effects_attempted=TRUE;
        if(!SudekiMpLanStoryEffectsInstall(module,effects_dispatch,NULL)) goto fail;
    }
    stop_worker=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!stop_worker) goto fail;
    worker=CreateThread(NULL,0,poll_network,NULL,0,NULL);
    if(!worker || !SudekiMpControlUpdateObserverGateEnable(&gate)) goto fail;
    if(!SudekiMpControlSeparationRegisterUpdateObserver(&owner,service)) goto fail;
    registered=TRUE;
    SudekiMpLogFormat("lan_story runtime=installed seat=%u profile=%s input_requires_native_view_ack=1 save_writes_by_mod=0\r\n",
        local_seat,saved_profile?"saved_story":"story_observe");
    return TRUE;
fail:
    {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallLanStoryRuntime()) return FALSE;
        SetLastError(error); return FALSE;
    }
}
static BOOL retire_runtime(BOOL exit_to_title) {
    if(!session && !observer_attempted) return TRUE;
    InterlockedExchange(&stopping,1);
    if(cast_attempted) SudekiMpLanStoryCastRequestStop();
    if(shots_attempted) SudekiMpLanStoryShotsCloseAdmission();
    InterlockedExchange(&runtime_ready,0);
    unsigned exited=saved_profile?SudekiMpLobbyGameplayStoryExitStatus():0u;
    if(shots_attempted && !SudekiMpLanStoryShotsUninstall()) return retain_module();
    shots_attempted=FALSE;
    if(loot_trace_attempted && !SudekiMpLanStoryLootTraceUninstall()) return retain_module();
    loot_trace_attempted=FALSE;
    /* Native Quit may have destroyed the world even if its final observation
     * was unknown. Never retry Quit or inspect the old actor leases then. */
    if(exited==2u) return retain_module();
    if(menu_initialized && !SudekiMpLanStoryMenuDrainTools()) return retain_module();
    if(quick_menu_attempted && !SudekiMpLanStoryQuickMenuUninstall()) return retain_module();
    quick_menu_attempted=FALSE;
    /* No task observer, timing owner, actor lease or realtime hook may retire
     * while a native cast can still call into its namespace. */
    if(cast_attempted && !SudekiMpLanStoryCastUninstall()) return retain_module();
    cast_attempted=FALSE;
    if(!exited) {
        if(host_attempted && !InterlockedCompareExchange(&host_drained,0,0)) {
            /* Host menu/filter ownership and recruited-player overrides are
             * separate native leases. Leave the service callback installed
             * until it has drained both; an idle host menu proves only one. */
            if(SudekiMpLanStoryHostControlRetains() || SudekiMpLanStoryControlRetains() ||
                SudekiMpLanStoryActivityRetains())
                return retain_module();
            for(unsigned p=1;p<4u;++p)
                if(host_control[p].native_key.token) return retain_module();
            InterlockedExchange(&host_drained,1);
        }
        if(split_attempted && !SudekiMpLanStorySplitUninstall()) return retain_module();
        split_attempted=FALSE;
        if(activity_attempted && !SudekiMpLanStoryActivityUninstall()) return retain_module();
        activity_attempted=FALSE;
        if(control_attempted && !SudekiMpLanStoryControlUninstall()) return retain_module();
        control_attempted=FALSE;
        if(host_attempted && !SudekiMpLanStoryHostControlUninstall()) return retain_module();
        host_attempted=FALSE;
        if(client_attempted && (SudekiMpLanStoryClientRetains() || SudekiMpLanStoryReplicaRetainsView())) {
            if(!exit_to_title || !InterlockedCompareExchange(&client_exit_prepared,0,0))
                return retain_module();
        }
        if(replica_attempted && !SudekiMpUninstallLanStoryReplica()) return retain_module();
        replica_attempted=FALSE;
    }
    presentation_sample.valid=effects_ready=FALSE;
    if(aim_pose_attempted && !SudekiMpLanAimUninstall()) return retain_module();
    aim_pose_attempted=FALSE;
    if(render_attempted && !SudekiMpLanStoryRenderUninstall()) return retain_module();
    render_attempted=FALSE;
    if(effects_attempted && !SudekiMpLanStoryEffectsUninstall()) return retain_module();
    effects_attempted=FALSE;
    if(world_attempted && !SudekiMpUninstallLanStoryWorld()) return retain_module();
    world_attempted=FALSE;
    /* Complete retryable observer/transport retirement while a spectator's
     * exact full-world pause and all input/trigger fences remain held. */
    if(split_attempted) {
        if(!SudekiMpLanStorySplitUninstall()) return retain_module();
        split_attempted=FALSE;
    }
    if(observer_attempted && !InterlockedCompareExchange(&observer_removed,0,0)) {
        if(SudekiMpLanStoryObserverUninstall()) InterlockedExchange(&observer_removed,1);
        else {
            /* Retry from the verified game-thread service, never free a
             * trampoline while a zone continuation could still return to it. */
            return retain_module();
        }
    }
    SudekiMpControlUpdateObserverGateDisable(&gate);
    if(registered && !SudekiMpControlSeparationUnregisterUpdateObserver(&owner)) return retain_module();
    registered=FALSE;
    SudekiMpControlUpdateObserverGateDrain(&gate);
    if(stop_worker) SetEvent(stop_worker);
    if(worker) {
        if(WaitForSingleObject(worker,1000u)!=WAIT_OBJECT_0) return retain_module();
        CloseHandle(worker); worker=NULL;
    }
    if(stop_worker) { CloseHandle(stop_worker); stop_worker=NULL; }
    if(exit_to_title && !exited) {
        if(client_attempted && !SudekiMpLanStoryClientDrain(NULL)) {
            (void)SudekiMpLanStoryClientReacquireExit(NULL);
            return retain_module();
        }
        InterlockedExchange(&client_drained,1);
        /* No other fallible teardown operation belongs between balancing the
         * native pause and the validated synchronous world exit. The bridge
         * keeps its entered/returned evidence for retries and never repeats
         * native Quit after it has entered. */
        exited=SudekiMpLobbyGameplayStoryExit();
        if(!exited) {
            if(client_attempted && SudekiMpLanStoryClientReacquireExit(NULL))
                InterlockedExchange(&client_drained,0);
            return retain_module();
        }
        if(exited!=1u) return retain_module();
    }
    if(exited && activity_attempted && !SudekiMpLanStoryActivityNativeExitReturned()) return retain_module();
    if(activity_attempted && !SudekiMpLanStoryActivityUninstall()) return retain_module();
    activity_attempted=FALSE;
    if(exited && local_control_attempted && !SudekiMpLanStoryLocalControlNativeExitReturned()) return retain_module();
    if(local_control_attempted && !SudekiMpLanStoryLocalControlUninstall()) return retain_module();
    local_control_attempted=FALSE;
    if(!SudekiMpLanStoryCinematicAudioReset()) return retain_module();
    if(exited && recruit_attempted && !SudekiMpLanStoryRecruitNativeExitReturned()) return retain_module();
    if(recruit_attempted && !SudekiMpLanStoryRecruitUninstall()) return retain_module();
    recruit_attempted=FALSE;
    if(exited && client_attempted && !SudekiMpLanStoryClientNativeExitReturned()) return retain_module();
    /* Verified native reset destroys these borrowed task identities. Forget
     * them before frontend tasks can reuse their addresses; no task is being
     * cancelled or declared normally retired by this plain-data reset. */
    if(exited && !SudekiMpLanStoryTaskTraceForgetExitedWorld()) return retain_module();
    if(client_attempted && !SudekiMpLanStoryClientUninstall()) return retain_module();
    client_attempted=FALSE;
    if(menu_attempted && !SudekiMpLanPartyMenuNativeUninstall()) return retain_module();
    menu_attempted=FALSE;
    if(menu_initialized && !SudekiMpLanStoryMenuReset()) return retain_module();
    menu_initialized=FALSE;
    if(input_attempted && !SudekiMpLanStoryInputUninstall()) return retain_module();
    input_attempted=FALSE;
    if(realtime_attempted && !SudekiMpLanStoryRealtimeUninstall()) return retain_module();
    realtime_attempted=FALSE;
    SudekiMpLanPartyDestroy(session,TRUE); session=NULL;
    observer_attempted=FALSE;
    return TRUE;
}
BOOL SudekiMpUninstallLanStoryRuntime(void) { return retire_runtime(FALSE); }
BOOL SudekiMpLanStoryRuntimeExitToTitle(void) {
    if(!saved_profile) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    return retire_runtime(TRUE);
}
