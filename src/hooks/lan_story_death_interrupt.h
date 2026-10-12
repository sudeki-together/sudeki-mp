/* Owner-approved #62 exception: a dead, host-owned Kurian Trooper may leave
 * COMBO3 for its selected native death before the slash completes. Do not
 * edit shared animation resources or manufacture death/loot/effect events.
 * Included by the spawn adapter so the occurrence's native weak lease owns
 * admission. Other creatures and living interruption remain native. */
enum { DEATH_INTERRUPT_SITE=0xe3b65u, DEATH_INTERRUPT_SIZE=12u };
static SudekiMpInlineHook death_interrupt_hook;
static BOOL death_interrupt_ready;
static unsigned death_interrupt_calls,death_interrupt_count;
static void *death_interrupt_next __attribute__((used));
static void *death_interrupt_skip __attribute__((used));
static const struct {const char *name;uint32_t rva,size,hash;} death_interrupt_contracts[]={
    {"KurianDeathTransitionSelect",0xe3af0u,0xfdu,0x0e6d9cbeu}
};
static BOOL death_interrupt_code(uint8_t *b) {
    if(!b) return FALSE;
    for(unsigned i=0;i<sizeof(death_interrupt_contracts)/sizeof(*death_interrupt_contracts);++i) {
        uint32_t h=2166136261u,rva=death_interrupt_contracts[i].rva,n=death_interrupt_contracts[i].size;
        if(!readable(b+rva,n)) return FALSE;
        /* This complete selector has no absolute relocations. It proves EDI
         * is CAnimation, ESI the four-byte connection, ECX the channel array,
         * and both continuation destinations, including the native predicate. */
        for(unsigned j=0;j<n;++j) h=(h^b[rva+j])*16777619u;
        if(h!=death_interrupt_contracts[i].hash) return FALSE;
    }
    return TRUE;
}
static BOOL death_interrupt_eligible(uint8_t *anim,const uint8_t *connection) {
    if(!death_interrupt_ready || client_role || !readable(connection,4u) ||
        !(connection[3]&1u) || connection[2]<0x17u || connection[2]>0x1au ||
        !spawn_event_begin() || !readable(anim,0x134u) ||
        *(void **)anim!=image_base+0x2c8504u) return FALSE;
    uint8_t *a=*(uint8_t **)(anim+0x10u),*state=*(uint8_t **)(anim+0xf8u);
    uint32_t serial;
    if(!spawn_event_identity(a,&serial) || *(uint32_t *)(a+0x34u)!=0x5b1e8abau ||
        *(void **)(a+0x58u)!=anim || !readable(state,16u) || state[2]!=0x7au ||
        (state[3]&1u) || state[10] || state[14] ||
        death_style(a)+0x16u!=connection[2]) return FALSE;
    uint8_t *stats=*(uint8_t **)(a+0x4cu);
    if(!readable(stats,0x30u) || *(void **)stats!=image_base+0x2cc064u ||
        *(void **)(stats+0x10u)!=a || (*(uint32_t *)(stats+0x2cu)&0x7fffffffu)!=0) return FALSE;
    /* The native selector supplied this connection from this state. Require
     * a positively finished spawn job as well as the current weak identity. */
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        const SpawnEventLease *e=&spawn_events[i];
        if(e->used && e->state.serial==serial && e->actor.object==a)
            return e->terminal && !e->job && !e->owned && !e->emitted;
    }
    return FALSE;
}
static BOOL __attribute__((used,noinline,noclone)) death_interrupt_decision(uint8_t *anim,const uint8_t *connection) {
    DWORD error=GetLastError();++death_interrupt_calls;
    BOOL result=death_interrupt_eligible(anim,connection);
    if(result) {
        ++death_interrupt_count;
        SudekiMpLogFormat("story_death_interrupt event=admitted creature=kurian_trooper from=122 to=%u count=%u ms=%lu\r\n",
            connection[2],death_interrupt_count,(unsigned long)GetTickCount());
    }
    --death_interrupt_calls;SetLastError(error);return result;
}
static void __attribute__((naked,noinline)) death_interrupt_entry(void) {
    __asm__ volatile(
        /* Most candidates retain the native decision without entering C. */
        "testb $1,3(%esi); jz 2f; testb $1,3(%ecx); jnz 2f;"
        "cmpb $0x17,2(%esi); jb 3f; cmpb $0x1a,2(%esi); ja 3f;"
        "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov %edi,(%esp); mov %esi,4(%esp); call _death_interrupt_decision;"
        "mov %eax,12(%ebp); add $16,%esp; fxrstor (%esp); mov %ebp,%esp;"
        "cmpl $0,12(%esp); jnz 1f; popal; popfl;"
        /* Reproduce both original branches: the trampoline contains relative
         * short jumps and must never execute. Unknown cases stay native. */
        "testb $1,3(%esi); jz 2f; testb $1,3(%ecx); jz 3f;"
        "2: jmp *_death_interrupt_next;"
        "3: jmp *_death_interrupt_skip;"
        "1: popal; popfl; jmp *_death_interrupt_next;");
}
static BOOL death_interrupt_install(uint8_t *b) {
    if(client_role) return !death_interrupt_hook.installed;
    if(death_interrupt_hook.installed || image_base!=b || !death_interrupt_code(b)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    uint8_t original[DEATH_INTERRUPT_SIZE];memcpy(original,b+DEATH_INTERRUPT_SITE,sizeof(original));
    death_interrupt_next=b+0xe3b71u;death_interrupt_skip=b+0xe3b94u;
    if(!SudekiMpInstallInlineHook(&death_interrupt_hook,b+DEATH_INTERRUPT_SITE,
        original,sizeof(original),(void *)death_interrupt_entry)) return FALSE;
    death_interrupt_ready=TRUE;
    SudekiMpLogWrite("story_death_interrupt event=installed scope=host_kurian_combo3\r\n");
    return TRUE;
}
static BOOL death_interrupt_uninstall(void) {
    if(death_interrupt_calls || (spawn_event_thread && spawn_event_thread!=GetCurrentThreadId())) return FALSE;
    death_interrupt_ready=FALSE;
    if(death_interrupt_hook.installed && !SudekiMpRestoreInlineHook(&death_interrupt_hook)) return FALSE;
    death_interrupt_next=death_interrupt_skip=NULL;
    return TRUE;
}
