/* Named exact-image contracts. FNV digests contain no executable payload. */
typedef struct SpawnEventContract {const char *name;uint32_t rva,size,hash,reloc[8];} SpawnEventContract;
static const SpawnEventContract spawn_event_contracts[]={
    {"SpawnEventChooseEntry",0x762b0u,58u,0xada9dda2u,{0u}},
    {"SpawnEventResourceName",0x762efu,89u,0xf1275149u,{86u,0u}},
    {"SpawnEventCreateAndAttach",0x76348u,295u,0x22a58242u,{0u}},
    {"SpawnEventWeakAssign",0x1750u,85u,0xb6987b3au,{0u}},
    {"SpawnEventWeakDestroy",0x4d30u,83u,0x308b7b8fu,{9u,0u}},
    {"SpawnEventMonsterDestroy",0x1464c0u,31u,0xab4b90ecu,{0u}},
    {"SpawnEventMonsterBaseDestroy",0x1467dcu,14u,0xc3177d48u,{0u}},
    /* E2: AllyEntity (the gate guards) leaves through the same DespawnEntity. */
    {"SpawnEventAllyDestroy",0x13f300u,31u,0x95736a36u,{0u}},
    {"SpawnEventAllyBaseDestroy",0x13f610u,14u,0x18c1b8dfu,{0u}},
    {"SpawnEventGroupDestroy",0x72f90u,30u,0xe15cbedeu,{0u}},
    {"SpawnEventGroupBaseDestroy",0x73039u,8u,0xc9f5aa95u,{0u}},
    {"SpawnEventEntityBaseDestroy",0x13db47u,10u,0x23e5298fu,{0u}},
    {"SpawnEventGroupUpdateABI",0x73210u,12u,0xe9856299u,{0u}},
};
static BOOL spawn_event_code(uint8_t *b) {
    for(unsigned i=0;i<sizeof(spawn_event_contracts)/sizeof(*spawn_event_contracts);++i) {
        const SpawnEventContract *c=&spawn_event_contracts[i];uint32_t h=2166136261u;
        if(!readable(b+c->rva,c->size)) return FALSE;
        for(unsigned j=0;j<c->size;++j) {
            uint8_t v=b[c->rva+j];
            for(unsigned k=0;k<8u && c->reloc[k];++k) {
                unsigned r=c->reloc[k]-1u;
                if(j>=r && j<r+4u) {
                    uint32_t word;memcpy(&word,b+c->rva+r,4);
                    word-=(uint32_t)(uintptr_t)b-0x400000u;v=(uint8_t)(word>>(8u*(j-r)));break;
                }
            }
            h=(h^v)*16777619u;
        }
        if(h!=c->hash) return FALSE;
    }
    return *(void **)(b+0x2d5fa0u)==b+0x1464c0u && *(void **)(b+0x2d55d4u)==b+0x13f300u &&
        *(void **)(b+0x2c9e20u)==b+0x72f90u;
}
