/* Real catalog, capture and client preflight on synthetic native storage.
 * No game constructors, native setters, rendering or gameplay execute. */
#include "hooks/lan_story_world.h"
unsigned SudekiMpLanStoryWorldOwnedRenderers(void **out,unsigned max) __attribute__((externally_visible));
uint32_t SudekiMpLanStoryWorldOwnershipSerial(void) __attribute__((externally_visible));
#include "../src/hooks/lan_story_world.c"

/* Separate production codec units have private helper names. */
#define name_valid capacity_0_name_valid
#define exterior_occupied capacity_0_exterior_occupied
#define put32 capacity_0_put32
#define get32 capacity_0_get32
#define name capacity_0_name
#include "../src/network/lan_party_story.c"
#undef name_valid
#undef exterior_occupied
#undef put32
#undef get32
#undef name

/* Separate production codec units have private helper names. */
#define count_actors capacity_1_count_actors
#define coordinate capacity_1_coordinate
#define empty_shot capacity_1_empty_shot
#define absent_actor capacity_1_absent_actor
#define get16 capacity_1_get16
#define get32 capacity_1_get32
#define put16 capacity_1_put16
#define put32 capacity_1_put32
#define get_float capacity_1_get_float
#define put_float capacity_1_put_float
#define write_actor capacity_1_write_actor
#define read_actor capacity_1_read_actor
#define lerp capacity_1_lerp
#define basis_dot capacity_1_basis_dot
#define basis_cross capacity_1_basis_cross
#define basis_normalize capacity_1_basis_normalize
#define view_interpolate capacity_1_view_interpolate
#define actor_interpolate capacity_1_actor_interpolate
#define actor_types capacity_1_actor_types
#define p capacity_1_p
#include "../src/network/lan_story_frame.c"
#undef count_actors
#undef coordinate
#undef empty_shot
#undef absent_actor
#undef get16
#undef get32
#undef put16
#undef put32
#undef get_float
#undef put_float
#undef write_actor
#undef read_actor
#undef lerp
#undef basis_dot
#undef basis_cross
#undef basis_normalize
#undef view_interpolate
#undef actor_interpolate
#undef actor_types
#undef p

/* Separate production codec units have private helper names. */
#define get16 capacity_2_get16
#define get32 capacity_2_get32
#define get64 capacity_2_get64
#define put16 capacity_2_put16
#define put32 capacity_2_put32
#define put64 capacity_2_put64
#define get_float capacity_2_get_float
#define put_float capacity_2_put_float
#define bounded capacity_2_bounded
#define channel_state capacity_2_channel_state
#define before capacity_2_before
#define chunk_count capacity_2_chunk_count
#define write_actor capacity_2_write_actor
#define read_actor capacity_2_read_actor
#define lerp capacity_2_lerp
#define p capacity_2_p
#include "../src/network/lan_story_world_frame.c"
#undef get16
#undef get32
#undef get64
#undef put16
#undef put32
#undef put64
#undef get_float
#undef put_float
#undef bounded
#undef channel_state
#undef before
#undef chunk_count
#undef write_actor
#undef read_actor
#undef lerp
#undef p

/* Separate production codec units have private helper names. */
#define bank_for capacity_3_bank_for
#define combat_bank capacity_3_combat_bank
#define banks capacity_3_banks
#define tal_combat_selectors capacity_3_tal_combat_selectors
#include "../src/network/lan_party_motion.c"
#undef bank_for
#undef combat_bank
#undef banks
#undef tal_combat_selectors

/* Separate production codec units have private helper names. */
#define write_u16 capacity_4_write_u16
#define write_u32 capacity_4_write_u32
#define write_u64 capacity_4_write_u64
#define read_u16 capacity_4_read_u16
#define read_u32 capacity_4_read_u32
#define read_u64 capacity_4_read_u64
#define write_s16 capacity_4_write_s16
#define read_s16 capacity_4_read_s16
#define write_float capacity_4_write_float
#define read_float capacity_4_read_float
#define write_s32 capacity_4_write_s32
#define read_s32 capacity_4_read_s32
#define valid_role capacity_4_valid_role
#define valid_simulation_node_role capacity_4_valid_simulation_node_role
#define valid_coordinate capacity_4_valid_coordinate
#define valid_input_aim capacity_4_valid_input_aim
#define valid_actor_type capacity_4_valid_actor_type
#define codec_roster_valid capacity_4_codec_roster_valid
#define ranged_actor_type capacity_4_ranged_actor_type
#define valid_actor_action_pair capacity_4_valid_actor_action_pair
#define action_sequence_newer capacity_4_action_sequence_newer
#define elco_weapon_item capacity_4_elco_weapon_item
#define ranged_weapon_item capacity_4_ranged_weapon_item
#define extended_actor_action_allowed capacity_4_extended_actor_action_allowed
#define valid_actor_action_history capacity_4_valid_actor_action_history
#define buki_spirit_presentation_valid capacity_4_buki_spirit_presentation_valid
#define locomotion_selector_table capacity_4_locomotion_selector_table
#define locomotion_state_index capacity_4_locomotion_state_index
#define write_locomotion capacity_4_write_locomotion
#define read_locomotion capacity_4_read_locomotion
#define valid_actor_snapshot capacity_4_valid_actor_snapshot
#define spirit_vfx_entry_empty capacity_4_spirit_vfx_entry_empty
#define spirit_vfx_roster_valid capacity_4_spirit_vfx_roster_valid
#define write_spirit_vfx capacity_4_write_spirit_vfx
#define read_spirit_vfx capacity_4_read_spirit_vfx
#define write_actor capacity_4_write_actor
#define read_actor capacity_4_read_actor
#define encode_payload capacity_4_encode_payload
#define lan_magic capacity_4_lan_magic
#define ailish_locomotion_selectors capacity_4_ailish_locomotion_selectors
#define elco_locomotion_selectors capacity_4_elco_locomotion_selectors
#define buki_locomotion_selectors capacity_4_buki_locomotion_selectors
#define locomotion_ids capacity_4_locomotion_ids
#define locomotion_states capacity_4_locomotion_states
#include "../src/network/lan_arena_protocol.c"
#undef write_u16
#undef write_u32
#undef write_u64
#undef read_u16
#undef read_u32
#undef read_u64
#undef write_s16
#undef read_s16
#undef write_float
#undef read_float
#undef write_s32
#undef read_s32
#undef valid_role
#undef valid_simulation_node_role
#undef valid_coordinate
#undef valid_input_aim
#undef valid_actor_type
#undef codec_roster_valid
#undef ranged_actor_type
#undef valid_actor_action_pair
#undef action_sequence_newer
#undef elco_weapon_item
#undef ranged_weapon_item
#undef extended_actor_action_allowed
#undef valid_actor_action_history
#undef buki_spirit_presentation_valid
#undef locomotion_selector_table
#undef locomotion_state_index
#undef write_locomotion
#undef read_locomotion
#undef valid_actor_snapshot
#undef spirit_vfx_entry_empty
#undef spirit_vfx_roster_valid
#undef write_spirit_vfx
#undef read_spirit_vfx
#undef write_actor
#undef read_actor
#undef encode_payload
#undef lan_magic
#undef ailish_locomotion_selectors
#undef elco_locomotion_selectors
#undef buki_locomotion_selectors
#undef locomotion_ids
#undef locomotion_states

/* Separate production codec units have private helper names. */
#define transition_for_variant capacity_5_transition_for_variant
#define buki_transition_for_variant capacity_5_buki_transition_for_variant
#define tal_combo_transitions capacity_5_tal_combo_transitions
#define buki_combo_transitions capacity_5_buki_combo_transitions
#include "../src/network/lan_arena_tal_combo_graph.c"
#undef transition_for_variant
#undef buki_transition_for_variant
#undef tal_combo_transitions
#undef buki_combo_transitions

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static unsigned omitted_log,ambiguous_log,target_log,hp_full_log,refusal_log,budget_log,prepared_log;
static char omission_text[128][768],refusal_text[768];
static uint16_t last_target_kind;
static char last_diagnostic[512],last_target_diagnostic[512],last_omitted_diagnostic[512];
void __attribute__((externally_visible)) SudekiMpLogFormat(const char *format,...) {
    va_list args;va_start(args,format);vsnprintf(last_diagnostic,sizeof(last_diagnostic),format,args);va_end(args);
    if(strstr(format,"event=world_omitted")) {++omitted_log;memcpy(last_omitted_diagnostic,last_diagnostic,sizeof(last_omitted_diagnostic));}
    if(strstr(format,"event=combatant_hp_full"))++hp_full_log;
    if(strstr(format,"event=ambiguous_scenery"))++ambiguous_log;
    if(strstr(format,"event=refused operation=")) {++refusal_log;strcpy(refusal_text,last_diagnostic);}
    if(strstr(format,"event=entity_omitted_budget")) ++budget_log;
    if(strstr(format,"event=clip_resource_prepared")) ++prepared_log;
    if(strstr(format,"stage=target_omitted")) {assert(target_log<128);strcpy(omission_text[target_log],last_diagnostic);++target_log;last_target_kind=stage_kind;memcpy(last_target_diagnostic,last_diagnostic,sizeof(last_target_diagnostic));}
}
BOOL __attribute__((externally_visible)) SudekiMpLogResearchEnabled(void) {return FALSE;}
static void pointer(void *p,unsigned o,const void *v) {memcpy((uint8_t *)p+o,&v,4);}
static void word(void *p,unsigned o,uint32_t v) {memcpy((uint8_t *)p+o,&v,4);}
static void number(void *p,unsigned o,float v) {memcpy((uint8_t *)p+o,&v,4);}
typedef struct Entity {
    uint8_t actor[0x800],model[0x168],wrapper[0x14],object[0xd0],renderer[0xb0];
    uint8_t bank[0x80],description[0x24],entries[28],resource[0x20];
    uint8_t channels[5*36],rows[5][24],blends[4*20],name[8],combat[0x74],stats[0x30];
} Entity;
static Entity entities[160];static void *registry_entries[160];static uint8_t registry_owner[0x40];
static SudekiMpLanStoryNativeRoster roster;
static SudekiMpLanStoryFrame party;
static SudekiMpLanStoryScene scene;
static uint32_t serial=1;
static void *death_failure;
static int late_corruption=-1,recheck_change=-1;
static BOOL load_fixture;static unsigned inspect_calls,acquire_calls;
static uint8_t load_package[0x40];
void test_ambient_capacity(void *late);
void test_ambient_rebind(unsigned phase,void *a,void *b);
BOOL SudekiMpLanStoryObserverRoster(void *controller,const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *s,SudekiMpLanStoryNativeRoster *out) {
    (void)controller;(void)w;assert(s==&scene);*out=roster;return TRUE;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {(void)w;if(recheck_change>=0) {word(entities[recheck_change].resource,0,124);recheck_change=-1;}return !memcmp(r,&roster,sizeof(*r));}
unsigned SudekiMpLanPartyLocalSeat(SudekiMpLanPartySession *s) {(void)s;return 0;}
BOOL SudekiMpLanStorySpawnIdentity(const void *e,uint32_t *id) {*id=*(const uint32_t *)((const uint8_t *)e+0x34);return TRUE;}
BOOL SudekiMpLanStoryGateIdentity(const void *e,uint32_t *id) {(void)e;(void)id;return FALSE;}
BOOL SudekiMpLanStorySpawnDeathState(const void *e,BOOL *dead,unsigned *emitted) {if(late_corruption>=0) {assert((*(const uint32_t *)((const uint8_t *)e+0x30)&0x1fff)==SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND);word(entities[late_corruption].rows[0],0,(192u<<16)|2u);late_corruption=-1;}*dead=FALSE;*emitted=0;return e!=death_failure;}
BOOL SudekiMpLanStoryRemoteInteriorHeldPose(unsigned c,const void *e,uint32_t epoch,float out[3]) {(void)c;(void)e;(void)epoch;(void)out;return FALSE;}
void SudekiMpLanStoryResearch54Monster(unsigned side,uint32_t id,const float position[3]) {(void)side;(void)id;(void)position;}
void SudekiMpLanStoryResearch54Name(const void *e,char out[16]) {(void)e;out[0]=0;}
void SudekiMpLanStoryAnimTraceWatch(const void *renderer) {(void)renderer;}
BOOL SudekiMpLanStoryMaterialOwnerExact(HMODULE image,const void *child,const void *resource) {(void)image;(void)child;(void)resource;assert(0);return FALSE;}
BOOL SudekiMpLanStoryCurveExact(HMODULE image,const void *curve,SudekiMpLanStoryCurveKind kind,unsigned n) {(void)image;(void)curve;(void)kind;(void)n;assert(0);return FALSE;}
static BOOL exact(const SudekiMpLanStoryNativeRoster *r,void *context) {(void)context;return r==&roster;}
static void entity(unsigned index,unsigned kind,uint32_t id,float x,unsigned character) {
    Entity *e=&entities[index];memset(e,0,sizeof(*e));registry_entries[index]=e->actor;
    uint32_t primary,update,resource;
    unsigned pos=0x150,channels=4;
    switch(kind) {
    case SUDEKIMP_LAN_STORY_WORLD_NPC_KIND:primary=0x2d65a0;update=0x2d65c4;resource=0x2d65e4;break;
    case SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND:primary=0x2d5b00;update=0x2d5b24;resource=0x2d5b44;pos=0x210;channels=1;break;
    case SUDEKIMP_LAN_STORY_WORLD_DOOR_KIND:primary=0x2d6430;update=0x2d6454;resource=0x2d6474;break;
    case SUDEKIMP_LAN_STORY_WORLD_CHEST_KIND:primary=0x2d5200;update=0x2d5224;resource=0x2d5244;break;
    case SUDEKIMP_LAN_STORY_WORLD_MONSTER_KIND:primary=0x2d5fa0;update=0x2d5fc4;resource=0;break;
    case SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND:primary=SUDEKIMP_STORY_ALLY_VTABLE;update=0x2d55f8;resource=0;break;
    default: {
        static const unsigned vt[4][3]={{0x2d5a88,0x2d5aac,0x2d5acc},{0x2d66fc,0x2d6720,0x2d6740},{0x2d5010,0x2d5034,0x2d5054},{0x2d555c,0x2d5580,0x2d55a0}};
        primary=vt[character][0];update=vt[character][1];resource=vt[character][2];pos=0x300;channels=character==1 || character==3?5:4;
        pointer(e->actor,0x130,e->model);pointer(e->model,0,base+(channels==5?0x2d5464:0x2c8504));pointer(e->model,0x10,e->actor);
        pointer(e->model,0x164,e->wrapper);roster.actors[character]=e->actor;
        static const char *names[]={"PC_BUKI","PC_ELCO","PC_TAL","PC_AILISH"};pointer(e->name,4,names[character]);
        break;
    }}
    pointer(e->actor,0,base+primary);pointer(e->actor,8,base+update);pointer(e->actor,0x2c,base+resource);
    word(e->actor,0x30,character<4?0xf80:kind);word(e->actor,0x34,id);word(e->name,0,1);
    if(character>=4)pointer(e->name,4,"fixture");
    pointer(e->actor,0x38,e->name);
    uint8_t *p=e->actor+pos;pointer(e->actor,0x44,p);pointer(p,0,base+0x2cdefc);pointer(p,0x10,e->actor);
    number(p,0x18,x);number(p,0x58,1);pointer(p,0xb4,e->wrapper);
    pointer(e->wrapper,8,e->object);pointer(e->wrapper,0x10,e->renderer);pointer(e->object,0x14,e->renderer);
    pointer(e->renderer,0,base+0x2df8ec);pointer(e->renderer,8,e->bank);
    pointer(e->bank,0x1c,e->description);pointer(e->bank,0x20,e->entries);word(e->description,0,1);word(e->description,0xc,1);
    pointer(e->entries,0,e->resource);word(e->resource,0,123);number(e->resource,4,60);
    pointer(e->renderer,0x98,e->channels);pointer(e->renderer,0x9c,channels==1?NULL:e->blends);word(e->renderer,0xa0,channels);word(e->renderer,0xa4,channels-1);
    for(unsigned c=0;c<channels;++c) {pointer(e->channels,c*36,e->rows[c]);word(e->rows[c],0,192u<<16);}
    static const uint32_t topology[]={0x00010000,0x00030002,0x80018000,0x00048002};
    for(unsigned c=0;c+1<channels;++c)word(e->blends,c*20,topology[c]);
    pointer(e->actor,0x4c,e->stats);pointer(e->stats,0,base+0x2cc064);pointer(e->stats,0x10,e->actor);number(e->stats,0x2c,100);
    pointer(e->actor,0xa4,e->combat);pointer(e->combat,0,base+0x2c8754);pointer(e->combat,0x10,e->actor);number(e->combat,0x18,100);
}
static void setup(void) {
    diagnostic_count=0;last_stage=NULL;last_identifier=last_detail=last_error=0;
    refusal_log=budget_log=prepared_log=0;memset(omission_text,0,sizeof(omission_text));refusal_text[0]=0;
    death_failure=NULL;late_corruption=recheck_change=-1;load_fixture=FALSE;inspect_calls=acquire_calls=0;
    resource_fault=FALSE;unsupported_logs=last_unsupported=0;
    pointer(base,0x2d5b44+0xc,base+0x3ae30);
    memset(&roster,0,sizeof(roster));roster.epoch=roster.revision=1;roster.available_mask=4;roster.leader_character=2;roster.world=registry_owner;
    memset(host_bound,0,sizeof(host_bound));memset(client_bound,0,sizeof(client_bound));host_seen=client_seen=FALSE;host_count=client_count=0;host_epoch=host_sequence=next_generation=0;active=FALSE;native_thread=0;
    memset(monster_hp,0,sizeof(monster_hp));monster_hp_full_logged=FALSE;hp_full_log=0;
    memset(omitted_namespace,0,sizeof(omitted_namespace));omitted_log=ambiguous_log=target_log=0;ambiguous_logs=0;
    for(unsigned i=0;i<140;++i)entity(i,i%2?SUDEKIMP_LAN_STORY_WORLD_NPC_KIND:SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND,1000+i,200-(float)i,4);
    for(unsigned i=140;i<144;++i)entity(i,i<142?SUDEKIMP_LAN_STORY_WORLD_DOOR_KIND:SUDEKIMP_LAN_STORY_WORLD_CHEST_KIND,1000+i,500,4);
    for(unsigned i=144;i<148;++i)entity(i,i<146?SUDEKIMP_LAN_STORY_WORLD_MONSTER_KIND:SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND,1000+i,500,4);
    entity(148,SUDEKIMP_LAN_STORY_WORLD_NPC_KIND,1001,1,4); /* pair spans capacity boundary */
    entity(149,SUDEKIMP_LAN_STORY_WORLD_PC_KIND,SudekiMpLanStoryWorldCharacterIdentifier(2),0,2);
    pointer(entities[2].actor+0x210,0xb4,NULL);pointer(entities[4].actor+0x210,0xb4,NULL);
    number(entities[2].actor+0x210,0x18,1); /* wrapperless but within the distance boundary */
    number(entities[3].actor+0x150,0x18,1000);number(entities[5].actor+0x150,0x18,1100);
    word(registry_owner,0x34,150);pointer(registry_owner,0x3c,registry_entries);pointer(base,0x409d8c,registry_owner);
    scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.observed_tick=100,.phase=SUDEKIMP_LAN_STORY_READY,.available_mask=4,.leader_seat=2,.world="fixture"};
    party=(SudekiMpLanStoryFrame){.epoch=1,.revision=1,.sequence=serial++,.host_tick=100,.available_mask=4,.leader_character=2};
    party.actors[2]=(SudekiMpLanStoryActor){.generation=1,.native_pose=1,.character=2,.facing_z=1,.hp=100};
}
static BOOL capture(SudekiMpLanStoryWorldFrame *out) {party.sequence=serial++;party.host_tick+=20;return SudekiMpLanStoryWorldCapture((void *)1,NULL,NULL,&scene,&party,out);}
static int find(const SudekiMpLanStoryWorldFrame *f,uint32_t id) {for(unsigned i=0;i<f->count;++i)if(f->actors[i].identifier==id)return (int)i;return -1;}

static unsigned placements;
static void __attribute__((fastcall)) place_position(void *p,const float *xyz) {
    memcpy((uint8_t *)p+0x18,xyz,12);++placements;
}
static void __attribute__((naked,noinline)) place_forward(void) {
    __asm__ volatile("movl 0(%ecx),%eax; movl %eax,0x50(%esi); movl 4(%ecx),%eax; movl %eax,0x54(%esi); movl 8(%ecx),%eax; movl %eax,0x58(%esi); ret");
}
static const float *__attribute__((thiscall)) matrix(void *position) {
    uint8_t *wrapper=*(uint8_t **)((uint8_t *)position+0xb4);
    uint8_t *object=*(uint8_t **)(wrapper+8);float *m=(float *)(object+0x90);
    memcpy(m+12,(uint8_t *)position+0x18,12);memcpy(m+8,(uint8_t *)position+0x50,12);return m;
}
BOOL SudekiMpLanStoryRevealMarkersHide(const float position[3]) {(void)position;assert(0);return FALSE;}
static void apply(const SudekiMpLanStoryWorldFrame *frame) {
    set_position=place_position;world_set_forward=place_forward;position_matrix=matrix;placements=0;
    BOOL ok=SudekiMpLanStoryWorldPrepare(&roster,frame,exact,NULL,FALSE);
    if(!ok)fprintf(stderr,"prepare stage=%s id=%x detail=%u error=%lu\n",stage,stage_identifier,stage_detail,GetLastError());
    assert(ok && prepared.count==frame->count);
    ok=SudekiMpLanStoryWorldApplyPrepared(&roster,frame,exact,NULL);
    if(!ok)fprintf(stderr,"apply stage=%s id=%x detail=%u error=%lu\n",stage,stage_identifier,stage_detail,GetLastError());
    assert(ok && placements==frame->count && client_count==frame->count);
}
static BOOL owned_renderer(void *renderer) {
    void *out[MAX_REGISTRY+8];unsigned n=SudekiMpLanStoryWorldOwnedRenderers(out,MAX_REGISTRY+8);
    for(unsigned i=0;i<n;++i)if(out[i]==renderer)return TRUE;return FALSE;
}
static void party_identity_failure(void) {
    setup();word(entities[149].actor,0x30,0x1f81);
    SudekiMpLanStoryWorldFrame f;
    assert(!capture(&f) && stage_kind==0x1f81 && stage_detail==149 && !strcmp(stage,"entity_identity"));
}
static void diagnostics_and_failures(void) {
    setup();pointer(entities[119].renderer,8,(void *)1);pointer(entities[140].renderer,8,(void *)1);
    SudekiMpLanStoryWorldFrame f;assert(capture(&f) && f.count==128 && target_log==2);
    assert(find(&f,1119)<0 && find(&f,1140)<0 && find(&f,1020)>=0);
    assert(capture(&f) && target_log==2 && omitted_log==1);
    assert(strstr(last_target_diagnostic,"cause=bank") && strstr(last_target_diagnostic," detail=119\r") && strstr(last_target_diagnostic,"vtable_rva=002d65a0"));
    ++scene.epoch;++party.epoch;roster.epoch=party.epoch;
    assert(capture(&f) && target_log==4 && omitted_log==2);
    setup();pointer(entities[149].renderer,8,(void *)1);
    assert(!capture(&f) && stage_detail==149 && stage_entity==entities[149].actor && !strcmp(stage,"bank") && refusal_log==1 && target_log==0);
    setup();word(entities[119].actor,0x30,0x0f9a);
    assert(capture(&f) && f.count==128 && find(&f,1119)<0 && target_log==1);
    assert(last_target_kind==0x0f9a);
    assert(strstr(last_target_diagnostic,"cause=entity_identity") && strstr(last_target_diagnostic," kind=0f9a") &&
        strstr(last_target_diagnostic,"wire_kind=0f9b") && strstr(last_target_diagnostic,"vtable_rva=002d65a0") &&
        strstr(last_target_diagnostic," identifier=0000045f") && strstr(last_target_diagnostic," detail=119\r"));
    party_identity_failure();
    setup();registry_entries[0]=registry_entries[149];
    assert(!capture(&f) && !strcmp(stage,"duplicate_entity"));
    setup();registry_entries[7]=(void *)1;
    assert(capture(&f) && target_log==1 && strstr(last_target_diagnostic,"cause=registry_entry") && strstr(last_target_diagnostic," vtable_rva=00000000 kind=0000 identifier=00000000") && strstr(last_target_diagnostic," detail=7\r"));
    char expected_pointer[40];snprintf(expected_pointer,sizeof(expected_pointer)," entity=%p ",(void *)1);assert(strstr(last_target_diagnostic,expected_pointer));
    setup();pointer(entities[0].actor,8,NULL);
    assert(capture(&f) && target_log==1 && strstr(last_target_diagnostic,"cause=entity_identity") && strstr(last_target_diagnostic," vtable_rva=002d5b00 kind=0f8f") && strstr(last_target_diagnostic," identifier=000003e8") && strstr(last_target_diagnostic," detail=0\r"));
    puts("PASS non-party target omissions, refill, party refusal, duplicate/unreadable/identity diagnostics and namespace budgets");
}
static void recruitment_client_view(void) {
    number(entities[149].actor+0x300,0x18,10000);
    for(unsigned i=151;i<=156;++i)entity(i,SUDEKIMP_LAN_STORY_WORLD_NPC_KIND,2000+i,10000,4);
    word(registry_owner,0x34,157);
}
static void recruitment_at_scale(void) {
    setup();entity(139,SUDEKIMP_LAN_STORY_WORLD_NPC_KIND,0x882ab028u,61,4);
    SudekiMpLanStoryWorldFrame before,after;assert(capture(&before) && before.count==128);
    host_seen=FALSE;apply(&before);
    /* 150 is reserved for Ailish and not yet in this registry. */
    recruitment_client_view();registry_entries[150]=entities[4].actor;
    uint32_t before_serial=SudekiMpLanStoryWorldOwnershipSerial();
    assert(SudekiMpLanStoryWorldBeginRecruitment(&roster,entities[139].actor,1,exact,NULL));
    assert(client_count==127 && recruitment.retained && SudekiMpLanStoryWorldOwnershipSerial()==before_serial+1u);
    /* The native recruit replaces the NPC with a distinct playable actor. */
    memmove(registry_entries+139,registry_entries+140,10*sizeof(void *));
    entity(150,SUDEKIMP_LAN_STORY_WORLD_PC_KIND,SudekiMpLanStoryWorldCharacterIdentifier(3),2,3);
    registry_entries[149]=entities[150].actor;
    scene.available_mask=roster.available_mask=party.available_mask=12;
    scene.epoch=roster.epoch=party.epoch=2;
    party.actors[3]=(SudekiMpLanStoryActor){.generation=2,.native_pose=1,.character=3,.facing_z=1,.hp=100,.x=2};
    number(entities[149].actor+0x300,0x18,0);word(registry_owner,0x34,150);
    client_seen=FALSE;assert(capture(&after) && after.count==128);client_seen=TRUE;host_seen=FALSE;
    recruitment_client_view();registry_entries[150]=entities[4].actor;
    before_serial=SudekiMpLanStoryWorldOwnershipSerial();
    assert(SudekiMpLanStoryWorldFinishRecruitment(&roster,&after,2,exact,NULL));
    assert(client_count==128 && !recruitment.retained && SudekiMpLanStoryWorldOwnershipSerial()==before_serial+1u);
    puts("PASS bound-set BeginRecruitment and frame-set FinishRecruitment with 127 survivors");
}
static void ownership_rebind(void) {
    setup();number(entities[20].actor+0x210,0x18,1000);
    SudekiMpLanStoryWorldFrame a,b;
    assert(capture(&a) && find(&a,1021)>=0 && find(&a,1019)<0);
    number(entities[21].actor+0x150,0x18,190);
    assert(capture(&b) && find(&b,1019)>=0 && find(&b,1021)<0);
    host_seen=FALSE;apply(&a);
    test_ambient_rebind(0,entities[21].renderer,entities[19].renderer);
    uint32_t before_serial=SudekiMpLanStoryWorldOwnershipSerial();
    apply(&b);assert(SudekiMpLanStoryWorldOwnershipSerial()==before_serial+1u);
    test_ambient_rebind(1,entities[21].renderer,entities[19].renderer);
    puts("PASS immediate ownership rebind without advancing the ambient clock or frame");
}
static void combatant_capacity(void) {
    setup();
    for(unsigned i=0;i<72;++i)entity(i,SUDEKIMP_LAN_STORY_WORLD_MONSTER_KIND,3000+i,(float)i,4);
    SudekiMpLanStoryWorldFrame f;assert(capture(&f));host_seen=FALSE;apply(&f);
    for(unsigned i=0;i<72;++i) {
        unsigned k=0;for(;k<MAX_MONSTER_HP && monster_hp[k].identity.identifier!=3000+i;++k) {}
        assert(k<MAX_MONSTER_HP && monster_hp[k].identity.entity==entities[i].actor && monster_hp[k].pending==0);
        int actor=find(&f,3000+i);assert(actor>=0);f.actors[actor].hit_points=90;
    }
    ++f.sequence;f.host_tick+=20;apply(&f);
    for(unsigned i=0;i<72;++i) {
        unsigned k=0;for(;k<MAX_MONSTER_HP && monster_hp[k].identity.identifier!=3000+i;++k) {}
        assert(k<MAX_MONSTER_HP && monster_hp[k].pending>0);
    }
    assert(hp_full_log==0);
    /* Saturated stale slots still report at most once in each epoch. */
    for(unsigned k=0;k<MAX_MONSTER_HP;++k)monster_hp[k].identity=(SudekiMpStoryDamageIdentity){.identifier=9000+k};
    apply(&f);apply(&f);assert(hp_full_log==1);
    ++f.epoch;client_seen=FALSE;apply(&f);assert(hp_full_log==2);
    puts("PASS 72 combatant HP slots and drops, bounded full-table diagnostic");
}
#include "story_world_pose_omit_fixture.h"

static void arrival_world_epoch(void) {
    setup();entity(150,SUDEKIMP_LAN_STORY_WORLD_PC_KIND,SudekiMpLanStoryWorldCharacterIdentifier(3),2,3);
    word(registry_owner,0x34,151);scene.available_mask=roster.available_mask=party.available_mask=12;
    party.actors[3]=(SudekiMpLanStoryActor){.generation=1,.native_pose=1,.character=3,.facing_z=1,.hp=100,.x=2};
    roster.ai[2]=entities[149].stats;roster.ai[3]=entities[150].stats;
    SudekiMpLanStoryWorldFrame pair,solo,back;assert(capture(&pair));host_seen=FALSE;apply(&pair);
    SudekiMpLanStoryNativeRoster full=roster;
    client_seen=FALSE;scene.available_mask=roster.available_mask=party.available_mask=4;
    roster.actors[3]=roster.ai[3]=NULL;memset(&party.actors[3],0,sizeof(party.actors[3]));
    scene.epoch=party.epoch=2;party.actors[2].generation=2;assert(capture(&solo));
    host_seen=FALSE;client_seen=TRUE;
    assert(!SudekiMpLanStoryWorldPrepare(&roster,&solo,exact,NULL,FALSE) && !strcmp(stage,"catalog_identity"));
    roster=full;SudekiMpLanStoryWorldPermitArrivalEpoch(&roster,1,3);
    assert(!arrival_epoch.before); /* cannot skip an epoch */
    SudekiMpLanStoryWorldPermitArrivalEpoch(&roster,1,2);assert(arrival_epoch.after==2);
    roster.available_mask=4;roster.actors[3]=roster.ai[3]=NULL;
    void *old_world=roster.world;roster.world=(void *)1;
    assert(!arrival_epoch_exact(&roster,&solo));roster.world=old_world;
    apply(&solo);assert(client_epoch==2 && !arrival_epoch.before);
    client_seen=FALSE;roster=full;scene.available_mask=party.available_mask=12;
    scene.epoch=party.epoch=3;party.actors[2].generation=3;
    party.actors[3]=(SudekiMpLanStoryActor){.generation=2,.native_pose=1,.character=3,.facing_z=1,.hp=100,.x=2};
    assert(capture(&back));host_seen=FALSE;client_seen=TRUE;
    assert(!SudekiMpLanStoryWorldPrepare(&roster,&back,exact,NULL,FALSE));
    SudekiMpLanStoryWorldPermitArrivalEpoch(&roster,2,3);apply(&back);assert(client_epoch==3);
    puts("PASS named ArrivalWorldEpoch real Prepare/Apply, closed grant, removal and re-add");
}
int main(int argc,char **argv) {
    base=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(base);
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)pointer(base,0x2df8ec+identities[i].slot,base+identities[i].rva);
    pointer(base,0x2df8ec+0x184,base+0x21bf50);pointer(base,0x2d65e4+0xc,base+0x3ae30);pointer(base,0x2d5b44+0xc,base+0x3ae30);
    if(argc>1 && !strcmp(argv[1],"arrival")) {arrival_world_epoch();return 0;}
    if(argc>1 && !strncmp(argv[1],"q2-",3)) {q2_case(argv[1]);return 0;}
    if(argc>1 && !strcmp(argv[1],"party-identity")) {party_identity_failure();return 0;}
    setup();SudekiMpLanStoryWorldFrame first,next,mid;
    BOOL ok=capture(&first);if(!ok)fprintf(stderr,"capture stage=%s id=%x detail=%u error=%lu\n",stage,stage_identifier,stage_detail,GetLastError());assert(ok);
    assert(first.count==128 && omitted_log==1);
    if(argc<2 || strcmp(argv[1],"priority-boundary"))assert(ambiguous_log==1 && find(&first,1001)<0);
    assert(find(&first,1021)>=0 && find(&first,1020)<0); /* displaced slot, independent of duplicate 1001 */
    for(unsigned i=0;i<140;++i)assert((find(&first,1000+i)>=0)==(i>=21));
    assert(target_log==0 && find(&first,1002)<0);
    assert(strstr(last_omitted_diagnostic,"side=host total=146 kept=128"));
    for(unsigned i=140;i<148;++i)assert(find(&first,1000+i)>=0);
    assert(SudekiMpLanStoryWorldFrameValid(&first));
    for(unsigned i=0;i<16;++i) {
        uint8_t bytes[SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE];size_t n;
        SudekiMpLanStoryWorldChunk chunk;
        assert(SudekiMpLanStoryWorldChunkEncode(&first,i,bytes,sizeof(bytes),&n));
        assert(SudekiMpLanStoryWorldChunkDecode(bytes,n,&chunk) && chunk.count==8);
        for(unsigned k=0;k<8;++k)assert(chunk.actors[k].identifier==first.actors[i*8+k].identifier);
    }
    /* A boundary crossing smaller than 5 m must not churn the kept set. */
    number(entities[21].actor+0x150,0x18,180.2f);
    assert(capture(&next) && omitted_log==1 && ambiguous_log==1);
    assert(SudekiMpLanStoryWorldFrameInterpolate(&first,&next,first.host_tick+10,&mid));
    for(unsigned i=0;i<128;++i)assert(first.actors[i].identifier==next.actors[i].identifier);
    number(entities[21].actor+0x150,0x18,186);
    assert(capture(&mid) && find(&mid,1021)<0 && find(&mid,1020)>=0);
    unsigned changed=0;for(unsigned i=0;i<128;++i)if(find(&first,mid.actors[i].identifier)<0)++changed;assert(changed==1);
    test_ambient_capacity(entities[149].renderer);
    host_seen=FALSE;
    number(entities[149].actor+0x300,0x18,10000); /* client lead ranks differently */
    for(unsigned i=150;i<156;++i)entity(i,SUDEKIMP_LAN_STORY_WORLD_NPC_KIND,2000+i,10000,4);
    word(registry_owner,0x34,156);
    apply(&first);
    assert(!owned_renderer(entities[3].renderer) && owned_renderer(entities[31].renderer));
    client_seen=FALSE;assert(owned_renderer(entities[3].renderer));client_seen=TRUE;
    setup();assert(capture(&first));number(entities[21].actor+0x150,0x18,180.2f);
    ++scene.epoch;++party.epoch;++roster.epoch;
    assert(capture(&next) && find(&next,1020)>=0 && find(&next,1021)<0);
    diagnostics_and_failures();recruitment_at_scale();ownership_rebind();combatant_capacity();q2_case("q2-all");
    puts("PASS 150-entry priority, stable selection, codec, client-only exclusion and local idle ownership");return 0;
}

BOOL SudekiMpLanStoryResearch54Enabled(void) {return FALSE;}
BOOL SudekiMpLanStoryMaterialShaderOutputExact(HMODULE image,const void *m,const void *o,unsigned n) {(void)image;(void)m;(void)o;(void)n;assert(0);return FALSE;}
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyInspect(void *r,void *b,unsigned s,SudekiMpLanStoryResidencyReport *out) {(void)out;assert(load_fixture && r==entities[33].renderer && b==entities[33].bank && s==0);++inspect_calls;return SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD;}
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyAcquire(void *r,void *b,unsigned s,SudekiMpLanStoryResidencyExact check,void *context,SudekiMpLanStoryResidencyReport *out) {(void)out;assert(load_fixture && r==entities[33].renderer && b==entities[33].bank && s==0 && check(context));++acquire_calls;number(entities[31].actor+0x150,0x18,NAN);return SUDEKIMP_STORY_RESIDENCY_LOADED;}
