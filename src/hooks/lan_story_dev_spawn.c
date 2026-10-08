#include "hooks/lan_story_dev_spawn.h"
#include "hooks/lan_story_world.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include "input/key_binding.h"
#include <math.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

/* Dev Play ([DevPlay] SpawnKey=<key> SpawnResource=<NAME>, saved-story host):
 * one key press spawns one generic entity beside the party leader through the
 * cleanroom engine's native spawn export. The spawned entity is native-owned;
 * nothing claims its lifetime. Bounded: 2 s cooldown, at most 16 per session. */
enum { COOLDOWN_MS=2000u, MAX_SPAWNS=16u };
static UINT key; static char resource[48]; static float offset[3]={4.0f,0.0f,4.0f};
static BOOL was_down; static DWORD last_at; static unsigned spawned;
static char mirror[48]; static void *mirror_entity; static unsigned mirror_attempts;
static DWORD mirror_spawn_at,mirror_wait_started; static BOOL mirror_timed_out;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL resource_name(const wchar_t *w,char out[48]) {
    unsigned n=0;
    for(;n<47u && w[n];++n) { wchar_t c=w[n];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) { n=0; break; }
        out[n]=(char)c; }
    out[n]=0; return n!=0u;
}
void SudekiMpLanStoryDevSpawnConfigure(const wchar_t *config_path) {
    key=0; resource[0]=0; mirror[0]=0;
    SudekiMpLanStoryWorldSetMirrorEnemy(NULL);
    if(!config_path) return;
    { wchar_t m[48]={0}; GetPrivateProfileStringW(L"DevPlay",L"ClientSpawnMirror",L"",m,47,config_path);
      if(resource_name(m,mirror)) {
          SudekiMpLanStoryWorldSetMirrorEnemy(mirror);
          SudekiMpLogFormat("dev_spawn event=configured_client resource=%s policy=spawn_before_pause_hidden_host_owned_ai\r\n",mirror); } }
    wchar_t w[48]={0}; GetPrivateProfileStringW(L"DevPlay",L"SpawnResource",L"",w,47,config_path);
    if(!resource_name(w,resource)) return;
    /* The host captures its first spawn of this resource for the client mirror. */
    if(!mirror[0]) SudekiMpLanStoryWorldSetMirrorEnemy(resource);
    wchar_t k[32]={0}; GetPrivateProfileStringW(L"DevPlay",L"SpawnKey",L"F7",k,31,config_path);
    if(!SudekiMpParseInputKey(k,&key)) { key=0; resource[0]=0;
        SudekiMpLogWrite("dev_spawn event=configure status=rejected reason=key\r\n"); return; }
    { wchar_t o[64]={0}; GetPrivateProfileStringW(L"DevPlay",L"SpawnOffset",L"",o,63,config_path);
      if(o[0]) { float f[3]; if(swscanf(o,L"%f,%f,%f",&f[0],&f[1],&f[2])==3 && isfinite(f[0]) && isfinite(f[1]) && isfinite(f[2])) memcpy(offset,f,sizeof(offset)); } }
    SudekiMpLogFormat("dev_spawn event=configured key=0x%02x resource=%s offset=%.1f,%.1f,%.1f policy=native_generic_entity_spawn_no_lifetime_claim\r\n",
        key,resource,(double)offset[0],(double)offset[1],(double)offset[2]);
}
void SudekiMpLanStoryDevSpawnService(const SudekiMpLanStoryNativeRoster *roster,HMODULE game_module) {
    if(!key || !resource[0]) return;
    BOOL down=(GetAsyncKeyState((int)key)&0x8000)!=0;
    BOOL rising=down && !was_down; was_down=down;
    if(!rising) return;
    DWORD now=GetTickCount();
    if(now-last_at<COOLDOWN_MS || spawned>=MAX_SPAWNS) return;
    if(!roster || !game_module || !SudekiMpCleanroomEngineWorldReady()) return;
    const uint8_t *leader=roster->actors[roster->leader_character];
    if(!readable(leader,0x48u)) return;
    const uint8_t *pos=*(const uint8_t *const *)(leader+0x44u);
    if(!readable(pos,0x24u) || *(const void *const *)pos!=(const uint8_t *)game_module+0x2cdefcu) return;
    float where[3]; memcpy(where,pos+0x18u,sizeof(where));
    for(unsigned i=0;i<3u;++i) { where[i]+=offset[i]; if(!isfinite(where[i])) return; }
    last_at=now;
    BOOL ok=SudekiMpCleanroomEngineSpawnEntityNamed(resource,where);
    if(ok) ++spawned;
    SudekiMpLogFormat("dev_spawn event=spawn resource=%s at=%.2f,%.2f,%.2f accepted=%u total=%u\r\n",
        resource,(double)where[0],(double)where[1],(double)where[2],ok,spawned);
}

/* Monster class (vtable 2D69EC) with its embedded CPosition and render
 * object; bit4 of the render object's +34 flags hides it (as for scenery). */
static uint32_t *mirror_flags(uint8_t *game,const uint8_t *e) {
    if(!readable(e,0x160u) || *(const void *const *)e!=game+0x2d69ecu) return NULL;
    const uint8_t *pos=*(const uint8_t *const *)(e+0x44u);
    if(pos!=e+0x150u || !readable(pos,0xb8u) || *(const void *const *)pos!=game+0x2cdefcu ||
        *(const void *const *)(pos+0x10u)!=e) return NULL;
    const uint8_t *wrapper=*(const uint8_t *const *)(pos+0xb4u);
    if(!readable(wrapper,0x14u)) return NULL;
    uint8_t *object=*(uint8_t *const *)(wrapper+8u);
    return readable(object,0x38u)?(uint32_t *)(object+0x34u):NULL;
}
void SudekiMpLanStoryDevSpawnClientService(const SudekiMpLanStoryNativeRoster *roster,BOOL may_spawn,HMODULE game_module) {
    if(!mirror[0] || !roster || !game_module) return;
    uint8_t *game=(uint8_t *)game_module;
    if(may_spawn && !mirror_wait_started) mirror_wait_started=GetTickCount();
    uint8_t *found=SudekiMpCleanroomEngineGenericEntity(mirror);
    if(mirror_entity) {
        if(found!=mirror_entity || !mirror_flags(game,found)) {
            SudekiMpLogFormat("dev_spawn event=client_mirror_lost resource=%s entity=%p\r\n",mirror,mirror_entity);
            mirror_entity=NULL;
        }
        return;
    }
    if(found) {
        uint32_t *flags=mirror_flags(game,found);
        if(!flags) return;
        /* Hidden until the host's world record shows it. */
        *flags|=4u; mirror_entity=found;
        SudekiMpLogFormat("dev_spawn event=client_mirror_resolved resource=%s entity=%p identifier=%08lx hidden=1\r\n",
            mirror,(void *)found,(unsigned long)*(const uint32_t *)(found+0x34u));
        return;
    }
    if(!may_spawn || !SudekiMpCleanroomEngineWorldReady()) return;
    DWORD now=GetTickCount();
    if(mirror_attempts && now-mirror_spawn_at<6000u) return;
    if(mirror_attempts>=3u) return;
    const uint8_t *leader=roster->actors[roster->leader_character];
    if(!readable(leader,0x48u)) return;
    const uint8_t *pos=*(const uint8_t *const *)(leader+0x44u);
    if(!readable(pos,0x24u) || *(const void *const *)pos!=game+0x2cdefcu) return;
    float where[3]; memcpy(where,pos+0x18u,sizeof(where));
    for(unsigned i=0;i<3u;++i) { where[i]+=offset[i]; if(!isfinite(where[i])) return; }
    ++mirror_attempts; mirror_spawn_at=now;
    BOOL ok=SudekiMpCleanroomEngineSpawnEntityNamed(mirror,where);
    SudekiMpLogFormat("dev_spawn event=client_mirror_spawn resource=%s at=%.2f,%.2f,%.2f accepted=%u attempt=%u\r\n",
        mirror,(double)where[0],(double)where[1],(double)where[2],ok,mirror_attempts);
}
BOOL SudekiMpLanStoryDevSpawnClientReady(void) {
    if(!mirror[0] || mirror_entity || mirror_timed_out) return TRUE;
    if(mirror_wait_started && GetTickCount()-mirror_wait_started>15000u) {
        mirror_timed_out=TRUE;
        SudekiMpLogFormat("dev_spawn event=client_mirror_wait_timeout resource=%s attempts=%u policy=pause_without_mirror\r\n",mirror,mirror_attempts);
        return TRUE;
    }
    return FALSE;
}
