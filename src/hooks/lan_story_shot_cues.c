#include "hooks/lan_story_shot_cues.h"
#include "engine/build_identity.h"
#include "network/lan_arena_protocol.h"
#include <math.h>
#include <string.h>

/* Read-only shot-cue resolver (#54 client shot audio). Plain memory reads of
 * this game's own data: no native call, no write to native memory and no call
 * into another SudekiMP module (the caller owns the native-thread latch and
 * logging). Game thread only: one static registry copy. Every uncertainty is
 * SILENT; DEFER means only that the world registry changed during the walk.
 *
 * Image contracts (CONFIRMED_STATIC, supported image sha256 8ceb1d3c...):
 * FNV-1a over each span with relocated operands normalised to base 0x400000,
 * the vtable slots that tie each span to the object this file reads, and the
 * field-name literals. SudekiMpStoryShotCuesImageMatches runs inside each
 * resolve. */
typedef struct ShotCueContract {const char *name;uint32_t rva,size,hash;uint8_t reloc[3];} ShotCueContract;
static const ShotCueContract contracts[]={
    /* Weapon item loader (vtable 0x2d3a28 slot 0): 'Missile Sound' -> +0x120,
     * 'Missile Fire Sound' -> +0x124, 'Reload Sound' -> +0x128, as string-table
     * indices through 0x130dc0. */
    {"ItemCueFields",0x12fcafu,51u,0xd1401b2cu,{2u,19u,36u}},
    /* String table getter (vtable 0x2dcc8c +0x10): entry=(+0x10)[index];
     * header bit 31 = text inline at entry+4, else entry+4 is a pointer. */
    {"StringTableGet",0x1b9c00u,30u,0x1d1654e4u,{0u}},
    /* String table lookup: count at +8 bounds the array at +0x10. */
    {"StringTableLookup",0x1b9c20u,47u,0x6c6974c5u,{0u}},
    /* CMissile init 0x186e10 cue choice: weapon=owner+0xc0, item=+0x268; item
     * +0x120/+0x124 through [0x809e10] getter (-1 = none); else the weapon
     * default string objects +0x3bc/+0x3dc (text +0x3c0/+0x3e0); arbiter
     * skip bits; local flag through the leader test 0x4ebe20. */
    {"CueChoiceWeapon",0x187021u,6u,0x8a9bdfe6u,{0u}},
    {"CueChoiceItem",0x18702fu,6u,0x6958f1d7u,{0u}},
    {"CueChoiceTravelIndex",0x187039u,25u,0xa33d6f42u,{14u}},
    {"CueChoiceFireIndex",0x187063u,25u,0xa579bfd5u,{14u}},
    {"CueChoiceTravelDefault",0x187084u,25u,0x840e1380u,{0u}},
    {"CueChoiceFireDefault",0x1870aau,22u,0x39c307f9u,{0u}},
    {"CueChoiceSkipBits",0x1870cau,32u,0x201f6a12u,{0u}},
    {"LocalFlag",0x186f5fu,39u,0xd07a37a4u,{0u}},
    {"LocalLeaderTest",0xebe20u,57u,0xd89ecabbu,{6u}},
    /* MonsterEntity component link (vtable 0x2d5fa0 slot 7): +0xbc=+0x1114
     * (missile manager, id 0x1e), +0xc0=+0x1210 (weapon, id 0x1f), each
     * component +0x10 = actor. */
    {"MonsterMissileManagerLink",0x146c2eu,25u,0xedf08698u,{0u}},
    {"MonsterWeaponLink",0x146c5bu,25u,0xd62510f0u,{0u}},
    /* CCharacterWeapon serializer (vtable 0x2d4d3c slot 0) fills the default
     * names; the constructor stores that vtable; monsters never equip an item
     * (0x43f170 type set has no 0x19). */
    {"WeaponDefaultNames",0xd7729u,41u,0x18e0b309u,{11u,34u}},
    {"WeaponConstructVtable",0x13a8fbu,6u,0x63d7f6a6u,{3u}},
    {"MonsterNoItem",0xd8033u,5u,0xf89eb5cbu,{0u}},
    /* Item database [0x808d80]: item = (db+0xc)[id]; family getter +0x88. */
    {"ItemDatabaseIndex",0x217e0u,34u,0x0d586822u,{2u}},
    {"ItemFamilyGetter",0x21be80u,7u,0x0db58e5eu,{0u}},
    /* World registry [0x809d8c]: count +0x34, entries +0x3c. */
    {"WorldRegistryCount",0x12cfu,9u,0x2b6872c8u,{3u}},
    {"WorldRegistryEntries",0x1304u,6u,0x467c638du,{0u}},
    /* CMissileManager (vtable 0x2d4c8c slot 0): MissileCombos at +0x40,
     * count +4 (manager+0x44), element array +0xc (manager+0x4c); each combo
     * 0xc4 bytes, vtable 0x2cc2c8, MissileData embedded at +0xc. */
    {"MissileCombosContainer",0xc6d61u,9u,0xeb6a54e7u,{0u}},
    {"MissileCombosLayout",0xc901du,36u,0xfc875c9au,{0u}},
    {"MissileComboAllocate",0xc8fe0u,24u,0x6acd1392u,{0u}},
    {"MissileComboConstruct",0xc8330u,36u,0x6eec6804u,{4u,14u,21u}},
    /* MissileData (vtable 0x2d931c): model ResourceName at +0x24 starts as
     * kind 0x7f, id 0x7ffff, no reference; the serializer (slot 0) reads
     * 'Model' into +0x24 as kind 0x29. */
    {"MissileDataConstruct",0x187970u,11u,0x9e557822u,{8u}},
    {"MissileDataModelDefault",0x187990u,24u,0xf22e003eu,{0u}},
    {"MissileDataModelField",0x187de9u,30u,0x0905d052u,{23u}},
};
static const struct {uint32_t slot,target;} slots[]={
    {0x2d3a28u,0x12fbf0u},{0x2d3a30u,0x21be80u},{0x2dcc9cu,0x1b9c00u},{0x2d4d3cu,0xd7630u},
    {0x2d5fbcu,0x1467f0u},{0x2cc2c8u,0xc83d0u},{0x2d931cu,0x187a80u},{0x2d4c8cu,0xc6ad0u},
};
static const struct {uint32_t rva;const char *text;} literals[]={
    {0x2d3834u,"Missile Sound"},{0x2d3844u,"Missile Fire Sound"},{0x2d3858u,"Reload Sound"},
    {0x2cc904u,"Default Missile Sound"},{0x2cc91cu,"Default Missile Fire Sound"},{0x2d92a8u,"Model"},
};
typedef struct ShotWorldKind {unsigned kind,mask;uint32_t id;const char *name;} ShotWorldKind;
static const ShotWorldKind world_kinds[]={
#define SHOT_WORLD_KIND(symbol,name,id,mask) {SUDEKIMP_LAN_STORY_VFX_WORLD_##symbol,mask,id,name},
    SUDEKIMP_STORY_WORLD_RESOURCES(SHOT_WORLD_KIND)
#undef SHOT_WORLD_KIND
};
enum {
    RVA_ITEM_DB=0x408d80u, RVA_STRING_TABLE=0x409e10u, RVA_WORLD_REGISTRY=0x409d8cu,
    VT_ITEM_DB=0x2c693cu, VT_WEAPON_ITEM=0x2d3a28u, VT_STRING_TABLE=0x2dcc8cu,
    VT_MONSTER=0x2d5fa0u, VT_MONSTER_NODE=0x2d5fc4u, VT_POSITION=0x2cdefcu,
    VT_WEAPON=0x2d4d3cu, VT_MISSILE_MANAGER=0x2d4c8cu, VT_COMBO=0x2cc2c8u, VT_MISSILE_DATA=0x2d931cu,
    REGISTRY_MAX=8192u, TABLE_MAX=65536u, COMBO_MAX=64u,
    CANDIDATE_NONE=0, CANDIDATE_MATCH=1, CANDIDATE_UNKNOWN=2
};
static void *registry_copy[REGISTRY_MAX];
#if defined(SUDEKIMP_STORY_SHOT_CUES_TESTING)
static void (*walk_probe_for_test)(unsigned index);
#endif

/* Every byte committed and readable; a range may span regions (a page whose
 * protection another adapter changed splits a code region). */
static BOOL readable(const void *pointer,size_t n) {
    uintptr_t p=(uintptr_t)pointer,end;
    if(!pointer || !n || p>UINTPTR_MAX-n) return FALSE;
    for(end=p+n;p<end;) {
        MEMORY_BASIC_INFORMATION m;DWORD a;uintptr_t next;
        if(VirtualQuery((const void *)p,&m,sizeof(m))!=sizeof(m) ||
            m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
        a=m.Protect&0xffu;
        if(a!=PAGE_READONLY && a!=PAGE_READWRITE && a!=PAGE_WRITECOPY &&
            a!=PAGE_EXECUTE_READ && a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
        next=(uintptr_t)m.BaseAddress+m.RegionSize;
        if(next<=p) return FALSE;
        p=next;
    }
    return TRUE;
}
static uint32_t u32(const void *p) {uint32_t v;memcpy(&v,p,4u);return v;}
static uint8_t *ptr(const void *p) {uint8_t *v;memcpy(&v,p,sizeof(v));return v;}
static BOOL contract_exact(const uint8_t *b,const ShotCueContract *c) {
    uint32_t h=2166136261u;
    if(!readable(b+c->rva,c->size)) return FALSE;
    for(unsigned j=0;j<c->size;++j) {
        uint8_t v=b[c->rva+j];
        for(unsigned k=0;k<3u && c->reloc[k];++k) {
            unsigned r=c->reloc[k]-1u;
            if(j>=r && j<r+4u) {
                uint32_t word=u32(b+c->rva+r)-((uint32_t)(uintptr_t)b-0x400000u);
                v=(uint8_t)(word>>(8u*(j-r)));break;
            }
        }
        h=(h^v)*16777619u;
    }
    return h==c->hash;
}
BOOL SudekiMpStoryShotCuesImageMatches(HMODULE image) {
    const uint8_t *b=(const uint8_t *)image;const IMAGE_NT_HEADERS32 *nt;LONG at;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER)) || ((const IMAGE_DOS_HEADER *)b)->e_magic!=IMAGE_DOS_SIGNATURE ||
        (at=((const IMAGE_DOS_HEADER *)b)->e_lfanew)<=0 || at>0x800 ||
        !readable(b+at,sizeof(*nt))) return FALSE;
    nt=(const IMAGE_NT_HEADERS32 *)(b+at);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp!=SUDEKIMP_EXPECTED_TIMESTAMP ||
        nt->OptionalHeader.SizeOfImage!=SUDEKIMP_EXPECTED_IMAGE_SIZE) return FALSE;
    for(unsigned i=0;i<sizeof(contracts)/sizeof(contracts[0]);++i)
        if(!contract_exact(b,&contracts[i])) return FALSE;
    for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i)
        if(!readable(b+slots[i].slot,4u) || ptr(b+slots[i].slot)!=b+slots[i].target) return FALSE;
    for(unsigned i=0;i<sizeof(literals)/sizeof(literals[0]);++i) {
        size_t n=strlen(literals[i].text)+1u;
        if(!readable(b+literals[i].rva,n) || memcmp(b+literals[i].rva,literals[i].text,n)) return FALSE;
    }
    return TRUE;
}

/* Printable ASCII with a NUL within `bound` bytes (bound < 100); readability
 * proven per byte when the whole bound is not one readable range. */
static BOOL bounded_text(const uint8_t *text,size_t bound,char out[100]) {
    BOOL whole=readable(text,bound);
    for(size_t i=0;i<bound;++i) {
        if(!whole && !readable(text+i,1u)) break;
        if(!text[i]) {memcpy(out,text,i);out[i]=0;return TRUE;}
        if(text[i]<0x20u || text[i]>0x7eu) break;
    }
    out[0]=0;return FALSE;
}
/* Native small string object (0x1b9c00, 0x187084): header bit 31 = text
 * inline at +4, else +4 holds a text pointer. `object` is readable for 8. */
static BOOL string_object(const uint8_t *object,size_t inline_bound,size_t pointer_bound,char out[100]) {
    return u32(object)&0x80000000u?bounded_text(object+4u,inline_bound,out):
        bounded_text(ptr(object+4u),pointer_bound,out);
}
static BOOL table_text(uint8_t *b,int32_t index,char out[100]) {
    uint8_t *table,*array,*entry;uint32_t count;
    out[0]=0;
    if(index==-1) return TRUE;
    if(index<0 || !readable(b+RVA_STRING_TABLE,4u) || !readable(table=ptr(b+RVA_STRING_TABLE),0x14u) ||
        ptr(table)!=b+VT_STRING_TABLE || !(count=u32(table+8u)) || count>TABLE_MAX ||
        (uint32_t)index>=count || !readable(array=ptr(table+0x10u),count*4u) ||
        !readable(entry=ptr(array+(uint32_t)index*4u),8u)) return FALSE;
    return string_object(entry,96u,96u,out);
}
/* Party weapon item (weapon_activation_abi.c item_matches_family pattern):
 * Ailish (3) ids 12-23 family 5, Elco (1) ids 24-35 family 7. */
static unsigned item_cues(uint8_t *b,unsigned character,unsigned item,SudekiMpStoryShotCueNames *out) {
    unsigned family=character==3u?5u:character==1u?7u:0u,first=character==3u?12u:24u;
    uint8_t *db,*it;
    if(!family || item<first || item>=first+12u || !readable(b+RVA_ITEM_DB,4u) ||
        !readable(db=ptr(b+RVA_ITEM_DB),12u+(item+1u)*4u) || ptr(db)!=b+VT_ITEM_DB ||
        !readable(it=ptr(db+12u+item*4u),0x12cu) || ptr(it)!=b+VT_WEAPON_ITEM ||
        u32(it+0x14u)!=item || u32(it+0x88u)!=family ||
        !table_text(b,(int32_t)u32(it+0x120u),out->travel) ||
        !table_text(b,(int32_t)u32(it+0x124u),out->fire)) {
        memset(out,0,sizeof(*out));return SUDEKIMP_SHOT_CUES_SILENT;
    }
    return SUDEKIMP_SHOT_CUES_OK;
}
unsigned SudekiMpStoryShotCuesForItem(HMODULE image,unsigned character,unsigned item,SudekiMpStoryShotCueNames *out) {
    if(!out) return SUDEKIMP_SHOT_CUES_SILENT;
    memset(out,0,sizeof(*out));
    if(!SudekiMpStoryShotCuesImageMatches(image)) return SUDEKIMP_SHOT_CUES_SILENT;
    return item_cues((uint8_t *)image,character,item,out);
}

/* A combo's MissileData model ResourceName (combo+0x30: kind bits, id,
 * reference whose +4 is the text) names the world kind. The constructor
 * leaves kind 0x7f/id 0x7ffff/no reference; the serializer requests kind
 * 0x29. Match by the kind's .HOM id, or by retained text whose native name
 * hash equals the id and whose name (.HOM stripped) equals the kind's. */
static unsigned model_names_kind(const uint8_t *rn,const ShotWorldKind *k) {
    uint32_t bits=u32(rn)&0x7fu,id=u32(rn+4u),hash=0;uint8_t *ref=ptr(rn+8u);char text[100];size_t n;
    if(bits!=0x29u && bits!=0x7fu) return CANDIDATE_UNKNOWN;
    if(id==k->id) return CANDIDATE_MATCH;
    if(id==0x7ffffu && !ref) return CANDIDATE_NONE;
    if(!readable(ref,8u) || !bounded_text(ptr(ref+4u),64u,text) || !(n=strlen(text))) return CANDIDATE_UNKNOWN;
    for(size_t i=0;i<n;++i) {
        if(text[i]>='a' && text[i]<='z') text[i]=(char)(text[i]-'a'+'A');
        hash=(i&1u)?hash*(uint8_t)text[i]:hash+(uint8_t)text[i];
    }
    if(hash!=id) return CANDIDATE_UNKNOWN;
    if(n>=4u && !strcmp(text+n-4u,".HOM")) text[n-4u]=0;
    return strcmp(text,k->name)?CANDIDATE_NONE:CANDIDATE_MATCH;
}
/* One registry entry. Only an exact MonsterEntity within 2.5 horizontal
 * (x,z) and 3.0 vertical (y) of the first position whose missile manager
 * holds a MissileData of this kind is a candidate. A monster whose identity,
 * position or in-range components cannot be proven is UNKNOWN (silence). */
static unsigned world_candidate(uint8_t *b,uint8_t *e,const ShotWorldKind *k,const float first[3],
    SudekiMpStoryShotCueNames *names) {
    uint8_t *p,*w,*mm,*rows;float at[3],dx,dy,dz;unsigned n,match=CANDIDATE_NONE;
    if(!readable(e,12u)) return CANDIDATE_UNKNOWN;
    if(ptr(e)!=b+VT_MONSTER) return CANDIDATE_NONE;
    if(!readable(e,0x1610u) || ptr(e+8u)!=b+VT_MONSTER_NODE ||
        !readable(p=ptr(e+0x44u),0x24u) || ptr(p)!=b+VT_POSITION || ptr(p+0x10u)!=e) return CANDIDATE_UNKNOWN;
    memcpy(at,p+0x18u,sizeof(at));
    if(!isfinite(at[0]) || !isfinite(at[1]) || !isfinite(at[2])) return CANDIDATE_UNKNOWN;
    dx=at[0]-first[0];dy=at[1]-first[1];dz=at[2]-first[2];
    if(!(dx*dx+dz*dz<=6.25f) || !(fabsf(dy)<=3.0f)) return CANDIDATE_NONE;
    w=ptr(e+0xc0u);mm=ptr(e+0xbcu);
    if(w!=e+0x1210u || ptr(w)!=b+VT_WEAPON || ptr(w+0x10u)!=e || w[0x14u]!=0x1fu ||
        mm!=e+0x1114u || ptr(mm)!=b+VT_MISSILE_MANAGER || ptr(mm+0x10u)!=e || mm[0x14u]!=0x1eu)
        return CANDIDATE_UNKNOWN;
    if(!(n=u32(mm+0x44u))) return CANDIDATE_NONE;
    if(n>COMBO_MAX || !readable(rows=ptr(mm+0x4cu),n*4u)) return CANDIDATE_UNKNOWN;
    for(unsigned i=0;i<n;++i) {
        uint8_t *row=ptr(rows+i*4u);unsigned m;
        if(!readable(row,0xc4u) || ptr(row)!=b+VT_COMBO || ptr(row+0xcu)!=b+VT_MISSILE_DATA)
            return CANDIDATE_UNKNOWN;
        m=model_names_kind(row+0x30u,k);
        if(m==CANDIDATE_MATCH || (m==CANDIDATE_UNKNOWN && match==CANDIDATE_NONE)) match=m;
    }
    if(match!=CANDIDATE_MATCH) return match;
    /* Native uses the item fields when an item is equipped: never for a
     * monster, so one present is not understood. */
    if(ptr(w+0x268u) || !string_object(w+0x3bcu,28u,64u,names->travel) ||
        !string_object(w+0x3dcu,28u,64u,names->fire)) return CANDIDATE_UNKNOWN;
    return CANDIDATE_MATCH;
}
static unsigned world_cues(uint8_t *b,unsigned vfx_kind,const float first[3],SudekiMpStoryShotCueNames *out) {
    const ShotWorldKind *k=NULL;uint8_t *manager,*entries;unsigned count;
    SudekiMpStoryShotCueNames names,agreed;BOOL found=FALSE,uncertain=FALSE;
    memset(out,0,sizeof(*out));memset(&agreed,0,sizeof(agreed));
    for(unsigned i=0;i<sizeof(world_kinds)/sizeof(world_kinds[0]);++i)
        if(world_kinds[i].kind==vfx_kind && (world_kinds[i].mask&2u)) k=&world_kinds[i];
    if(!k || !first || !isfinite(first[0]) || !isfinite(first[1]) || !isfinite(first[2]) ||
        !readable(b+RVA_WORLD_REGISTRY,4u) || !readable(manager=ptr(b+RVA_WORLD_REGISTRY),0x40u))
        return SUDEKIMP_SHOT_CUES_SILENT;
    count=u32(manager+0x34u);entries=ptr(manager+0x3cu);
    if(count>REGISTRY_MAX || (count && !readable(entries,count*4u))) return SUDEKIMP_SHOT_CUES_SILENT;
    if(count) memcpy(registry_copy,entries,count*4u);
    for(unsigned i=0;i<count && !uncertain;++i) {
        unsigned r;
#if defined(SUDEKIMP_STORY_SHOT_CUES_TESTING)
        if(walk_probe_for_test) walk_probe_for_test(i);
#endif
        memset(&names,0,sizeof(names));
        r=world_candidate(b,registry_copy[i],k,first,&names);
        if(r==CANDIDATE_UNKNOWN) uncertain=TRUE;
        else if(r==CANDIDATE_MATCH && !found) {agreed=names;found=TRUE;}
        else if(r==CANDIDATE_MATCH && (strcmp(names.travel,agreed.travel) || strcmp(names.fire,agreed.fire)))
            uncertain=TRUE;
    }
    if(ptr(b+RVA_WORLD_REGISTRY)!=manager || !readable(manager,0x40u) || u32(manager+0x34u)!=count ||
        ptr(manager+0x3cu)!=entries ||
        (count && (!readable(entries,count*4u) || memcmp(entries,registry_copy,count*4u))))
        return SUDEKIMP_SHOT_CUES_DEFER;
    if(uncertain || !found) return SUDEKIMP_SHOT_CUES_SILENT;
    *out=agreed;return SUDEKIMP_SHOT_CUES_OK;
}
unsigned SudekiMpStoryShotCuesForWorldMissile(HMODULE image,unsigned vfx_kind,const float first_position[3],
    SudekiMpStoryShotCueNames *out) {
    if(!out) return SUDEKIMP_SHOT_CUES_SILENT;
    memset(out,0,sizeof(*out));
    if(!SudekiMpStoryShotCuesImageMatches(image)) return SUDEKIMP_SHOT_CUES_SILENT;
    return world_cues((uint8_t *)image,vfx_kind,first_position,out);
}
