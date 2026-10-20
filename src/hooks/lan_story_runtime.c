#include "hooks/lan_story_runtime.h"
#include "network/lan_story_step_away.h"
#include "ui/story_away_banner.h"
#include "hooks/lan_story_host_pause.h"
#include "hooks/lan_story_name_tags.h"
#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_test_start.h"
#include "hooks/lan_story_area_follow.h"
#include "hooks/lan_story_xp.h"
#include "hooks/lan_story_spawn_trace.h"
#include "hooks/lan_story_death_hold.h"
#include "hooks/story_enemy_targeting.h"
#include "hooks/lan_story_sky.h"
#include "hooks/lan_story_music.h"
#include "hooks/lan_story_character_audio.h"
#include "hooks/lan_story_zone_banner.h"
#include "hooks/lan_story_shop.h"
#include "hooks/lan_story_npc_gesture.h"
#include "hooks/lan_story_combo_hud.h"
#include "cleanroom/engine.h"
#include "engine/spirit_instance_abi.h"
#include "engine/cast_light_abi.h"
#include "hooks/lan_story_objects.h"
#include "hooks/lan_story_area_membership.h"
#include "hooks/lan_story_loot_trace.h"
#include "hooks/lan_story_snapshot.h"
#include "hooks/lan_story_shots.h"
#include "hooks/lan_story_muzzle.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "hooks/lan_story_load.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_story_client.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_replica.h"
#include "hooks/lan_story_world.h"
#include "hooks/lan_story_effects.h"
#include "hooks/lan_story_realtime.h"
#include "hooks/lan_story_ambient.h"
#include "hooks/lan_story_area_fade.h"
#include "hooks/lan_story_quick_menu.h"
#include "hooks/lan_story_render.h"
#include "hooks/lan_story_host_control.h"
#include "hooks/lan_story_control.h"
#include "hooks/lan_story_cast.h"
#include "hooks/lan_story_door_camera.h"
#include "hooks/lan_story_client_camera.h"
#include "hooks/lan_story_inventory.h"
#include "hooks/lan_story_remote_use.h"
#include "hooks/lan_story_remote_interior.h"
#include "hooks/lan_story_main_menu.h"
#include "hooks/armour_menu.h"
#include "ui/party_menu.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/lan_arena_spirit_vfx.h"
#include "hooks/lan_story_recruit.h"
#include "hooks/lan_story_local_control.h"
#include "hooks/lan_story_context_prompt.h"
#include "hooks/lan_story_activity.h"
#include "hooks/lan_story_cinematic.h"
#include "ui/story_cinematic_view.h"
#include "hooks/lan_story_menu.h"
#include "hooks/lan_party_menu_native.h"
#include "hooks/lobby_gameplay.h"
#include "hooks/lan_story_split.h"
#include "hooks/lan_story_research54.h"
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
static BOOL quick_menu_attempted,remote_use_attempted,shop_attempted;
static BOOL main_menu_attempted; static HMODULE main_menu_module;
static void action_trace(const char *reason,unsigned kind,unsigned slot,unsigned detail) {
    SudekiMpLogFormat("story_action event=expired reason=%s kind=%u slot=%u detail=%#x\r\n",reason,kind,slot,detail);
}
static BOOL main_menu_open(void) { return main_menu_attempted && local_seat && SudekiMpLanStoryMainMenuVisible(); }
/* H1b: this client's own shop screen (D11) holds the body and the session
 * menu key like the Main Menu does. */
static BOOL shop_open(void) { return shop_attempted && local_seat && SudekiMpLanStoryShopActive(); }
/* H1b: an examinable's box of this client's own (the gramophone) holds the
 * body and takes the interact press until it closes, as natively. */
static BOOL examine_open(void) { return shop_attempted && local_seat && SudekiMpLanStoryZoneBannerExamineActive(); }
static BOOL table_modal(void) { return shop_open() || examine_open(); }
static BOOL inventory_bound;
typedef struct StoryPendingLoot {
    struct StoryPendingLoot *next;
    SudekiMpStoryInventoryEvent event;
    SudekiMpLanPartyLease recipients[4];
    unsigned mask;
} StoryPendingLoot;
static StoryPendingLoot *loot_head,*loot_tail;
static unsigned loot_pending,loot_fault_reported;
static BOOL loot_capture_fault;
static BOOL cast_attempted;
static BOOL host_attempted,menu_initialized,menu_lobby_known,menu_scene_known,host_binding_ready;
/* Host front in a native use animation (chest, breakable): remote players
 * keep their own control while only the front is held (H1 (a)). */
static BOOL host_front_use;
static BOOL control_attempted,recruit_attempted,local_control_attempted,activity_attempted;
static BOOL split_attempted;
static uint8_t traced_foreign;
static const char *client_pending_trace;
static unsigned client_pending_traces;
static const char *control_block_trace[4];
static unsigned control_block_traces;
static volatile LONG runtime_ready,client_exit_prepared,client_drained,host_drained;
static uint8_t *game_base;
static BOOL client_area_matches(const SudekiMpLanStoryScene *native,const SudekiMpLanStoryScene *remote);
/* Cosmetic native objects retain their own weak handles. Keep their session
 * and resource obligations until Reset succeeds at the residency boundary. */
static struct {
    BOOL active;
    SudekiMpLanPartyLease lease;
    uint64_t stream,serial;
    uint32_t epoch,sequence;
    BOOL ready;
    char current[SUDEKIMP_STORY_AREA_NAME];
} story_vfx;
static SudekiMpStoryMuzzleProjection story_muzzles;
static uint64_t project_story_muzzles(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyPeerStatus *peer,uint32_t now,SudekiMpStoryVfx *copy);
static BOOL vfx_reset_residency(void *context) {
    BOOL *done=context;
    *done=SudekiMpLanArenaSpiritVfxResetVisuals((HMODULE)game_base);
    return TRUE; /* Even a pending reset can have changed native residency. */
}
static BOOL reset_story_vfx(const SudekiMpControlUpdateDispatchWitness *w) {
    if(!story_vfx.active) return TRUE;
    BOOL done=FALSE;
    if(!SudekiMpLanStoryClientResidencyCall(w,vfx_reset_residency,&done) || !done) return FALSE;
    story_vfx.active=FALSE;
    memset(&story_muzzles,0,sizeof(story_muzzles));
    return TRUE;
}
typedef struct StoryVfxCall {
    const SudekiMpStoryVfx *state;
    SudekiMpLanPartyLease lease;
    BOOL ready;
    uint64_t prepared;
    SudekiMpLanStoryNativeRoster roster;
} StoryVfxCall;
static BOOL vfx_sound_setup(void *context,const SudekiMpLanArenaSpiritVfxSnapshot *visual,void *sound) {
    /* Native PlaySFXWithAll inherits the emitting actor's bank. Parent-free
     * replicas keep only the sound listener; provide the same exact roster
     * bank without installing any actor-event forwarding listener. */
    BOOL tal_spirit=visual->kind==SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE ||
        visual->kind==SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_ONE;
    if(!visual->skill_sequence || (!tal_spirit && !SudekiMpLanStorySkillVfxOwnerMask(visual->kind))) return TRUE;
    unsigned character;
    switch(visual->owner_actor_type) {
        case 5u: character=0u;break;
        case 14u: character=1u;break;
        case 0u: case 35u: character=2u;break;
        case 1u: character=3u;break;
        default: return FALSE;
    }
    return SudekiMpStoryCharacterAudioBindEffect(context,character,sound);
}
static BOOL vfx_service_residency(void *context) {
    StoryVfxCall *call=context; const SudekiMpStoryVfx *s=call->state;
    if(!story_vfx.active) {
        if(story_vfx.serial==UINT64_MAX) return FALSE;
        ++story_vfx.serial; story_vfx.active=TRUE;
        story_vfx.lease=call->lease; story_vfx.stream=s->stream; story_vfx.epoch=s->epoch;
        story_vfx.sequence=0; story_vfx.ready=FALSE;
        memcpy(story_vfx.current,s->current,sizeof(story_vfx.current));
    }
    call->ready=SudekiMpLanStoryVfxServiceVisuals((HMODULE)game_base,s,story_vfx.serial,call->prepared,vfx_sound_setup,&call->roster);
    return TRUE; /* Adopt cache/spawn/retire changes even when resources wait. */
}
static void vfx_wait(const char *reason) {
    static const char *previous; static unsigned reports;
    if(reports<24u && (!previous || strcmp(previous,reason))) {
        ++reports; previous=reason;
        SudekiMpLogFormat("story_vfx event=waiting reason=%s\r\n",reason);
    }
}
static void follow_story_vfx(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyPeerStatus *peer,uint32_t now,const SudekiMpLanStoryScene *native) {
    static DWORD attempted_at;
    if(attempted_at && now-attempted_at<15u) return;
    attempted_at=now;
    SudekiMpStoryVfx state;
    if(!SudekiMpLanPartyGetStoryVfx(session,&peer->lease,now,&state)) {
        vfx_wait("fresh_packet"); return; /* Unknown retains. */
    }
    /* This is the post-controller seam, not the presentation pause seam.
     * Use its freshly observed scene; ResidencyCall proves the retained
     * roster/pause with this dispatch witness before any native operation. */
    SudekiMpLanStoryScene remote;
    if(!SudekiMpLanPartyGetStoryScene(session,&peer->lease,now,&remote)) {vfx_wait("fresh_scene");return;}
    if(remote.epoch!=state.epoch || remote.revision!=state.revision) {vfx_wait("scene_fence");return;}
    if(!native || native->phase!=SUDEKIMP_LAN_STORY_READY || remote.phase!=SUDEKIMP_LAN_STORY_READY ||
        native->available_mask!=remote.available_mask || !client_area_matches(native,&remote)) {
        vfx_wait("native_scene");return;
    }
    SudekiMpStoryEffectState area={0}; memcpy(area.current,state.current,sizeof(area.current));
    if(!SudekiMpLanStoryEffectsInCurrentArea(&area)) {vfx_wait("native_area");return;}
    if(story_vfx.active && (story_vfx.lease.token!=peer->lease.token ||
        story_vfx.lease.generation!=peer->lease.generation || story_vfx.lease.seat!=peer->lease.seat ||
        story_vfx.stream!=state.stream || story_vfx.epoch!=state.epoch ||
        _stricmp(story_vfx.current,state.current)) && !reset_story_vfx(w)) return;
    /* Reapplying the same absolute phase can rewind native particle work.
     * Resource/adoption failures remain retryable on the next dispatch. */
    if(story_vfx.active && story_vfx.ready && story_vfx.sequence==state.sequence) return;
    uint64_t prepared=project_story_muzzles(w,peer,now,&state);
    StoryVfxCall call={.state=&state,.lease=peer->lease,.prepared=prepared};
    if(!SudekiMpLanStoryObserverRoster(controller,w,native,&call.roster)) {vfx_wait("sound_roster");return;}
    vfx_wait("servicing");
    BOOL adopted=SudekiMpLanStoryClientResidencyCall(w,vfx_service_residency,&call);
    story_vfx.ready=adopted && call.ready;
    if(story_vfx.ready) story_vfx.sequence=state.sequence;
    static DWORD last_at; static unsigned reports;
    if((state.count || !adopted || !call.ready || !last_at) && reports<32u && (!last_at || now-last_at>=1000u)) {
        ++reports; last_at=now;
        SudekiMpLogFormat("story_vfx event=client count=%u adopted=%u ready=%u error=%lu\r\n",
            state.count,(unsigned)adopted,(unsigned)call.ready,(unsigned long)GetLastError());
    }
}
static BOOL door_camera_ready;
/* Client native Exploration camera (#71): default on; [StoryAreas] ClientMathCamera=true keeps the mathematical view. */
static BOOL client_native_camera=TRUE,client_camera_attempted;
void SudekiMpLanStoryRuntimeSetClientNativeCamera(BOOL enabled) { client_native_camera=enabled; }
/* Animated summon pose/phase follows the controller/party cadence, not the
 * slower area/sky snapshot. Capture area membership afresh for each packet;
 * unknown native observations never manufacture an empty effect roster. */
static void publish_story_vfx(uint32_t now,void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *native) {
    static uint32_t attempted_at;
    if(attempted_at && now-attempted_at<15u) return;
    attempted_at=now;
    SudekiMpStoryAreaState area;
    BOOL area_captured=SudekiMpLanStoryAreaCapture(&area);
    SudekiMpStoryVfx vfx;
    BOOL captured=area_captured && cast_attempted && SudekiMpLanStoryCastCaptureVisuals(&vfx),published=FALSE;
    /* World observation must never turn a transient pending item/model into
     * a failed party casting initialization. It gates publication only. */
    if(captured) captured=SudekiMpLanStoryWorldVisuals(vfx.stream,&vfx);
    if(captured) {
        memcpy(vfx.current,area.current,sizeof(vfx.current));
        published=SudekiMpLanPartyPublishStoryVfx(session,&vfx);
    }
    SudekiMpStoryCastView camera; SudekiMpLanStoryNativeRoster door_roster; BOOL door_roster_known=FALSE;
    if(area_captured && cast_attempted) for(unsigned character=0;character<4u;++character)
        if(SudekiMpLanStoryCastCaptureView(character,&camera)) {
            /* Door camera (#67): a remote character in an open door's traverse
             * zone gets the door's authored static view in place of "normal". */
            if(!camera.kind && door_camera_ready) {
                if(!door_roster_known) door_roster_known=SudekiMpLanStoryObserverRoster(controller,w,native,&door_roster);
                if(door_roster_known) (void)SudekiMpLanStoryDoorCameraView(&door_roster,character,now,&camera);
            }
            memcpy(camera.current,area.current,sizeof(camera.current));
            (void)SudekiMpLanPartyPublishStoryCastView(session,&camera);
        }
    static unsigned reports,last_health; static DWORD trace_at;
    BOOL ready=SudekiMpLanStoryCastReady();
    unsigned health=(ready?1u:0u)|(area_captured?2u:0u)|(captured?4u:0u)|(published?8u:0u);
    if((reports<12u || health!=last_health) && (!trace_at || now-trace_at>=2000u)) {
        ++reports; trace_at=now;
        last_health=health;
        SudekiMpLogFormat("story_vfx event=host cast_ready=%u area=%u captured=%u published=%u count=%u\r\n",
            (unsigned)ready,(unsigned)area_captured,(unsigned)captured,(unsigned)published,captured?vfx.count:0u);
    }
}
static SudekiMpLobbyStatus menu_lobby;
static SudekiMpLanStoryScene menu_scene;
static uint64_t story_lobby_tickets[4];
static DWORD runtime_thread,last_present_trace,last_received_frame;
static BOOL enemy_targeting_attempted;
static struct {BOOL known;uint32_t epoch,revision;unsigned count,last[4];} away_refused_logs;
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
static BOOL story_step_away=TRUE;
static SudekiMpStoryAwayClient away_client;
static SudekiMpStoryCombatSample away_combat;
static uint32_t away_pending_request,away_retry_at,away_ui_at;
static BOOL away_ui_seen;
static SudekiMpLanPartyLease away_connection;
static void service_step_away(void);
void SudekiMpLanStoryRuntimeSetStepAway(BOOL enabled) { story_step_away=enabled; }
BOOL SudekiMpLanStoryRuntimeStepAwayEnabled(void) { return saved_profile && story_step_away; }
BOOL SudekiMpLanStoryRuntimeCharacterAway(unsigned c) {
    SudekiMpLanPartyPresence presence;
    if(!session || !saved_profile || c>=4u || runtime_thread!=GetCurrentThreadId() ||
        !SudekiMpLanPartyGetPresence(session,&presence)) return FALSE;
    unsigned p=SudekiMpPartyCharacterOwner(&presence.ownership.assignment,c);
    return p<4u && (presence.ownership.away&(1u<<p));
}
BOOL SudekiMpLanStoryRuntimeAwayIntent(void) { return away_client.phase!=SUDEKIMP_STORY_AWAY_NONE; }
BOOL SudekiMpLanStoryRuntimeRequestAway(BOOL away) {
    if(!saved_profile || !story_step_away || !session || !local_seat ||
        runtime_thread!=GetCurrentThreadId() || SudekiMpLanPartyLocalCharacter(session)>=4u ||
        InterlockedCompareExchange(&stopping,0,0)) return FALSE;
    SudekiMpStoryAwayIntent(&away_client,away,GetTickCount());
    SudekiMpLanStoryInputClear();
    return TRUE;
}

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
static SudekiMpStoryCastView presented_cast_view;
static BOOL host_cinematic_view;
/* One clock for ordinary presentation and the independently retained camera.
 * Source-frame receipts and gameplay readiness never advance with this clock. */
static struct {
    SudekiMpLanArenaOwnerViewLease owner;
    uint32_t tick;
    BOOL known,applied;
} native_camera_clock;
static SudekiMpLanPartyLease client_control_connection;
static uint32_t client_block_sent_at;
static BOOL client_block_last_held,client_block_sent;
static uint32_t client_input_sequence,client_input_sent_at,client_ranged_sequence,client_block_sequence;
static SudekiMpStoryMeleeSender client_melee_sender;
static uint32_t client_melee_generation;
static uint32_t client_ranged_physical_press,client_ranged_wire_press;
static SudekiMpLanStoryActionRequest client_action;
static SudekiMpLanPartyLease client_action_connection;
static uint32_t client_action_serial,client_action_sent_at;
static BOOL client_action_pending;
static unsigned client_control_trace_state,client_control_trace_count;
static unsigned host_control_hold_traces;
static struct {
    SudekiMpLanPartyLease connection,native_key;
    SudekiMpLanStoryControlFence fence;
    /* Borrowed identity only. Every use revalidates native split residency
     * and actor ownership; this is never a copied dispatch witness. */
    SudekiMpLanStoryNativeRoster exterior_roster;
    BOOL acquired,draining,ready,input_held;
    uint32_t transaction,last_input_at;
    /* Receipt of this player's latest movement packet. Never zeroed by a
     * host-side hold (last_input_at is): it only says the client is still
     * sending (H1b shop visits end a member whose client stopped). */
    uint32_t heard_at;
    SudekiMpLanStoryMovement movement;
    SudekiMpLanStoryRanged ranged;
    uint32_t ranged_at,ranged_press;
    SudekiMpLanStoryBlock block;
    uint32_t block_at;
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
    /* Identity only: compared with a freshly observed roster, never reused as
     * a native access witness across dispatches. */
    SudekiMpLanStoryNativeRoster native_roster;
    uint32_t sequence,host_tick,receipt;
} presentation_sample;
static BOOL client_revision_gap;
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
        /* H1a: remote chest use, same native thread and boundary. */
        remote_use_attempted=TRUE;
        if(!SudekiMpLanStoryRemoteUseInstall((HMODULE)game_base))
            SudekiMpLogFormat("story_remote_use event=unavailable error=%lu\r\n",(unsigned long)GetLastError());
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
static BOOL native_camera_owner_same(const SudekiMpLanArenaOwnerViewLease *owner) {
    const SudekiMpLanArenaOwnerViewLease *before=&native_camera_clock.owner;
    return native_camera_clock.known && owner->valid && before->valid &&
        owner->camera_mode==before->camera_mode && owner->camera==before->camera &&
        owner->render_state==before->render_state && owner->scene_manager==before->scene_manager &&
        owner->scene_renderer==before->scene_renderer &&
        owner->scene_render_state_slot==before->scene_render_state_slot;
}
static BOOL present_native_camera(const SudekiMpLanStoryNativeRoster *roster,
    uint32_t now,float look_x,float look_y) {
    SudekiMpLanArenaOwnerViewLease owner;
    if(!roster || roster->leader_character>=4u || !replica_exact(roster,NULL) ||
        !SudekiMpLanStoryInputExact(roster->controller,roster->actors[roster->leader_character]) ||
        !SudekiMpLanStoryLocalControlAdoptNative(roster,replica_exact,NULL) ||
        !SudekiMpLanStoryLocalControlViewOwner(&owner)) return FALSE;
    BOOL same=native_camera_owner_same(&owner);
    if(same && now==native_camera_clock.tick)
        return native_camera_clock.applied &&
            SudekiMpLanStoryLocalControlAdoptNative(roster,replica_exact,NULL);
    float dt=same && now-native_camera_clock.tick<200u?
        (float)(now-native_camera_clock.tick)/1000.0f:0.0f;
    native_camera_clock.owner=owner; native_camera_clock.tick=now;
    native_camera_clock.known=TRUE; native_camera_clock.applied=FALSE;
    native_camera_clock.applied=SudekiMpLanStoryClientCameraTick(&owner,dt,look_x,look_y) &&
        SudekiMpLanStoryLocalControlAdoptNative(roster,replica_exact,NULL);
    return native_camera_clock.applied;
}
static BOOL client_revision_gap_camera(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *unused);
static BOOL audio_present_exact(void *context) {
    return SudekiMpLanStoryClientRosterExact(context);
}
static BOOL audio_cleanup_exact(void *context) {
    return SudekiMpLanStoryClientCleanupRosterExact(context);
}
/* Shot audio records waiting for a Present that is not coming (#31/#57). */
static BOOL drain_shot_audio(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)scene; (void)unused;
    return SudekiMpStoryShotAudioDrain(audio_present_exact,(void *)roster);
}
static BOOL stop_stale_audio(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)scene; (void)unused;
    BOOL ok=SudekiMpLanStoryCinematicAudioStop(audio_present_exact,(void *)roster);
    for(unsigned c=0;c<4;++c)if(!SudekiMpStoryCharacterAudioStop(c,audio_present_exact,(void *)roster))ok=FALSE;
    return ok;
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
/* #42 diagnostic: which presentation step refused (rate limited). */
static void present_refusal(const char *step,DWORD error) {
    static const char *last; static DWORD last_error; static uint32_t at; static unsigned logs;
    uint32_t now=GetTickCount();
    if(logs<200u && (step!=last || error!=last_error || now-at>=3000u)) {
        ++logs; last=step; last_error=error; at=now;
        SudekiMpLogFormat("lan_story event=present_refused step=%s error=%lu\r\n",step,(unsigned long)error);
    }
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
        present_refusal("prepare",prepare_error);
        return FALSE;
    }
    /* Host cutscene (#54): while the host's letterbox is up, a client that
     * drives its own character shows the host's cinematic camera too (same
     * view publication spectators use), and its own camera resumes after. */
    BOOL cinematic=client_local_selected && frame->dialogue && !shop_open() &&
        (frame->dialogue->flags&SUDEKIMP_STORY_LETTERBOX_PRESENT) && frame->party->view.valid;
    if(cinematic!=host_cinematic_view)
        SudekiMpLogFormat("lan_story_client event=host_cinematic_view active=%u\r\n",(unsigned)cinematic);
    if(cinematic && !SudekiMpLanStoryLocalControlRestoreCastView(roster,replica_exact,NULL)) {
        present_refusal("cast_view_restore",GetLastError());
        (void)SudekiMpLanStoryWorldCancelPrepared(); return FALSE;
    }
    if(cinematic) memset(&presented_cast_view,0,sizeof(presented_cast_view));
    started=timing_begin();
    BOOL party_applied=SudekiMpLanStoryReplicaApply(roster,frame->party,
        (!client_switch_prepared && !client_local_selected) || cinematic,replica_exact,NULL);
    timing_end(&party_apply_timing,started);
    if(!party_applied) {
        present_refusal("party_apply",GetLastError());
        (void)SudekiMpLanStoryWorldCancelPrepared(); return FALSE;
    }
    started=timing_begin();
    BOOL world_applied=SudekiMpLanStoryWorldApplyPrepared(roster,frame->world,replica_exact,NULL);
    timing_end(&world_apply_timing,started);
    if(host_cinematic_view && !cinematic && client_local_selected) {
        if(!SudekiMpLanStoryReplicaRestoreView(roster,replica_exact,NULL))
            SudekiMpLogWrite("lan_story_client event=host_cinematic_view_restore result=retained\r\n");
    }
    host_cinematic_view=cinematic;
    if(world_applied && client_local_selected && !cinematic) {
        float yaw=0,pitch=0;
        if((client_input_ready || away_client.phase!=SUDEKIMP_STORY_AWAY_NONE) && !SudekiMpLanStoryMenuCapturesInput() &&
            !SudekiMpLanStoryQuickMenuCapturesInput() &&
            !SudekiMpLanStoryInputOrbit(roster->controller,roster->actors[client_control_fence.character],
                client_control_fence.transaction,&yaw,&pitch)) {
            client_input_ready=FALSE; SudekiMpLanStoryInputClear();
            yaw=pitch=0;
        }
        SudekiMpStoryCastView camera;
        SudekiMpLanPartyPeerStatus peer;
        const SudekiMpLanStoryView *cast_view=NULL;
        if(SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
            SudekiMpLanPartyGetStoryCastView(session,&peer.lease,&client_control_fence,
                GetTickCount(),frame->party->host_tick,&camera) &&
            camera.fence.epoch==frame->party->epoch && camera.revision==frame->party->revision &&
            camera.fence.actor_generation==frame->party->actors[client_control_fence.character].generation) {
            SudekiMpStoryEffectState area={0}; memcpy(area.current,camera.current,sizeof(area.current));
            if(SudekiMpLanStoryEffectsInCurrentArea(&area)) cast_view=&camera.view;
        }
        const SudekiMpLanStoryView normal={0};
        if(!cast_view && presented_cast_view.kind) {
            SudekiMpStoryAreaState area;
            if(!SudekiMpLanStoryControlFenceSame(&presented_cast_view.fence,&client_control_fence) ||
                presented_cast_view.fence.epoch!=frame->party->epoch ||
                (SudekiMpLanStoryAreaCapture(&area) && _stricmp(area.current,presented_cast_view.current)))
                cast_view=&normal; /* Positively changed owner/world/area, not a timeout. */
        }
        /* Native camera (#71): with no cast/door override, let the real
         * Exploration camera advance with this frame's time and adopt its
         * view; any refusal falls back to the mathematical presentation. */
        BOOL native_view=FALSE;
        static BOOL override_was_active,reseat_pending;
        /* H1b Kamo talk: this player's own conversation has the native speech
         * camera selected; the client camera gives way (no tick, no cast
         * write) until the conversation selects "default" again. */
        int speech_camera=client_camera_attempted?SudekiMpLanStoryClientCameraSpeechObserve():SUDEKIMP_SPEECH_CAMERA_UNKNOWN;
        BOOL speech_yield=shop_attempted && SudekiMpLanStoryShopSpeechCameraHeld() &&
            speech_camera==SUDEKIMP_SPEECH_CAMERA_SPEECH;
        if(speech_camera==SUDEKIMP_SPEECH_CAMERA_SPEECH && !speech_yield) {
            static unsigned unowned_logs;
            if(unowned_logs<4u) { ++unowned_logs; SudekiMpLogWrite("story_client_camera event=speech_unowned policy=not_yielded\r\n"); }
        }
        BOOL override_now=(cast_view && cast_view->valid) || presented_cast_view.kind || SudekiMpLanStoryLocalControlCastActive() ||
            speech_yield;
        if(override_was_active && !override_now) reseat_pending=TRUE;
        if(SudekiMpLanStoryRemoteInteriorClientTakeReseat()) reseat_pending=TRUE;
        override_was_active=override_now;
        if(client_camera_attempted && !override_now) {
            SudekiMpLanArenaOwnerViewLease owner; float look_x=0,look_y=0;
            if(reseat_pending && speech_camera!=SUDEKIMP_SPEECH_CAMERA_SPEECH && SudekiMpLanStoryLocalControlViewOwner(&owner)) {
                /* H1b: settled in a house with an authored static camera, use
                 * it (as its cluster script does natively); back out, the
                 * vanilla exit's Exploration; else the ordinary reseat. */
                static BOOL house_camera;
                const char *house=SudekiMpLanStoryRemoteInteriorClientHouseCamera();
                if(house && SudekiMpLanStoryClientCameraHouseStatic(&owner,house)) house_camera=TRUE;
                else if(house_camera) { (void)SudekiMpLanStoryClientCameraHouseRelease(&owner); house_camera=FALSE; }
                else (void)SudekiMpLanStoryClientCameraReseat(&owner);
                reseat_pending=FALSE;
            }
            if(!SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput())
                (void)SudekiMpLanStoryInputLookAxes(&look_x,&look_y);
            native_view=present_native_camera(roster,GetTickCount(),look_x,look_y);
        }
        if(speech_yield) native_view=TRUE;
        if(!native_view && !SudekiMpLanStoryLocalControlPresentCast(roster,replica_exact,NULL,yaw,pitch,cast_view)) {
            present_refusal("local_control",GetLastError()); return FALSE; }
        if(cast_view==&camera.view) presented_cast_view=camera;
        else if(cast_view==&normal) memset(&presented_cast_view,0,sizeof(presented_cast_view));
        static unsigned camera_kind=3,camera_reports;
        if(cast_view==&camera.view && camera.kind!=camera_kind && camera_reports<64) {
            ++camera_reports; camera_kind=camera.kind;
            SudekiMpLogFormat("story_cast_view event=client character=%u kind=%u cast=%u tick=%lu world_tick=%lu\r\n",
                camera.fence.character,camera.kind,camera.cast_sequence,
                (unsigned long)camera.tick,(unsigned long)frame->party->host_tick);
        }
    }
    if(world_applied) {
        BOOL combo_visible=client_local_selected &&
            (client_control_fence.character==0u || client_control_fence.character==2u) &&
            !cinematic && !presented_cast_view.kind && !SudekiMpLanStoryMenuCapturesInput() &&
            !SudekiMpLanStoryQuickMenuCapturesInput();
        (void)SudekiMpLanStoryComboHudPresent(roster,frame->party,combo_visible,replica_exact,NULL);
        /* Host-authoritative party HP/SP for the client's HUD (#54): the
         * host's native stats travel in every frame; mirror them into the
         * client's presentation replica only when they differ. */
        static const SudekiMpCleanroomActor resource_types[4]={SUDEKIMP_CLEANROOM_BUKI,
            SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
        for(unsigned c=0;c<4u;++c) if(frame->party->available_mask&(1u<<c)) {
            float hp=0.0f,sp=0.0f;
            const SudekiMpLanStoryActor *a=&frame->party->actors[c];
            if(SudekiMpCleanroomEngineActorResources(resource_types[c],&hp,&sp) &&
                ((uint32_t)hp!=a->hp || (uint32_t)sp!=a->sp))
            {
                BOOL set=SudekiMpCleanroomEngineSetActorResources(resource_types[c],(float)a->hp,(float)a->sp);
                static unsigned resource_logs;
                if(resource_logs<200u) { ++resource_logs;
                    SudekiMpLogFormat("lan_story_client event=party_resources character=%u hp=%lu->%lu sp=%lu->%lu ok=%u\r\n",
                        c,(unsigned long)hp,(unsigned long)a->hp,(unsigned long)sp,(unsigned long)a->sp,(unsigned)set); }
            }
        }
        /* Character cues follow native host ownership; they do not reopen
         * the contained client's actor simulation or block/attack state. */
        {
            /* The same proof the shot audio uses for "this game's own". */
            unsigned own=session?SudekiMpLanPartyLocalCharacter(session):4u;
            SudekiMpStoryCharacterAudioSetLocalCharacter(
                (own<4u && client_local_selected && roster->leader_character==own)?own:4u);
        }
        for(unsigned c=0;c<4;++c) {
            SudekiMpStoryAudio audio;
            if(frame->connection && (roster->available_mask&(1u<<c)) &&
                SudekiMpLanPartyGetStoryAudio(session,frame->connection,GetTickCount(),c,&audio) &&
                audio.epoch==frame->party->epoch && audio.revision==frame->party->revision)
                (void)SudekiMpStoryCharacterAudioPresent(roster,&audio,audio_present_exact,(void *)roster);
            else (void)SudekiMpStoryCharacterAudioStop(c,audio_present_exact,(void *)roster);
        }
        if(frame->connection) {
            /* Ranged shot cues (party weapons, Kurian Shooter missiles): this
             * game plays them itself, positionally, on its own emitters and
             * from its own data (#31/#57; owner 2026-10-18). A shot is this
             * game's own only once it positively controls that character. */
            SudekiMpStoryVfx shot_vfx; uint32_t shot_now=GetTickCount();
            BOOL vfx_fresh=SudekiMpLanPartyGetStoryVfx(session,frame->connection,shot_now,&shot_vfx) &&
                shot_vfx.epoch==frame->party->epoch && shot_vfx.revision==frame->party->revision;
            unsigned own=session?SudekiMpLanPartyLocalCharacter(session):4u;
            /* The newest received journal (as the muzzle projection uses), not
             * the interpolated sample: that carries the older bracket's journal,
             * lags the flash, and can step back after a timeline reset. */
            const SudekiMpLanStoryFrame *shot_journal=frame->party;
            if(history_count && presentation_fresh(shot_now,history_receipts[history_count-1u]) &&
                history_lease.token==frame->connection->token &&
                history_lease.generation==frame->connection->generation &&
                history[history_count-1u].epoch==frame->party->epoch &&
                history[history_count-1u].revision==frame->party->revision)
                shot_journal=&history[history_count-1u];
            SudekiMpStoryShotAudioInput shot_input={.now=shot_now,
                .namespace_token=frame->connection->token^((uint64_t)frame->connection->generation<<40),
                .local_character=(own<4u && client_local_selected && roster->leader_character==own)?own:4u,
                .available_mask=roster->available_mask,.journal=shot_journal,.vfx=vfx_fresh?&shot_vfx:NULL};
            (void)SudekiMpStoryShotAudioPresent(roster,&shot_input,audio_present_exact,(void *)roster);
        }
        SudekiMpLanStoryAreaFadeApply(roster);
        if(aim_pose_attempted) SudekiMpLanAimActors(roster->actors[3],NULL);
        /* H1b: this player's own shopkeeper conversation or shop is up on
         * this game; host lines arriving meanwhile are not presented (D11). */
        if(frame->dialogue && shop_open()) (void)SudekiMpLanStoryCinematicAudioStop(audio_present_exact,(void *)roster);
        else if(frame->dialogue) {
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
    /* H1b: players in the same shop see each other (owner decision
     * 2026-10-17, D11, replacing the first per-visitor instancing). */
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
typedef struct StoryMuzzleObservation {
    const SudekiMpLanStoryFrame *frame;
    uint8_t needed;
    SudekiMpStoryMuzzle tips[4];
} StoryMuzzleObservation;
static BOOL observe_story_muzzles(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *context) {
    StoryMuzzleObservation *o=context;SudekiMpLanStoryScene remote;
    if(!current_presentation(native,&remote) || !replica_exact(roster,NULL) ||
        !SudekiMpLanStoryFrameMatchesScene(o->frame,&remote)) return FALSE;
    for(unsigned c=1u;c<4u;c+=2u) if((o->needed&o->frame->available_mask&(1u<<c)) &&
        o->frame->actors[c].generation==presented_generations[c])
        (void)SudekiMpStoryMuzzleObserve((HMODULE)game_base,roster->actors[c],
            c==3u?1u:0x0eu,&o->tips[c]);
    return replica_exact(roster,NULL);
}
static uint64_t project_story_muzzles(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyPeerStatus *peer,uint32_t now,SudekiMpStoryVfx *copy) {
    uint64_t prepared=SudekiMpStoryMuzzleUnchangedMask(copy);
    if(!copy->count || !history_count || !presentation_sample.valid ||
        history_lease.token!=peer->lease.token || history_lease.generation!=peer->lease.generation ||
        history_lease.seat!=peer->lease.seat ||
        !presentation_fresh(now,history_receipts[history_count-1u])) return prepared;
    const SudekiMpLanStoryFrame *frame=&history[history_count-1u];
    if(frame->epoch!=copy->epoch || frame->revision!=copy->revision) return prepared;
    StoryMuzzleObservation observation={.frame=frame};
    for(unsigned i=0;i<copy->count;++i) {
        const SudekiMpLanArenaSpiritVfxSnapshot *v=&copy->entries[i];
        if(v->skill_sequence) continue;
        if(v->owner_actor_type==1u && ((v->kind>=35u && v->kind<=39u) ||
            (v->kind>=43u && v->kind<=51u))) observation.needed|=8u;
        if(v->owner_actor_type==0x0eu && ((v->kind>=52u && v->kind<=57u) ||
            (v->kind>=59u && v->kind<=66u))) observation.needed|=2u;
    }
    if(!observation.needed) return prepared;
    if(!SudekiMpLanStoryClientObserveDispatch(w,observe_story_muzzles,&observation)) return prepared;
    SudekiMpStoryVfx before=*copy;
    prepared=SudekiMpStoryMuzzleProject(&story_muzzles,copy,frame,observation.tips);
    static unsigned reports;static DWORD last_report;
    if(SudekiMpLogResearchEnabled() && reports<32u && (!last_report || now-last_report>=500u)) {
        unsigned changed=0;
        for(unsigned i=0;i<copy->count;++i)
            if(memcmp(before.entries[i].position,copy->entries[i].position,sizeof(copy->entries[i].position))) ++changed;
        ++reports;last_report=now;
        SudekiMpLogFormat("story_muzzle event=projection wanted=%u ailish_tip=%u elco_tip=%u changed=%u prepared=%llu\r\n",
            observation.needed,(unsigned)observation.tips[3].valid,(unsigned)observation.tips[1].valid,changed,(unsigned long long)prepared);
    }
    return prepared;
}
static BOOL advance_effects(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *unused) {
    (void)unused; SudekiMpLanStoryScene remote;
    if(!current_presentation(native,&remote)) return FALSE;
    void *clocks[SUDEKIMP_SPIRIT_MODEL_CLOCK_CAPACITY]; unsigned count=0;
    if(!SudekiMpLanArenaSpiritVfxCollectModelClocks((HMODULE)game_base,
        clocks,SUDEKIMP_SPIRIT_MODEL_CLOCK_CAPACITY,&count)) return FALSE;
    return SudekiMpLanStoryEffectsAdvance(roster,remote.epoch,remote.revision,
        presentation_sample.sequence,presentation_sample.host_tick,presentation_sample.receipt,replica_exact,NULL,
        clocks,count);
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
/* The containment adapter accounts native allocation/removal immediately;
 * these callbacks never renew the separate asynchronous area-load window. */
static BOOL flush_damage_residency(void *context) {
    (void)context;
    if(!SudekiMpLanStoryWorldMonsterDamagePending()) return FALSE;
    return SudekiMpLanStoryWorldFlushMonsterDamage();
}
static BOOL tick_spawn_residency(void *context) {
    return SudekiMpLanStorySpawnTick(*(const uint32_t *)context,TRUE);
}
/* H1b: the door's 2D spot cue (native cue only; no entity or task). */
static BOOL door_cue_residency(void *context) {
    (void)SudekiMpStoryCharacterAudioPlaySpot((const char *)context);
    return FALSE;
}
static BOOL spawns_residency(void *context) {
    return SudekiMpLanStorySpawnEventsFollow(context);
}
static BOOL gates_residency(void *context) {
    return SudekiMpLanStoryGatesFollow(context);
}
static void present(void);
static void render_present(void *unused) { (void)unused; present(); }
static void invalidate_presentation(void) {
    client_revision_gap=FALSE;
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
            SudekiMpLanStoryRenderWitness,NULL)) {
        if(main_menu_open()) SudekiMpLogFormat("story_main_menu event=render_dispatch_refused error=%lu\r\n",(unsigned long)GetLastError());
        invalidate_presentation();
    }
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
    if(shop_open()) return; /* Escape is the shop's own back key while it is shown */
    if(examine_open()) { SudekiMpLanStoryZoneBannerExamineClose("escape"); return; }
    refresh_menu();
    SudekiMpLanStoryMenuToggle();
}
typedef struct ShopDrainStep { BOOL ran,idle; } ShopDrainStep;
static BOOL drain_shop(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryScene *scene,void *context) {
    ShopDrainStep *step=context; (void)r; (void)scene;
    step->ran=TRUE; step->idle=SudekiMpLanStoryShopDrain(GetTickCount());
    return TRUE;
}
static BOOL retire_client(void) {
    SudekiMpLanStoryClientReport report;
    SudekiMpLanStoryInputClear(); client_input_ready=FALSE;
    /* H1b: this client's own shop screen closes first (its back command);
     * the shop owner refuses to uninstall while it may still be shown. */
    if(shop_attempted && !SudekiMpLanStoryShopIdle()) {
        ShopDrainStep step={0};
        (void)SudekiMpLanStoryClientPresent(drain_shop,&step);
        /* Teardown with the presentation scope lost: the lease still ends
         * through its own witnessed close steps (never left half open). */
        if(!step.ran) {
            /* Unscoped: the gesture driver makes no native call here. */
            SudekiMpLanStoryNpcGestureDrain("unscoped");
            step.idle=SudekiMpLanStoryShopDrain(GetTickCount());
        }
        if(!step.idle) return FALSE;
    }
    if(!reset_story_vfx(NULL)) return FALSE;
    if(!SudekiMpLanStorySkyReleaseLighting()) return FALSE;
    if(quick_menu_attempted && SudekiMpLanStoryQuickMenuCapturesInput() &&
        !SudekiMpLanStoryClientPresent(service_quick_menu,NULL)) return FALSE;
    if(SudekiMpLanStoryClientRecruiting()) {
        SudekiMpLanStoryRecruitReport recruitment;
        (void)SudekiMpLanStoryClientRecruitService(&recruitment);
        return FALSE; /* Native async completion is committed on its normal dispatch. */
    }
    if(SudekiMpStoryCharacterAudioRetains()) {
        SudekiMpLanStoryNativeRoster roster;SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene))return FALSE;
        for(unsigned c=0;c<4;++c)if(!SudekiMpStoryCharacterAudioStop(c,audio_cleanup_exact,&roster))return FALSE;
    }
    if(SudekiMpStoryShotAudioRetains()) {
        SudekiMpLanStoryNativeRoster roster;SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene) ||
            !SudekiMpStoryShotAudioStop(audio_cleanup_exact,&roster))return FALSE;
    }
    if(SudekiMpLanStoryCinematicAudioRetains()) {
        SudekiMpLanStoryNativeRoster roster; SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene) ||
            !SudekiMpLanStoryCinematicAudioStop(audio_cleanup_exact,&roster)) return FALSE;
    }
    if(SudekiMpLanStoryComboHudRetains()) {
        SudekiMpLanStoryNativeRoster roster; SudekiMpLanStoryScene scene;
        if(!SudekiMpLanStoryClientCleanupRoster(&roster,&scene) ||
            !SudekiMpLanStoryComboHudRelease(&roster,cleanup_exact,NULL)) return FALSE;
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
    /* Owner 2026-10-17: a host conversation does not reach a player inside
     * their own shop visit while the lead is elsewhere (interim rule until
     * the co-op conversation design is decided): that client is given the
     * idle record for the same frame. */
    static SudekiMpLanStoryPresentation shown_dialogue;
    if(dialogue_index>=0) {
        shown_dialogue=dialogue_history[dialogue_index];
        if(!SudekiMpLanStorySceneConversationShared(&remote,chosen<4u?chosen:remote.leader_seat,4u)) {
            static uint32_t hidden_epoch,hidden_conversation; static unsigned hidden_logs;
            if(shown_dialogue.flags && (shown_dialogue.epoch!=hidden_epoch ||
                shown_dialogue.conversation_serial!=hidden_conversation) && hidden_logs<32u) {
                ++hidden_logs; hidden_epoch=shown_dialogue.epoch; hidden_conversation=shown_dialogue.conversation_serial;
                SudekiMpLogFormat("story_cinematic event=area_hidden epoch=%lu conversation=%lu viewer=%u lead=%u inside=%u temporary=%s\r\n",
                    (unsigned long)shown_dialogue.epoch,(unsigned long)shown_dialogue.conversation_serial,chosen,
                    (unsigned)remote.leader_seat,(unsigned)remote.inside_mask,remote.temporary[0]?remote.temporary:"-");
            }
            SudekiMpLanStoryPresentationClearDialogue(&shown_dialogue);
        }
    }
    StoryPresentation presented={&sample,&world_sample,FALSE,
        control_offered?&control:NULL,&peer.lease,
        dialogue_index>=0?&shown_dialogue:NULL};
    LARGE_INTEGER present_start=timing_begin();
    {
        uint8_t foreign=client_foreign_characters(&remote);
        SudekiMpLanStoryReplicaSetForeignCharacters(foreign);
        SudekiMpLanStoryWorldSetForeignCharacters(foreign);
        SudekiMpLanStoryAreaFadeSetForeign(foreign);
        if(foreign!=traced_foreign) {
            traced_foreign=foreign;
            SudekiMpLogFormat("lan_story event=client_area foreign=%u inside=%u temporary=%s epoch=%lu revision=%lu\r\n",
                foreign,remote.inside_mask,remote.temporary,(unsigned long)remote.epoch,(unsigned long)remote.revision);
        }
    }
    result=SudekiMpLanStoryClientPresent(apply_presented_frame,&presented)?4u:3u;
    /* Replayed host monster hits run natively OUTSIDE the presentation proof
     * (they may create tasks/effects); the call scope immediately re-contains. */
    if(result==4u && SudekiMpLanStoryWorldMonsterDamagePending()) {
        (void)SudekiMpLanStoryClientResidencyCall(NULL,flush_damage_residency,NULL);
    }
    if(result==4u) SudekiMpLanStoryWorldTickMonsterFeedback();
    if(result==4u) SudekiMpLanStoryAmbientSetActive(TRUE);
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
        if(SudekiMpLogResearchEnabled() && (present_timing.last_us>50000u || gap_us>90000u) && hitch_logs<200u) {
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
            visible_dialogue=shown_dialogue;
            visible_dialogue_receipt=dialogue_receipts[dialogue_index];
        }
        presentation_sample.lease=peer.lease; presentation_sample.scene=remote;
        presentation_sample.native_roster=roster;
        presentation_sample.sequence=sample.sequence; presentation_sample.host_tick=sample.host_tick;
        presentation_sample.receipt=sample_receipt; presentation_sample.valid=effects_ready=TRUE;
        away_combat=(SudekiMpStoryCombatSample){sample_receipt,sample.epoch,sample.revision,1,sample.combat_mode};
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
    /* Only a missing coherent pair may retain an established input binding.
     * No old frame is reapplied, relabelled, or given a newer receipt. */
    client_revision_gap=result==6u && (presentation_sample.valid || client_revision_gap);
    if(result!=4u) { presentation_buffer_ms=66u; presentation_sample.valid=effects_ready=FALSE; visible_dialogue_valid=FALSE; SudekiMpLanStoryEffectsResetClock(); }
    InterlockedExchange(&runtime_ready,result==4u);
    if(client_revision_gap && client_camera_attempted) {
        BOOL camera_applied=SudekiMpLanStoryClientPresent(client_revision_gap_camera,NULL);
        if(!camera_applied) {
            /* A refused camera frame contributes no deferred catch-up time. */
            native_camera_clock.tick=now; native_camera_clock.applied=FALSE;
        }
        static uint32_t camera_gap_revision; static BOOL camera_gap_applied; static unsigned camera_gap_logs;
        if(camera_gap_logs<64u && (camera_gap_revision!=remote.revision || camera_gap_applied!=camera_applied)) {
            ++camera_gap_logs;camera_gap_revision=remote.revision;camera_gap_applied=camera_applied;
            SudekiMpLogFormat("lan_story event=revision_gap_camera_attempt revision=%lu applied=%u receipt_age=%lu\r\n",
                (unsigned long)remote.revision,(unsigned)camera_applied,
                (unsigned long)(now-presentation_sample.receipt));
        }
    }
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
typedef enum ExteriorControlPermission {
    EXTERIOR_RETAIN,EXTERIOR_ACTION,EXTERIOR_STEERING
} ExteriorControlPermission;
static BOOL exterior_control_allowed(unsigned player,ExteriorControlPermission permission) {
    if(!split_attempted || player==0u || player>=4u || !host_control[player].acquired ||
        host_control[player].draining || !host_control[player].exterior_roster.epoch ||
        host_control[player].exterior_roster.epoch!=host_control[player].fence.epoch) return FALSE;
    unsigned c=host_control[player].native_key.seat;
    const SudekiMpLanStoryNativeRoster *r=&host_control[player].exterior_roster;
    if(permission==EXTERIOR_STEERING)
        return SudekiMpLanStoryHostControlExteriorNativeSteeringReady(r,c);
    if(permission==EXTERIOR_ACTION)
        return SudekiMpLanStoryHostControlExteriorNativeReady(r,c);
    return permission==EXTERIOR_RETAIN && SudekiMpLanStoryHostControlExteriorNativeBound(r,c);
}
/* H1b: the body is parked at the door outside while the lead's own entry
 * reloads the house this player is in; their screen keeps them inside, so
 * nothing they do may act on the parked body. */
static BOOL member_parked(unsigned player) {
    return player<4u && SudekiMpLanStoryRemoteInteriorMemberParked(host_control[player].native_key.seat);
}
static BOOL remote_input_ready(unsigned player) {
    /* Action admission retains its positive host/shop modal contract. */
    return !member_parked(player) &&
        (host_binding_ready || host_front_use || exterior_control_allowed(player,EXTERIOR_ACTION));
}
static BOOL remote_steering_ready(unsigned player) {
    /* A settled exterior player's movement does not borrow the host's
     * dialogue/filter state. Native body/world pauses still hold movement. */
    return !member_parked(player) &&
        (host_binding_ready || host_front_use || exterior_control_allowed(player,EXTERIOR_STEERING));
}
static BOOL remote_action_ready(unsigned player,unsigned kind) {
    /* Existing same-area casts have their own native modal admission. An
     * exterior shop handoff also needs positive input readiness: retaining
     * the body during an unknown interaction must not submit an item/cast. */
    return !member_parked(player) &&
        ((kind!=SUDEKIMP_STORY_ACTION_MELEE && !host_control[player].exterior_roster.epoch) ||
        remote_input_ready(player));
}
static BOOL runtime_control_lease_exact(const SudekiMpLanPartyLease *key,BOOL input_ready) {
    if(!key || !session || !saved_profile || !control_attempted || local_seat ||
        !runtime_thread || runtime_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&stopping,0,0) ||
        SudekiMpLanStoryMenuCapturesInput()) return FALSE;
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer={0};
        if(!same_connection(key,&host_control[p].native_key) || !host_control[p].acquired ||
            (input_ready && !host_control[p].ready) || host_control[p].draining || story_swap_blocks(p) ||
            key->seat!=SudekiMpLanPartyPlayerCharacter(session,p) ||
            !SudekiMpLanPartyPeerStatusGet(session,p,&peer) ||
            peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !peer.transport_confirmed ||
            !same_connection(&peer.lease,&host_control[p].connection) ||
            (!host_binding_ready && !host_front_use && !exterior_control_allowed(p,input_ready?EXTERIOR_STEERING:EXTERIOR_RETAIN)) ||
            (input_ready && !SudekiMpLanPartyStoryControlAcknowledged(session,&peer.lease,&host_control[p].fence))) continue;
        return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryRuntimeTurnLeaseExact(const SudekiMpLanPartyLease *key) {
    return runtime_control_lease_exact(key,TRUE);
}
BOOL SudekiMpLanStoryRuntimeAiLeaseExact(const SudekiMpLanPartyLease *key) {
    /* Reserving a native actor suppresses autonomous follower movement while
     * its client prepares input. It does not authorize player steering. */
    return runtime_control_lease_exact(key,FALSE);
}
enum SudekiMpLanStoryRuntimeAwaySeatRefusal SudekiMpLanStoryRuntimeAwaySeatRefusal(
    unsigned c,const SudekiMpLanPartyLease *connection) {
    if(c>=4 || !connection || !connection->token) return STORY_AWAY_REFUSAL_INVALID_ARGUMENT;
    if(!session || !saved_profile || !control_attempted || !story_policy_initialized) return STORY_AWAY_REFUSAL_NOT_INITIALIZED;
    if(local_seat) return STORY_AWAY_REFUSAL_CLIENT_SEAT;
    if(!runtime_thread || runtime_thread!=GetCurrentThreadId()) return STORY_AWAY_REFUSAL_THREAD;
    if(InterlockedCompareExchange(&stopping,0,0)) return STORY_AWAY_REFUSAL_STOPPING;
    if(!host_binding_ready) return STORY_AWAY_REFUSAL_BINDING_NOT_READY;
    /* Host menus do not remove combat pressure from a released away body.
     * CompanionSeatExact independently retains its accepted modal guard. */
    const SudekiMpPartyOwnership *o=&story_presence.ownership;
    unsigned p=SudekiMpPartyCharacterOwner(&o->assignment,c);
    if(!p || p>=4) return STORY_AWAY_REFUSAL_NOT_OWNER;
    if(o->phase!=SUDEKIMP_PARTY_SWAP_IDLE) return STORY_AWAY_REFUSAL_SWAP_PHASE;
    if(!(o->connected&(1u<<p))) return STORY_AWAY_REFUSAL_NOT_CONNECTED;
    if(!(o->away&(1u<<p))) return STORY_AWAY_REFUSAL_NOT_AWAY;
    if(o->control[p]!=SUDEKIMP_PARTY_CONTROL_AI) return STORY_AWAY_REFUSAL_CONTROL_NOT_AI;
    if(host_control[p].native_key.token) return STORY_AWAY_REFUSAL_NATIVE_KEY_RETAINED;
    if(host_control[p].acquired) return STORY_AWAY_REFUSAL_ACQUIRED;
    if(host_control[p].draining) return STORY_AWAY_REFUSAL_DRAINING;
    if(SudekiMpLanPartyPlayerCharacter(session,p)!=c) return STORY_AWAY_REFUSAL_CHARACTER_MISMATCH;
    SudekiMpLanPartyPeerStatus peer={0};
    if(!SudekiMpLanPartyPeerStatusGet(session,p,&peer)) return STORY_AWAY_REFUSAL_PEER_STATUS;
    if(peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING) return STORY_AWAY_REFUSAL_PEER_PHASE;
    if(!peer.transport_confirmed) return STORY_AWAY_REFUSAL_TRANSPORT;
    if(!same_connection(connection,&peer.lease)) return STORY_AWAY_REFUSAL_CONNECTION_MISMATCH;
    return STORY_AWAY_REFUSAL_NONE;
}
BOOL SudekiMpLanStoryRuntimeAwaySeatExact(unsigned c,const SudekiMpLanPartyLease *connection) {
    return SudekiMpLanStoryRuntimeAwaySeatRefusal(c,connection)==STORY_AWAY_REFUSAL_NONE;
}
BOOL SudekiMpLanStoryRuntimeCompanionSeatExact(unsigned c) {
    if(c>=4 || !session || !saved_profile || !control_attempted || local_seat ||
        !story_policy_initialized || !runtime_thread || runtime_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&stopping,0,0) || !host_binding_ready ||
        SudekiMpLanStoryMenuCapturesInput() || story_presence.ownership.phase!=SUDEKIMP_PARTY_SWAP_IDLE)
        return FALSE;
    if(!(story_presence.ownership.assignment.available&(1u<<c))) return FALSE;
    unsigned p=SudekiMpPartyCharacterOwner(&story_presence.ownership.assignment,c);
    if(p>=4) return TRUE;
    SudekiMpLanPartyPeerStatus peer={0};
    return p && SudekiMpLanPartyPeerStatusGet(session,p,&peer) &&
        SudekiMpLanStoryRuntimeAwaySeatExact(c,&peer.lease);
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
    SudekiMpLanStoryDoorCameraUninstall(); door_camera_ready=FALSE;
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
        if(host_control[p].ready && !host_control[p].draining && !story_swap_blocks(p) && !(o->away&(1u<<p)))
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
        if(sender && story_step_away && command.kind==SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE) {
            result=SUDEKIMP_PARTY_SWAP_STALE;
            if(command.world==o->assignment.world && command.revision==o->assignment.revision &&
                command.generation==(c<4u?o->assignment.generation[c]:0u)) {
                SudekiMpStoryAwayAdmission admission={
                    .same_area=c<4u && SudekiMpLanStorySceneSameArea(scene,roster.leader_character,c),
                    .swap_involves=o->phase!=SUDEKIMP_PARTY_SWAP_IDLE &&
                        (o->player==p || o->displaced==p || !o->player),
                    .prepare=host_control[p].native_key.token && !host_control[p].ready && !host_control[p].draining};
                BOOL away=(command.value&SUDEKIMP_LAN_PARTY_PRESENCE_AWAY)!=0;
                result=SudekiMpStoryAwayAdmit(o,p,away,&admission);
                if(result==SUDEKIMP_PARTY_SWAP_OK)
                    result=SudekiMpPartyPresenceRequest(o,p,command.request,command.world,command.revision,0,away);
            }
            SudekiMpLogFormat("story_step_away event=request player=%u away=%u request=%lu result=%u revision=%lu\r\n",
                p,!!(command.value&SUDEKIMP_LAN_PARTY_PRESENCE_AWAY),(unsigned long)command.request,
                result,(unsigned long)o->assignment.revision);
        }
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
    if(story_pending_request && state.ack_request[local_seat]>story_pending_request) story_pending_request=0;
    if(story_pending_request && state.ack_request[local_seat]==story_pending_request) {
        unsigned result=state.ack_result[local_seat]; story_pending_request=0;
        SudekiMpLanStoryMenuNotice(result==SUDEKIMP_PARTY_SWAP_OK?"Switching character...":
            result==SUDEKIMP_PARTY_SWAP_OCCUPIED?"That character is already claimed.":
            result==SUDEKIMP_PARTY_SWAP_UNAVAILABLE?"That character is not in the party yet.":
            result==SUDEKIMP_PARTY_SWAP_STALE?"Party changed; press F1 again.":
            "Wait until combat, dialogue and other transfers have finished.");
    }
    if(!pressed || SudekiMpLanStoryMenuCapturesInput() || away_client.phase!=SUDEKIMP_STORY_AWAY_NONE) return;
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
/* The host lead stands in the current exterior (H1b: other members may be
 * in a background-resident house; the lead may still use a door). */
static BOOL scene_lead_outside(const SudekiMpLanStoryScene *s) {
    return !s->temporary[0] || (s->leader_seat<4u && !(s->inside_mask&(1u<<s->leader_seat)));
}
static void refresh_split_eligibility(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,const SudekiMpLanStoryNativeRoster *roster) {
    if(!split_attempted || !saved_profile || !control_attempted || local_seat ||
        !runtime_thread || runtime_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&stopping,0,0)) return;
    unsigned mask=0;
    BOOL scene_ready=scene && SudekiMpLanStorySceneValid(scene) &&
        scene->phase==SUDEKIMP_LAN_STORY_READY;
    BOOL known=scene_ready && roster && roster->epoch==scene->epoch &&
        roster->revision==scene->revision && roster->available_mask==scene->available_mask &&
        roster->leader_character==scene->leader_seat;
    if(known) {
        for(unsigned p=1;p<4u;++p) {
            SudekiMpLanPartyPeerStatus peer;
            unsigned c=host_control[p].native_key.seat;
            if(c>=4u || c==roster->leader_character || !(roster->available_mask&(1u<<c)) ||
                !host_control[p].acquired || host_control[p].draining || story_swap_blocks(p) ||
                !SudekiMpLanPartyPeerStatusGet(session,p,&peer) ||
                peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !peer.transport_confirmed ||
                !same_connection(&peer.lease,&host_control[p].connection) ||
                c!=SudekiMpLanPartyPlayerCharacter(session,p) ||
                host_control[p].fence.player!=p || host_control[p].fence.character!=c ||
                !SudekiMpLanStoryControlMatchesScene(&host_control[p].fence,scene) ||
                !SudekiMpLanStoryControlExact(w,roster,&host_control[p].native_key)) continue;
            mask|=1u<<c;
        }
    }
    /* A TEMP gap clears eligibility while its roster is unknown. Restore
     * only freshly proved retained leases BEFORE querying exterior steering
     * on the first settled dispatch; otherwise it submits a neutral tick and
     * starts an idle transition. PREPARE retains residency, not input rights.
     * The ordinary per-peer service still rechecks/drains, and the final
     * post-service mask publication below remains responsible for removals. */
    SudekiMpLanStorySplitSetEligibility(known && scene_lead_outside(scene),(uint8_t)mask);
}
static void trace_control_block(unsigned p,const char *reason) {
    if(p>=4u || control_block_trace[p]==reason) return;
    control_block_trace[p]=reason;
    if(control_block_traces>=96u) return;
    ++control_block_traces;
    SudekiMpLogFormat("lan_story_control event=host_control_blocked player=%u reason=%s\r\n",p,reason?reason:"none");
}
/* Observer callback, native thread, before a whole-world zone call runs
 * (#42): return NPC activity and every held character to native control so
 * the native load starts with the whole party under default control. The
 * client is told on the next dispatch (draining -> revoke -> drained). */
static BOOL area_follow_ready;
static void before_world_change(void) {
    if(local_seat || !control_attempted) return;
    if(activity_attempted && SudekiMpLanStoryActivityRetains() &&
        !SudekiMpLanStoryActivityReleaseBeforeWorldChange())
        SudekiMpLogWrite("lan_story_control event=preload_activity_retained\r\n");
    for(unsigned p=1;p<4u;++p) if(host_control[p].native_key.token) {
        host_control[p].ready=FALSE;
        if(!host_control[p].draining) {
            host_control[p].draining=TRUE;
            SudekiMpLogFormat("lan_story_control event=host_control_drain player=%u character=%u transaction=%lu reason=world_change\r\n",
                p,host_control[p].native_key.seat,(unsigned long)host_control[p].fence.transaction);
        }
        if(!SudekiMpLanStoryControlReleaseBeforeWorldChange(&host_control[p].native_key))
            SudekiMpLogFormat("lan_story_control event=preload_release_refused player=%u\r\n",p);
    }
}
/* A reserved native actor already belongs to its client during PREPARE, even
 * before that client's first focused input frame acknowledges the offer.
 * TEMP travel must retain its exterior then too. Activity and gameplay still
 * require READY; preserving residency grants neither input nor an ACK. The
 * per-player service below drains stale connections/scenes before this read. */
static void host_control_masks(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,unsigned *activity_mask,unsigned *split_mask) {
    *activity_mask=*split_mask=0u;
    for(unsigned p=1;p<4u;++p) {
        unsigned c=host_control[p].native_key.seat;
        if(c>=4u || !host_control[p].acquired || host_control[p].draining ||
            !SudekiMpLanStoryControlExact(w,roster,&host_control[p].native_key)) continue;
        *split_mask|=1u<<c;
        if(host_control[p].ready) *activity_mask|=1u<<c;
    }
}
/* Translate current runtime ownership and input evidence for one character.
 * No native writes; the caller supplies its already-proven roster/witness. */
static void enemy_targeting_seat(unsigned c,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,SudekiMpStoryThreatSeat *seat,
    unsigned *firing,SudekiMpLanPartyLease *away_key,BOOL *coop,uint32_t now) {
    if(r && (!away_refused_logs.known || away_refused_logs.epoch!=r->epoch || away_refused_logs.revision!=r->revision)) {
        memset(&away_refused_logs,0,sizeof(away_refused_logs));away_refused_logs.known=TRUE;
        away_refused_logs.epoch=r->epoch;away_refused_logs.revision=r->revision;
        for(unsigned i=0;i<4;++i) away_refused_logs.last[i]=STORY_AWAY_REFUSAL_NONE;
    }
    memset(seat,0,sizeof(*seat));*firing=0;memset(away_key,0,sizeof(*away_key));
    seat->max_age=SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS;
    seat->absent=SudekiMpLanStoryRemoteInteriorMemberParked(c);
    unsigned p=SudekiMpPartyCharacterOwner(&story_presence.ownership.assignment,c);
    if(p>=4) {seat->owned_ai=c!=(r?r->leader_character:0);return;}
    seat->away=(story_presence.ownership.away&(1u<<p))!=0;
    seat->owned_ai=story_presence.ownership.control[p]==SUDEKIMP_PARTY_CONTROL_AI;
    if(!p) return;
    SudekiMpLanPartyPeerStatus peer={0};
    seat->connected=SudekiMpLanPartyPeerStatusGet(session,p,&peer) &&
        peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed;
    unsigned refusal=STORY_AWAY_REFUSAL_NONE;
    if(seat->connected && seat->away) {
        refusal=SudekiMpLanStoryRuntimeAwaySeatRefusal(c,&peer.lease);
        if(refusal==STORY_AWAY_REFUSAL_NONE) {*away_key=peer.lease;*coop=TRUE;}
        else if(refusal!=away_refused_logs.last[c] && away_refused_logs.count<64) {
            static const char *const reasons[]={"invalid_argument","not_initialized","client_seat","thread","stopping","binding_not_ready","menu_captures","not_owner","swap_phase","not_connected","not_away","control_not_ai","native_key_retained","acquired","draining","character_mismatch","peer_status","peer_phase","transport","connection_mismatch","none"};
            ++away_refused_logs.count;
            SudekiMpLogFormat("story_enemy_targeting event=away_refused seat=%u reason=%s\r\n",c,reasons[refusal]);
        }
    }
    away_refused_logs.last[c]=refusal;
    BOOL lease=r && host_control[p].acquired && host_control[p].native_key.seat==c &&
        same_connection(&peer.lease,&host_control[p].connection) &&
        SudekiMpLanStoryControlExact(w,r,&host_control[p].native_key);
    seat->ready=lease && host_control[p].ready &&
        story_presence.ownership.control[p]==SUDEKIMP_PARTY_CONTROL_HUMAN;
    seat->draining=host_control[p].draining;
    /* A host-side busy-body hold is not client silence. Packet receipt
     * remains fresh while movement application is held. 1 encodes receipt 0. */
    seat->held=!lease;
    seat->receipt=host_control[p].heard_at==1u?0:host_control[p].heard_at;
    /* Saved-story uses its own accepted ranged intent, whose held
     * bit already combines first-person aim and weak attack. */
    *firing=host_control[p].ranged_at &&
        (uint32_t)(now-host_control[p].ranged_at)<=SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS &&
        SudekiMpLanStoryControlFenceSame(&host_control[p].ranged.fence,&host_control[p].fence) &&
        host_control[p].ranged.held;
}
static void service_story_controls(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    if(!control_attempted) return;
    if(split_attempted && host_binding_ready) SudekiMpLanStorySplitClearReturned();
    SudekiMpLanStoryNativeRoster roster; memset(&roster,0,sizeof(roster));
    BOOL known=SudekiMpLanStoryObserverRoster(controller,w,scene,&roster);
    SudekiMpLanStoryControlObserveIdleContact(w,known?&roster:NULL);
    refresh_split_eligibility(w,scene,known?&roster:NULL);
    uint32_t now=GetTickCount();
    BOOL remote=FALSE;
    {   /* Away peers cease to keep the dead-lead hold armed after native
         * release. Until then a retained lease still needs its original lead
         * identity to finish draining; dropping that hold early strands it. */
        void *remote_actors[3]={0};
        for(unsigned p=1;p<4u;++p) {
            SudekiMpLanPartyPeerStatus peer={0}; unsigned c=SudekiMpLanPartyPlayerCharacter(session,p);
            if(SudekiMpLanPartyPeerStatusGet(session,p,&peer) && peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                peer.transport_confirmed && c<4u &&
                (!(story_presence.ownership.away&(1u<<p)) || host_control[p].native_key.token)) {
                remote=TRUE;
                if(known && (roster.available_mask&(1u<<c))) remote_actors[p-1u]=roster.actors[c];
            }
        }
        /* Keep the last proved actors while the roster is briefly unknown. */
        if(known || !remote) SudekiMpLanStoryDeathHoldSetRemoteActors(remote_actors);
        SudekiMpLanStoryDeathHoldSetActive(remote);
        SudekiMpLanStoryDeathHoldReport();
    }
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer={0};
        BOOL connected=SudekiMpLanPartyPeerStatusGet(session,p,&peer) &&
            peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed;
        unsigned character=SudekiMpLanPartyPlayerCharacter(session,p);
        connected=connected && character<4u;
        memset(&host_control[p].exterior_roster,0,sizeof(host_control[p].exterior_roster));
        if(known && connected && split_attempted && host_control[p].acquired &&
            !host_control[p].draining && !story_swap_blocks(p) &&
            character==host_control[p].native_key.seat &&
            same_connection(&peer.lease,&host_control[p].connection) &&
            SudekiMpLanStoryControlMatchesScene(&host_control[p].fence,scene) &&
            SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key) &&
            SudekiMpLanStoryHostControlExteriorBound(controller,w,scene,character))
            host_control[p].exterior_roster=roster;
        if(host_control[p].native_key.token && !host_control[p].draining &&
            SudekiMpLanStoryObserverHolding()) {
            /* A background zone load briefly cleared the exact party proof
             * (and the host binding with it); the observer holds the
             * unchanged scene. Hold input likewise instead of draining. */
            trace_control_block(p,"background_load_hold"); continue;
        }
        if(host_control[p].native_key.token && !known && !host_control[p].draining &&
            split_attempted && SudekiMpLanStorySplitActive()) {
            /* Host lead's split-area transition: the exterior player's actor is
             * unchanged. Hold input until the roster is exact again. */
            trace_control_block(p,"split_transition_hold"); continue;
        }
        if(host_control[p].native_key.token) {
            const char *drain_reason=(story_presence.ownership.away&(1u<<p))?"stepped_away":
                story_swap_blocks(p)?"character_swap":
                !connected?"peer_not_observing":
                character!=host_control[p].native_key.seat?"character_changed":
                !same_connection(&peer.lease,&host_control[p].connection)?"connection_changed":
                !known?"roster_unknown":
                !SudekiMpLanStoryControlMatchesScene(&host_control[p].fence,scene)?"scene_changed":
                !SudekiMpLanStoryHostControlBound(controller,w,scene) &&
                    !exterior_control_allowed(p,EXTERIOR_RETAIN)?"host_binding_changed":NULL;
            if(drain_reason && !host_control[p].draining) {
                SudekiMpLogFormat("lan_story_control event=host_control_drain player=%u character=%u transaction=%lu reason=%s\r\n",
                    p,host_control[p].native_key.seat,
                    (unsigned long)host_control[p].fence.transaction,drain_reason);
                host_control[p].draining=TRUE;
            }
            if(host_control[p].draining) {
                host_control[p].ready=FALSE;
                (void)SudekiMpLanPartyRevokeStoryControl(session,&host_control[p].connection);
                /* A native world load (scene LOADING) has no roster; release
                 * NPC activity and the held AI from their exact leases so the
                 * load can complete (#42). */
                BOOL world_load=!known && scene && scene->phase==SUDEKIMP_LAN_STORY_LOADING;
                if(activity_attempted && SudekiMpLanStoryActivityRetains() &&
                    (world_load ? !SudekiMpLanStoryActivityReleaseForLoad(w) :
                     (!known || !SudekiMpLanStoryActivityService(w,&roster,0u)))) {
                    trace_control_block(p,world_load?"drain_load_activity":
                        !known?"drain_roster_unknown":"drain_activity_retained"); continue;
                }
                if(world_load ? SudekiMpLanStoryControlLoadDrain(w,&host_control[p].native_key) :
                    (known && SudekiMpLanStoryControlDrain(w,&roster,&host_control[p].native_key))) {
                    uint32_t transaction=host_control[p].transaction;
                    memset(&host_control[p],0,sizeof(host_control[p])); host_control[p].transaction=transaction;
                    story_control_released(p);
                    trace_control_block(p,"drained");
                } else trace_control_block(p,world_load?"drain_load_native":
                    !known?"drain_roster_unknown":"drain_native_refused");
                continue;
            }
        } else {
            if(connected) trace_control_block(p,(story_presence.ownership.away&(1u<<p))?"offer_stepped_away":story_swap_blocks(p)?"offer_swap":!known?"offer_roster_unknown":
                !host_binding_ready?"offer_host_binding":character==roster.leader_character?"offer_is_leader":
                !(roster.available_mask&(1u<<character))?"offer_unavailable":
                capture.epoch!=scene->epoch || capture.revision!=scene->revision?"offer_capture_scene":
                capture.actors[character]!=roster.actors[character] || !capture.generation[character]?"offer_capture_actor":
                !presentation_fresh(now,capture.last_tick)?"offer_capture_stale":
                !SudekiMpLanPartyStoryCatchupComplete(session,&peer.lease)?"offer_catchup":"offer_attempt");
            if((story_presence.ownership.away&(1u<<p)) || story_swap_blocks(p) || !connected || !known || !host_binding_ready || character==roster.leader_character ||
                !(roster.available_mask&(1u<<character)) ||
                capture.epoch!=scene->epoch || capture.revision!=scene->revision ||
                capture.actors[character]!=roster.actors[character] || !capture.generation[character] ||
                !presentation_fresh(now,capture.last_tick) ||
                !SudekiMpLanPartyStoryCatchupComplete(session,&peer.lease) ||
                host_control[p].transaction==UINT32_MAX) continue;
            /* #74 follow-up diagnostics: the three refusals below used to be
             * silent; a client stuck "waiting_for_host_offer" after a host
             * house visit needs the failing step named. */
            if(!SudekiMpLanStoryControlBegin(w,&roster)) { trace_control_block(p,"offer_begin_refused"); continue; }
            SudekiMpLanPartyLease key;
            if(!SudekiMpLanStoryControlNextLease(w,&peer.lease,character,&key)) { trace_control_block(p,"offer_lease_refused"); continue; }
            host_control[p].connection=peer.lease; host_control[p].native_key=key;
            host_control[p].fence=(SudekiMpLanStoryControlFence){scene->epoch,scene->revision,
                ++host_control[p].transaction,capture.generation[character],(uint8_t)p,(uint8_t)character};
            if(!SudekiMpLanStoryControlAcquire(w,&roster,&key)) {
                trace_control_block(p,SudekiMpLanStoryControlRetainsKey(&key)?"offer_acquire_retained":"offer_acquire_refused");
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
        BOOL input_ready=remote_steering_ready(p);
        SudekiMpLanStoryControlState state={host_control[p].fence,now,
            SUDEKIMP_STORY_CONTROL_PREPARE};
        if(SudekiMpLanPartyStoryControlAcknowledged(session,&peer.lease,&state.fence))
            state.phase=SUDEKIMP_STORY_CONTROL_READY;
        BOOL published=SudekiMpLanPartyPublishStoryControl(session,&peer.lease,&state);
        host_control[p].ready=published && state.phase==SUDEKIMP_STORY_CONTROL_READY;
        if(!host_control[p].ready) host_control[p].last_input_at=0;
        SudekiMpLanStoryMovement input; uint32_t receipt=0; BOOL took=FALSE;
        if(host_control[p].ready &&
            SudekiMpLanPartyTakeStoryMovement(session,&peer.lease,now,&input,&receipt)) {
            host_control[p].movement=input; host_control[p].last_input_at=receipt; took=TRUE;
            host_control[p].heard_at=receipt?receipt:1u;
        }
        /* Block is serviced before movement, including neutral/expired intent.
         * Native movement/action locks must never swallow the release. */
        if(character==0u || character==2u) {
            SudekiMpLanStoryBlock block; uint32_t received=0;
            if(SudekiMpLanPartyTakeStoryBlock(session,&peer.lease,now,&block,&received)) {
                host_control[p].block=block;host_control[p].block_at=received;
            }
            int32_t age=(int32_t)(now-host_control[p].block_at);
            BOOL held=remote_input_ready(p) && host_control[p].ready &&
                !SudekiMpLanStoryMenuCapturesInput() && host_control[p].block_at &&
                age>=-100 && age<=100 && host_control[p].block.held &&
                SudekiMpLanStoryControlFenceSame(&host_control[p].block.fence,&state.fence);
            if(!SudekiMpLanStoryControlBlock(w,&roster,&host_control[p].native_key,held)) {
                host_control[p].draining=TRUE;continue;
            }
        }
        float x=0,z=0;
        /* The receipt stamp comes from the transport worker's clock and can be
         * a few milliseconds newer than this tick's `now`; a negative age is
         * fresh, not a wrapped-around stale value (live: one zeroed tick per
         * such packet showed as an idle pop between run cycles). */
        int32_t input_age=(int32_t)(now-host_control[p].last_input_at);
        BOOL fresh=host_control[p].last_input_at && input_age<=100;
        BOOL fence_ok=SudekiMpLanStoryControlFenceSame(&host_control[p].movement.fence,&state.fence);
        if(input_ready && host_control[p].ready && fresh && fence_ok) {
            x=host_control[p].movement.world_x; z=host_control[p].movement.world_z;
        }
        {
            /* Bounded research diagnostic: applied-movement transitions and why. */
            static BOOL was_moving[4]; static unsigned move_logs;
            BOOL moving=(x!=0.0f || z!=0.0f);
            if(SudekiMpLogResearchEnabled() && moving!=was_moving[p] && move_logs<400u) {
                ++move_logs; was_moving[p]=moving;
                SudekiMpLogFormat("lan_story_control event=host_input ms=%lu player=%u moving=%u binding=%u ready=%u fresh=%u age_ms=%ld fence=%u packet=%.2f,%.2f\r\n",
                    (unsigned long)GetTickCount(),p,moving,input_ready,host_control[p].ready,fresh,
                    host_control[p].last_input_at?(long)input_age:0l,fence_ok,
                    (double)host_control[p].movement.world_x,(double)host_control[p].movement.world_z);
            }
        }
        /* Unknown residency and native pauses still hold this body. A
         * proven settled exterior actor can steer during the host's modal
         * transition; resuming any real hold requires a fresh packet. */
        if(!input_ready) host_control[p].last_input_at=0;
        BOOL native_held=FALSE;
        if(!SudekiMpLanStoryControlMove(w,&roster,&host_control[p].native_key,x,z,&native_held)) {
            host_control[p].last_input_at=0;
            (void)SudekiMpLanPartyRevokeStoryControl(session,&peer.lease);
            host_control[p].draining=TRUE;
        } else {
            BOOL held=!input_ready || native_held;
            if(held) host_control[p].last_input_at=0;
            if(held!=host_control[p].input_held && host_control_hold_traces<64u) {
                ++host_control_hold_traces;
                SudekiMpLogFormat("lan_story_control event=host_input_hold player=%u transaction=%lu held=%u reason=%s ownership=retained steering=%u actions=%u host_ready=%u exterior=%u ms=%lu\r\n",
                    p,(unsigned long)state.fence.transaction,held?1u:0u,
                    !input_ready?"steering_not_ready":native_held?"native_body_or_world_busy":"ready",
                    input_ready?1u:0u,remote_input_ready(p)?1u:0u,host_binding_ready?1u:0u,
                    host_control[p].exterior_roster.epoch?1u:0u,(unsigned long)GetTickCount());
            }
            host_control[p].input_held=held;
            SudekiMpLanStoryResearch54Input(p,took,receipt,now,fresh,
                host_control[p].movement.world_x!=0.0f || host_control[p].movement.world_z!=0.0f,
                x!=0.0f || z!=0.0f,held);
        }
    }
    if(activity_attempted && known) {
        unsigned mask=0u,split_mask=0u;
        host_control_masks(w,&roster,&mask,&split_mask);
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
        if(host_binding_ready && !scene->temporary[0])
            for(unsigned p=1;p<4u;++p) {
                unsigned c=host_control[p].native_key.seat;
                if(c<4u && (mask&(1u<<c)) && !host_control[p].input_held &&
                    SudekiMpLanStorySceneSameArea(scene,roster.leader_character,c) &&
                    SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key))
                    (void)SudekiMpLanStoryWorldCollectNearby(roster.actors[c],SudekiMpLanPartyActorType(c));
            }
        /* Retain every exactly acquired client actor through a TEMP door,
         * including a client still preparing its local view/input. This is
         * shared by all authored TEMP interiors, including every Kamo shop. */
        if(split_attempted) SudekiMpLanStorySplitSetEligibility(
            scene->phase==SUDEKIMP_LAN_STORY_READY && scene_lead_outside(scene),(uint8_t)split_mask);
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
            door_camera_ready=SudekiMpLanStoryDoorCameraInstall((HMODULE)game_base);
            if(!door_camera_ready) SudekiMpLogFormat("story_door_camera event=install_refused error=%lu\r\n",(unsigned long)GetLastError());
            if(!SudekiMpLanStoryCastInstall((HMODULE)game_base))
                SudekiMpLogWrite("story_cast event=install_refused admission=closed\r\n");
        }
        if(remote_ready && cast_attempted && !SudekiMpLanStoryMenuCapturesInput())
            (void)SudekiMpLanStoryCastTryBind(w,&roster);
    }
    for(unsigned p=1;p<4u;++p) {
        unsigned c=host_control[p].native_key.seat;
        if(!host_control[p].acquired || (c!=1u && c!=3u)) continue;
        SudekiMpLanStoryRanged input;uint32_t receipt=0;
        if(SudekiMpLanPartyTakeStoryRanged(session,&host_control[p].connection,now,&input,&receipt)) {
            host_control[p].ranged=input;host_control[p].ranged_at=receipt;
        }
        const SudekiMpLanStoryRanged *fire=&host_control[p].ranged;
        BOOL allowed=known && remote_input_ready(p) && host_control[p].ready && !host_control[p].draining &&
            !story_swap_blocks(p) && !SudekiMpLanStoryMenuCapturesInput() &&
            host_control[p].ranged_at && (int32_t)(now-host_control[p].ranged_at)<=100 &&
            SudekiMpLanStoryControlFenceSame(&fire->fence,&host_control[p].fence) &&
            SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key);
        BOOL press=fire->press>host_control[p].ranged_press;
        /* Consume even a gated press: leaving a menu/cutscene must not fire
         * an old click. Repeats require fresh held intent every boundary. */
        host_control[p].ranged_press=fire->press;
        if(!allowed || !SudekiMpLanStoryShotsAim(w,&roster,c,fire,host_control[p].ranged_at)) {
            (void)SudekiMpLanStoryShotsAim(w,&roster,c,NULL,0);continue;
        }
        if(press || fire->held) {
            unsigned outcome=SudekiMpLanStoryControlRanged(w,&roster,&host_control[p].native_key,press?1u:2u);
            if(press) SudekiMpLogFormat("story_shots event=remote_input character=%u press=%lu outcome=%u\r\n",
                c,(unsigned long)fire->press,outcome);
        }
    }
    SudekiMpLanStoryRemoteUseService(); /* H1a: once per host frame */
    for(unsigned p=1;p<4u;++p) {
        if(!host_control[p].native_key.token) continue;
        unsigned character=host_control[p].native_key.seat;
        if(character!=0u && character!=2u) continue;
        BOOL admitted=known && remote_input_ready(p) && host_control[p].ready &&
            !host_control[p].draining && !story_swap_blocks(p) && !SudekiMpLanStoryMenuCapturesInput() &&
            (SudekiMpLanStoryHostControlBound(controller,w,scene) || exterior_control_allowed(p,EXTERIOR_ACTION)) &&
            SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key);
        uint8_t buttons=0;uint32_t serial=0;
        if(!SudekiMpLanPartyTakeStoryMelee(session,&host_control[p].connection,GetTickCount(),
            admitted,&buttons,&serial)) continue;
        {   /* H1a: a weak-attack press facing an unopened chest is a use,
             * exactly the native rule for the front player. The press is
             * consumed; while the open runs the opener does not attack. */
            void *actor=character<4u?roster.actors[character]:NULL;
            if(actor && SudekiMpLanStoryRemoteUseActive(actor)) buttons=0;
            else if(admitted && actor && (buttons&3u)==1u && SudekiMpLanStoryRemoteUseTry(actor)) {
                buttons&=(uint8_t)~3u;
                SudekiMpLogFormat("story_remote_use event=press_used player=%u character=%u serial=%lu\r\n",
                    p,character,(unsigned long)serial);
            }
        }
        unsigned outcome=SudekiMpLanStoryControlMeleeSnapshot(w,&roster,&host_control[p].native_key,buttons);
        static unsigned snapshot_logs;
        if(serial && SudekiMpLogResearchEnabled() && snapshot_logs++<4096u)
            SudekiMpLogFormat("story_melee event=native_snapshot player=%u character=%u serial=%lu buttons=%u selected=%u outcome=%u ms=%lu\r\n",
                p,character,(unsigned long)serial,buttons,SudekiMpStoryMeleePress(buttons),outcome,(unsigned long)GetTickCount());
    }
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanStoryActionRequest request;
        if(!host_control[p].native_key.token ||
            !SudekiMpLanPartyTakeStoryAction(session,&host_control[p].connection,GetTickCount(),&request)) continue;
        unsigned outcome=SUDEKIMP_STORY_ACTION_UNAVAILABLE;
        if(known && host_control[p].ready && !host_control[p].draining && !story_swap_blocks(p) &&
            !SudekiMpLanStoryMenuCapturesInput() &&
            remote_action_ready(p,request.kind) &&
            SudekiMpLanStoryControlFenceSame(&request.fence,&host_control[p].fence) &&
            (SudekiMpLanStoryHostControlBound(controller,w,scene) || exterior_control_allowed(p,EXTERIOR_ACTION)) &&
            SudekiMpLanStoryControlExact(w,&roster,&host_control[p].native_key)) {
            if(request.kind==SUDEKIMP_STORY_ACTION_SKILL)
                outcome=SudekiMpLanStoryCastSubmit(w,&roster,&host_control[p].native_key,request.slot);
            else if(request.kind==SUDEKIMP_STORY_ACTION_SPIRIT)
                outcome=SudekiMpLanStoryCastSubmitSpirit(w,&roster,&host_control[p].native_key,request.slot);
            else if(request.kind==SUDEKIMP_STORY_ACTION_EQUIP_WEAPON)
                outcome=SudekiMpLanStoryControlEquipWeapon(w,&roster,&host_control[p].native_key,request.slot);
            else if(request.kind==SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR)
                outcome=SudekiMpLanStoryControlEquipArmour(w,&roster,&host_control[p].native_key,request.slot);
            else if(request.kind==SUDEKIMP_STORY_ACTION_USE_ITEM)
                outcome=SudekiMpLanStoryControlUseItem(w,&roster,&host_control[p].native_key,
                    SUDEKIMP_STORY_ACTION_ITEM(request.slot,request.extra));
            /* Profile21 melee belongs exclusively to the snapshot stream.
             * Retired edge requests receive UNAVAILABLE, never a second path. */
        }
        /* Take consumed this ID before entering native code. Even an unknown
         * native outcome is terminal for this request: retransmission only
         * resends the retained result, never repeats Use or melee submission. */
        SudekiMpLanStoryActionResult result={request.fence,request.request,GetTickCount(),
            request.kind,request.slot,(uint8_t)outcome,request.extra};
        (void)SudekiMpLanPartyPublishStoryActionResult(session,&host_control[p].connection,&result);
        SudekiMpLogFormat("story_cast event=request_result player=%u character=%u request=%lu kind=%u slot=%u outcome=%u\r\n",
            p,request.fence.character,(unsigned long)request.request,request.kind,request.slot,outcome);
    }
    if(enemy_targeting_attempted) {
        SudekiMpStoryThreatSeat seats[4]={{0}};unsigned firing[4]={0};
        SudekiMpLanPartyLease away_keys[4]={{0}};BOOL enemy_coop=remote;
        for(unsigned c=0;c<4;++c)
            enemy_targeting_seat(c,w,known?&roster:NULL,&seats[c],&firing[c],&away_keys[c],&enemy_coop,now);
        /* Away AI keeps normal combat pressure after its input lease drains.
         * The dead-lead hold above still uses its original remote predicate. */
        SudekiMpStoryEnemyTargetingService(known?&roster:NULL,scene,seats,firing,away_keys,enemy_coop,now);
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
    if(SudekiMpStoryCharacterAudioRetains())
        for(unsigned c=0;c<4;++c)if(!SudekiMpStoryCharacterAudioStop(c,audio_cleanup_exact,&roster))return;
    if(SudekiMpStoryShotAudioRetains() && !SudekiMpStoryShotAudioStop(audio_cleanup_exact,&roster)) return;
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
/* #75 M-menu scope. Characters by exact party-actor vtable (Buki, Elco, Tal,
 * Ailish). The host may edit its own character and the AI companions, never a
 * character a connected client plays; a client edits its own character only. */
static BOOL menu_actor_readable(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    return p && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && (uintptr_t)p+4u<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static unsigned menu_character_of(void *actor) {
    static const uint32_t vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    uint8_t *b=(uint8_t *)main_menu_module;
    if(!b || !actor || !menu_actor_readable(actor)) return 4u;
    for(unsigned c=0;c<4u;++c) if(*(void **)actor==b+vt[c]) return c;
    return 4u;
}
static BOOL menu_slot_allowed(void *actor) {
    unsigned character=menu_character_of(actor);
    if(character>=4u) return FALSE;
    if(local_seat) return character==client_seed.leader_character;
    if(!session) return TRUE;
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer={0};
        if(SudekiMpLanPartyPeerStatusGet(session,p,&peer) && peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
            SudekiMpLanPartyPlayerCharacter(session,p)==character) return FALSE;
    }
    return TRUE;
}
static BOOL queue_client_action(void *actor,unsigned kind,unsigned slot);
static unsigned menu_equip_request(void *actor,unsigned item) {
    if(!local_seat) return SUDEKIMP_STORY_MENU_EQUIP_NATIVE;
    /* The armour choice page reuses the Weapons layer and its equip call;
     * its rows are armour item ids (100..139). */
    unsigned kind=item>=100u && item<=139u?SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR:SUDEKIMP_STORY_ACTION_EQUIP_WEAPON;
    return queue_client_action(actor,kind,item)?
        SUDEKIMP_STORY_MENU_EQUIP_QUEUED:SUDEKIMP_STORY_MENU_EQUIP_REFUSED;
}
static unsigned menu_use_request(void *actor,unsigned item) {
    if(!local_seat) return SUDEKIMP_STORY_MENU_EQUIP_NATIVE;
    return queue_client_action(actor,SUDEKIMP_STORY_ACTION_USE_ITEM,item)?
        SUDEKIMP_STORY_MENU_EQUIP_QUEUED:SUDEKIMP_STORY_MENU_EQUIP_REFUSED;
}
static BOOL queue_client_action(void *actor,unsigned kind,unsigned slot) {
    unsigned character=client_control_fence.character;
    if(!session || !local_seat || !client_input_ready || character>=4u ||
        actor!=client_seed.actors[character] || client_action_pending || client_action_serial==UINT32_MAX ||
        InterlockedCompareExchange(&stopping,0,0) || (!presentation_sample.valid && !main_menu_open()) ||
        (!presentation_fresh(GetTickCount(),presentation_sample.receipt) && !main_menu_open()) ||
        !same_connection(&client_control_connection,&presentation_sample.lease)) return FALSE;
    if(kind>UINT8_MAX || slot>(kind==SUDEKIMP_STORY_ACTION_USE_ITEM?998u:(unsigned)UINT8_MAX)) return FALSE;
    SudekiMpLanStoryActionRequest request={client_control_fence,client_action_serial+1u,
        presentation_sample.sequence,(uint8_t)kind,(uint8_t)(slot&0xffu),
        (uint8_t)(kind==SUDEKIMP_STORY_ACTION_USE_ITEM?slot>>8:0u)};
    if(!SudekiMpLanStoryActionRequestValid(&request)) return FALSE;
    client_action=request; ++client_action_serial;
    client_action_connection=client_control_connection;
    client_action_pending=TRUE; client_action_sent_at=0;
    return TRUE; /* Plain local outbox only. Never client-side CSkill::Use. */
}
static void service_client_action(const SudekiMpLanPartyLease *connection,
    const SudekiMpLanStoryControlFence *fence,uint32_t now) {
    if(!client_action_pending) return;
    /* A missing/aged offer is unknown ownership, not completion. Transport
     * may already own this ID. Keep it until the same fence recovers its
     * result, or a positively different owner is observed. Otherwise a later
     * request waits forever behind the abandoned transport journal entry. */
    if(!connection || !fence) return;
    if(!same_connection(connection,&client_action_connection) ||
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
        if(result.kind==SUDEKIMP_STORY_ACTION_USE_ITEM) {
            unsigned item=SUDEKIMP_STORY_ACTION_ITEM(result.slot,result.extra);
            if(result.outcome==SUDEKIMP_STORY_ACTION_STARTED) {
                /* STARTED belongs to the host's action. Only spend this game's
                 * copy here; authoritative resources, animation and heal VFX
                 * arrive through their existing replication paths. Replaying
                 * native apply locally can spawn a scripted entity outside
                 * ClientPresent and invalidate containment (#54 D6). */
                BOOL reduced=SudekiMpReduceLocalItem(item);
                BOOL refreshed=main_menu_open() && SudekiMpLanStoryMainMenuRefreshItems();
                SudekiMpLogFormat("story_main_menu event=use_applied item=%u local_apply=0 local_reduced=%u refreshed=%u presentation=host_replication\r\n",
                    item,(unsigned)reduced,(unsigned)refreshed);
            } else SudekiMpLanStoryMenuNotice(result.outcome==SUDEKIMP_STORY_ACTION_BUSY?
                "That item cannot be used right now.":"That item could not be used.");
        }
        if(result.kind==SUDEKIMP_STORY_ACTION_EQUIP_WEAPON || result.kind==SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR) {
            /* While this game's menu is shown its world presentation is
             * skipped, so the replica cannot follow the host's equipment until
             * the menu closes and the menu preview stayed stale (owner,
             * 2026-10-15). After the host accepted, equip this game's own
             * character with the same native routine its menu would have used
             * and refresh the open page; later frames then already match. */
            unsigned character=fence->character; void *actor=character<4u?client_seed.actors[character]:NULL;
            BOOL local=FALSE,refreshed=FALSE;
            if(result.outcome==SUDEKIMP_STORY_ACTION_STARTED && actor) {
                if(result.kind==SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR) {
                    SudekiMpStoryInventoryGrant grant; SudekiMpStoryInventoryGrantResult granted={0};
                    if(SudekiMpLanStoryInventoryPrepare(result.slot,1u,&grant) && !grant.before)
                        (void)SudekiMpLanStoryInventoryApply(&grant,&granted);
                    local=SudekiMpEquipCharacterArmourItem(actor,character,result.slot);
                } else local=SudekiMpEquipCharacterWeaponItem(actor,result.slot);
                if(main_menu_open()) {
                    refreshed=SudekiMpArmourMenuRefreshShown(actor);
                    if(result.kind==SUDEKIMP_STORY_ACTION_EQUIP_WEAPON) refreshed|=SudekiMpLanStoryMainMenuRefreshInventory();
                }
            } else SudekiMpLanStoryMenuNotice(result.outcome==SUDEKIMP_STORY_ACTION_BUSY?
                "Your character is busy; try again when ready.":
                result.kind==SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR?"That armour could not be equipped.":
                "That weapon could not be equipped.");
            SudekiMpLogFormat("story_main_menu event=equip_result kind=%u item=%u outcome=%u local=%u refreshed=%u\r\n",
                result.kind,result.slot,result.outcome,(unsigned)local,(unsigned)refreshed);
        }
        if((result.kind==SUDEKIMP_STORY_ACTION_SKILL || result.kind==SUDEKIMP_STORY_ACTION_SPIRIT) &&
            result.outcome!=SUDEKIMP_STORY_ACTION_STARTED)
            SudekiMpLanStoryMenuNotice(result.outcome==SUDEKIMP_STORY_ACTION_NO_SP?
                (result.kind==SUDEKIMP_STORY_ACTION_SPIRIT?"Not enough SSP for that Spirit Strike.":
                "Not enough SP for that skill."):result.outcome==SUDEKIMP_STORY_ACTION_BUSY?
                "Your character is busy; try again when ready.":
                result.outcome==SUDEKIMP_STORY_ACTION_EXPIRED?
                "The request expired; reopen Q and try again.":
                "That move is not available right now.");
        client_action_pending=FALSE; return;
    }
    if(!client_action_sent_at || (uint32_t)(now-client_action_sent_at)>=100u) {
        /* A send error is uncertain delivery. Retry this SAME request ID and
         * acknowledged frame; transport dedup owns at-most-once execution. */
        (void)SudekiMpLanPartySendStoryAction(session,connection,&client_action);
        client_action_sent_at=now;
    }
}
static void away_note_ui(uint32_t now) {
    if(main_menu_open() || shop_open() || examine_open()) { away_ui_seen=TRUE; away_ui_at=now; }
}
static void away_m_decision(BOOL owned,BOOL admitted,uint32_t now) {
    SudekiMpLanStoryScene remote={0}; SudekiMpLanPartyPeerStatus peer={0};
    BOOL known=SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
        SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote);
    SudekiMpStoryCombatSample sample=away_combat;
    sample.valid=sample.valid && presentation_sample.valid;
    SudekiMpStoryMenuInput input={.phase=away_client.phase,
        .combat=SudekiMpStoryCombatJudge(&sample,now,known && remote.phase==SUDEKIMP_LAN_STORY_READY,remote.epoch,remote.revision),
        .last_ui_at=away_ui_at,.ui_seen=away_ui_seen,
        .party_menu=SudekiMpLanStoryMenuCapturesInput(),.main_menu=main_menu_open(),
        .owned=owned,.admitted=admitted,.quick_menu=SudekiMpLanStoryQuickMenuCapturesInput(),
        .shop=shop_open(),.examine=examine_open(),.cinematic=host_cinematic_view};
    SudekiMpStoryMenuDecision decision=SudekiMpStoryAwayMenuDecision(&input,now);
    static const char *const names[]={"IGNORE","RETURN","CLOSE","STEP_AWAY","OPEN","WAIT"};
    SudekiMpLogFormat("story_step_away event=M decision=%s combat=%u phase=%u age_ms=%ld\r\n",
        names[decision],input.combat,input.phase,(long)(int32_t)(now-sample.receipt));
    switch(decision) {
    case SUDEKIMP_STORY_M_RETURN: (void)SudekiMpLanStoryRuntimeRequestAway(FALSE); break;
    case SUDEKIMP_STORY_M_STEP_AWAY: (void)SudekiMpLanStoryRuntimeRequestAway(TRUE); break;
    case SUDEKIMP_STORY_M_OPEN: (void)SudekiMpLanStoryMainMenuRequestOpen(); break;
    case SUDEKIMP_STORY_M_CLOSE: (void)SudekiMpLanStoryMainMenuRequestClose(); break;
    case SUDEKIMP_STORY_M_WAIT: SudekiMpLanStoryMenuNotice("Waiting for the host..."); break;
    default: break;
    }
}
static void service_step_away(void) {
    if(!saved_profile || !story_step_away || !local_seat || !session) return;
    uint32_t now=GetTickCount(); away_note_ui(now);
    SudekiMpLanPartyPeerStatus peer={0}; SudekiMpLanPartyPresence presence;
    BOOL connected=SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) &&
        peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed;
    if(!connected || (away_connection.token && !same_connection(&away_connection,&peer.lease))) {
        (void)SudekiMpStoryAwayObserve(&away_client,0,0,SUDEKIMP_PARTY_CONTROL_AI,0,now);
        away_pending_request=away_retry_at=0; memset(&away_connection,0,sizeof(away_connection));
        away_combat.valid=0;
        (void)SudekiMpLanStoryInputTakeMainMenuRaw();
        if(!connected) return;
    }
    away_connection=peer.lease;
    if(away_client.phase!=SUDEKIMP_STORY_AWAY_NONE) {
        if(SudekiMpLanStoryInputTakeMainMenuRaw()) away_m_decision(FALSE,FALSE,now);
    } else if((!presentation_sample.valid || !presentation_fresh(now,presentation_sample.receipt)) &&
        SudekiMpLanStoryInputTakeMainMenuRaw()) {
        /* Even an unpresentable/stale world must answer M with UNKNOWN,
         * instead of retaining the edge until a later fresh combat frame. */
        away_m_decision(client_local_selected,connected,now);
    }
    if(!SudekiMpLanPartyGetPresence(session,&presence)) {
        if(away_client.phase==SUDEKIMP_STORY_AWAY_RETURNING &&
            SudekiMpStoryAwayObserve(&away_client,connected,1,SUDEKIMP_PARTY_CONTROL_AI,0,now))
            SudekiMpLanStoryMenuNotice("Couldn't take control - press M to try again");
        return;
    }
    const SudekiMpPartyOwnership *o=&presence.ownership;
    BOOL away=(o->away&(1u<<local_seat))!=0;
    if(away_pending_request && presence.ack_request[local_seat]>away_pending_request) away_pending_request=0;
    if(away_pending_request && presence.ack_request[local_seat]==away_pending_request) {
        unsigned result=presence.ack_result[local_seat]; away_pending_request=0;
        if(result==SUDEKIMP_PARTY_SWAP_BUSY) away_retry_at=now+250u;
        else if(result!=SUDEKIMP_PARTY_SWAP_OK && result!=SUDEKIMP_PARTY_SWAP_STALE) {
            SudekiMpStoryAwayRefused(&away_client,away);
            SudekiMpLanStoryMenuNotice(result==SUDEKIMP_PARTY_AWAY_OTHER_AREA?
                "Too far from the party to step away":"Could not step away - try again");
        }
    }
    /* A second M can reverse intent before the first request's ACK. Do not
     * call the return complete while that first AWAY is still in flight. */
    BOOL drives=client_input_ready;
    uint32_t return_at=away_client.return_at;
    unsigned previous=away_client.phase;
    if(SudekiMpStoryAwayObserve(&away_client,connected,away,(away_pending_request && away_client.phase==SUDEKIMP_STORY_AWAY_RETURNING)?
        SUDEKIMP_PARTY_CONTROL_AI:o->control[local_seat],drives,now))
        SudekiMpLanStoryMenuNotice("Couldn't take control - press M to try again");
    if(previous==SUDEKIMP_STORY_AWAY_RETURNING && away_client.phase==SUDEKIMP_STORY_AWAY_NONE)
        SudekiMpLogFormat("story_step_away event=returned press_to_control_ms=%lu\r\n",(unsigned long)(now-return_at));
    if(away_pending_request || away_client.want_away==away ||
        (away_retry_at && (int32_t)(now-away_retry_at)<0)) return;
    unsigned c=o->assignment.character[local_seat]; if(c>=4u) return;
    uint32_t floor=presence.ack_request[local_seat];
    if(floor<o->last_request[local_seat]) floor=o->last_request[local_seat];
    if(story_next_request<floor) story_next_request=floor;
    if(story_next_request==UINT32_MAX) return;
    SudekiMpLanPartyCommand command={.request=++story_next_request,.world=o->assignment.world,
        .revision=o->assignment.revision,.generation=o->assignment.generation[c],
        .kind=SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,.player=(uint8_t)local_seat,.target=4,
        .value=away_client.want_away?SUDEKIMP_LAN_PARTY_PRESENCE_AWAY:0};
    if(SudekiMpLanPartyQueueCommand(session,&command)) { away_pending_request=command.request; away_retry_at=0; }
    else away_retry_at=now+250u;
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
    away_note_ui(now);
    if(toggle) (void)SudekiMpLanStoryInputTakeMainMenuRaw();
    if(main_menu_attempted && !toggle) {
        BOOL pressed=SudekiMpLanStoryInputTakeMainMenu(r->controller,r->actors[r->leader_character],client_control_fence.transaction);
        if(story_step_away) {
            pressed=SudekiMpLanStoryInputTakeMainMenuRaw() || pressed;
            if(pressed) {
                SudekiMpLanPartyPeerStatus current={0};
                BOOL mine=client_local_selected && r->leader_character==SudekiMpLanPartyLocalCharacter(session);
                BOOL live=SudekiMpLanPartyPeerStatusGet(session,local_seat,&current) &&
                    current.transport_confirmed && current.phase==SUDEKIMP_LAN_PARTY_OBSERVING;
                away_m_decision(mine,live,now);
            }
        } else if(pressed && !SudekiMpLanStoryMainMenuVisible() && owned && admitted &&
            SudekiMpLanStoryMainMenuRequestOpen()) (void)SudekiMpLanStoryInputTakeMainMenuRaw();
    }
    if(away_client.phase!=SUDEKIMP_STORY_AWAY_NONE) { owned=admitted=toggle=FALSE; }
    return SudekiMpLanStoryQuickMenuService(r,client_control_fence.transaction,owned,admitted,toggle);
}
/* Retain only an ALREADY armed binding through another character's split
 * revision. The caller may send freshly sampled movement under the unchanged
 * control fence, but presents no old media and discards combat/menu edges. */
static BOOL client_revision_gap_matches(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,const SudekiMpLanPartyPeerStatus *peer,
    const SudekiMpLanStoryScene *remote,const SudekiMpLanStoryControlState *state,uint32_t now) {
    if(!session || !saved_profile || !local_seat || local_seat>=4u ||
        !local_control_attempted || !client_switch_prepared || !client_local_selected ||
        !client_revision_gap || presentation_sample.valid || presentation_changed ||
        client_control_trace_state!=5u || !client_input_sequence ||
        InterlockedCompareExchange(&stopping,0,0) || !roster || !native || !peer || !remote || !state)
        return FALSE;
    int32_t age=(int32_t)(now-presentation_sample.receipt);
    unsigned chosen=state->fence.character;
    const SudekiMpLanStoryScene *prior=&presentation_sample.scene;
    const SudekiMpLanStoryNativeRoster *before=&presentation_sample.native_roster;
    if(age<0 || age>250 || chosen>=4u || !presentation_sample.sequence ||
        state->phase!=SUDEKIMP_STORY_CONTROL_READY || state->fence.player!=local_seat ||
        peer->phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !peer->transport_confirmed ||
        !same_connection(&peer->lease,&presentation_sample.lease) ||
        !same_connection(&peer->lease,&client_control_connection) ||
        !SudekiMpLanStoryControlFenceSame(&state->fence,&client_control_fence) ||
        !SudekiMpLanStoryControlMatchesScene(&state->fence,prior) ||
        !SudekiMpLanStoryControlMatchesScene(&state->fence,remote) ||
        presented_generations[chosen]!=state->fence.actor_generation ||
        prior->epoch!=presented_epoch || remote->epoch!=prior->epoch ||
        remote->revision<=prior->revision || prior->available_mask!=remote->available_mask ||
        prior->leader_seat!=remote->leader_seat ||
        memcmp(prior->world,remote->world,sizeof(prior->world)) ||
        !SudekiMpLanStorySceneValid(native) || native->phase!=SUDEKIMP_LAN_STORY_READY ||
        !SudekiMpLanStorySceneCharacterAreaMatches(prior,chosen,native->world,native->temporary) ||
        !SudekiMpLanStorySceneCharacterAreaMatches(remote,chosen,native->world,native->temporary) ||
        roster->epoch!=native->epoch || roster->revision!=native->revision ||
        roster->available_mask!=remote->available_mask || roster->leader_character!=chosen ||
        roster->epoch!=before->epoch || roster->revision!=before->revision ||
        roster->available_mask!=before->available_mask || roster->leader_character!=before->leader_character ||
        !roster->world || !roster->descriptor || !roster->group || !roster->controller ||
        roster->world!=before->world || roster->descriptor!=before->descriptor ||
        roster->group!=before->group || roster->controller!=before->controller ||
        memcmp(roster->actors,before->actors,sizeof(roster->actors)) ||
        memcmp(roster->ai,before->ai,sizeof(roster->ai))) return FALSE;
    return roster->actors[chosen] && roster->ai[chosen];
}
/* A host-only area revision is not a reason to stop the unchanged client's
 * owned Exploration camera. This callback runs inside the normal native
 * containment bracket and re-proves the connection, roster and original
 * receipt horizon. It cannot apply poses, resources, effects, audio or ACKs. */
static BOOL client_revision_gap_camera(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,void *unused) {
    (void)unused;
    SudekiMpLanPartyPeerStatus peer; SudekiMpLanStoryScene remote;
    SudekiMpLanStoryControlState state; SudekiMpStoryCastView camera;
    SudekiMpLanArenaOwnerViewLease owner;
    uint32_t now=GetTickCount();
    if(!client_camera_attempted || !runtime_thread || runtime_thread!=GetCurrentThreadId() ||
        host_cinematic_view || presented_cast_view.kind || (shop_attempted && SudekiMpLanStoryShopSpeechCameraHeld()) ||
        SudekiMpLanStoryLocalControlCastActive() || SudekiMpLanStoryMenuCapturesInput() ||
        SudekiMpLanStoryQuickMenuCapturesInput() || main_menu_open() ||
        SudekiMpLanStoryWorldRecruiting() || SudekiMpLanStoryClientRecruiting() ||
        !SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        !SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) ||
        !SudekiMpLanPartyGetStoryControl(session,&peer.lease,now,&state) ||
        !client_revision_gap_matches(roster,native,&peer,&remote,&state,now) ||
        !replica_exact(roster,NULL) ||
        !SudekiMpLanStoryInputExact(roster->controller,roster->actors[state.fence.character]) ||
        !SudekiMpLanStoryLocalControlViewOwner(&owner) || !native_camera_owner_same(&owner)) return FALSE;
    /* Unknown camera/cinematic state is not permission to take over. Require
     * current-revision observations, independent of the missing world batch.
     * A normal cast selector has no area-dependent view to replay. */
    if(!SudekiMpLanPartyGetStoryCastView(session,&peer.lease,&state.fence,now,
            remote.observed_tick,&camera) || camera.kind || camera.view.valid ||
        camera.revision!=remote.revision ||
        !SudekiMpLanStoryControlFenceSame(&camera.fence,&state.fence)) return FALSE;
    if(!dialogue_count) return FALSE;
    unsigned last=dialogue_count-1u;
    if(!presentation_fresh(now,dialogue_receipts[last]) ||
        dialogue_history[last].epoch!=remote.epoch || dialogue_history[last].revision!=remote.revision ||
        ((dialogue_history[last].flags&SUDEKIMP_STORY_LETTERBOX_PRESENT) &&
         SudekiMpLanStorySceneConversationShared(&remote,state.fence.character,4u))) return FALSE;
    /* ViewOwner is a stored identity, not a fresh target proof. Validate the
     * selected actor/view binding before any camera call; continuation must
     * also refuse a native state takeover instead of requesting Exploration. */
    if(!SudekiMpLanStoryLocalControlAdoptNative(roster,replica_exact,NULL) ||
        !SudekiMpLanStoryClientCameraContinuityReady(&owner)) {
        native_camera_clock.applied=FALSE;
        return FALSE;
    }
    float look_x=0,look_y=0;
    (void)SudekiMpLanStoryInputLookAxes(&look_x,&look_y);
    BOOL applied=present_native_camera(roster,now,look_x,look_y);
    static uint32_t traced_revision; static unsigned traces;
    if(applied && traced_revision!=remote.revision && traces<64u) {
        traced_revision=remote.revision; ++traces;
        SudekiMpLogFormat("lan_story event=revision_gap_camera revision=%lu receipt_age=%lu camera_only=1 presentation_valid=0\r\n",
            (unsigned long)remote.revision,(unsigned long)(now-presentation_sample.receipt));
    }
    return applied;
}
/* Called only after the fresh same-area/native-binding proof above. The
 * session independently bounds the old acknowledged-frame reference; this
 * path does not ACK a new frame or promote general client input readiness. */
static BOOL client_revision_gap_send_motion(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *native,const SudekiMpLanPartyLease *peer,
    const SudekiMpLanStoryControlFence *fence,uint32_t now,float local_x,float local_z) {
    if(client_input_sequence==UINT32_MAX ||
        (client_input_sent_at && (uint32_t)(now-client_input_sent_at)<15u)) return FALSE;
    float x=0,z=0;
    if(!SudekiMpLanStoryLocalControlDirection(controller,w,roster,native,local_x,local_z,&x,&z))
        return FALSE;
    SudekiMpLanStoryMovement input={*fence,++client_input_sequence,presentation_sample.sequence,x,z};
    if(!SudekiMpLanPartySendStoryMovement(session,peer,&input)) return FALSE;
    client_input_sent_at=now;
    return TRUE;
}
static void service_client_control(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryScene *native) {
    if(!local_control_attempted) return;
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanStoryScene remote;
    SudekiMpLanStoryControlState state;
    uint32_t now=GetTickCount();
    unsigned chosen=SudekiMpLanPartyLocalCharacter(session);
    BOOL away_look=story_step_away && away_client.phase!=SUDEKIMP_STORY_AWAY_NONE &&
        client_local_selected && chosen<4u && !SudekiMpLanStoryMenuCapturesInput() &&
        !SudekiMpLanStoryQuickMenuCapturesInput() && !main_menu_open() && !shop_open();
    SudekiMpLanStoryNativeRoster away_roster;
    if(away_look && SudekiMpLanStoryObserverRoster(controller,w,native,&away_roster) &&
        away_roster.leader_character==chosen)
        (void)SudekiMpLanStoryInputAwayLook(controller,away_roster.actors[chosen],TRUE);
    else (void)SudekiMpLanStoryInputAwayLook(NULL,NULL,FALSE);
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
    if(!offered || away_client.want_away) {
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
    BOOL roster_known=SudekiMpLanStoryObserverRoster(controller,w,native,&roster);
    BOOL local_ready=roster_known && SudekiMpLanStoryLocalControlReady(controller,w,&roster,native);
    const char *pending=!roster_known?"pending_roster":
        /* #75: the native Main Menu owns the camera while shown; the local
         * view cannot be ready and no frame can be presented. Hold the lease. */
        !local_ready && !main_menu_open() && !shop_open()?"pending_local_control":
        !presentation_sample.valid && !main_menu_open() && !shop_open()?"pending_presentation_invalid":
        /* #75: with the native Main Menu shown the world-render branch is
         * skipped, so no frame can be presented; keep this control lease and
         * its (muted) input alive on the last presented frame instead of
         * decaying to a stale presentation and losing the host offer. */
        !presentation_fresh(now,presentation_sample.receipt) && !main_menu_open() && !shop_open()?"pending_presentation_stale":
        presentation_sample.scene.epoch!=state.fence.epoch?"pending_epoch":
        presentation_sample.scene.revision<state.fence.revision?"pending_revision":
        presented_generations[chosen]!=state.fence.actor_generation?"pending_generation":
        !same_connection(&presentation_sample.lease,&peer.lease)?"pending_connection":
        !SudekiMpLanStoryInputArm(controller,roster.actors[chosen],state.fence.transaction)?"pending_input_arm":NULL;
    if(pending) {
        float retained_x,retained_z;
        if(local_ready && client_revision_gap_matches(&roster,native,&peer,&remote,&state,now) &&
            !SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput() &&
            !main_menu_open() && SudekiMpLanStoryInputSample(controller,roster.actors[chosen],
                state.fence.transaction,&retained_x,&retained_z)) {
            /* Keep fresh steering alive while the other area's new media is
             * assembled. Never resend a held vector or replay gameplay edges. */
            BOOL movement_sent=client_revision_gap_send_motion(controller,w,&roster,native,
                &peer.lease,&state.fence,now,retained_x,retained_z);
            SudekiMpLanStoryInputMuteForCamera();
            BOOL discarded_held;uint32_t discarded_press;
            if(SudekiMpLanStoryInputRanged(controller,roster.actors[chosen],state.fence.transaction,
                &discarded_held,&discarded_press)) client_ranged_physical_press=discarded_press;
            (void)SudekiMpLanStoryInputTakeQuickMenu(controller,roster.actors[chosen],state.fence.transaction);
            (void)SudekiMpLanStoryInputTakeMainMenu(controller,roster.actors[chosen],state.fence.transaction);
            if(away_client.phase==SUDEKIMP_STORY_AWAY_NONE) (void)SudekiMpLanStoryInputTakeMainMenuRaw();
            static uint32_t traced_epoch,traced_revision;
            static unsigned traces;
            if(traces<64u && (traced_epoch!=remote.epoch || traced_revision!=remote.revision)) {
                ++traces;traced_epoch=remote.epoch;traced_revision=remote.revision;
                SudekiMpLogFormat("lan_story_control event=client_revision_gap epoch=%lu prior_revision=%lu revision=%lu receipt_age=%lu transaction=%lu same_fence=1 binding=retained input_admitted=0 presentation_valid=0 movement_sent=%u\r\n",
                    (unsigned long)remote.epoch,(unsigned long)presentation_sample.scene.revision,
                    (unsigned long)remote.revision,(unsigned long)(now-presentation_sample.receipt),
                    (unsigned long)state.fence.transaction,movement_sent?1u:0u);
            }
            return;
        }
        client_revision_gap=FALSE;
        SudekiMpLanStoryInputClear();
        if(main_menu_open()) { static const char *last_hold; if(pending!=last_hold) { last_hold=pending;
            SudekiMpLogFormat("story_main_menu event=hold_pending reason=%s\r\n",pending); } }
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
        client_input_sequence=client_input_sent_at=client_ranged_sequence=client_block_sequence=0;
        memset(&client_melee_sender,0,sizeof(client_melee_sender));
        client_melee_generation=0;
        client_block_sent_at=0;client_block_sent=client_block_last_held=FALSE;
        client_ranged_physical_press=client_ranged_wire_press=0;
    }
    if(state.phase==SUDEKIMP_STORY_CONTROL_PREPARE) {
        if(SudekiMpLanPartyAcknowledgeStoryControl(session,&peer.lease,&state.fence))
            trace_client_control(4u,"native_binding_acknowledged",state.fence.transaction);
        return;
    }
    service_client_action(&peer.lease,&state.fence,now);
    float local_x=0,local_z=0,x=0,z=0;
    if(SudekiMpLanStoryMenuCapturesInput()) SudekiMpLanStoryInputClear();
    else if(SudekiMpLanStoryQuickMenuCapturesInput() || main_menu_open() || table_modal()) SudekiMpLanStoryInputMuteMovement();
    else if(!SudekiMpLanStoryInputSample(controller,roster.actors[chosen],state.fence.transaction,&local_x,&local_z) ||
        !SudekiMpLanStoryLocalControlDirection(controller,w,&roster,native,local_x,local_z,&x,&z)) {
        static unsigned sample_fail_logs;
        if(SudekiMpLogResearchEnabled() && sample_fail_logs<200u) { ++sample_fail_logs; SudekiMpLogFormat("lan_story_control event=client_input_sample_failed ms=%lu\r\n",(unsigned long)GetTickCount()); }
        SudekiMpLanStoryInputClear(); return;
    }
    {
        /* Bounded research diagnostic: moving/stopped transitions of the sent input. */
        static BOOL was_moving; static unsigned move_logs;
        BOOL moving=(x!=0.0f || z!=0.0f);
        if(SudekiMpLogResearchEnabled() && moving!=was_moving && move_logs<400u) {
            ++move_logs; was_moving=moving;
            SudekiMpLogFormat("lan_story_control event=client_input ms=%lu moving=%u local=%.2f,%.2f world=%.2f,%.2f\r\n",
                (unsigned long)GetTickCount(),moving,(double)local_x,(double)local_z,(double)x,(double)z);
        }
    }
    client_input_ready=TRUE;
    if(chosen==0u || chosen==2u) {
        uint8_t buttons=0;uint32_t generation=0;
        BOOL active=SudekiMpLanStoryInputMelee(controller,roster.actors[chosen],state.fence.transaction,
            &buttons,&generation) && !SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput() && !main_menu_open() &&
            !table_modal();
        /* H1b: while this client's examinable box is up, its interact press
         * and release are the box's (confirm), never a swing on the host. */
        if(examine_open()) {
            static BOOL examine_pressed;
            unsigned weak=buttons&3u;
            if(weak==1u) { SudekiMpLanStoryZoneBannerExamineInput(TRUE); examine_pressed=TRUE; }
            else if(weak==3u) {
                if(!examine_pressed) SudekiMpLanStoryZoneBannerExamineInput(TRUE);
                SudekiMpLanStoryZoneBannerExamineInput(FALSE); examine_pressed=FALSE;
            }
            if(weak) SudekiMpLanStoryInputConsumeWeak();
            buttons=0;
        }
        /* H1b: during this game's own Kamo conversation the interact press
         * is the conversation's (e.g. it closes the farewell). */
        if(shop_open() && ((buttons&3u)==1u || (buttons&3u)==3u)) {
            SudekiMpLanStoryShopInteractPress(); SudekiMpLanStoryInputConsumeWeak(); buttons&=~3u;
        }
        /* H1b: with the shopkeeper's talk prompt shown, the interact (weak)
         * press is this game's own, never a swing on the host. */
        if(active && shop_attempted && SudekiMpLanStoryShopPromptShown() &&
            ((buttons&3u)==1u || (buttons&3u)==3u)) {       /* a tap may sample as release */
            SudekiMpLanStoryShopRequestOpen(); SudekiMpLanStoryInputConsumeWeak(); buttons&=~3u;
        }
        if(generation!=client_melee_generation) {
            SudekiMpStoryMeleeCancel(&client_melee_sender);client_melee_generation=generation;
        }
        SudekiMpLanStoryMelee melee;
        if(SudekiMpStoryMeleeBuild(&client_melee_sender,&state.fence,presentation_sample.sequence,
            now,buttons,active,&melee)) {
            (void)SudekiMpLanPartySendStoryMelee(session,&peer.lease,&melee);
            static unsigned sample_logs;
            if(SudekiMpLogResearchEnabled() && SudekiMpStoryMeleePress(buttons) && sample_logs++<4096u)
                SudekiMpLogFormat("story_melee event=sample ms=%lu serial=%lu buttons=%u selected=%u\r\n",
                    (unsigned long)now,(unsigned long)client_melee_sender.serial,buttons,SudekiMpStoryMeleePress(buttons));
        }
    }
    trace_client_control(5u,"host_confirmed_movement",state.fence.transaction);
    if(client_input_sequence==UINT32_MAX) { client_input_ready=FALSE; SudekiMpLanStoryInputClear(); return; }
    /* A block press/release is an edge, not a movement heartbeat. Send it
     * on this dispatch even when the 15 ms movement cadence has not elapsed. */
    if((chosen==0u || chosen==2u) && client_block_sequence!=UINT32_MAX) {
        BOOL held=!SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput() && !main_menu_open() &&
            SudekiMpLanStoryInputBlock(controller,roster.actors[chosen],state.fence.transaction);
        if(!client_block_sent || held!=client_block_last_held || now-client_block_sent_at>=15u) {
            SudekiMpLanStoryBlock block={.fence=state.fence,.sequence=++client_block_sequence,
                .acknowledged_frame=presentation_sample.sequence,.held=held};
            if(SudekiMpLanPartySendStoryBlock(session,&peer.lease,&block)) {
                if(SudekiMpLogResearchEnabled() && (!client_block_sent || held!=client_block_last_held))
                    SudekiMpLogFormat("story_block event=input_sent ms=%lu held=%u sequence=%lu\r\n",
                        (unsigned long)now,held,(unsigned long)block.sequence);
                client_block_sent=TRUE;client_block_last_held=held;client_block_sent_at=now;
            }
        }
    }
    /* Send on every client frame (~21 ms). A 33 ms gate on a ~21 ms frame
     * sent every second frame (42-63 ms), which periodically outran the
     * host's input hold and showed as an idle pose between run cycles. */
    if(client_input_sent_at && (uint32_t)(now-client_input_sent_at)<15u) return;
    SudekiMpLanStoryMovement input={state.fence,++client_input_sequence,presentation_sample.sequence,x,z};
    if(SudekiMpLanPartySendStoryMovement(session,&peer.lease,&input)) client_input_sent_at=now;
    if((chosen==1u || chosen==3u) && client_ranged_sequence!=UINT32_MAX) {
        SudekiMpLanStoryRanged ranged={0}; BOOL held=FALSE;
        ranged.fence=state.fence;ranged.sequence=++client_ranged_sequence;
        ranged.acknowledged_frame=presentation_sample.sequence;
        uint32_t physical=client_ranged_physical_press;
        BOOL sampled=SudekiMpLanStoryInputRanged(controller,roster.actors[chosen],state.fence.transaction,&held,&physical);
        BOOL permitted=sampled && !SudekiMpLanStoryMenuCapturesInput() && !SudekiMpLanStoryQuickMenuCapturesInput() && !main_menu_open();
        if(physical!=client_ranged_physical_press) {
            if(permitted && client_ranged_wire_press!=UINT32_MAX) ++client_ranged_wire_press;
            client_ranged_physical_press=physical;
        }
        ranged.press=client_ranged_wire_press;ranged.held=permitted && held && ranged.press;
        if(SudekiMpLanStoryLocalControlAim(controller,w,&roster,native,ranged.direction,ranged.target))
            (void)SudekiMpLanPartySendStoryRanged(session,&peer.lease,&ranged);
    }
}

/* H1b: the shopkeeper's talk prompt and this client's own shop lease (D11),
 * inside the exact client presentation step. */
static BOOL service_shop(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)unused; (void)scene;
    unsigned c=session?SudekiMpLanPartyLocalCharacter(session):4u;
    /* Only Buki and Tal send the weak press this prompt claims. */
    void *actor=(c==0u || c==2u) && (r->available_mask&(1u<<c))?r->actors[c]:NULL;
    uint32_t now=GetTickCount();
    BOOL blocked=!client_input_ready || InterlockedCompareExchange(&stopping,0,0) ||
        SudekiMpLanStoryMenuCapturesInput() || SudekiMpLanStoryQuickMenuCapturesInput() || main_menu_open() ||
        host_cinematic_view ||
        /* A host line or letterbox actually on screen (a valid record alone
         * is the idle state: live 2026-10-17 it hid the prompt throughout;
         * a conversation flag without a line draws nothing, and the host's
         * own shopkeeper conversation is sent that way until its first line
         * classifies it). */
        (visible_dialogue_valid && presentation_fresh(now,visible_dialogue_receipt) &&
            (visible_dialogue.flags&(SUDEKIMP_STORY_LINE_ACTIVE|SUDEKIMP_STORY_LETTERBOX_PRESENT)));
    static BOOL last_blocked=2; static unsigned blocked_logs;
    if(blocked!=last_blocked && blocked_logs<64u) { last_blocked=blocked; ++blocked_logs;
        SudekiMpLogFormat("story_shop event=gate blocked=%u input=%u menu=%u quick=%u main=%u cinematic=%u dialogue=%u in_shop=%s actor=%u\r\n",
            (unsigned)blocked,(unsigned)client_input_ready,(unsigned)SudekiMpLanStoryMenuCapturesInput(),
            (unsigned)SudekiMpLanStoryQuickMenuCapturesInput(),(unsigned)main_menu_open(),(unsigned)host_cinematic_view,
            (unsigned)(visible_dialogue_valid?visible_dialogue.flags:0u),
            SudekiMpLanStoryRemoteInteriorClientShopZone()?SudekiMpLanStoryRemoteInteriorClientShopZone():"-",(unsigned)(actor!=NULL)); }
    SudekiMpLanStoryShopService(actor,SudekiMpLanStoryRemoteInteriorClientShopZone(),blocked,now);
    return FALSE;
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
            /* #75: the native Main Menu can always be left with M, and the
             * containment tolerates the menu's own UI tasks while it is shown
             * and for a moment after it closes. */
            {
                static DWORD menu_seen_at,shop_seen_at; DWORD t=GetTickCount();
                BOOL shown=main_menu_attempted && SudekiMpLanStoryMainMenuVisible(),shop=shop_open();
                away_note_ui(t);
                if(shown) menu_seen_at=t;
                if(shop) shop_seen_at=t;
                if(main_menu_attempted || shop_attempted)
                    SudekiMpLanStoryClientAllowUiTasks(shown || shop || (menu_seen_at && t-menu_seen_at<3000u) ||
                        (shop_seen_at && t-shop_seen_at<3000u));
                if(shown) { (void)SudekiMpLanStoryInputTakeMainMenuRaw(); SudekiMpLanStoryMainMenuPollKeys(); }
                /* H1b: the shop screen takes Escape/M/F3; a due house exit
                 * waits until it is closed (and asks it to close). */
                if(shop) SudekiMpLanStoryShopPollKeys();
                if(shop_attempted) {
                    SudekiMpLanStoryRemoteInteriorClientHoldExit(shop);
                    if(shop && SudekiMpLanStoryRemoteInteriorClientWantsOut()) SudekiMpLanStoryShopRequestClose("interior_exit");
                    if(examine_open() && SudekiMpLanStoryRemoteInteriorClientWantsOut())
                        SudekiMpLanStoryZoneBannerExamineClose("interior_exit");
                }
            }
            SudekiMpLanStoryClientReport report={0};
            BOOL contained=SudekiMpLanStoryClientRetains()?
                SudekiMpLanStoryClientService(&report):SudekiMpLanStoryClientAcquire(&report);
            /* #75: with the client's Main Menu shown the world-render branch
             * (and so the presentation attempt) is skipped by the engine; that
             * is a hold, not an invalid presentation. */
            if(!contained || (!presentation_attempted && !main_menu_open() && !shop_open())) invalidate_presentation();
            /* A stale native interaction prompt (e.g. the save point's hand
             * icon and "Save") survives the pause; clear it through the HUD's
             * own update every half second while contained. */
            if(contained) { static DWORD prompt_checked; DWORD t=GetTickCount();
                /* Not under this client's own talk prompt or shop screen. */
                if(t-prompt_checked>=500u && !table_modal() && !(shop_attempted && SudekiMpLanStoryShopPromptShown())) {
                    prompt_checked=t; (void)SudekiMpLanStoryContextPromptClear((HMODULE)game_base); } }
            if(contained && quick_menu_attempted) {
                BOOL serviced=SudekiMpLanStoryClientPresent(service_quick_menu,NULL);
                if(main_menu_open()) { static BOOL last=TRUE; static DWORD at; DWORD t=GetTickCount();
                    if(serviced!=last || t-at>=2000u) { last=serviced; at=t;
                        SudekiMpLogFormat("story_main_menu event=menu_service present=%u error=%lu\r\n",(unsigned)serviced,(unsigned long)GetLastError()); } }
            }
            if(contained && shop_attempted) (void)SudekiMpLanStoryClientPresent(service_shop,NULL);
            if(contained && (SudekiMpLanStoryCinematicAudioRetains() || SudekiMpStoryCharacterAudioRetains()) &&
                (!visible_dialogue_valid || !presentation_fresh(GetTickCount(),visible_dialogue_receipt)))
                (void)SudekiMpLanStoryClientPresent(stop_stale_audio,NULL);
            if(contained && SudekiMpStoryShotAudioNeedsService(GetTickCount()))
                (void)SudekiMpLanStoryClientPresent(drain_shot_audio,NULL);
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
        if(local_seat && visible_dialogue_valid && !shop_open() && presentation_fresh(GetTickCount(),visible_dialogue_receipt)) {
            BOOL ok=SudekiMpStoryCinematicViewRender(*slot,&visible_dialogue);
            DWORD error=ok?ERROR_SUCCESS:GetLastError();
            trace_cinematic(&overlay_trace,"overlay",&visible_dialogue,ok,error);
        }
        if(!main_menu_open()) SudekiMpLanStoryNameTagsRender(*slot); /* no world behind the client's menu */
        (void)SudekiMpLanStoryMenuRender(*slot);
        if(local_seat && story_step_away)
            (void)SudekiMpStoryAwayBannerRender(game_base,*slot,away_client.phase,SudekiMpLanPartyLocalCharacter(session));
        unsigned prompt_choice;
        if(local_seat && SudekiMpLanStoryMainMenuPrompt(&prompt_choice)) {
            BOOL ok=SudekiMpPartyMenuRenderConfirm(*slot,"Use this item on yourself?",prompt_choice);
            SudekiMpLanStoryMainMenuPromptRendered(ok,ok?ERROR_SUCCESS:GetLastError());
        }
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
BOOL SudekiMpLanStoryRuntimePlayerName(unsigned character,char out[32]) {
    if(!out) return FALSE; out[0]=0;
    if(character>=4u || !menu_lobby_known || runtime_thread!=GetCurrentThreadId()) return FALSE;
    for(unsigned i=0;i<4u;++i) {
        const SudekiMpLobbyMember *m=&menu_lobby.members[i];
        if(!m->present || !m->reserved || m->character!=character || !m->name[0]) continue;
        memcpy(out,m->name,31); out[31]=0; return TRUE;
    }
    return FALSE;
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
/* D9: one native pickup, copied to each connected game's own inventory.
 * This journal owns plain data only. Network backpressure retains its head;
 * it never substitutes a new serial or overwrites an undelivered pickup. */
static unsigned loot_recipients(SudekiMpLanPartyLease out[4]) {
    unsigned mask=0;
    for(unsigned p=1;p<4u;++p) {
        SudekiMpLanPartyPeerStatus peer;
        if(SudekiMpLanPartyPeerStatusGet(session,p,&peer) && peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed) {
            out[p]=peer.lease; mask|=1u<<p;
        }
    }
    return mask;
}
static void observe_story_loot(const SudekiMpStoryInventoryEvent *event,void *context) {
    (void)context;
    if(!session || local_seat || !saved_profile || GetCurrentThreadId()!=runtime_thread ||
        InterlockedCompareExchange(&stopping,0,0) || !event || !event->source) return;
    SudekiMpLanPartyLease recipients[4]={{0}};
    unsigned mask=loot_recipients(recipients);
    if(!mask) return; /* No joined player is owed a grant before admission. */
    if(!event->capture_valid) {
        loot_capture_fault=TRUE;
        SudekiMpLogFormat("story_loot event=capture_unknown source=%lu item=%ld quantity=%ld admission=quarantined\r\n",
            (unsigned long)event->source,(long)event->item,(long)event->quantity);
        return;
    }
    StoryPendingLoot *pending=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*pending));
    if(!pending) {
        loot_capture_fault=TRUE;
        SudekiMpLogFormat("story_loot event=capture_failed source=%lu item=%ld quantity=%ld reason=allocation admission=quarantined\r\n",
            (unsigned long)event->source,(long)event->item,(long)event->quantity);
        return;
    }
    pending->event=*event; pending->mask=mask;
    memcpy(pending->recipients,recipients,sizeof(recipients));
    if(loot_tail) loot_tail->next=pending; else loot_head=pending;
    loot_tail=pending; ++loot_pending;
}
static void clear_story_loot(void) {
    if(loot_pending) SudekiMpLogFormat("story_loot event=session_ended pending=%u persisted=0\r\n",loot_pending);
    while(loot_head) {
        StoryPendingLoot *old=loot_head; loot_head=old->next;
        HeapFree(GetProcessHeap(),0,old);
    }
    loot_tail=NULL; loot_pending=loot_fault_reported=0;
    loot_capture_fault=inventory_bound=FALSE;
}
static void service_story_loot(BOOL native_ready) {
    if(!session || !saved_profile) return;
    if(!inventory_bound) {
        inventory_bound=SudekiMpLanStoryInventoryBind(runtime_thread,
            local_seat?NULL:observe_story_loot,NULL);
        if(!inventory_bound) return;
    }
    unsigned fault=SudekiMpLanPartyStoryLootGrantFault(session);
    if(loot_capture_fault) fault|=0x100u;
    if(fault) {
        if(fault!=loot_fault_reported) {
            loot_fault_reported=fault;
            SudekiMpLogFormat("story_loot event=quarantined fault=%u pending=%u\r\n",fault,loot_pending);
            SudekiMpLanStoryMenuNotice("Loot delivery stopped; check the session log before continuing.");
        }
        return;
    }
    if(!local_seat) {
        for(unsigned count=0;loot_head && count<8u;++count) {
            StoryPendingLoot *pending=loot_head;
            SudekiMpLanPartyLease recipients[4]={{0}};
            unsigned mask=loot_recipients(recipients);
            BOOL exact=mask==pending->mask;
            for(unsigned p=1;p<4u;++p)
                if((pending->mask&(1u<<p)) && !same_connection(&recipients[p],&pending->recipients[p])) exact=FALSE;
            if(!exact) {
                loot_capture_fault=TRUE;
                SudekiMpLogWrite("story_loot event=recipient_changed admission=quarantined pending=retained\r\n");
                return;
            }
            const SudekiMpStoryInventoryEvent *e=&pending->event;
            uint32_t serial=0;
            if(!SudekiMpLanPartyPublishStoryLootGrantForRecipients(session,pending->recipients,pending->mask,
                e->source,(unsigned)e->item,
                (unsigned)e->quantity,(unsigned)e->show,&serial)) {
                static DWORD last_wait;
                DWORD now=GetTickCount();
                if(!last_wait || now-last_wait>=2000u) {
                    last_wait=now;
                    SudekiMpLogFormat("story_loot event=waiting_transport pending=%u item=%ld retained=1\r\n",
                        loot_pending,(long)e->item);
                }
                return;
            }
            SudekiMpLogFormat("story_loot event=queued serial=%lu source=%lu item=%ld quantity=%ld host_before=%lu host_after=%lu money=%lu recipients=%u\r\n",
                (unsigned long)serial,(unsigned long)e->source,(long)e->item,(long)e->quantity,
                (unsigned long)e->before,(unsigned long)e->after,(unsigned long)e->money,pending->mask);
            loot_head=pending->next; if(!loot_head) loot_tail=NULL;
            --loot_pending; HeapFree(GetProcessHeap(),0,pending);
        }
        return;
    }
    if(!native_ready) return; /* Menu is allowed; title/loading is not. */
    if(table_modal()) return; /* granted once this client's shop screen or examine box closes */
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpStoryLootGrant grant;
    SudekiMpStoryInventoryGrant native_grant;
    if(!SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
        !SudekiMpLanPartyPeekStoryLootGrant(session,&peer.lease,&grant)) return;
    if(!SudekiMpLanStoryInventoryPrepare(grant.item,grant.quantity,&native_grant)) {
        static uint32_t last_wait_serial,last_wait_at;
        uint32_t now=GetTickCount();
        if(grant.serial!=last_wait_serial || now-last_wait_at>=2000u) {
            last_wait_serial=grant.serial;last_wait_at=now;
            SudekiMpLogFormat("story_loot event=waiting_inventory serial=%lu item=%lu error=%lu consumed=0\r\n",
                (unsigned long)grant.serial,(unsigned long)grant.item,(unsigned long)GetLastError());
        }
        return;
    }
    if(!SudekiMpLanPartyTakeStoryLootGrant(session,&peer.lease,&grant)) return;
    SudekiMpStoryInventoryGrantResult result={0};
    BOOL entered=SudekiMpLanStoryInventoryApply(&native_grant,&result);
    BOOL terminal=entered && (result.status==SUDEKIMP_STORY_INVENTORY_GRANTED ||
        result.status==SUDEKIMP_STORY_INVENTORY_CAPPED);
    BOOL completed=SudekiMpLanPartyCompleteStoryLootGrant(session,&peer.lease,&grant,terminal);
    BOOL refreshed=terminal && main_menu_open() && SudekiMpLanStoryMainMenuRefreshInventory();
    SudekiMpLogFormat("story_loot event=%s serial=%lu source=%u item=%lu quantity=%lu before=%lu after=%lu money=%lu capped=%u completed=%u refreshed=%u\r\n",
        terminal?"granted":"apply_uncertain",(unsigned long)grant.serial,grant.source,
        (unsigned long)grant.item,(unsigned long)grant.quantity,(unsigned long)result.before,
        (unsigned long)result.after,(unsigned long)result.money,
        result.status==SUDEKIMP_STORY_INVENTORY_CAPPED,(unsigned)completed,(unsigned)refreshed);
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
    if(enemy_targeting_attempted) {
        SudekiMpStoryEnemyTargetingSuspend();
        if(!InterlockedCompareExchange(&stopping,0,0) &&
            !SudekiMpStoryEnemyTargetingBindThread(runtime_thread)) goto done;
    }
    if(saved_profile && !local_seat) {
        controller_start=timing_begin();
        timing_end(&controller_gap_timing,previous_controller_stamp);
        previous_controller_stamp=controller_start;
    }
    if(InterlockedCompareExchange(&stopping,0,0)) {
        if(enemy_targeting_attempted) {
            if(!SudekiMpStoryEnemyTargetingUninstall()) goto done;
            enemy_targeting_attempted=FALSE;memset(&away_refused_logs,0,sizeof(away_refused_logs));
        }
        /* Native inventory hooks and callbacks retire on their bound game
         * thread; the heap journal remains until the service gate drains. */
        if(!SudekiMpLanStoryInventoryUninstall()) goto done;
        SudekiMpStoryCharacterAudioClose();
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
    {
        /* #42 diagnostic: report the first spirit-instance / cast-light fault
         * from this service seam (never from inside a native callback). */
        static unsigned reported_spirit,reported_light;
        unsigned spirit_site=SudekiMpSpiritInstanceFaultSite(),light_line=SudekiMpCastLightFaultLine();
        if((spirit_site && spirit_site!=reported_spirit) || (light_line && light_line!=reported_light)) {
            reported_spirit=spirit_site; reported_light=light_line;
            SudekiMpLogFormat("story_fault event=first spirit_site=%u enter_failure=%u named_fail=%u light_line=%u light_site=%u light_ready_failure=%u\r\n",
                spirit_site,SudekiMpSpiritInstanceEnterFailure(),SudekiMpSpiritInstanceNamedFail(),
                light_line,SudekiMpCastLightFaultSite(),SudekiMpCastLightReadyFailure());
        }
        static unsigned reported_moves;
        unsigned moves=SudekiMpSpiritInstanceNamedBankMoves();
        if(moves!=reported_moves) { reported_moves=moves;
            SudekiMpLogFormat("story_fault event=named_bank_moved moves=%u\r\n",moves); }
    }
    DWORD now=GetTickCount();
    if(saved_profile) SudekiMpLanStoryResearch54Sample(local_seat!=0);
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
            {
                BOOL front_use=loaded && known && !host_binding_ready &&
                    SudekiMpLanStoryHostControlFrontUseLock(controller,w,&native);
                static unsigned front_use_logs;
                if(front_use!=host_front_use && front_use_logs<64u) { ++front_use_logs;
                    SudekiMpLogFormat("lan_story_control event=host_front_use active=%u ms=%lu\r\n",
                        front_use?1u:0u,(unsigned long)GetTickCount()); }
                host_front_use=front_use;
            }
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
    if(saved_profile && !local_seat) {
        SudekiMpLanStoryNativeRoster audio_roster;
        if(known && SudekiMpLanStoryObserverRoster(controller,w,&native,&audio_roster))
            SudekiMpStoryCharacterAudioBind(&audio_roster);
        else SudekiMpStoryCharacterAudioClose();
    }
    if(saved_profile) service_story_loot(known && native.phase==SUDEKIMP_LAN_STORY_READY && load_finished());
    if(saved_profile && known) poll_story_swap();
    if(!local_seat && saved_profile && !known)
        SudekiMpLanStoryControlObserveIdleContact(w,NULL);
    if(!local_seat && saved_profile && known) {
        SudekiMpLanStoryNativeRoster cast_roster;
        LARGE_INTEGER cast_start=timing_begin();
        if(cast_attempted && SudekiMpLanStoryObserverRoster(controller,w,&native,&cast_roster))
            (void)SudekiMpLanStoryCastService(w,&cast_roster);
        {
            SudekiMpLanStoryNativeRoster start_roster;
            if(native.phase==SUDEKIMP_LAN_STORY_READY && load_finished() &&
                SudekiMpLanStoryObserverRoster(controller,w,&native,&start_roster))
                { SudekiMpLanStoryTestStartService(&start_roster); SudekiMpLanStoryTestStartResearchService(&start_roster);
                  SudekiMpLanStorySpawnTestAllyService();
                  uint8_t remote_characters=0,input_fresh=0; uint32_t at=GetTickCount();
                  for(unsigned p=1;p<4u;++p) {
                      SudekiMpLanPartyPeerStatus peer={0}; unsigned c=SudekiMpLanPartyPlayerCharacter(session,p);
                      if(c<4u && SudekiMpLanPartyPeerStatusGet(session,p,&peer) &&
                          peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && peer.transport_confirmed) {
                          remote_characters|=(uint8_t)(1u<<c);
                          /* While the host itself holds this player's input
                           * (control not ready, menus, shop UI, a busy body)
                           * it takes no packets: that is not client silence. */
                          BOOL host_holds=!host_control[p].ready || host_control[p].input_held;
                          if(host_holds || (host_control[p].heard_at && at-host_control[p].heard_at<=2000u))
                              input_fresh|=(uint8_t)(1u<<c);
                      }
                  }
                  SudekiMpLanStoryRemoteInteriorHostService(&start_roster,&native,remote_characters,input_fresh,at); }
            if(native.phase==SUDEKIMP_LAN_STORY_READY && load_finished()) SudekiMpLanStoryAreaResearchService(GetTickCount());
        }
        timing_end(&cast_service_timing,cast_start);
        LARGE_INTEGER control_start=timing_begin();
        service_story_ownership(controller,w,&native);
        service_story_controls(controller,w,&native);
        (void)publish_story_ownership();
        timing_end(&control_service_timing,control_start);
    }
    if(local_seat && saved_profile && !known) {
        client_input_ready=FALSE;
        (void)SudekiMpLanStoryInputAwayLook(NULL,NULL,FALSE);
    }
    if(local_seat && saved_profile && known) service_client_control(controller,w,&native);
    if(saved_profile && local_seat) service_step_away();
    now=GetTickCount(); /* Host control may have required a fresh observation. */
    if(shots_attempted && (!known || native.phase!=SUDEKIMP_LAN_STORY_READY))
        SudekiMpLanStoryShotsCloseAdmission();
    if(!local_seat && known && (!last_publish || now-last_publish>=50u)) {
        if(SudekiMpLanPartyPublishStoryScene(session,&native)) last_publish=now;
    }
    if(saved_profile && !local_seat && known) {
        static uint32_t last_audio_at;
        if(!last_audio_at || now-last_audio_at>=20u) {
            last_audio_at=now;
            for(unsigned c=0;c<4;++c) {
                SudekiMpStoryAudio audio;
                if(SudekiMpStoryCharacterAudioCapture(c,&audio)) {
                    SudekiMpLanStoryCastCaptureAudioStart(c,&audio.start_sequence,&audio.start_tick);
                    (void)SudekiMpLanPartyPublishStoryAudio(session,&audio);
                }
            }
        }
    }
    /* Native area residency (#42): host publishes, client steers toward it. */
    if(saved_profile && area_follow_ready) {
        static uint32_t last_area_at;
        /* #54 research: the host only watches its natively started spawn groups. */
        if(!local_seat && known) {
            (void)SudekiMpLanStorySpawnTick(now,FALSE);
            publish_story_vfx(now,controller,w,&native);
        }
        /* Client: tick replayed groups every service, including cutscenes. */
        if(local_seat) (void)SudekiMpLanStoryClientResidencyCall(w,tick_spawn_residency,&now);
        if(!local_seat && known && (!last_area_at || now-last_area_at>=100u)) {
            SudekiMpStoryAreaState area;
            last_area_at=now?now:1u;
            BOOL area_captured=SudekiMpLanStoryAreaCapture(&area);
            if(area_captured) {
                /* H1b: the lead's interior marker lets clients follow her in. */
                if(native.temporary[0]) (void)SudekiMpLanStoryObserverLastTemporary(area.temporary_zone,area.temporary_marker);
                (void)SudekiMpLanPartyPublishStoryArea(session,&area);
                SudekiMpStoryXp xp;
                if(SudekiMpLanStoryXpCapture(native.epoch,native.revision,now,area.current,&xp))
                    (void)SudekiMpLanPartyPublishStoryXp(session,&xp);
            }
            SudekiMpStoryEffectState effects;
            if(SudekiMpLanStoryEffectsCapture(&effects)) {
                (void)SudekiMpLanStoryGatesCapture(&effects);
                SudekiMpLanStoryMusicCapture(&effects,w);
                (void)SudekiMpLanPartyPublishStoryEffects(session,&effects);
            }
        } else if(local_seat) {
            SudekiMpLanStoryRemoteInteriorClientTick(now);
            SudekiMpLanPartyPeerStatus area_peer; SudekiMpStoryAreaState area;
            BOOL area_peer_known=SudekiMpLanPartyPeerStatusGet(session,local_seat,&area_peer);
            {   /* H1b: this player's character in a house (alone or with the lead). */
                SudekiMpLanStoryScene host_scene; SudekiMpStoryAreaState host_area;
                BOOL have_area=area_peer_known && area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                    SudekiMpLanPartyGetStoryArea(session,&area_peer.lease,now,&host_area);
                if(area_peer_known && known && area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                    SudekiMpLanPartyGetStoryScene(session,&area_peer.lease,now,&host_scene))
                    SudekiMpLanStoryRemoteInteriorClientService(&host_scene,have_area?&host_area:NULL,&native,
                        SudekiMpLanPartyLocalCharacter(session),now);
                /* The door script's spot cue, music fade and zone banner, for
                 * this client's local entry. */
                const char *door_cue;
                if(SudekiMpLanStoryRemoteInteriorClientTakeDoorCue(&door_cue))
                    (void)SudekiMpLanStoryClientResidencyCall(w,door_cue_residency,(void *)door_cue);
                (void)SudekiMpLanStoryMusicInterior(SudekiMpLanStoryRemoteInteriorClientHolds(),w);
                SudekiMpLanStoryZoneBannerService(now,w);
                SudekiMpLanStoryWorldSetLocalZone(SudekiMpLanStoryRemoteInteriorClientShopData());
            }
            if(area_peer_known &&
                area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                SudekiMpLanPartyGetStoryArea(session,&area_peer.lease,now,&area)) {
                BOOL exterior=SudekiMpStoryAreaStateExterior(&area);
                static BOOL follow_held;static unsigned follow_hold_logs;
                if(follow_held!=!exterior) {
                    follow_held=!exterior;
                    if(follow_hold_logs++<32u)
                        SudekiMpLogFormat("story_area_follow event=policy held=%u current=%s reason=%s\r\n",
                            follow_held,area.current,exterior?"ordinary_zone":"temporary_or_unknown");
                }
                /* A host house entry does not move an exterior client or
                 * release that client's exterior effect owners. */
                if(exterior && !SudekiMpLanStoryAreaResearchTemporaryArmed() &&
                    !SudekiMpLanStoryRemoteInteriorClientHolds() &&
                    (!story_vfx.active || !_stricmp(story_vfx.current,area.current) || reset_story_vfx(w)))
                    SudekiMpLanStoryAreaFollow(&area,now,w);
            }
            if(known && native.phase==SUDEKIMP_LAN_STORY_READY) SudekiMpLanStoryAreaResearchClientService(now);
            SudekiMpStoryXp xp;uint32_t xp_age=0;
            if(area_peer_known && known && area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                SudekiMpLanPartyGetStoryXp(session,&area_peer.lease,now,&xp,&xp_age))
                (void)SudekiMpLanStoryXpPresent(&xp,now,xp_age);
            SudekiMpStoryEffectState effects;
            if(area_peer_known && area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                SudekiMpLanPartyGetStoryEffects(session,&area_peer.lease,now,&effects) &&
                SudekiMpLanStoryEffectsInCurrentArea(&effects)) {
                (void)SudekiMpLanStoryClientResidencyCall(w,gates_residency,&effects);
                SudekiMpLanStoryEffectsFollowGroups(&effects,w);
                (void)SudekiMpLanStoryClientResidencyCall(w,spawns_residency,&effects);
                SudekiMpLanStorySkyFollow(&effects);
                (void)SudekiMpLanStoryMusicFollow(&effects,w);
                SudekiMpLanStorySpawnSigilsFollow(effects.sigils);
            }
            /* Missing samples are unknown. A positively ended observation
             * releases the override; failed owner proof retains it for retry. */
            if(area_peer_known && area_peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING)
                (void)SudekiMpLanStorySkyReleaseLighting();
            if(area_peer_known && area_peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && known)
                follow_story_vfx(controller,w,&area_peer,now,&native);
            else if(area_peer_known && area_peer.phase!=SUDEKIMP_LAN_PARTY_OBSERVING)
                (void)reset_story_vfx(w);
            /* Re-contain after a followed zone change (entities/tasks/descriptor). */
            uint32_t followed=SudekiMpLanStoryAreaFollowLastCall();
            if(known && native.phase==SUDEKIMP_LAN_STORY_READY)
                (void)SudekiMpLanStoryClientAdoptResidency(controller,w,&native,
                    followed && now-followed<30000u);
            else {
                static uint32_t not_ready_logged;
                if(followed && now-followed<30000u && now-not_ready_logged>=2000u) {
                    not_ready_logged=now;
                    SudekiMpLogFormat("lan_story_client event=residency_waiting known=%u phase=%u\r\n",
                        (unsigned)known,known?(unsigned)native.phase:9u);
                }
            }
        }
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
                SudekiMpLanStoryWorldSetExcludedHouse(SudekiMpLanStoryRemoteInteriorHostHouseData());
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
    memset(&away_client,0,sizeof(away_client)); memset(&away_combat,0,sizeof(away_combat));
    memset(&away_connection,0,sizeof(away_connection)); away_pending_request=away_retry_at=away_ui_at=0; away_ui_seen=FALSE;
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
    menu_lobby_known=menu_scene_known=host_binding_ready=host_front_use=FALSE;
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
    memset(&presented_cast_view,0,sizeof(presented_cast_view));
    host_cinematic_view=FALSE; memset(&native_camera_clock,0,sizeof(native_camera_clock));
    memset(&client_control_connection,0,sizeof(client_control_connection));
    client_input_sequence=client_input_sent_at=client_ranged_sequence=client_block_sequence=0;
    memset(&client_melee_sender,0,sizeof(client_melee_sender));
    client_melee_generation=0;
        client_block_sent_at=0;client_block_sent=client_block_last_held=FALSE;
        client_ranged_physical_press=client_ranged_wire_press=0;
    memset(&client_action,0,sizeof(client_action));
    memset(&client_action_connection,0,sizeof(client_action_connection));
    client_action_serial=client_action_sent_at=0; client_action_pending=FALSE;
    client_control_trace_count=client_control_trace_state=0;
    host_control_hold_traces=0;
    dialogue_count=0; visible_dialogue_valid=FALSE; visible_dialogue_receipt=0;
    memset(&audio_trace,0,sizeof(audio_trace)); memset(&overlay_trace,0,sizeof(overlay_trace));
    memset(presented_generations,0,sizeof(presented_generations));
    memset(&story_muzzles,0,sizeof(story_muzzles));
    memset(&history_lease,0,sizeof(history_lease)); memset(&client_seed,0,sizeof(client_seed));
    memset(&initial_objects,0,sizeof(initial_objects)); memset(&initial_object_scene,0,sizeof(initial_object_scene));
    initial_objects_attempted=initial_objects_known=FALSE;
    memset(&area_observation,0,sizeof(area_observation));
    loot_trace_attempted=FALSE;
    clear_story_loot();
    memset(&presentation_sample,0,sizeof(presentation_sample));
    client_revision_gap=FALSE;
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
    /* Before the observer hooks the zone exports it verifies. */
    area_follow_ready=saved_profile && SudekiMpLanStoryAreaFollowInitialize(module);
    if(saved_profile && !(local_seat?SudekiMpLanStoryInventoryInitialize(module):
        SudekiMpLanStoryInventoryInstall(module))) goto fail;
    if(saved_profile && !SudekiMpLanStoryRemoteInteriorInitialize(module))
        SudekiMpLogWrite("story_remote_interior event=unavailable\r\n");
    if(saved_profile && local_seat && !SudekiMpLanStoryRemoteUseIconInitialize(module))
        SudekiMpLogWrite("story_remote_use event=icon_unavailable\r\n");
    if(saved_profile && local_seat) {
        shop_attempted=SudekiMpLanStoryShopInstall(module);
        if(!shop_attempted) SudekiMpLogFormat("story_shop event=unavailable error=%lu\r\n",(unsigned long)GetLastError());
    }
    if(saved_profile && !SudekiMpLanStoryXpInitialize(module,!local_seat)) goto fail;
    if(saved_profile && !SudekiMpStoryCharacterAudioInitialize(module,!local_seat)) goto fail;
    if(saved_profile && !SudekiMpLanStoryMusicInitialize(module))
        SudekiMpLogWrite("story_music event=unavailable\r\n");
    if(saved_profile && local_seat && !SudekiMpLanStoryZoneBannerInitialize(module))
        SudekiMpLogWrite("story_zone_banner event=unavailable\r\n");
    /* Spawn tracking is functional (#54: client group replay and monster
     * identity), not only research. */
    if(saved_profile && !SudekiMpLanStorySpawnTraceInstall(module,local_seat!=0))
        SudekiMpLogFormat("story_spawn_trace event=install_failed error=%lu\r\n",(unsigned long)GetLastError());
    if(saved_profile && !area_follow_ready) SudekiMpLogWrite("story_area_follow event=unavailable\r\n");
    if(saved_profile && !SudekiMpLanStorySkyInstall(module))
        SudekiMpLogFormat("story_sky event=install_failed error=%lu\r\n",(unsigned long)GetLastError());
    if(saved_profile && !SudekiMpLanStoryDeathHoldInstall(module))
        SudekiMpLogFormat("story_death_hold event=install_failed error=%lu\r\n",(unsigned long)GetLastError());
    if(saved_profile && !SudekiMpLanStoryComboHudInitialize(module,!local_seat)) goto fail;
    if(!SudekiMpLanStoryObserverInstall(module)) goto fail;
    /* Identity is unknown (not mismatched) while a world load is pending. */
    SudekiMpCastLightSetUnknownWitness(SudekiMpLanStoryObserverIdentityUnknown);
    SudekiMpSpiritInstanceSetUnknownWitness(SudekiMpLanStoryObserverIdentityUnknown);
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
            if(story_step_away && !SudekiMpLanStoryHostPauseInstall(module)) goto fail;
            activity_attempted=TRUE;
            if(!SudekiMpLanStoryActivityInitialize(module)) goto fail;
            split_attempted=TRUE;
            if(!SudekiMpLanStorySplitInstall(module)) goto fail;
            (void)SudekiMpLanStoryObserverSetWorldChangeHook(before_world_change);
            shots_attempted=TRUE;
            if(!SudekiMpLanStoryShotsInstall(module)) goto fail;
            enemy_targeting_attempted=TRUE;
            if(!SudekiMpStoryEnemyTargetingInstall(module)) goto fail;
        }
    }
    if(saved_profile && local_seat) {
        input_attempted=TRUE;
        if(!SudekiMpLanStoryInputInstall(module,local_seat)) goto fail;
        quick_menu_attempted=TRUE;
        if(!SudekiMpLanStoryQuickMenuInstall(module,queue_client_action)) goto fail;
        client_attempted=TRUE;
        if(!SudekiMpLanStoryClientInstall(module,input_closed)) goto fail;
        replica_attempted=TRUE;
        if(!SudekiMpInitializeLanStoryReplica(module)) goto fail;
        recruit_attempted=TRUE;
        if(!SudekiMpLanStoryRecruitInstall(module)) goto fail;
        local_control_attempted=TRUE;
        if(!SudekiMpLanStoryLocalControlInstall(module)) goto fail;
        if(client_native_camera) {
            client_camera_attempted=SudekiMpLanStoryClientCameraInstall(module);
            if(!client_camera_attempted) SudekiMpLogFormat("story_client_camera event=install_refused error=%lu\r\n",(unsigned long)GetLastError());
        }
        aim_pose_attempted=TRUE;
        if(!SudekiMpLanAimPoseOnlyInstall(module,story_aim_witness)) goto fail;
    }
    if(saved_profile) {
        menu_attempted=TRUE;
        if(!SudekiMpLanPartyMenuNativeInstall(module,menu_admission,menu_toggle,menu_frame)) goto fail;
    }
    if(saved_profile) {
        SudekiMpLanPartySetActionTrace(action_trace);
        main_menu_module=module; main_menu_attempted=TRUE;
        if(!SudekiMpLanStoryMainMenuInstall(module,menu_slot_allowed,local_seat?menu_equip_request:NULL,local_seat?menu_use_request:NULL)) goto fail;
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
    /* Retire the owned camera seam before releasing its view/world dependencies.
     * A retained native hook must keep the module and its owner state alive. */
    if(client_camera_attempted && !SudekiMpLanStoryClientCameraUninstall()) return retain_module();
    client_camera_attempted=FALSE;
    unsigned exited=saved_profile?SudekiMpLobbyGameplayStoryExitStatus():0u;
    if(enemy_targeting_attempted && !SudekiMpStoryEnemyTargetingUninstall()) return retain_module();
    enemy_targeting_attempted=FALSE;memset(&away_refused_logs,0,sizeof(away_refused_logs));
    if(shots_attempted && !SudekiMpLanStoryShotsUninstall()) return retain_module();
    shots_attempted=FALSE;
    if(shop_attempted && !SudekiMpLanStoryShopUninstall()) return retain_module();
    shop_attempted=FALSE;
    if(remote_use_attempted && !SudekiMpLanStoryRemoteUseUninstall()) return retain_module();
    remote_use_attempted=FALSE;
    if(loot_trace_attempted && !SudekiMpLanStoryLootTraceUninstall()) return retain_module();
    loot_trace_attempted=FALSE;
    /* Native Quit may have destroyed the world even if its final observation
     * was unknown. Never retry Quit or inspect the old actor leases then. */
    if(exited==2u) return retain_module();
    /* H1b member visits own native state (positions, snap flag, held light
     * calls, a background house, a client fade): drain before the observer
     * and sky owners go. Guarded native reads only; a vanished world refuses. */
    if(saved_profile) {
        SudekiMpLanStoryRemoteInteriorReset(); (void)SudekiMpLanStoryMusicInteriorReset();
        SudekiMpLanStoryZoneBannerReset();
        SudekiMpLanStoryWorldSetLocalZone(NULL); SudekiMpLanStoryWorldSetExcludedHouse(NULL);
    }
    /* The game-thread cleanup in retire_client owns cosmetic retirement.
     * Loader/network teardown must never discard its pending weak handles. */
    if(story_vfx.active) return retain_module();
    if(menu_initialized && !SudekiMpLanStoryMenuDrainTools()) return retain_module();
    if(quick_menu_attempted && !SudekiMpLanStoryQuickMenuUninstall()) return retain_module();
    quick_menu_attempted=FALSE;
    if(main_menu_attempted && !SudekiMpLanStoryMainMenuUninstall()) return retain_module();
    main_menu_attempted=FALSE;
    /* No task observer, timing owner, actor lease or realtime hook may retire
     * while a native cast can still call into its namespace. */
    SudekiMpLanStoryDoorCameraUninstall(); door_camera_ready=FALSE;
    if(!SudekiMpLanStoryInventoryUninstall()) return retain_module();
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
        if(!SudekiMpLanStoryHostPauseUninstall()) return retain_module();
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
    client_revision_gap=FALSE;
    presentation_sample.valid=effects_ready=FALSE;
    host_cinematic_view=FALSE; memset(&native_camera_clock,0,sizeof(native_camera_clock));
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
    if(!SudekiMpLanStoryComboHudUninstall()) return retain_module();
    if(!SudekiMpLanStorySkyUninstall()) return retain_module();
    if(observer_attempted && !InterlockedCompareExchange(&observer_removed,0,0)) {
if(!SudekiMpLanStorySpawnTraceUninstall()) return retain_module();
(void)SudekiMpLanStoryDeathHoldUninstall();
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
    clear_story_loot();
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
    SudekiMpLanStoryXpReset();
    SudekiMpStoryCharacterAudioClose();
    if(SudekiMpStoryCharacterAudioRetains() || SudekiMpStoryShotAudioRetains() ||
        !SudekiMpLanStoryCinematicAudioReset()) return retain_module();
    if(exited && recruit_attempted && !SudekiMpLanStoryRecruitNativeExitReturned()) return retain_module();
    if(recruit_attempted && !SudekiMpLanStoryRecruitUninstall()) return retain_module();
    recruit_attempted=FALSE;
    if(exited && client_attempted && !SudekiMpLanStoryClientNativeExitReturned()) return retain_module();
    /* Verified native reset destroys these borrowed task identities. Forget
     * them before frontend tasks can reuse their addresses; no task is being
     * cancelled or declared normally retired by this plain-data reset. */
    if(exited && !SudekiMpLanStoryTaskTraceForgetExitedWorld()) return retain_module();
    SudekiMpLanStoryAmbientSetActive(FALSE);
    SudekiMpLanStoryAreaFadeReset();
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
