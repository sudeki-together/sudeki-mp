#include "hooks/lan_story_menu.h"
#include "ui/party_menu.h"
#include "cleanroom/menu.h"
#include "cleanroom/engine.h"
#include "hooks/lan_story_host_control.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include <stdio.h>
#include <string.h>

static const char *const characters[6]={"Buki","Elco","Tal","Ailish","No character","Talos"};
static DWORD native_thread;
static unsigned local_player;
static BOOL initialized,menu_open,leave_requested;
static SudekiMpPartyMenuState state;
static char notice[128];
static DWORD notice_until;
static BOOL tools_installed,tools_stopping,tools_commands_ready;
static void *tools_controller;
static const SudekiMpControlUpdateDispatchWitness *tools_witness;
static const SudekiMpLanStoryScene *tools_scene;
static unsigned resource_owned;
static BOOL resource_before[3],resource_expected[3];
static DWORD resources_at;
static BOOL combat_owned,combat_before,combat_expected;
static void *combat_group,*combat_world;

static BOOL thread_exact(void) {
    return initialized && native_thread==GetCurrentThreadId();
}
static int resource_index(unsigned action) {
    switch(action) {
    case SUDEKIMP_PARTY_TOOL_INFINITE_SP: return 0;
    case SUDEKIMP_PARTY_TOOL_INFINITE_SPIRIT: return 1;
    case SUDEKIMP_PARTY_TOOL_INFINITE_JETPACK: return 2;
    default: return -1;
    }
}
static BOOL resource_get(unsigned i,BOOL *enabled) {
    switch(i) {
    case 0: return SudekiMpCleanroomEngineInfiniteSp(enabled);
    case 1: return SudekiMpCleanroomEngineInfiniteSpirit(enabled);
    case 2: return SudekiMpCleanroomEngineInfiniteJetpackFuel(enabled);
    default: return FALSE;
    }
}
static BOOL resource_set(unsigned i,BOOL enabled) {
    switch(i) {
    case 0: return SudekiMpCleanroomEngineSetInfiniteSp(enabled);
    case 1: return SudekiMpCleanroomEngineSetStoryInfiniteSpirit(enabled);
    case 2: return SudekiMpCleanroomEngineSetStoryInfiniteJetpackFuel(enabled);
    default: return FALSE;
    }
}
static BOOL tools_query(unsigned action,BOOL *enabled) {
    int i=resource_index(action);
    if(action==SUDEKIMP_PARTY_TOOL_COMBAT)
        return thread_exact() && !local_player && !tools_stopping && enabled &&
            SudekiMpCleanroomEngineCombatMode(enabled);
    return thread_exact() && !local_player && !tools_stopping && enabled &&
        i>=0 && resource_get((unsigned)i,enabled);
}
static BOOL tools_host_exact(void) {
    SudekiMpLanStoryNativeRoster roster;
    return thread_exact() && !local_player && !tools_stopping && !leave_requested &&
        tools_commands_ready && tools_controller && tools_witness && tools_scene &&
        tools_scene->phase==SUDEKIMP_LAN_STORY_READY &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(tools_witness) &&
        SudekiMpLanStoryHostControlReady(tools_controller,tools_witness,tools_scene) &&
        SudekiMpLanStoryObserverRoster(tools_controller,tools_witness,tools_scene,&roster) &&
        SudekiMpLanStoryObserverRosterStillExact(tools_witness,&roster);
}
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a+n>=a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL combat_boundary(SudekiMpLanStoryNativeRoster *roster) {
    int spirit=-1;
    if(!thread_exact() || local_player || !tools_controller || !tools_witness || !tools_scene ||
        tools_scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        !SudekiMpLanStoryObserverRoster(tools_controller,tools_witness,tools_scene,roster) ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit) return FALSE;
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        SudekiMpCharacterSkillState skill; BOOL pending=TRUE;
        if(!SudekiMpObserveCharacterSkill(roster->actors[c],&skill) || skill.active ||
            !readable(skill.skill,0x78u) ||
            !SudekiMpWeaponActivationPending(roster->actors[c],&pending) || pending) return FALSE;
        void *task=*(void **)((uint8_t *)skill.skill+0x74u);
        if(task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    }
    return SudekiMpLanStoryObserverRosterStillExact(tools_witness,roster);
}
static BOOL tools_command(unsigned action) {
    int i=resource_index(action); BOOL enabled=FALSE,current=FALSE,ok=FALSE;
    if(action==SUDEKIMP_PARTY_TOOL_COMBAT) {
        SudekiMpLanStoryNativeRoster roster;
        const char *reason="host_input_or_dispatch_not_ready";
        BOOL observed=FALSE;
        if(tools_host_exact()) {
            reason="native_combat_boundary_busy";
            if(combat_boundary(&roster)) {
                reason="combat_state_unknown";
                observed=SudekiMpCleanroomEngineCombatMode(&enabled);
            }
        }
        if(observed && combat_owned && roster.group==combat_group && roster.world==combat_world &&
            enabled==combat_before) {
            /* The native game may end a forced combat state during dialogue.
             * A positively observed original value closes our flag lease;
             * it is not an unknown/conflicting value to keep rejecting. */
            combat_owned=FALSE; combat_group=combat_world=NULL;
        }
        if(observed) reason="combat_owner_changed";
        if(observed && (!combat_owned || (roster.group==combat_group && roster.world==combat_world &&
                enabled==combat_expected))) {
            if(!combat_owned) combat_before=enabled;
            combat_group=roster.group; combat_world=roster.world;
            combat_expected=!enabled; combat_owned=TRUE;
            reason="native_transition_not_confirmed";
            ok=SudekiMpCleanroomEngineSetCombatMode(combat_expected) &&
                SudekiMpCleanroomEngineCombatMode(&current) && current==combat_expected &&
                SudekiMpLanStoryObserverRosterStillExact(tools_witness,&roster);
        }
        SudekiMpLogFormat("lan_story_tools event=combat_request result=%s before_known=%u before=%u after_confirmed=%u reason=%s prime_pending=%u policy=host_native_transition\r\n",
            ok?"confirmed":"rejected",observed?1u:0u,enabled?1u:0u,ok?1u:0u,
            ok?"ready":reason,SudekiMpCleanroomEngineRangedCombatPrimePending());
        if(!ok) SudekiMpLanStoryMenuNotice("Combat change waiting: close native menus and finish the current action.");
        return ok;
    }
    if(i>=0 && tools_host_exact() && resource_get((unsigned)i,&enabled) &&
        (!(resource_owned&(1u<<i)) || enabled==resource_expected[i])) {
        if(!(resource_owned&(1u<<i))) resource_before[i]=enabled;
        /* Retain restore responsibility before entering native code, including
         * a setter that changes its flag but fails its post-call observation. */
        resource_expected[i]=!enabled; resource_owned|=1u<<i;
        ok=resource_set((unsigned)i,!enabled) && resource_get((unsigned)i,&current) &&
            current==resource_expected[i];
        resources_at=0;
    }
    SudekiMpLogFormat("lan_story_tools event=command player=%u action=%u result=%s policy=host_exact_dispatch_resource_only\r\n",
        local_player,action,ok?"confirmed":"rejected");
    return ok;
}
static void tools_status(char *text,unsigned capacity) {
    if(!text || !capacity) return;
    const char *status=!thread_exact()?"SESSION UNAVAILABLE":leave_requested?
        "LEAVING SESSION":(state.input_ready&(1u<<local_player))?"PLAYING":
        state.character[local_player]==SUDEKIMP_LOBBY_TALOS?"WAITING FOR AVATAR":
        (state.connected&(1u<<local_player))?"SPECTATING / SYNCHRONIZING":"WAITING FOR SESSION";
    snprintf(text,capacity,"%s",status);
}
void SudekiMpLanStoryMenuServiceTools(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    BOOL host_commands_ready) {
    if(!thread_exact() || !tools_installed || tools_stopping) return;
    if(!local_player && SudekiMpCleanroomEngineRangedCombatPrimePending())
        (void)SudekiMpCleanroomEngineServiceRangedPrime();
    tools_controller=controller; tools_witness=w; tools_scene=scene;
    tools_commands_ready=host_commands_ready;
    if(!menu_open && !leave_requested) SudekiMpCleanroomMenuUpdate();
    DWORD now=GetTickCount();
    BOOL resources_enabled=FALSE;
    for(unsigned i=0;i<3u;++i)
        if((resource_owned&(1u<<i)) && resource_expected[i]) resources_enabled=TRUE;
    if(resources_enabled && (!resources_at || now-resources_at>=200u) && tools_host_exact()) {
        SudekiMpCleanroomEngineMaintainStoryResources(); resources_at=now;
    }
    tools_commands_ready=FALSE; tools_controller=NULL; tools_witness=NULL; tools_scene=NULL;
}
BOOL SudekiMpLanStoryMenuDrainTools(void) {
    if(!initialized) return TRUE;
    if(!thread_exact()) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!tools_stopping && tools_installed) SudekiMpLanPartyToolsClose();
    tools_stopping=TRUE;
    if(!local_player && SudekiMpCleanroomEngineRangedCombatPrimePending()) return FALSE;
    if(combat_owned) {
        SudekiMpLanStoryNativeRoster roster; BOOL current;
        if(!combat_boundary(&roster) || !SudekiMpCleanroomEngineCombatMode(&current)) return FALSE;
        /* Never restore a flag into a replacement world/group. The current
         * observer proves that this is a different native owner. */
        if(roster.group==combat_group && roster.world==combat_world && current!=combat_before) {
            if(current!=combat_expected || !SudekiMpCleanroomEngineSetCombatMode(combat_before) ||
                !SudekiMpCleanroomEngineCombatMode(&current) || current!=combat_before ||
                !SudekiMpLanStoryObserverRosterStillExact(tools_witness,&roster)) return FALSE;
        }
        combat_owned=FALSE; combat_group=combat_world=NULL;
        if(SudekiMpCleanroomEngineRangedCombatPrimePending()) return FALSE;
    }
    for(unsigned i=0;i<3u;++i) if(resource_owned&(1u<<i)) {
        BOOL current;
        if(!resource_get(i,&current)) return FALSE;
        if(current!=resource_before[i]) {
            if(current!=resource_expected[i] || !resource_set(i,resource_before[i]) ||
                !resource_get(i,&current) || current!=resource_before[i]) return FALSE;
        }
        resource_owned&=~(1u<<i);
    }
    return !tools_installed || SudekiMpLanPartyToolsRenderDrained();
}
BOOL SudekiMpLanStoryMenuDrainToolsOnDispatch(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    if(!thread_exact()) return FALSE;
    if(!local_player && SudekiMpCleanroomEngineRangedCombatPrimePending())
        (void)SudekiMpCleanroomEngineServiceRangedPrime();
    tools_controller=controller; tools_witness=w; tools_scene=scene;
    BOOL drained=SudekiMpLanStoryMenuDrainTools();
    tools_controller=NULL; tools_witness=NULL; tools_scene=NULL;
    return drained;
}
static BOOL command(SudekiMpPartyMenuAction action,unsigned argument) {
    if(!thread_exact() || leave_requested) return FALSE;
    switch(action) {
    case SUDEKIMP_PARTY_MENU_OPEN:
    case SUDEKIMP_PARTY_MENU_CLOSE:
        if(argument!=local_player) return FALSE;
        menu_open=action==SUDEKIMP_PARTY_MENU_OPEN;
        SudekiMpLogFormat("lan_story_menu event=%s player=%u world_pause_requested=0\r\n",
            menu_open?"opened":"closed",local_player);
        return TRUE;
    case SUDEKIMP_PARTY_MENU_LEAVE:
        if(argument) return FALSE;
        leave_requested=TRUE;
        snprintf(state.status,sizeof(state.status),"%s",
            local_player?"Leaving the session...":"Ending the session...");
        SudekiMpPartyMenuStateSet(&state);
        SudekiMpLogFormat("lan_story_menu event=leave_requested player=%u native_cleanup=pending\r\n",
            local_player);
        return TRUE;
    default:
        /* UI visibility is not authorization. Reject unsupported commands
         * even if an old page or a future caller tries to submit one. */
        return FALSE;
    }
}
BOOL SudekiMpLanStoryMenuInitialize(HMODULE image,unsigned player) {
    if(initialized || player>=4u) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    native_thread=GetCurrentThreadId(); local_player=player;
    initialized=TRUE; menu_open=leave_requested=FALSE;
    tools_stopping=tools_commands_ready=FALSE; resource_owned=0; resources_at=0;
    combat_owned=FALSE; combat_group=combat_world=NULL;
    tools_controller=NULL; tools_witness=NULL; tools_scene=NULL;
    if(!SudekiMpInstallLanStoryToolsMenu(image,VK_F8,player,tools_command,tools_query,tools_status)) {
        initialized=FALSE; native_thread=0; return FALSE;
    }
    tools_installed=TRUE;
    notice[0]=0; notice_until=0;
    memset(&state,0,sizeof(state)); memset(state.character,4,sizeof(state.character));
    state.local_player=player; state.active=TRUE; state.host=player==0u;
    state.saved_story=TRUE;
    snprintf(state.status,sizeof(state.status),"Waiting for the saved game...");
    SudekiMpPartyMenuInitialize(command); SudekiMpPartyMenuStateSet(&state);
    return TRUE;
}
void SudekiMpLanStoryMenuRefresh(const SudekiMpLobbyStatus *lobby,
    unsigned leader,unsigned available,BOOL scene_ready,BOOL host_control_ready,
    BOOL spectator_playback_ready,BOOL client_control_ready) {
    if(!thread_exact()) return;
    SudekiMpPartyMenuState next={0};
    next.local_player=local_player; next.active=TRUE; next.host=local_player==0u;
    next.saved_story=TRUE; next.transition=TRUE;
    memset(next.character,4,sizeof(next.character));
    BOOL dev_play=lobby && lobby->mode==SUDEKIMP_LOBBY_MODE_DEV_PLAY;
    /* The caller has already validated the native scene. An all-avatar Dev
     * party has no canonical hero leader or availability bit; local control
     * below still requires its independently confirmed input lease. */
    BOOL known=scene_ready && ((leader<4u && available<=15u && (available&(1u<<leader))) ||
        (dev_play && leader==SUDEKIMP_LAN_STORY_NO_SEAT && !available));
    if(lobby && lobby->local_slot==local_player) {
        for(unsigned p=0;p<4u;++p) {
            const SudekiMpLobbyMember *member=&lobby->members[p];
            unsigned bit=1u<<p;
            if(member->present) next.connected|=(uint8_t)bit;
            if(member->reserved && member->locked && (member->character<4u ||
                (dev_play && member->character==SUDEKIMP_LOBBY_TALOS))) {
                next.reserved|=(uint8_t)bit;
                next.character[p]=member->character;
            }
            snprintf(next.name[p],sizeof(next.name[p]),"%.31s",member->name);
            unsigned chosen=next.character[p];
            const char *choice=characters[chosen];
            if(dev_play && chosen==SUDEKIMP_LOBBY_TALOS && !next.name[p][0])
                snprintf(next.name[p],sizeof(next.name[p]),"Talos");
            if(!member->present) {
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),"%s",
                    member->reserved?"Disconnected; releasing selection":"Open slot");
            } else if(!known) {
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Waiting for the story",choice);
            } else if(dev_play && chosen==SUDEKIMP_LOBBY_TALOS) {
                /* Avatar IDs have no native party-slot bit. Only the local
                 * input lease can prove control; remote rows state selection. */
                if(p==local_player && (local_player?client_control_ready:host_control_ready)) {
                    next.controlling|=(uint8_t)bit; next.input_ready|=(uint8_t)bit;
                    snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                        "Talos - %s",menu_open?"Menu open":"Playing");
                } else if(p==local_player)
                    snprintf(next.story_detail[p],sizeof(next.story_detail[p]),"Talos - Waiting for avatar control");
                else snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "Talos - %s",p?"Avatar selected":"Host avatar");
            } else if(!p && !local_player && host_control_ready && chosen==leader) {
                next.controlling|=1u; next.input_ready|=1u;
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - %s",choice,menu_open?"Menu open":"Playing");
            } else if(!p && local_player) {
                /* The scene proves the host's native leader, not its exact
                 * local input lease. Avoid reporting remote input readiness. */
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Host view: %s",choice,characters[leader]);
            } else if(!p && !local_player) {
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Synchronizing host",choice);
            } else if(p==local_player && client_control_ready && chosen<4u && (available&(1u<<chosen))) {
                next.controlling|=(uint8_t)bit; next.input_ready|=(uint8_t)bit;
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - %s",choice,menu_open?"Menu open":"Playing");
            } else if(p==local_player && chosen<4u && (available&(1u<<chosen))) {
                /* Loss of input admission does not retarget the retained
                 * local camera to the host. Report authority, not a guessed
                 * spectator target derived from the remote party leader. */
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Waiting for %s",choice,
                    spectator_playback_ready?"control":"world synchronization");
            } else if(p==local_player && !spectator_playback_ready) {
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Waiting to spectate %s",choice,characters[leader]);
            } else {
                snprintf(next.story_detail[p],sizeof(next.story_detail[p]),
                    "%s - Spectating %s%s",choice,characters[leader],
                    chosen<4u && !(available&(1u<<chosen))?"; not yet in party":"");
            }
        }
    }
    if(leave_requested)
        snprintf(next.status,sizeof(next.status),"%s",
            local_player?"Leaving the session...":"Ending the session...");
    else if(!lobby || lobby->local_slot!=local_player)
        snprintf(next.status,sizeof(next.status),"Waiting for session information...");
    else if(lobby->phase==SUDEKIMP_LOBBY_ERROR || lobby->phase==SUDEKIMP_LOBBY_IDLE)
        snprintf(next.status,sizeof(next.status),"Session ended; returning to the intro...");
    else if(!known)
        snprintf(next.status,sizeof(next.status),"Waiting for the story...");
    else if(dev_play && next.character[local_player]==SUDEKIMP_LOBBY_TALOS)
        snprintf(next.status,sizeof(next.status),"%s",(next.input_ready&(1u<<local_player))?
            "Playing Talos":"Waiting for Talos avatar control...");
    else if(local_player && client_control_ready)
        snprintf(next.status,sizeof(next.status),"Playing %s",characters[next.character[local_player]<=4u?next.character[local_player]:4u]);
    else if(local_player && !spectator_playback_ready)
        snprintf(next.status,sizeof(next.status),"Waiting for host world synchronization...");
    else if(local_player && next.character[local_player]<4u &&
        (available&(1u<<next.character[local_player])))
        snprintf(next.status,sizeof(next.status),"Waiting for %s control...",
            characters[next.character[local_player]]);
    else if(local_player)
        snprintf(next.status,sizeof(next.status),"Spectating %s",characters[leader]);
    else if(!(next.input_ready&1u))
        snprintf(next.status,sizeof(next.status),"Synchronizing host control...");
    if(!leave_requested && notice[0] && (int32_t)(notice_until-GetTickCount())>0)
        snprintf(next.status,sizeof(next.status),"%s",notice);
    state=next; SudekiMpPartyMenuStateSet(&state);
}
void SudekiMpLanStoryMenuNotice(const char *text) {
    if(!thread_exact() || !text || leave_requested) return;
    snprintf(notice,sizeof(notice),"%s",text); notice_until=GetTickCount()+4000u;
    snprintf(state.status,sizeof(state.status),"%s",notice); SudekiMpPartyMenuStateSet(&state);
}
void SudekiMpLanStoryMenuToggle(void) {
    if(!thread_exact()) return;
    /* Escape closes the F8 overlay before it can open a second menu. */
    if(SudekiMpLanPartyToolsCaptureInput()) SudekiMpLanPartyToolsClose();
    else SudekiMpPartyMenuToggle();
}
BOOL SudekiMpLanStoryMenuRender(void *device) {
    if(!thread_exact() || !SudekiMpLanPartyToolsRestoreRenderState()) return FALSE;
    /* A contained spectator need not have a playable controller yet. Its
     * console stays available on the existing render callback; no host
     * mutation is admitted here because no dispatch context is supplied. */
    if(local_player) SudekiMpLanStoryMenuServiceTools(NULL,NULL,NULL,FALSE);
    BOOL ok=SudekiMpPartyMenuRender(device);
    if(tools_installed) SudekiMpCleanroomMenuRender();
    return ok && SudekiMpLanPartyToolsRenderDrained();
}
BOOL SudekiMpLanStoryMenuCapturesInput(void) {
    return thread_exact() && (leave_requested || SudekiMpPartyMenuCapturesInput() ||
        (tools_installed && SudekiMpLanPartyToolsCaptureInput()));
}
BOOL SudekiMpLanStoryMenuLeaveRequested(void) {
    return thread_exact() && leave_requested;
}
BOOL SudekiMpLanStoryMenuReset(void) {
    if(!initialized) return TRUE;
    if(!thread_exact()) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!SudekiMpLanStoryMenuDrainTools()) return FALSE;
    if(tools_installed) {
        SudekiMpUninstallCleanroomMenu(); tools_installed=FALSE;
    }
    SudekiMpPartyMenuReset(); memset(&state,0,sizeof(state));
    initialized=menu_open=leave_requested=FALSE; native_thread=0; local_player=0;
    return TRUE;
}
