/* Execute the retail transition selector and death predicate with a synthetic
 * exact leased actor. Only resource lookup and transition execution are inert
 * endpoints; no game allocation, renderer, damage or loot runs here. */
static uint8_t interrupt_actor[0x200],interrupt_anim[0x140],interrupt_state[16];
static uint8_t interrupt_combat[0x60],interrupt_arb[0x64],interrupt_stats[0x30];
static uint8_t interrupt_definition[16],interrupt_connections[4][4];
static volatile unsigned interrupt_selected,interrupt_transitions;
static void * __attribute__((used,noinline)) interrupt_lookup(void) {return interrupt_definition;}
static void __attribute__((used,noinline)) interrupt_transition_record(uint8_t *anim,uint8_t *connection) {
    assert(anim==interrupt_anim);
    assert(connection>=interrupt_connections[0] && connection<interrupt_connections[0]+16);
    interrupt_selected=connection[2];++interrupt_transitions;
}
static void __attribute__((naked,noinline)) interrupt_transition(void) {
    __asm__ volatile("pushl %eax; pushl %ecx; call _interrupt_transition_record; add $8,%esp; ret;");
}
static void interrupt_native_select(void) {
    void *fn=image_base+0xe3af0u,*a=interrupt_anim;
    unsigned before=interrupt_transitions;
    /* Native code follows pointers outside the compiler's call graph. Declare
     * its fixture memory explicitly so IPA cannot fold away native inputs. */
    __asm__ volatile("call *%10" : "+D"(a),"+m"(interrupt_actor),"+m"(interrupt_anim),
        "+m"(interrupt_state),"+m"(interrupt_combat),"+m"(interrupt_arb),"+m"(interrupt_stats),
        "+m"(interrupt_definition),"+m"(interrupt_connections),"+m"(interrupt_transitions)
        : "m"(fn) : "eax","ecx","edx","memory","cc");
    assert(a==interrupt_anim && interrupt_transitions<=before+1);
}
static void interrupt_refused(void) {
    unsigned before=interrupt_transitions;
    interrupt_native_select();assert(interrupt_transitions==before);
}
static void death_interrupt_fixture(void) {
    assert(death_interrupt_code(image_base));
    for(unsigned i=0;i<sizeof(death_interrupt_contracts)/sizeof(*death_interrupt_contracts);++i) {
        uint32_t r=death_interrupt_contracts[i].rva,n=death_interrupt_contracts[i].size;
        for(unsigned j=0;j<n;++j) {
            image_base[r+j]^=1;assert(!death_interrupt_code(image_base));
            assert(!death_interrupt_install(image_base) && !death_interrupt_hook.installed);
            image_base[r+j]^=1;
        }
        printf("%s every-byte mutation refusals PASS\n",death_interrupt_contracts[i].name);
    }
    gate_base=image_base;client_role=FALSE;assert(spawn_events_install(image_base));
    uint8_t *a=interrupt_actor,*anim=interrupt_anim,*combat=interrupt_combat,*arb=interrupt_arb,*stats=interrupt_stats;
    *(void **)a=image_base+0x2d5fa0u;*(uint32_t *)(a+0x34)=0x5b1e8abau;
    *(void **)(a+0x58)=anim;*(void **)anim=image_base+0x2c8504u;*(void **)(anim+0x10)=a;
    *(void **)(anim+0xf8)=interrupt_state;anim[0x133]=6;interrupt_state[2]=0x7a;
    *(void **)(a+0x90)=arb;*(void **)arb=image_base+0x2cc9acu;*(void **)(arb+0x10)=a;
    *(uint32_t *)(arb+0x50)=0x4000u;
    *(void **)(a+0xa4)=combat;*(void **)combat=image_base+0x2c8754u;*(void **)(combat+0x10)=a;
    *(uint32_t *)(combat+0x5c)=0x18u<<18;
    *(void **)(a+0x4c)=stats;*(void **)stats=image_base+0x2cc064u;*(void **)(stats+0x10)=a;
    SpawnEventLease *e=&spawn_events[0];memset(e,0,sizeof(*e));
    e->used=TRUE;e->terminal=TRUE;e->state.serial=123;e->state.resource=0x5b1e8abau;
    gate_bind(&e->actor,a);registry_entries[0]=a;
    *(void **)(registry+0x3c)=registry_entries;*(unsigned *)(registry+0x34)=1;
    *(void **)(image_base+RVA_REGISTRY)=registry;
    *(unsigned *)(interrupt_definition+4)=4;*(void **)(interrupt_definition+12)=interrupt_connections;
    for(unsigned i=0;i<4;++i) {interrupt_connections[i][2]=0x17+i;interrupt_connections[i][3]=3;}
    uint8_t saved_lookup[5],saved_transition[5];
    event_redirect(0xe42f0u,(void *)interrupt_lookup,saved_lookup);
    event_redirect(0xe3a60u,(void *)interrupt_transition,saved_transition);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    interrupt_refused(); /* Original selector waits although native death is selected. */
    assert(death_interrupt_install(image_base));assert(!death_interrupt_install(image_base));
    for(unsigned action=0x17;action<=0x1a;++action) {
        *(uint32_t *)(combat+0x5c)=action<<18;
        unsigned before=interrupt_transitions;interrupt_native_select();
        assert(interrupt_transitions==before+1 && interrupt_selected==action);
        assert(!interrupt_state[3] && !e->emitted && !e->death); /* No forged completion/effect. */
    }
    uint8_t saved_actor[sizeof(interrupt_actor)],saved_channels[sizeof(interrupt_state)];
    memcpy(saved_actor,a,sizeof(saved_actor));memcpy(saved_channels,interrupt_state,sizeof(saved_channels));
    client_role=TRUE;interrupt_refused();client_role=FALSE;
    *(float *)(stats+0x2c)=1;interrupt_refused();*(float *)(stats+0x2c)=0;
    *(uint32_t *)(arb+0x50)=0;interrupt_refused();*(uint32_t *)(arb+0x50)=0x4000;
    *(uint32_t *)(a+0x34)=94;e->state.resource=94;interrupt_refused();
    *(uint32_t *)(a+0x34)=e->state.resource=0x5b1e8abau;
    *(void **)a=image_base+0x2d555cu;interrupt_refused();*(void **)a=image_base+0x2d5fa0u;
    interrupt_state[2]=0x78;interrupt_refused();interrupt_state[2]=0x7a;
    *(void **)(anim+0x10)=NULL;interrupt_refused();*(void **)(anim+0x10)=a;
    *(void **)(stats+0x10)=NULL;interrupt_refused();*(void **)(stats+0x10)=a;
    *(void **)(combat+0x10)=NULL;interrupt_refused();*(void **)(combat+0x10)=a;
    *(void **)(arb+0x10)=NULL;interrupt_refused();*(void **)(arb+0x10)=a;
    e->terminal=FALSE;interrupt_refused();e->terminal=TRUE;
    e->owned=TRUE;interrupt_refused();e->owned=FALSE;
    e->job=a;interrupt_refused();e->job=NULL;
    e->emitted=4;interrupt_refused();e->emitted=0;
    spawn_event_fault=TRUE;interrupt_refused();spawn_event_fault=FALSE;
    *(unsigned *)(registry+0x34)=0;interrupt_refused();*(unsigned *)(registry+0x34)=1;
    gate_bind(&e->actor,NULL);interrupt_refused();gate_bind(&e->actor,a);
    uint8_t nondeath[4]={0,0,0x7a,3};assert(!death_interrupt_eligible(anim,nondeath));
    assert(!memcmp(saved_actor,a,sizeof(saved_actor)) && !memcmp(saved_channels,interrupt_state,sizeof(saved_channels)));
    /* Already finished and naturally interruptible connections stay native. */
    client_role=TRUE;interrupt_state[3]=1;
    unsigned before=interrupt_transitions;interrupt_native_select();assert(interrupt_transitions==before+1);
    interrupt_state[3]=0;interrupt_connections[3][3]=2;
    before=interrupt_transitions;interrupt_native_select();assert(interrupt_transitions==before+1);
    interrupt_connections[3][3]=3;client_role=FALSE;
    /* Foreign patch ownership quarantines without dropping callback storage;
     * restoring our bytes permits teardown/reinstallation on the game thread. */
    image_base[DEATH_INTERRUPT_SITE+5]^=1;
    assert(!death_interrupt_uninstall() && death_interrupt_hook.installed && death_interrupt_next);
    image_base[DEATH_INTERRUPT_SITE+5]^=1;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base+DEATH_INTERRUPT_SITE,DEATH_INTERRUPT_SIZE));
    interrupt_refused(); /* Failed teardown closes override admission. */
    assert(death_interrupt_uninstall() && death_interrupt_code(image_base));
    interrupt_refused();
    assert(death_interrupt_install(image_base) && death_interrupt_uninstall());
    client_role=TRUE;assert(death_interrupt_install(image_base) && !death_interrupt_hook.installed);
    client_role=FALSE;assert(spawn_events_uninstall());
    memcpy(image_base+0xe42f0u,saved_lookup,5);memcpy(image_base+0xe3a60u,saved_transition,5);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    memset(spawn_events,0,sizeof(spawn_events));spawn_event_thread=0;gate_base=NULL;
    fixture();registry_entries[0]=entity;
    puts("Kurian lethal death interruption: retail selector/predicate, four death types, living/client/identity refusals and restore retry PASS");
}
