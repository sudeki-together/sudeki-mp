/* [SudekiMP] Shot-cue resolver on fake memory: the unchecked readers behind
 * SudekiMpStoryShotCuesForItem/ForWorldMissile (bounded inline/pointer text,
 * string-table bounds, item identity, the MissileData kind filter, pairing
 * agreement, distance bounds, registry change -> DEFER). The fake base is not
 * the supported image, so the public entry points must stay silent on it. No
 * native routine exists or executes here. */
#define SUDEKIMP_STORY_SHOT_CUES_TESTING
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_shot_cues.c"

enum {
    K183=SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK105_PROJECTILE02, K231=SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK106_PROJECTILE03,
    K172=SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK101_PROJECTILE, K195=SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK102_BLOCK_INTO,
    KMONEY=SUDEKIMP_LAN_STORY_VFX_WORLD_I315_MONEY, KPU=SUDEKIMP_LAN_STORY_VFX_WORLD_SFXPU101_ITEMFX_APPEAR
};
static uint8_t *b,*limit; /* limit: first byte of a NOACCESS page */
static void put32(void *p,uint32_t v) {memcpy(p,&v,4);}
static void putp(void *p,const void *v) {memcpy(p,&v,sizeof(v));}
static uint32_t native_hash(const char *s) {
    uint32_t h=0;
    for(size_t i=0;s[i];++i) {unsigned c=(unsigned char)s[i];if(c>='a' && c<='z')c-=32u;h=(i&1u)?h*c:h+c;}
    return h;
}
static SudekiMpStoryShotCueNames names;
static BOOL empty(void) {return !names.travel[0] && !names.fire[0];}
static BOOL named(const char *travel,const char *fire) {return !strcmp(names.travel,travel) && !strcmp(names.fire,fire);}

/* Item database and string table. */
static uint8_t db[0x1000],items[48][0x130],table[0x14],*array[24],strings[24][128];
static uint8_t *item(unsigned id,unsigned family,int32_t travel,int32_t fire) {
    uint8_t *it=items[id];memset(it,0,sizeof(items[id]));
    putp(it,b+VT_WEAPON_ITEM);put32(it+0x14,id);put32(it+0x88,family);
    put32(it+0x120,(uint32_t)travel);put32(it+0x124,(uint32_t)fire);putp(db+12+id*4,it);return it;
}
static void inline_entry(unsigned i,const char *text) {
    memset(strings[i],0,sizeof(strings[i]));put32(strings[i],0x80000000u|(uint32_t)strlen(text));
    memcpy(strings[i]+4,text,strlen(text));array[i]=strings[i];
}
static void pointer_entry(unsigned i,const char *text) {
    memset(strings[i],0,sizeof(strings[i]));put32(strings[i],(uint32_t)strlen(text));putp(strings[i]+4,text);array[i]=strings[i];
}
static void item_world(void) {
    memset(db,0,sizeof(db));putp(db,b+VT_ITEM_DB);putp(b+RVA_ITEM_DB,db);
    putp(table,b+VT_STRING_TABLE);put32(table+8,8);putp(table+0x10,array);putp(b+RVA_STRING_TABLE,table);
    inline_entry(0,"");inline_entry(1,"Royal_Sceptre_projectile");pointer_entry(2,"Royal_Sceptre_fire");
    pointer_entry(3,"Xurics_Shock_staff");inline_entry(4,"Mk1_Pistol_projectile");pointer_entry(5,"Mk1_Pistol_fire");
    inline_entry(6,"Night_Shadow_projectile");pointer_entry(7,"Night_Shadow_fire");
    item(12,5,1,2);item(17,5,-1,3);item(23,5,6,7);item(24,7,4,5);
}
static unsigned for_item(unsigned character,unsigned id) {
    memset(&names,0x55,sizeof(names));return item_cues(b,character,id,&names);
}
static void test_text(void) {
    char out[100];uint8_t buffer[128],object[8];
    /* Inline 28-byte bound (weapon defaults): NUL at index 27 passes, 28
     * characters without a NUL fail; pointer text 64 and table text 96. */
    memset(buffer,'a',sizeof(buffer));buffer[27]=0;
    assert(bounded_text(buffer,28,out) && strlen(out)==27);
    buffer[27]='a';assert(!bounded_text(buffer,28,out) && !out[0]);
    buffer[63]=0;assert(bounded_text(buffer,64,out) && strlen(out)==63);
    buffer[63]='a';assert(!bounded_text(buffer,64,out));
    buffer[95]=0;assert(bounded_text(buffer,96,out) && strlen(out)==95);
    buffer[95]='a';assert(!bounded_text(buffer,96,out));
    memcpy(buffer,"ok\x01x",5);assert(!bounded_text(buffer,28,out));
    memcpy(buffer,"ok\x7fx",5);assert(!bounded_text(buffer,28,out));
    memcpy(buffer,"ok\x80x",5);assert(!bounded_text(buffer,28,out));
    assert(bounded_text((const uint8_t *)"",28,out) && !out[0]);
    assert(!bounded_text(NULL,28,out));
    /* Text that ends at an unreadable page: per-byte readability. */
    memcpy(limit-5,"abcd",5);assert(bounded_text(limit-5,96,out) && !strcmp(out,"abcd"));
    memcpy(limit-5,"abcde",5);assert(!bounded_text(limit-5,96,out));
    /* A range across two readable regions of different protection passes;
     * one that reaches a NOACCESS page does not. */
    {uint8_t *two=VirtualAlloc(NULL,0x3000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);DWORD old;
     assert(two && VirtualProtect(two+0x1000,0x1000,PAGE_READONLY,&old) && VirtualProtect(two+0x2000,0x1000,PAGE_NOACCESS,&old));
     memset(two+0xff0,'r',0x10);
     assert(readable(two+0xff0,0x20) && readable(two,0x2000) && !readable(two+0xff0,0x1011) && !readable(two+0x2000,1));
     assert(bounded_text(two+0xff0,96,out) && strlen(out)==0x10);}
    /* String objects: bit 31 inline at +4, otherwise +4 is a pointer. */
    put32(object,0x80000000u);memcpy(object+4,"abc",4);assert(string_object(object,4,64,out) && !strcmp(out,"abc"));
    put32(object,3u);putp(object+4,"pointed");assert(string_object(object,4,64,out) && !strcmp(out,"pointed"));
    putp(object+4,NULL);assert(!string_object(object,4,64,out));
}
static void test_items(void) {
    item_world();
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && named("Royal_Sceptre_projectile","Royal_Sceptre_fire"));
    assert(for_item(3,17)==SUDEKIMP_SHOT_CUES_OK && named("","Xurics_Shock_staff"));
    assert(for_item(3,23)==SUDEKIMP_SHOT_CUES_OK && named("Night_Shadow_projectile","Night_Shadow_fire"));
    assert(for_item(1,24)==SUDEKIMP_SHOT_CUES_OK && named("Mk1_Pistol_projectile","Mk1_Pistol_fire"));
    /* Character/range/family rule. */
    assert(for_item(1,12)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    assert(for_item(3,24)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    for(unsigned c=0;c<6;++c) if(c!=1 && c!=3) assert(for_item(c,12)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    assert(for_item(3,11)==SUDEKIMP_SHOT_CUES_SILENT && for_item(1,36)==SUDEKIMP_SHOT_CUES_SILENT);
    /* Empty database slot. */
    assert(for_item(3,13)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    /* Wrong vtable, id, family. */
    uint8_t *it=items[12];
    putp(it,b+VT_WEAPON_ITEM+4);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT && empty());putp(it,b+VT_WEAPON_ITEM);
    put32(it+0x14,13);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);put32(it+0x14,12);
    put32(it+0x88,7);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);put32(it+0x88,4);
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);put32(it+0x88,5);
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK);
    /* Item record not readable through +0x12c. */
    uint8_t *tail=limit-0x12b;memcpy(tail,it,0x12b);putp(db+12+12*4,tail);
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    tail=limit-0x12c;memcpy(tail,it,0x12c);putp(db+12+12*4,tail);
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && named("Royal_Sceptre_projectile","Royal_Sceptre_fire"));
    putp(db+12+12*4,it);
    /* Database identity and global. */
    putp(db,b+VT_ITEM_DB+4);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);putp(db,b+VT_ITEM_DB);
    putp(b+RVA_ITEM_DB,NULL);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);putp(b+RVA_ITEM_DB,db);
    /* Index -1 is empty; -2 and index >= count are refused. */
    put32(it+0x120,0xffffffffu);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && named("","Royal_Sceptre_fire"));
    put32(it+0x124,0xffffffffu);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && empty());
    put32(it+0x120,0xfffffffeu);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    put32(it+0x120,8);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(it+0x120,7);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && named("Night_Shadow_fire",""));
    put32(it+0x120,0);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && empty());
    put32(it+0x120,1);put32(it+0x124,2);
    /* String table identity, count bounds, array readability. */
    putp(table,b+VT_STRING_TABLE+4);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);putp(table,b+VT_STRING_TABLE);
    assert(for_item(3,17)==SUDEKIMP_SHOT_CUES_OK);
    put32(table+8,0);assert(for_item(3,17)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(table+8,65537);assert(for_item(3,17)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(table+8,8);putp(table+0x10,NULL);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);putp(table+0x10,array);
    putp(b+RVA_STRING_TABLE,NULL);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    assert(for_item(3,17)==SUDEKIMP_SHOT_CUES_SILENT);putp(b+RVA_STRING_TABLE,table);
    /* Entry text bounds: 95 characters inline or pointed pass, 96 fail;
     * non-printable or NULL text is silent. */
    static char long_text[128];memset(long_text,'x',sizeof(long_text));long_text[95]=0;
    pointer_entry(1,long_text);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && strlen(names.travel)==95);
    long_text[95]='x';long_text[96]=0;assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    memset(strings[1],'y',sizeof(strings[1]));put32(strings[1],0x80000000u);strings[1][4+95]=0;
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && strlen(names.travel)==95);
    strings[1][4+95]='y';assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    inline_entry(1,"Royal\tSceptre");assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    pointer_entry(1,"x");putp(strings[1]+4,NULL);assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);
    inline_entry(1,"Royal_Sceptre_projectile");
    array[2]=NULL;assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_SILENT);pointer_entry(2,"Royal_Sceptre_fire");
    assert(for_item(3,12)==SUDEKIMP_SHOT_CUES_OK && named("Royal_Sceptre_projectile","Royal_Sceptre_fire"));
}

/* World registry and MonsterEntity copies. */
static uint8_t manager[0x40];static void *entries[16];static unsigned entry_count;
static void registry(void) {
    memset(manager,0,sizeof(manager));putp(b+RVA_WORLD_REGISTRY,manager);
    put32(manager+0x34,entry_count);putp(manager+0x3c,entries);
}
static void add(void *e) {assert(entry_count<16);entries[entry_count++]=e;put32(manager+0x34,entry_count);}
static void clear_registry(void) {entry_count=0;memset(entries,0,sizeof(entries));registry();}
static void inline_name(uint8_t *object,const char *text) {
    assert(strlen(text)<28);put32(object,0x80000000u);memset(object+4,0,28);memcpy(object+4,text,strlen(text)+1);
}
static void pointer_name(uint8_t *object,const char *text) {put32(object,(uint32_t)strlen(text));putp(object+4,text);}
static uint8_t *row(uint8_t *e,unsigned i) {return ptr(ptr(e+0x1114+0x4c)+i*4);}
static void model(uint8_t *e,unsigned i,uint32_t bits,uint32_t id,const char *text) {
    uint8_t *rn=row(e,i)+0x30;put32(rn,0xf80u|bits);put32(rn+4,id);
    if(text) {uint8_t *ref=calloc(1,8);put32(ref,1);putp(ref+4,text);putp(rn+8,ref);} else putp(rn+8,NULL);
}
static uint8_t *monster(float x,float y,float z,unsigned combos) {
    uint8_t *e=calloc(1,0x1690),*p=calloc(1,0x30),*w=e+0x1210,*mm=e+0x1114;float at[3]={x,y,z};
    assert(e && p);
    putp(e,b+VT_MONSTER);putp(e+8,b+VT_MONSTER_NODE);putp(e+0x44,p);
    putp(p,b+VT_POSITION);putp(p+0x10,e);memcpy(p+0x18,at,12);
    putp(e+0xc0,w);putp(e+0xbc,mm);
    putp(w,b+VT_WEAPON);putp(w+0x10,e);w[0x14]=0x1f;
    putp(mm,b+VT_MISSILE_MANAGER);putp(mm+0x10,e);mm[0x14]=0x1e;put32(mm+0x44,combos);
    if(combos) {
        uint8_t *rows=calloc(combos,4);assert(rows);putp(mm+0x4c,rows);
        for(unsigned i=0;i<combos;++i) {
            uint8_t *r=calloc(1,0xc4);assert(r);putp(rows+i*4,r);putp(r,b+VT_COMBO);putp(r+0xc,b+VT_MISSILE_DATA);
            model(e,i,0x7f,0x7ffff,NULL);
        }
    }
    inline_name(w+0x3bc,"");inline_name(w+0x3dc,"");
    return e;
}
/* MON_Kurian_Shooter: one PISTOLVOLLEY combo, model SFXKK105_PROJECTILE02. */
static uint8_t *shooter(float x,float y,float z) {
    uint8_t *e=monster(x,y,z,1);
    model(e,0,0x29,0x75d53da9u,"SFXKK105_PROJECTILE02.HOM");
    inline_name(e+0x1210+0x3bc,"kurianKnight_projectile");inline_name(e+0x1210+0x3dc,"kurian_knight_fire_effect");
    return e;
}
static const float origin[3]={-92.9f,8.0f,-152.5f};
static unsigned for_world(unsigned kind,const float *at) {
    memset(&names,0x55,sizeof(names));return world_cues(b,kind,at,&names);
}
static BOOL kurian(void) {return named("kurianKnight_projectile","kurian_knight_fire_effect");}
static void (*mutation)(void);static unsigned mutate_at;
static void probe(unsigned index) {if(index==mutate_at && mutation) {mutation();mutation=NULL;}}
static void *spare;
static void swap_entry(void) {entries[0]=spare;}
static void grow_count(void) {put32(manager+0x34,entry_count+1);entries[entry_count]=spare;}
static uint8_t other_manager[0x40];
static void move_manager(void) {memcpy(other_manager,manager,sizeof(manager));putp(b+RVA_WORLD_REGISTRY,other_manager);}
static void move_entries(void) {static void *copy[16];memcpy(copy,entries,sizeof(entries));putp(manager+0x3c,copy);}

static void test_world(void) {
    float at[3];
    assert(K183==183 && K231==231 && K172==172 && K195==195);
    assert(native_hash("SFXKK105_PROJECTILE02")==0x20ee3c62u && native_hash("SFXKK105_PROJECTILE02.HOM")==0x75d53da9u);
    clear_registry();
    /* None. */
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    /* One candidate by the .HOM id, 0.9 units away. */
    uint8_t *s1=shooter(-93.8f,8.0f,-152.45f);add(s1);
    static uint8_t other_entity[0x100];putp(other_entity,b+0x2d5a10);add(other_entity);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* Pointer (non-inline) strings, and their 64-byte bound. */
    pointer_name(s1+0x1210+0x3bc,"kurianKnight_projectile");pointer_name(s1+0x1210+0x3dc,"kurian_knight_fire_effect");
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    static char long_name[80];memset(long_name,'k',sizeof(long_name));long_name[63]=0;
    pointer_name(s1+0x1210+0x3bc,long_name);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && strlen(names.travel)==63);
    long_name[63]='k';long_name[64]=0;assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    pointer_name(s1+0x1210+0x3bc,"kurianKnight_projectile");
    /* Inline 28-byte bound; unterminated or non-printable text is silent. */
    memset(s1+0x1210+0x3e0,'f',28);put32(s1+0x1210+0x3dc,0x80000000u);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    (s1+0x1210+0x3e0)[27]=0;assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && strlen(names.fire)==27);
    inline_name(s1+0x1210+0x3dc,"kurian\x1b");assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    inline_name(s1+0x1210+0x3dc,"kurian_knight_fire_effect");
    /* Empty defaults (HC_NW_Kurian_Shooter): OK with no cue. */
    uint8_t *silent_shooter=shooter(-92.0f,8.0f,-152.0f);
    inline_name(silent_shooter+0x1210+0x3bc,"");inline_name(silent_shooter+0x1210+0x3dc,"");
    clear_registry();add(silent_shooter);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && empty());
    /* Mixed templates with the same model disagree: silent. */
    add(s1);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    /* Several agreeing candidates. */
    uint8_t *s2=shooter(-92.0f,8.5f,-153.0f),*s3=shooter(-91.5f,7.5f,-151.6f);
    clear_registry();add(s1);add(s2);add(s3);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* Check fix #2: a mixed group. A Levy (no MissileData) and a Kurian
     * Knight whose missile model is another kind, with other names, stand
     * nearer than the Shooter; neither counts. */
    uint8_t *levy=monster(-92.9f,8.0f,-152.4f,0);
    inline_name(levy+0x1210+0x3bc,"levy_projectile");inline_name(levy+0x1210+0x3dc,"levy_fire");
    uint8_t *knight=monster(-92.8f,8.0f,-152.6f,2);
    model(knight,0,0x29,0x5c5bdb37u,"SFXKK101_PROJECTILE.HOM");
    model(knight,1,0x7f,native_hash("SFXKK113_FASTPROJECTILE"),"SFXKK113_FASTPROJECTILE");
    inline_name(knight+0x1210+0x3bc,"knight_projectile");inline_name(knight+0x1210+0x3dc,"knight_fire");
    clear_registry();add(levy);add(knight);add(s1);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* The knight's own kind pairs with the knight only. */
    assert(for_world(K172,origin)==SUDEKIMP_SHOT_CUES_OK && named("knight_projectile","knight_fire"));
    /* Block effects (source-1 SpecialEffects with mask 3) have no MissileData:
     * silent (check fix #1 holds in the resolver too). */
    assert(for_world(K195,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    /* SFXKK106 is not any candidate's model. */
    assert(for_world(K231,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    /* Not a missile source kind, or not a world kind. */
    assert(for_world(KMONEY,origin)==SUDEKIMP_SHOT_CUES_SILENT && for_world(KPU,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    assert(for_world(44,origin)==SUDEKIMP_SHOT_CUES_SILENT && for_world(0,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    assert(for_world(SUDEKIMP_LAN_STORY_VFX_WORLD_END,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    /* Model by retained text: bare name (data hash 0x20ee3c62) with the
     * constructor's unresolved kind 0x7f, or lower-case .hom text. */
    model(s1,0,0x7f,0x20ee3c62u,"SFXKK105_PROJECTILE02");
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    model(s1,0,0x29,native_hash("sfxkk105_projectile02"),"sfxkk105_projectile02");
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* Text that does not hash to the id, an id without text, unreadable or
     * unterminated text, or another kind encoding is unknown: silent. */
    model(s1,0,0x29,0x20ee3c63u,"SFXKK105_PROJECTILE02");assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    model(s1,0,0x29,0x20ee3c62u,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    model(s1,0,0x29,0x20ee3c62u,"SFXKK105_PROJECTILE02");putp(ptr(row(s1,0)+0x38)+4,NULL);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    memset(limit-21,'S',21);model(s1,0,0x29,0x20ee3c62u,(const char *)limit-21);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    model(s1,0,0x26,0x75d53da9u,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    /* A knight with only an unknown model near the Shooter also silences. */
    model(s1,0,0x29,0x75d53da9u,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    model(knight,1,0x29,0x12345678u,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    /* An unknown row next to a matching row on the same Shooter still pairs. */
    model(knight,1,0x7f,0x7ffff,NULL);
    uint8_t *two=monster(-92.5f,8.0f,-152.5f,2);
    model(two,0,0x29,0x12345678u,NULL);model(two,1,0x29,0x75d53da9u,NULL);
    inline_name(two+0x1210+0x3bc,"kurianKnight_projectile");inline_name(two+0x1210+0x3dc,"kurian_knight_fire_effect");
    clear_registry();add(two);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* Distance: x,z horizontal within 2.5 (inclusive), |y| within 3.0. */
    clear_registry();uint8_t *d=shooter(origin[0]+2.5f,origin[1],origin[2]);add(d);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    memcpy(at,origin,12);at[0]-=0.01f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    memcpy(at,origin,12);at[0]+=1.0f;at[2]+=2.0f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_OK);
    at[2]+=0.6f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT);
    memcpy(at,origin,12);at[0]+=2.5f;at[1]+=3.0f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_OK);
    at[1]+=0.01f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT);
    at[1]=origin[1]-3.01f;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT);
    /* Bad first positions. */
    memcpy(at,origin,12);at[1]=NAN;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT);
    at[1]=INFINITY;assert(for_world(K183,at)==SUDEKIMP_SHOT_CUES_SILENT);
    assert(for_world(K183,NULL)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    /* An item on a matching candidate is not understood: silent. */
    clear_registry();add(s1);putp(s1+0x1210+0x268,items[12]);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());putp(s1+0x1210+0x268,NULL);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK);
    /* In-range identity failures are unknown (silent); the same failures
     * out of range are ignored. */
    struct {unsigned offset;BOOL byte;} breaks[]={
        {0x8,FALSE},{0x44,FALSE},{0xc0,FALSE},{0xbc,FALSE},{0x1210,FALSE},{0x1220,FALSE},{0x1224,TRUE},
        {0x1114,FALSE},{0x1124,FALSE},{0x1128,TRUE},{0x1160,FALSE}};
    uint8_t *distant=shooter(origin[0]+40.0f,origin[1],origin[2]);
    for(unsigned i=0;i<sizeof(breaks)/sizeof(breaks[0]);++i) {
        clear_registry();add(s1);
        if(breaks[i].byte) s1[breaks[i].offset]^=1u; else put32(s1+breaks[i].offset,u32(s1+breaks[i].offset)+4u);
        assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
        if(breaks[i].byte) s1[breaks[i].offset]^=1u; else put32(s1+breaks[i].offset,u32(s1+breaks[i].offset)-4u);
        if(breaks[i].byte) distant[breaks[i].offset]^=1u; else put32(distant+breaks[i].offset,u32(distant+breaks[i].offset)+4u);
        if(breaks[i].offset!=0x8 && breaks[i].offset!=0x44) {
            add(distant);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
        }
        if(breaks[i].byte) distant[breaks[i].offset]^=1u; else put32(distant+breaks[i].offset,u32(distant+breaks[i].offset)-4u);
    }
    /* CPosition identity and finiteness. */
    clear_registry();add(s1);uint8_t *p=ptr(s1+0x44);
    putp(p,b+VT_POSITION+4);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);putp(p,b+VT_POSITION);
    putp(p+0x10,s2);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);putp(p+0x10,s1);
    float nan=NAN,keep;memcpy(&keep,p+0x1c,4);memcpy(p+0x1c,&nan,4);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);memcpy(p+0x1c,&keep,4);
    /* Combo bounds and identity. */
    uint8_t *mm=s1+0x1114,*rows=ptr(mm+0x4c);
    put32(mm+0x44,65);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(mm+0x44,0);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(mm+0x44,1);putp(mm+0x4c,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);putp(mm+0x4c,rows);
    putp(row(s1,0),b+VT_COMBO+4);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);putp(row(s1,0),b+VT_COMBO);
    putp(row(s1,0)+0xc,b+VT_MISSILE_DATA+4);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    putp(row(s1,0)+0xc,b+VT_MISSILE_DATA);
    /* A combo not readable for its 0xc4 bytes. */
    uint8_t *r0=row(s1,0);
    memcpy(limit-0xc3,r0,0xc3);putp(rows,limit-0xc3);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    memcpy(limit-0xc4,r0,0xc4);putp(rows,limit-0xc4);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    putp(rows,r0);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* AllyEntity and other entities never count; a MonsterEntity smaller
     * than its proven layout or an unreadable entry is unknown. */
    static uint8_t ally[0x1690];memcpy(ally,s1,sizeof(ally));putp(ally,b+0x2d55d4);
    clear_registry();add(ally);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    add(s1);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    uint8_t *short_monster=limit-0x160f;putp(short_monster,b+VT_MONSTER);add(short_monster);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT && empty());
    clear_registry();add(s1);add(NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    clear_registry();add(s1);add(limit);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    /* Registry bounds. */
    clear_registry();add(s1);
    put32(manager+0x34,8193);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    put32(manager+0x34,1);putp(manager+0x3c,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    putp(manager+0x3c,entries);putp(b+RVA_WORLD_REGISTRY,NULL);assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_SILENT);
    registry();assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    /* Registry changed during the walk: DEFER, never names. */
    walk_probe_for_test=probe;spare=s2;
    clear_registry();add(s1);add(levy);add(s3);
    void (*changes[])(void)={swap_entry,grow_count,move_manager,move_entries};
    for(unsigned i=0;i<sizeof(changes)/sizeof(changes[0]);++i) {
        clear_registry();add(s1);add(levy);add(s3);mutation=changes[i];mutate_at=1;
        assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_DEFER && empty() && !mutation);
    }
    /* A change during a walk that was already silent is still DEFER. */
    clear_registry();add(s1);add(silent_shooter);add(levy);mutation=swap_entry;mutate_at=1;
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_DEFER && empty() && !mutation);
    clear_registry();add(s1);add(levy);add(s3);
    assert(for_world(K183,origin)==SUDEKIMP_SHOT_CUES_OK && kurian());
    walk_probe_for_test=NULL;
}
static void test_public(void) {
    /* The fake base is not the supported image: every public resolve is
     * silent and clears the names. */
    SudekiMpStoryShotCueNames out;
    assert(!SudekiMpStoryShotCuesImageMatches((HMODULE)b) && !SudekiMpStoryShotCuesImageMatches(NULL));
    memset(&out,0x55,sizeof(out));
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,&out)==SUDEKIMP_SHOT_CUES_SILENT && !out.travel[0] && !out.fire[0]);
    memset(&out,0x55,sizeof(out));
    assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,K183,origin,&out)==SUDEKIMP_SHOT_CUES_SILENT && !out.travel[0] && !out.fire[0]);
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,NULL)==SUDEKIMP_SHOT_CUES_SILENT);
    assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,K183,origin,NULL)==SUDEKIMP_SHOT_CUES_SILENT);
}
int main(void) {
    DWORD old;
    b=VirtualAlloc(NULL,0x410000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    uint8_t *pages=VirtualAlloc(NULL,0x3000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    /* Objects placed to end exactly at `limit` cannot be read past it. */
    assert(b && pages && VirtualProtect(pages+0x2000,0x1000,PAGE_NOACCESS,&old));
    limit=pages+0x2000;
    test_text();
    test_items();
    test_world();
    test_public();
    puts("StoryShotCuesTest: PASS (bounded inline/pointer text 28/64/96, string table -1/count/identity, "
        "item vtable/id/family/empty slot/readable through +0x12c, MissileData kind filter by id and "
        "retained text, mixed groups, agreement, 2.5/3.0 bounds, item present, identity failures, "
        "registry change -> DEFER, public entry points silent off-image)");
    return 0;
}
