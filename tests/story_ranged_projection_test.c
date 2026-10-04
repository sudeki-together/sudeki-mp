/* Exercise the real read-only adapter on synthetic owned storage. No native
 * method is invoked; this is NOT an executable-installation/gameplay test. */
#include "../src/hooks/lan_story_world.c"
#include <assert.h>
#include <stdio.h>

typedef struct TestRenderer {
    uint8_t wrapper[0x14],object[0xd0],renderer[0xb0],bank[0x5c],description[0x24];
    uint8_t entries[3*28],resources[3][0x20],channels[5*36],blends[4*20],rows[5][24];
} TestRenderer;
typedef struct Fixture {
    uint8_t actor[0x200],model[0x168],position[0x108],arbiter[0x64];
    uint8_t manager[0x64],missile_record[0xc4];
    uint8_t table[0x414],states[20],definitions[4][0x28];
    TestRenderer body,arms;
} Fixture;
static void pointer(uint8_t *p,unsigned offset,void *v) { memcpy(p+offset,&v,4); }
static void word(uint8_t *p,unsigned offset,uint32_t v) { memcpy(p+offset,&v,4); }
static void number(uint8_t *p,unsigned offset,float v) { memcpy(p+offset,&v,4); }
static void renderer(TestRenderer *r,uint32_t first,uint32_t second) {
    pointer(r->wrapper,8,r->object); pointer(r->wrapper,0x10,r->renderer);
    pointer(r->object,0x14,r->renderer); pointer(r->renderer,0,base+0x2df8ec);
    pointer(r->renderer,8,r->bank); pointer(r->bank,0x1c,r->description);
    pointer(r->bank,0x20,r->entries); word(r->description,0,3); word(r->description,0xc,1);
    pointer(r->renderer,0x98,r->channels); pointer(r->renderer,0x9c,r->blends);
    word(r->renderer,0xa0,5); word(r->renderer,0xa4,4);
    for(unsigned i=0;i<3;++i) pointer(r->entries,i*28,r->resources[i]);
    word(r->resources[1],0,first); word(r->resources[2],0,second);
    number(r->resources[1],4,60); number(r->resources[2],4,60);
    for(unsigned c=0;c<5;++c) pointer(r->channels,c*36,r->rows[c]);
    static const uint32_t topology[]={0x00010000,0x00030002,0x80018000,0x00048002};
    for(unsigned i=0;i<4;++i) word(r->blends,i*20,topology[i]);
}
int main(void) {
    base=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    assert(base);
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        pointer(base,0x2df8ec+identities[i].slot,base+identities[i].rva);
    pointer(base,0x2df8ec+0x184,base+0x21bf50);
    pointer(base,0x2d65e4+0xc,base+0x3ae30);
    Fixture *f=calloc(1,sizeof(*f)),*before=malloc(sizeof(*f)); assert(f && before);
    renderer(&f->body,0x11111111,0x22222222);
    renderer(&f->arms,0x33333333,0x44444444);
    number(f->body.resources[1],4,55);
    pointer(f->actor,0x134,f->model); pointer(f->actor,0x90,f->arbiter);
    pointer(f->model,0x160,f->arms.wrapper); pointer(f->model,0x164,f->body.wrapper);
    pointer(f->model,0xdc,f->table); pointer(f->model,0xf8,f->states);
    pointer(f->position,0xb4,f->arms.wrapper); number(f->position,0x58,1);
    pointer(f->arbiter,0,base+0x2cc9ac); pointer(f->arbiter,0x10,f->actor);
    word(f->arbiter,0x50,0x400000);
    f->states[2]=5;
    pointer(f->table,0x14+5*4,f->definitions[0]); word(f->definitions[0],0x14,0x33333333);
    pointer(f->table,0x14+2*4,f->definitions[1]); word(f->definitions[1],0x14,0x11111111);
    word(f->definitions[1],0x20,0x22222222); /* must not choose exploration */
    word(f->arms.rows[0],0,1); number(f->arms.rows[0],4,12);
    number(f->arms.rows[0],8,30); number(f->arms.rows[0],0xc,30);
    for(unsigned c=1;c<5;++c) word(f->arms.rows[c],0,192u<<16);
    Target t={.entity=f->actor,.model=f->model,.position=f->position,
        .wrapper=f->body.wrapper,.attached_wrapper=f->arms.wrapper,.character=3,
        .kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND,.identifier=0x8557d453};
    assert(renderer_target(t,&t,FALSE));
    SudekiMpLanStoryWorldActor out;
    memcpy(before,f,sizeof(*f));
    assert(read_pose(&t,&out));
    assert(out.clip[0]==0x11111111 && out.state[0]==0 && out.time[0]==0);
    assert(out.rate[0]==12 && out.bank_fingerprint==t.fingerprint);
    for(unsigned c=1;c<5;++c) assert(!out.clip[c] && out.state[c]==192 && !out.time[c]);
    assert(!memcmp(before,f,sizeof(*f))); /* no host-side renderer writes */
    /* Repeated native FP channel2 -> channel0 promotion must NEVER replace
     * the body base. Its independent clock survives the next shot's reset. */
    pointer(f->table,0x14+0x8c*4,f->definitions[2]);
    pointer(f->table,0x14+0x85*4,f->definitions[3]);
    word(f->definitions[2],0x14,0x44444444); word(f->definitions[3],0x14,0x22222222);
    pointer(f->actor,0xbc,f->manager); pointer(f->manager,0,base+0x2d4c8c);
    pointer(f->manager,0x10,f->actor); pointer(f->manager,0x60,f->missile_record);
    word(f->missile_record,0x98,0x85); word(f->missile_record,0x9c,0x8c);
    number(f->arms.resources[2],4,4); number(f->body.resources[2],4,4);
    f->states[10]=0x8c; word(f->arms.rows[2],0,2|(1u<<16));
    number(f->arms.blends,2*20+0xc,.4f);
    BOOL stable=FALSE; assert(read_pose_detail(&t,&out,&stable) && stable);
    assert(out.clip[0]==0x11111111 && out.clip[2]==0 && out.clip[4]==0x22222222);
    assert(out.state[4]==1 && out.time[4]==0 && out.blend[2]==0 && fabsf(out.blend[3]-.4f)<.0001f);
    word(f->missile_record,0x9c,0x8d); assert(!read_pose_detail(&t,&out,&stable));
    word(f->missile_record,0x9c,0x8c);
    word(f->missile_record,0x98,0x87); assert(!read_pose_detail(&t,&out,&stable));
    pointer(f->table,0x14+0x87*4,f->definitions[3]);
    assert(read_pose_detail(&t,&out,&stable) && out.clip[4]==0x22222222);
    word(f->missile_record,0x98,0x85);
    assert(read_pose_detail(&t,&out,&stable));
    Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000};
    prior.previous.time[0]=10;
    assert(retain_ranged_base(&t,&prior,1100,&out) && fabsf(out.time[0]-11.2f)<.001f);
    f->states[2]=0x8c; f->states[10]=0;
    word(f->arms.rows[0],0,2|(1u<<16)); number(f->arms.rows[0],8,2);
    number(f->arms.rows[0],0xc,2); number(f->arms.rows[0],4,24);
    word(f->arms.rows[2],0,192u<<16); number(f->arms.blends,2*20+0xc,0);
    assert(read_pose_detail(&t,&out,&stable) && stable);
    assert(out.clip[0]==0x11111111 && out.clip[4]==0x22222222 && out.time[4]==2);
    assert(out.blend[3]==1 && out.rate[0]==12);
    assert(retain_ranged_base(&t,&prior,1200,&out) && fabsf(out.time[0]-12.4f)<.001f);
    prior.previous.time[0]=54.5f;
    assert(retain_ranged_base(&t,&prior,1100,&out) && fabsf(out.time[0]-.7f)<.001f);
    out.time[0]=0; assert(retain_ranged_base(&t,&prior,1300,&out) && out.time[0]==0);
    prior.target.entity=NULL;
    assert(retain_ranged_base(&t,&prior,1100,&out) && out.time[0]==0);
    memcpy(f,before,sizeof(*f));
    word(f->definitions[0],0x14,0x77777777); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    word(f->definitions[1],0x14,0x77777777); word(f->definitions[1],0x20,0x7ffff);
    assert(!read_pose(&t,&out)); /* no raw FP selector fallback */
    memcpy(f,before,sizeof(*f));
    word(f->arms.rows[0],0,0); assert(!read_pose(&t,&out)); /* no empty body */
    memcpy(f,before,sizeof(*f));
    number(f->arms.rows[0],0xc,61); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    word(f->arbiter,0x50,0); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    pointer(f->position,0xb4,f->body.wrapper); assert(!read_pose(&t,&out));
    /* Ordinary attached world pose remains unchanged by this bridge. */
    t.attached_wrapper=t.wrapper;
    word(f->body.rows[0],0,1); number(f->body.rows[0],4,12);
    number(f->body.rows[0],0xc,10);
    assert(read_pose(&t,&out) && out.clip[0]==0x11111111 && out.time[0]==10);
    free(before); free(f); VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("story ranged projection adapter tests passed"); return 0;
}
