/* Private spawn-trace observer. Uses the existing SoloSpawnSetup adapter;
 * observing a job never grants it a client residency allowance. */
enum { SIGIL_RECEIPTS=64u, SIGIL_IDENTITIES=512u };
typedef struct SigilReceipt {
    SoloJob source;
    SudekiMpStorySigilState state;
    uint8_t *effect;
    uint32_t effect_id;
    BOOL applied,superseded;
} SigilReceipt;
static SigilReceipt sigils[SIGIL_RECEIPTS];
static struct {uint32_t identity,occurrence;} sigil_occurrences[SIGIL_IDENTITIES];
static SudekiMpStorySigilState sigil_host[SUDEKIMP_STORY_SIGIL_MAX];
static uint32_t sigil_serial;
static DWORD sigil_thread;
static BOOL sigil_fault;
static uint32_t sigil_identity(const SoloJob *s) {
    unsigned point=(unsigned)(s->point-*(uint8_t **)(s->group+0x170u))/0x80u;
    uint32_t h=2166136261u,words[3]={s->group_id,point,s->resource_id};
    for(unsigned i=0;i<3u;++i) for(unsigned b=0;b<4u;++b) h=(h^((words[i]>>(8u*b))&255u))*16777619u;
    return h?h:1u;
}
static void sigil_constructed(void *raw) {
    uint8_t *j=raw;
    if(sigil_fault || !image_base || !readable(j,0x28u) || *(void **)j!=image_base+SOLO_VT || j[0x24u]) return;
    SoloJob s={.job=j,.group=*(void **)(j+0x10u),.point=*(void **)(j+0x1cu),.sequence=*(void **)(j+0x20u)};
    if(!readable(s.group,0x1b4u) || !readable(s.sequence,0x50u)) return;
    s.group_id=*(uint32_t *)(s.group+0x34u); s.resource_id=*(uint32_t *)(s.sequence+0x10u);
    if(!solo_group_fields(&s)) return;
    if(!sigil_thread) sigil_thread=GetCurrentThreadId();
    if(sigil_thread!=GetCurrentThreadId()) {sigil_fault=TRUE;return;}
    uint32_t id=sigil_identity(&s); unsigned key=0;
    for(;key<SIGIL_IDENTITIES && sigil_occurrences[key].identity && sigil_occurrences[key].identity!=id;++key) {}
    if(key==SIGIL_IDENTITIES || sigil_occurrences[key].occurrence==UINT32_MAX || sigil_serial==UINT32_MAX) {
        sigil_fault=TRUE;return; /* Never evict a generation floor or wrap it. */
    }
    unsigned slot=SIGIL_RECEIPTS;
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) {
        if(sigils[i].source.job==j) {sigil_fault=TRUE;return;}
        if(!sigils[i].state.spawn_serial) {slot=i;break;}
        if(!sigils[i].source.job && (!sigils[i].effect || sigils[i].state.ended || sigils[i].applied) &&
            (slot==SIGIL_RECEIPTS || sigils[i].state.spawn_serial<sigils[slot].state.spawn_serial)) slot=i;
    }
    /* Count even refused tracking so later occurrences cannot alias. */
    sigil_occurrences[key].identity=id; uint32_t occurrence=++sigil_occurrences[key].occurrence;
    if(slot==SIGIL_RECEIPTS) {sigil_fault=TRUE;return;}
    /* The constructor is positive evidence of a new occurrence. Invalidate
     * older local ownership even if its end was never observed. Retain its
     * counter floor, but never let reused actor/point addresses revive it. */
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].state.identity==id) {
        sigils[i].superseded=TRUE; sigils[i].effect=NULL; sigils[i].source.actor=NULL;
    }
    sigils[slot]=(SigilReceipt){.source=s,.state={id,occurrence,++sigil_serial,0}};
}
static void sigil_job_step(void *raw) {
    if(sigil_fault || sigil_thread!=GetCurrentThreadId()) return;
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) {
        SoloJob *s=&sigils[i].source;
        if(sigils[i].superseded || s->job!=raw || !readable(raw,0x28u) || *(void **)raw!=image_base+SOLO_VT ||
            *(void **)((uint8_t *)raw+0x10u)!=s->group || *(void **)((uint8_t *)raw+0x1cu)!=s->point ||
            *(void **)((uint8_t *)raw+0x20u)!=s->sequence || !solo_group_fields(s)) continue;
        uint8_t *actor=solo_actor(s);
        if(actor && in_registry(actor) && *(void **)actor==image_base+0x2d5fa0u) {
            s->actor=actor; s->actor_id=*(uint32_t *)(actor+0x34u);
        }
    }
}
static void sigil_job_destroyed(void *raw) {
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].source.job==raw) sigils[i].source.job=NULL;
}
static uint8_t *sigil_owner_effect(SigilReceipt *r) {
    SoloJob *s=&r->source; uint8_t *a=s->actor;
    if(!a || !solo_group_fields(s) || *(void **)(s->point+0x6cu)!=a || !in_registry(a) ||
        !readable(a,0x5cu) || *(void **)a!=image_base+0x2d5fa0u || *(uint32_t *)(a+0x34u)!=s->actor_id) return NULL;
    uint8_t *anim=*(uint8_t **)(a+0x58u);
    if(!readable(anim,0x104u) || *(void **)anim!=image_base+0x2c8504u || *(void **)(anim+0x10u)!=a) return NULL;
    uint8_t *effect=*(uint8_t **)(anim+0x100u);
    return fx_sigil(effect)?effect:NULL;
}
static void sigil_tick(void) {
    if(sigil_fault || sigil_thread!=GetCurrentThreadId()) return;
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) {
        SigilReceipt *r=&sigils[i];
        if(!r->state.spawn_serial || r->superseded || r->state.ended || r->applied) continue;
        uint8_t *e=sigil_owner_effect(r);
        if(!e) continue; /* Missing owner/effect is unknown, never an end. */
        if(!r->effect) {
            /* A newly proved owner supersedes any stale observation of this
             * address. Never infer an end for the displaced receipt. */
            for(unsigned j=0;j<SIGIL_RECEIPTS;++j) if(j!=i && sigils[j].effect==e) sigils[j].effect=NULL;
            r->effect=e; r->effect_id=*(uint32_t *)(e+0x34u);
            SudekiMpLogFormat("story_spawn_trace event=sigil_bound identity=%08lx occurrence=%lu serial=%lu client=%u\r\n",
                (unsigned long)r->state.identity,(unsigned long)r->state.occurrence,(unsigned long)r->state.spawn_serial,client_role);
        }
        if(e!=r->effect || *(uint32_t *)(e+0x34u)!=r->effect_id) continue;
        if(client_role) for(unsigned j=0;j<SUDEKIMP_STORY_SIGIL_MAX;++j) {
            const SudekiMpStorySigilState *h=&sigil_host[j];
            if(h->spawn_serial && h->ended && h->identity==r->state.identity && h->occurrence==r->state.occurrence && fx_hide(e)) {
                r->applied=TRUE;
                SudekiMpLogFormat("story_spawn_trace event=sigil_host_end_applied identity=%08lx occurrence=%lu host_serial=%lu\r\n",
                    (unsigned long)h->identity,(unsigned long)h->occurrence,(unsigned long)h->spawn_serial);
                break;
            }
        }
    }
}
/* Called before native retirement while the effect can still be proved. The
 * returned serial is a plain-data receipt, not a pointer to native storage. */
static uint32_t sigil_retiring(void *raw) {
    if(sigil_fault || sigil_thread!=GetCurrentThreadId() || !fx_sigil(raw)) return 0;
    sigil_tick();
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].effect==raw && !sigils[i].state.ended &&
        sigils[i].effect_id==*(uint32_t *)((uint8_t *)raw+0x34u)) return sigils[i].state.spawn_serial;
    return 0;
}
static void sigil_retired(uint32_t serial) {
    if(!serial) return;
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].state.spawn_serial==serial) {
        sigils[i].state.ended=1; sigils[i].effect=NULL;
        SudekiMpLogFormat("story_spawn_trace event=sigil_native_ended identity=%08lx occurrence=%lu serial=%lu client=%u\r\n",
            (unsigned long)sigils[i].state.identity,(unsigned long)sigils[i].state.occurrence,(unsigned long)serial,client_role);
        break;
    }
}
void SudekiMpLanStorySpawnSigilsFollow(const SudekiMpStorySigilState *host) {
    if(host) memcpy(sigil_host,host,sizeof(sigil_host));
}
void SudekiMpLanStorySpawnEffectsCapture(SudekiMpStoryEffectState *out) {
    if(!out || !image_base) return;
    uint8_t *m=cluster_manager(),*c=m?*(uint8_t **)(m+0x24u):NULL;
    if(m && readable(c,0x14u) && *(void **)c==image_base+RVA_CLUSTER_OBJECT_VTABLE &&
        *(void **)(c+4u)==image_base+RVA_CLUSTER_NAME_VTABLE && m[0x34u]<=1u) {
        out->cluster_valid=1; out->cluster_hash=*(uint32_t *)(c+0xcu);
        out->sector=*(uint16_t *)(m+0x2cu); out->in_combat=m[0x34u];
        out->flag36=m[0x36u]; out->suspended=*(uint32_t *)(m+0x30u)!=0;
    }
    /* Most recent bounded receipts in serial order; all are retransmitted,
     * including ended receipts, so packet loss never becomes a timer. */
    uint32_t after=0; unsigned count=0;
    memset(out->sigils,0,sizeof(out->sigils));
    for(unsigned n=0;n<SIGIL_RECEIPTS;++n) {
        SigilReceipt *best=NULL;
        for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].state.spawn_serial>after && !sigils[i].superseded &&
            (sigils[i].effect || sigils[i].state.ended) &&
            (!best || sigils[i].state.spawn_serial<best->state.spawn_serial)) best=&sigils[i];
        if(!best) break;
        after=best->state.spawn_serial;
        if(count==SUDEKIMP_STORY_SIGIL_MAX) {memmove(out->sigils,out->sigils+1,(count-1u)*sizeof(*out->sigils));--count;}
        out->sigils[count++]=best->state;
    }
}
