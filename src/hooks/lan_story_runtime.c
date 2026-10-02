#include "hooks/lan_story_runtime.h"
#include "hooks/lan_story_observer.h"
#include "engine/log.h"
#include <string.h>

static SudekiMpLanPartySession *session;
static SudekiMpControlUpdateObserverGate gate;
static HANDLE worker,stop_worker;
static volatile LONG stopping,observer_removed;
static BOOL registered,observer_attempted;
static unsigned char owner;
static unsigned local_seat;
static DWORD last_trace,last_publish;
static uint32_t last_remote_revision;
static int phases[4];
static BOOL retain_module(void) {
    HMODULE retained;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&owner,&retained);
    SetLastError(ERROR_BUSY);
    return FALSE;
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
    if(!SudekiMpControlUpdateObserverGateTryEnter(&gate)) return;
    if(InterlockedCompareExchange(&stopping,0,0)) {
        if(SudekiMpLanStoryObserverUninstall()) InterlockedExchange(&observer_removed,1);
        goto done;
    }
    if(!w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) goto done;
    DWORD now=GetTickCount();
    SudekiMpLanStoryScene native;
    BOOL known=SudekiMpLanStoryObserverSample(controller,w,&native);
    if(!local_seat && known && (!last_publish || now-last_publish>=50u)) {
        if(SudekiMpLanPartyPublishStoryScene(session,&native)) last_publish=now;
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
        /* No native lease was acquired by this probe; transport retirement
         * has no actor task to cancel or AI ownership to restore. */
        if(!local_seat && peer.phase==SUDEKIMP_LAN_PARTY_DRAINING)
            (void)SudekiMpLanPartyReleaseDrained(session,&peer.lease);
        if(local_seat) {
            SudekiMpLanStoryScene remote;
            if(SudekiMpLanPartyGetStoryScene(session,&peer.lease,now,&remote) &&
                (last_remote_revision!=remote.revision || !last_trace || now-last_trace>=1000u)) {
                BOOL same_area=known && native.phase==SUDEKIMP_LAN_STORY_READY &&
                    remote.phase==SUDEKIMP_LAN_STORY_READY &&
                    !strcmp(native.world,remote.world) && !strcmp(native.temporary,remote.temporary);
                unsigned view=SudekiMpLanStoryViewSeat(&remote,local_seat,SUDEKIMP_LAN_STORY_NO_SEAT);
                SudekiMpLogFormat("lan_story event=host_scene epoch=%lu revision=%lu phase=%u available=%u lead=%u desired_view=%u same_area=%u world=%s temporary=%s input_admitted=0 replica_applied=0\r\n",
                    (unsigned long)remote.epoch,(unsigned long)remote.revision,
                    remote.phase,remote.available_mask,remote.leader_seat,view,same_area,
                    remote.world,remote.temporary);
                last_trace=now; last_remote_revision=remote.revision;
            }
        }
    }
done:
    SudekiMpControlUpdateObserverGateLeave(&gate);
}
BOOL SudekiMpInstallLanStoryRuntime(HMODULE module,const SudekiMpLanPartyConfig *config) {
    if(!module || !config || config->story_observation!=1u || session || observer_attempted) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    session=SudekiMpLanPartyCreate(config);
    if(!session) return FALSE;
    local_seat=config->local_seat; last_trace=last_publish=last_remote_revision=0;
    for(unsigned i=0;i<4u;++i) phases[i]=-1;
    InterlockedExchange(&stopping,0); InterlockedExchange(&observer_removed,0);
    observer_attempted=TRUE;
    if(!SudekiMpLanStoryObserverInstall(module)) goto fail;
    stop_worker=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!stop_worker) goto fail;
    worker=CreateThread(NULL,0,poll_network,NULL,0,NULL);
    if(!worker || !SudekiMpControlUpdateObserverGateEnable(&gate)) goto fail;
    if(!SudekiMpControlSeparationRegisterUpdateObserver(&owner,service)) goto fail;
    registered=TRUE;
    SudekiMpLogFormat("lan_story runtime=installed seat=%u profile=story_observe gameplay_enabled=0 save_writes_by_mod=0\r\n",local_seat);
    return TRUE;
fail:
    {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallLanStoryRuntime()) return FALSE;
        SetLastError(error); return FALSE;
    }
}
BOOL SudekiMpUninstallLanStoryRuntime(void) {
    if(!session && !observer_attempted) return TRUE;
    InterlockedExchange(&stopping,1);
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
    SudekiMpLanPartyDestroy(session,TRUE); session=NULL;
    observer_attempted=FALSE;
    return TRUE;
}
