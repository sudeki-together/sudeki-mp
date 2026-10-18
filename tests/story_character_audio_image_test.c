#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hooks/lan_story_character_audio.c"
static BOOL log_enabled;
void SudekiMpLogFormat(const char *format,...) {
    if(!log_enabled)return;
    va_list args;va_start(args,format);vprintf(format,args);va_end(args);
}
static BOOL admitted=TRUE;
static unsigned startup_calls;
BOOL SudekiMpLanArenaSpiritAudioReplayLocalCue(HMODULE image,SudekiMpLanArenaSpiritAudioCue cue) {
    assert(image==(HMODULE)base && cue==SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START);
    ++startup_calls;return TRUE;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *roster) {(void)roster;return admitted;}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(base); memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(base+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(base+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage));
    free(raw); return base;
}


static void ptr(void *p,void *v){memcpy(p,&v,4);}
static void num(void *p,uint32_t v){memcpy(p,&v,4);}
static volatile uint32_t abi[6] __attribute__((used));
static volatile uint32_t stub_handle __attribute__((used))=0x123400,stub_calls __attribute__((used));
static volatile uint32_t emitter_calls __attribute__((used));
static volatile uint32_t spot_abi[6] __attribute__((used)),spot_calls __attribute__((used));
static uint8_t *volatile spot_pool __attribute__((used));
/* EmitterPlay's ABI as CSound::PlayCue uses it: EAX = spot emitter, then
 * (cue, NULL, 1, 0, 0), RET 0x14. Records, bumps the pool generation. */
__attribute__((naked,used,noinline)) static void spot_emitter_stub(void) {
    __asm__ volatile("movl %eax,_spot_abi\n\t"
        "movl 4(%esp),%eax\n\tmovl %eax,_spot_abi+4\n\t"
        "movl 8(%esp),%eax\n\tmovl %eax,_spot_abi+8\n\t"
        "movl 12(%esp),%eax\n\tmovl %eax,_spot_abi+12\n\t"
        "movl 16(%esp),%eax\n\tmovl %eax,_spot_abi+16\n\t"
        "movl 20(%esp),%eax\n\tmovl %eax,_spot_abi+20\n\t"
        "incl _spot_calls\n\tmovl _spot_pool,%eax\n\tincl 4(%eax)\n\txorl %eax,%eax\n\tret $20\n\t");
}
static BOOL playback_exact(void *unused){(void)unused;return admitted;}
__attribute__((naked,used,noinline)) static void native_play_stub(void) {
    __asm__ volatile("movl %ecx,_abi\n\tmovl %esi,_abi+4\n\t"
        "movl 4(%esp),%eax\n\tmovl %eax,_abi+8\n\t"
        "movl 8(%esp),%eax\n\tmovl %eax,_abi+12\n\t"
        "movl 12(%esp),%eax\n\tmovl %eax,_abi+16\n\t"
        "movl 16(%esp),%eax\n\tmovl %eax,_abi+20\n\t"
        "incl _stub_calls\n\tmovl _stub_handle,%eax\n\tmovl %eax,(%esi)\n\tmovl %esi,%eax\n\tret $16\n\t");
}
__attribute__((naked,used,noinline)) static void native_emitter_stub(void) {
    __asm__ volatile("incl _emitter_calls\n\tmovl 8(%esp),%eax\n\t"
        "movl $512,(%eax)\n\tret $20\n\t");
}
/* ---- Client shot audio executor. The planner and the cue resolver are
 * separate units not linked into this target: these doubles script their
 * results so the executor runs against the exact image. ---- */
static SudekiMpStoryShotAudioAction plan_queue[SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS];
static unsigned plan_queued,plan_steps,plan_resets,plan_commits,plan_results[SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS];
void SudekiMpStoryShotAudioPlanReset(SudekiMpStoryShotAudioPlan *plan) {memset(plan,0,sizeof(*plan));++plan_resets;}
unsigned SudekiMpStoryShotAudioPlanStep(SudekiMpStoryShotAudioPlan *plan,const SudekiMpStoryShotAudioInput *input,
    SudekiMpStoryShotAudioAction *out,unsigned max) {
    assert(plan==&shot_plan && input && out && max==SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS);
    unsigned n=plan_queued;memcpy(out,plan_queue,n*sizeof(*out));plan_queued=0;plan_commits=0;++plan_steps;return n;
}
void SudekiMpStoryShotAudioPlanCommit(SudekiMpStoryShotAudioPlan *plan,const SudekiMpStoryShotAudioAction *action,unsigned result) {
    assert(plan==&shot_plan && action && result<=SUDEKIMP_SHOT_AUDIO_DEFER && plan_commits<SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS);
    plan_results[plan_commits++]=result;
}
static BOOL cues_image=TRUE;
static unsigned cues_result=SUDEKIMP_SHOT_CUES_OK,item_calls,world_calls;
static SudekiMpStoryShotCueNames cue_double;
BOOL SudekiMpStoryShotCuesImageMatches(HMODULE image) {assert(image==(HMODULE)base);return cues_image;}
unsigned SudekiMpStoryShotCuesForItem(HMODULE image,unsigned character,unsigned item,SudekiMpStoryShotCueNames *out) {
    assert(image==(HMODULE)base && character<4 && out);(void)item;++item_calls;*out=cue_double;return cues_result;
}
unsigned SudekiMpStoryShotCuesForWorldMissile(HMODULE image,unsigned kind,const float first[3],SudekiMpStoryShotCueNames *out) {
    assert(image==(HMODULE)base && first && out);(void)kind;++world_calls;*out=cue_double;return cues_result;
}
static void names(const char *travel,const char *fire) {
    memset(&cue_double,0,sizeof(cue_double));
    snprintf(cue_double.travel,sizeof(cue_double.travel),"%s",travel);snprintf(cue_double.fire,sizeof(cue_double.fire),"%s",fire);
}
/* Fake native sound manager, cue pool and missiles.xsb, as the client's own
 * sound init leaves them (bank pinned once). */
static uint8_t s_manager[0x15c],s_pool[0x14],s_bank[0x34],s_bank2[0x34],s_sb[0x3c],s_sb2[0x3c],s_entries[16*24];
static uint8_t s_cues[16][0x30],s_attachments[8][0x54],s_att_heads[8][12],s_list_head[12];
static uint8_t s_rows[8*24],s_variants[8*16],s_sounds[8*80],s_categories[28],s_params[24];
static uint8_t s_camera_manager[0x60],s_camera[0xbc],s_camera_state[0xdc],*s_backend;
static char s_bank_name[96]="missiles.xsb",s_bank2_name[96]="spot_sounds.xsb",s_param_name[64]="Volume",s_text[256];
static void *s_banks[2];
static const char *const s_cue_names[]={"Royal_Sceptre_fire","Royal_Sceptre_fire_2D","Royal_Sceptre_projectile",
    "kurianKnight_projectile","kurian_knight_fire_effect","Xurics_Shock_staff","Unknown_wave_cue"};
static const unsigned s_cue_count=sizeof(s_cue_names)/sizeof(s_cue_names[0]);
static void reset_pool(void) {
    memset(s_entries,0,sizeof(s_entries));memset(s_cues,0,sizeof(s_cues));
    num(s_pool+0xc,16);num(s_bank+0x18,1);
}
static void shot_world_setup(void) {
    s_backend=calloc(1,0xafbc);assert(s_backend);
    ptr(s_manager,base+0x2e1dc0);ptr(s_manager+0x10,s_backend);s_banks[0]=s_bank;ptr(s_manager+0x2c,s_banks);num(s_manager+0x30,1);
    ptr(s_manager+0x128,s_categories);*(uint16_t *)(s_manager+0x12c)=1;*(uint16_t *)(s_manager+0x12e)=0;
    ptr(s_backend,base+0x2e1e90);ptr(s_backend+0xafac,s_list_head);ptr(s_list_head,s_list_head);ptr(s_list_head+4,s_list_head);
    ptr(base+0x3c37d8,s_backend);ptr(base+0x409e58,s_manager);ptr(base+0x409e8c,s_pool);
    ptr(s_pool,base+0x2e1ee0);num(s_pool+4,100);num(s_pool+8,16);ptr(s_pool+0x10,s_entries);
    ptr(s_bank,base+0x2e1e40);ptr(s_bank+0x10,s_bank_name);num(s_bank+0x14,SHOT_MISSILES_ID);ptr(s_bank+0x1c,s_sb);
    memcpy(s_bank2,s_bank,sizeof(s_bank2));ptr(s_bank2+0x10,s_bank2_name);num(s_bank2+0x14,bank_name_id(s_bank2_name));
    ptr(s_sb,base+0x2e1f68);num(s_sb+4,1);memcpy(s_sb+8,"SSBN",4);memcpy(s_sb2,s_sb,sizeof(s_sb2));
    unsigned offset=0;
    for(unsigned i=0;i<s_cue_count;++i) {
        num(s_rows+i*24,offset);num(s_rows+i*24+4,1);ptr(s_rows+i*24+0x10,s_variants+i*16);
        num(s_variants+i*16,100+i);num(s_sounds+i*80,i==6?999:100+i); /* cue 6 names a missing wave key */
        memcpy(s_text+offset,s_cue_names[i],strlen(s_cue_names[i])+1);offset+=strlen(s_cue_names[i])+1;
    }
    num(s_sb+0x18,s_cue_count);num(s_sb+0x1c,s_cue_count);num(s_sb+0x2c,sizeof(s_text));ptr(s_sb+0x30,s_text);
    ptr(s_sb+0x34,s_sounds);ptr(s_sb+0x38,s_rows);
    num(base+0x3c37d4,1);ptr(base+0x3c2374,s_params);ptr(s_params+20,s_param_name);
    ptr(s_camera_manager,base+0x2c7b80);ptr(s_camera_manager+0x20,s_camera);ptr(s_camera_manager+0x24,s_camera);
    ptr(s_camera,base+0x2cce5c);ptr(s_camera+0x34,s_camera_state);ptr(s_camera_state,base+0x2dd638);
    ptr(base+0x409d7c,s_camera_manager);
    reset_pool();
}
static unsigned s_cue_index(const char *name) {
    for(unsigned i=0;i<s_cue_count;++i)if(!strcmp(s_cue_names[i],name))return i;
    assert(!"cue name outside the fake bank");return 0;
}
/* EmitterPlay double: the native order (handle zeroed; lazy 3D attachment
 * registered in a manager cell; CueRegister: state 1, generation, cue, bank
 * reference, one unit). Requires an active emitter that holds the bank. */
static volatile uint32_t play_calls,play_fail,play_attach_then_fail,play_status;
static volatile uint32_t play_args[6];
static const char *volatile played_cue;
static unsigned next_attachment;
static uint32_t __attribute__((cdecl,used,noinline)) play_double(uint8_t *emitter,const char *name,uint32_t *handle,
    uint32_t spatial,uint32_t rows,uint32_t count) {
    play_args[0]=(uint32_t)(uintptr_t)emitter;play_args[1]=(uint32_t)(uintptr_t)name;play_args[2]=(uint32_t)(uintptr_t)handle;
    play_args[3]=spatial;play_args[4]=rows;play_args[5]=count;played_cue=name;++play_calls;
    assert(handle && emitter[0x10]==1 && *(void **)(emitter+4)==s_bank && *(void **)emitter==base+0x2e1db4 && !rows && !count);
    *handle=0;if(play_fail)return 1;
    uint8_t *slot=*(uint8_t **)(emitter+8);
    if((spatial&0xff) && slot && !*(void **)slot) {
        unsigned k=next_attachment++%8u;uint8_t *att=s_attachments[k];memset(att,0,0x54);
        ptr(att,base+0x2e1f34);num(att+4,2);ptr(att+0x48,s_att_heads[k]);
        ptr(s_att_heads[k],s_att_heads[k]);ptr(s_att_heads[k]+4,s_att_heads[k]);
        unsigned cell=0;while(cell<60 && *(void **)(s_manager+0x38+4*cell))++cell;assert(cell<60);
        ptr(slot,att);ptr(s_manager+0x38+4*cell,slot);num(s_manager+0x130,u32(s_manager+0x130)+1);
    }
    if(play_attach_then_fail)return 1;
    unsigned i=0;while(i<16 && (s_entries[i*24]&63))++i;assert(i<16);
    uint8_t *e=s_entries+i*24,*cue=s_cues[i];uint32_t generation=u32(s_pool+4);
    memset(cue,0,0x30);ptr(cue+8,s_sb);num(cue+4,s_cue_index(name));
    num(e,1);num(e+8,generation);ptr(e+0xc,cue);ptr(e+0x10,s_bank);num(e+0x14,1);
    num(s_bank+0x18,u32(s_bank+0x18)+1);num(s_pool+0xc,u32(s_pool+0xc)-1);num(s_pool+4,generation+1);
    *handle=(generation<<8)|i;return play_status;
}
__attribute__((naked,used,noinline)) static void play_double_stub(void) {
    __asm__ volatile("pushl 20(%esp)\n\tpushl 20(%esp)\n\tpushl 20(%esp)\n\tpushl 20(%esp)\n\tpushl 20(%esp)\n\t"
        "pushl %eax\n\tcall _play_double\n\taddl $24,%esp\n\tret $20\n\t");
}
/* Native recycle 0x284840 after the cue-notification: a natural end. */
static void recycle(uint32_t handle) {
    uint8_t *e=s_entries+(handle&255u)*24;assert(e[0]&63);
    e[0]&=0xc0;num(e+8,0);ptr(e+0xc,NULL);ptr(e+0x10,NULL);num(e+0x14,0);
    num(s_bank+0x18,u32(s_bank+0x18)-1);num(s_pool+0xc,u32(s_pool+0xc)+1);
}
static unsigned stop_calls;
static unsigned __attribute__((stdcall)) cue_stop_double(void *soundbank,unsigned which,unsigned zero,void *cue) {
    assert(soundbank==s_sb && which==UINT32_MAX && !zero && cue);++stop_calls;return 0;
}
/* Counting trampolines in front of the retail routines. */
static void *volatile bank_set_real __attribute__((used)),*volatile position_real __attribute__((used));
static void *volatile detach_real __attribute__((used)),*volatile release_real __attribute__((used));
static volatile uint32_t bank_set_calls __attribute__((used)),position_calls __attribute__((used));
static volatile uint32_t detach_calls __attribute__((used)),release_calls __attribute__((used));
static volatile uint32_t release_stub_result __attribute__((used));
__attribute__((naked,used,noinline)) static void bank_set_counter(void) {__asm__ volatile("incl _bank_set_calls\n\tjmp *_bank_set_real\n\t");}
__attribute__((naked,used,noinline)) static void position_counter(void) {__asm__ volatile("incl _position_calls\n\tjmp *_position_real\n\t");}
__attribute__((naked,used,noinline)) static void detach_counter(void) {__asm__ volatile("incl _detach_calls\n\tjmp *_detach_real\n\t");}
__attribute__((naked,used,noinline)) static void release_counter(void) {__asm__ volatile("incl _release_calls\n\tjmp *_release_real\n\t");}
/* A release that answers without touching the table (fault injection). */
__attribute__((naked,used,noinline)) static void release_fault(void) {
    __asm__ volatile("incl _release_calls\n\tmovl _release_stub_result,%eax\n\tret $4\n\t");
}
/* ABI recorders. */
static volatile uint32_t shot_abi[6] __attribute__((used)),detach_abi __attribute__((used));
static volatile uint32_t release_abi[3] __attribute__((used)),bank_set_abi[2] __attribute__((used));
__attribute__((naked,used,noinline)) static void emitter_play_abi_stub(void) {
    __asm__ volatile("movl %eax,_shot_abi\n\t"
        "movl 4(%esp),%eax\n\tmovl %eax,_shot_abi+4\n\tmovl 8(%esp),%eax\n\tmovl %eax,_shot_abi+8\n\t"
        "movl 12(%esp),%eax\n\tmovl %eax,_shot_abi+12\n\tmovl 16(%esp),%eax\n\tmovl %eax,_shot_abi+16\n\t"
        "movl 20(%esp),%eax\n\tmovl %eax,_shot_abi+20\n\tmovl $0x55,%eax\n\tret $20\n\t");
}
__attribute__((naked,used,noinline)) static void detach_abi_stub(void) {
    __asm__ volatile("movl 4(%esp),%eax\n\tmovl %eax,_detach_abi\n\txorl %eax,%eax\n\tret $4\n\t");
}
__attribute__((naked,used,noinline)) static void release_abi_stub(void) {
    __asm__ volatile("movl %edi,_release_abi\n\tmovl %edx,_release_abi+4\n\tmovl 4(%esp),%eax\n\t"
        "movl %eax,_release_abi+8\n\tmovl $0x77,%eax\n\tret $4\n\t");
}
__attribute__((naked,used,noinline)) static void bank_set_abi_stub(void) {
    __asm__ volatile("movl %esi,_bank_set_abi\n\tmovl 4(%esp),%eax\n\tmovl %eax,_bank_set_abi+4\n\t"
        "movl $0x101,%eax\n\tret $4\n\t");
}
static SudekiMpLanStoryNativeRoster shot_roster;
static uint64_t shot_namespace=7;
static unsigned shot_local=2;
static uint32_t shot_clock;
static DWORD WINAPI test_tick(void) {return shot_clock;}
static uint32_t queue_tick=1000;
static void queue(uint8_t type,uint8_t character,uint8_t item,uint8_t local,uint8_t kind,uint32_t key,float x,float y,float z) {
    assert(plan_queued<SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS);
    plan_queue[plan_queued++]=(SudekiMpStoryShotAudioAction){.type=type,.character=character,.item=item,.local=local,
        .vfx_kind=kind,.key=key,.emitted_tick=queue_tick,.position={x,y,z}};
}
static BOOL present(uint32_t now) {
    SudekiMpStoryShotAudioInput in={.now=now,.namespace_token=shot_namespace,.local_character=shot_local,.available_mask=0xf};
    return SudekiMpStoryShotAudioPresent(&shot_roster,&in,playback_exact,NULL);
}
static DWORD WINAPI foreign_thread(LPVOID unused) {
    (void)unused;
    return (present(1)?1u:0u)|(SudekiMpStoryShotAudioStop(playback_exact,NULL)?2u:0u)|
        (SudekiMpStoryShotAudioDrain(playback_exact,NULL)?4u:0u);
}
static unsigned records_in(unsigned state) {
    unsigned n=0;for(unsigned i=0;i<SHOT_RECORDS;++i)if(shot_records[i].state==state)++n;return n;
}
static ShotRecord *record_for(uint32_t key,unsigned role) {
    for(unsigned i=0;i<SHOT_RECORDS;++i) {
        ShotRecord *r=&shot_records[i];
        if(r->state!=SHOT_FREE && r->state!=SHOT_QUARANTINED && r->key==key && r->role==role)return r;
    }
    return NULL;
}
static unsigned cells_used(void) {unsigned n=0;for(unsigned i=0;i<60;++i)if(*(void **)(s_manager+0x38+4*i))++n;return n;}
/* Clean slate between cases: nothing retained, bank back to the init pin. */
static void assert_idle(void) {
    assert(!SudekiMpStoryShotAudioRetains() && records_in(SHOT_PLAYING)+records_in(SHOT_STOPPING)+
        records_in(SHOT_RELEASING)+records_in(SHOT_BOUND)==0);
    assert(!cells_used() && !u32(s_manager+0x130) && u32(s_bank+0x18)==1 && u32(s_pool+0xc)==16);
}
static void refused_before_native(unsigned expected,const char *what) {
    uint32_t plays=play_calls,sets=bank_set_calls,refs=u32(s_bank+0x18),steps=plan_steps;
    queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,4000+steps,1,2,3);
    assert(present(5000) && plan_steps==steps+1 && plan_commits==1 && plan_results[0]==expected);
    assert(play_calls==plays && bank_set_calls==sets && u32(s_bank+0x18)==refs && records_in(SHOT_FREE)+records_in(SHOT_QUARANTINED)==SHOT_RECORDS);
    printf("PASS shot refusal before any native call: %s\n",what);
}
static void shot_tests(void) {
    shot_world_setup();
    void *decrement=(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),"InterlockedDecrement");
    assert(decrement);ptr(base+0x29a0d8,decrement); /* bind the one import AttachmentRelease uses */
    assert(methods(base) && shot_methods(base) && bank_name_id((const char *)base+0x2c621c)==SHOT_MISSILES_ID);
    /* Check #9: shot-only guards never gate character audio. */
    for(unsigned i=0;i<sizeof(shot_codes)/sizeof(shot_codes[0]);++i) {
        for(unsigned j=0;j<shot_codes[i].size;++j) {
            base[shot_codes[i].rva+j]^=1;assert(!shot_methods(base) && methods(base));
            base[shot_codes[i].rva+j]^=1;assert(shot_methods(base));
        }
        printf("Named shot guard: %s PASS\n",shot_codes[i].name);
    }
    for(unsigned i=0;i<sizeof(shot_relocations)/sizeof(shot_relocations[0]);++i) {
        base[shot_relocations[i].rva]^=1;assert(!shot_methods(base) && methods(base));
        base[shot_relocations[i].rva]^=1;assert(shot_methods(base));
    }
    for(unsigned i=0;i<sizeof(shot_slots)/sizeof(shot_slots[0]);++i) {
        base[shot_slots[i].rva]^=4;assert(!shot_methods(base) && methods(base));
        base[shot_slots[i].rva]^=4;assert(shot_methods(base));
    }
    for(unsigned i=0;i<13;++i) {base[0x2c621c+i]^=1;assert(!shot_methods(base));base[0x2c621c+i]^=1;}
    for(unsigned i=0;i<4;++i) {base[0x2d9154+i]^=1;assert(!shot_methods(base));base[0x2d9154+i]^=1;}
    ptr(base+0x29a0d8,NULL);assert(!shot_methods(base) && methods(base));ptr(base+0x29a0d8,decrement);
    assert(shot_methods(base));
    puts("PASS shot guards: every byte, relocation, vtable slot, literal and import; character-audio guards unaffected");

    {   /* Bridges. */
        uint8_t emitter[0x14]={0};uint32_t handle=9;const char *cue="Royal_Sceptre_fire";
        assert(invoke_emitter_play(emitter_play_abi_stub,emitter,cue,&handle,1)==0x55);
        assert(shot_abi[0]==(uint32_t)(uintptr_t)emitter && shot_abi[1]==(uint32_t)(uintptr_t)cue &&
            shot_abi[2]==(uint32_t)(uintptr_t)&handle && shot_abi[3]==1 && !shot_abi[4] && !shot_abi[5]);
        assert(invoke_emitter_play(emitter_play_abi_stub,emitter,cue,&handle,0)==0x55 && !shot_abi[3]);
        assert(invoke_bank_set(bank_set_abi_stub,emitter,cue)==1 && bank_set_abi[0]==(uint32_t)(uintptr_t)emitter &&
            bank_set_abi[1]==(uint32_t)(uintptr_t)cue);
        uint8_t slot[0x1c]={0},attachment[0x54]={0};
        assert(invoke_release(release_abi_stub,s_manager,slot,attachment)==0x77 &&
            release_abi[0]==(uint32_t)(uintptr_t)s_manager && release_abi[1]==(uint32_t)(uintptr_t)slot &&
            release_abi[2]==(uint32_t)(uintptr_t)attachment);
        assert(!invoke_detach(detach_abi_stub,attachment) && detach_abi==(uint32_t)(uintptr_t)attachment);
        assert(!invoke_detach(detach_abi_stub,slot) && detach_abi==(uint32_t)(uintptr_t)slot);
        puts("PASS shot bridges: EmitterPlay EAX+5 args/RET 0x14, EmitterBankSet ESI/RET 4/AL, AttachmentRelease EDI/EDX/RET 4, DetachAll stdcall");
    }
    {   /* Retail EmitterBankSet + ManagerBankFind + SoundBankNameId on the fake manager. */
        uint8_t emitter[0x14]={0},other[0x14]={0};ptr(emitter,base+0x2e1db4);ptr(other,base+0x2e1db4);
        uint32_t refs=u32(s_bank+0x18);
        assert(invoke_bank_set(base+0x28ae50,emitter,(const char *)base+0x2c621c)==1);
        assert(*(void **)(emitter+4)==s_bank && u32(s_bank+0x18)==refs && !emitter[0x10] && !u32(emitter+8));
        assert(invoke_bank_set(base+0x28ae50,other,"no_such_bank.xsb")==0 && !*(void **)(other+4) && u32(s_bank+0x18)==refs);
        puts("PASS retail EmitterBankSet on an inactive emitter: bank stored, no reference taken, unknown name 0");
    }
    {   /* Retail AttachmentRelease through the bound InterlockedDecrement import. */
        uint8_t attachment[0x54]={0},slot[0x1c]={0},stranger[0x1c]={0};
        ptr(attachment,base+0x2e1f34);num(attachment+4,2);ptr(slot,attachment);
        ptr(s_manager+0x38+4*5,slot);num(s_manager+0x130,1);
        assert(invoke_release(base+0x2893c0,s_manager,stranger,attachment)==0x80070057 &&
            *(void **)(s_manager+0x38+4*5)==slot && u32(s_manager+0x130)==1 && u32(attachment+4)==2);
        assert(!invoke_release(base+0x2893c0,s_manager,slot,attachment) && !*(void **)(s_manager+0x38+4*5) &&
            !u32(s_manager+0x130) && u32(attachment+4)==1 && *(void **)slot==attachment);
        /* Retail DetachAll on an empty sound list: the list is reset, nothing freed. */
        uint8_t head[12];ptr(head,head);ptr(head+4,head);ptr(attachment+0x48,head);num(attachment+0x4c,3);
        assert(!invoke_detach(base+0x281a70,attachment) && !u32(attachment+0x4c) &&
            *(void **)head==head && *(void **)(head+4)==head);
        puts("PASS retail AttachmentRelease (cell, count, refcount 2->1; no cell -> E_INVALIDARG, nothing changed) and DetachAll");
    }
    /* The executor through counting trampolines in front of the retail
     * routines; only EmitterPlay (a registration double) and CueStop are stubs. */
    bank_set_real=base+0x28ae50;position_real=base+0x284520;detach_real=base+0x281a70;release_real=base+0x2893c0;
    shot_entry.bank_set=(void *)bank_set_counter;shot_entry.position=(void *)position_counter;
    shot_entry.detach=(void *)detach_counter;shot_entry.release=(void *)release_counter;shot_entry.play=(void *)play_double_stub;
    cue_stop_entry=(void *)cue_stop_double;shot_tick=test_tick;
    shot_roster=(SudekiMpLanStoryNativeRoster){.epoch=3,.revision=3,.available_mask=0xf,.leader_character=2};
    host_mode=FALSE;assert(!busy);
    uint32_t pin_before=shot_pinned;assert(!pin_before);
    {   /* Party travel: claim, play, move, retire, stop, release, FREE. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,100,1,2,3);
        assert(present(1000) && plan_commits==1 && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && item_calls==1);
        ShotRecord *r=record_for(100,SHOT_TRAVEL);assert(r && r->state==SHOT_PLAYING && r==&shot_records[0]);
        assert(bank_set_calls==1 && play_calls==1 && u32(s_bank+0x18)==3); /* init pin + record + cue */
        assert(play_args[0]==(uint32_t)(uintptr_t)r->emitter && !strcmp(played_cue,"Royal_Sceptre_projectile") &&
            played_cue>=s_text && played_cue<s_text+sizeof(s_text) &&
            play_args[2]==(uint32_t)(uintptr_t)&r->cue.handle && play_args[3]==1);
        assert(*(void **)r->emitter==base+0x2e1db4 && *(void **)(r->emitter+4)==s_bank &&
            *(void **)(r->emitter+8)==r->slot && r->emitter[0x10]==1 && !u32(r->emitter+0xc));
        float one;memcpy(&one,r->slot+0x18,4);assert(one==1 && ((float *)(r->slot+4))[0]==1 && ((float *)(r->slot+4))[2]==3);
        uint8_t *attachment=*(uint8_t **)r->slot;assert(attachment && cells_used()==1 && u32(s_manager+0x130)==1);
        assert(!shot_pinned); /* Check #7: no pin at claim. */
        queue(SUDEKIMP_SHOT_AUDIO_MOVE,3,12,0,44,100,4,5,6);
        assert(present(1016) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && position_calls==1);
        assert(((float *)(attachment+0xc))[0]==4 && ((float *)(attachment+0xc))[2]==6 && ((float *)(r->slot+4))[1]==5);
        queue(SUDEKIMP_SHOT_AUDIO_MOVE,3,12,0,44,100,4,5,6);
        assert(present(1032) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && position_calls==1);
        queue(SUDEKIMP_SHOT_AUDIO_MOVE,3,12,0,44,777,4,5,6);
        assert(present(1040) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT);
        uint32_t handle=r->cue.handle;
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,3,0,0,44,100,0,0,0);
        assert(present(1048) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && stop_calls==1);
        assert(r->state==SHOT_FREE && detach_calls==1 && release_calls==1 && !cells_used() && !u32(s_manager+0x130));
        assert(u32(attachment+4)==1 && (s_entries[(handle&255)*24]&63)==2 && u32(s_bank+0x18)==2);
        recycle(handle);assert_idle();
        puts("PASS shot travel lifecycle: claim (no load), play, move only on change, retire, stop, release in destructor order, FREE");
    }
    {   /* Party fire drains to its natural end; a 4 s cap stops it. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,7,1,1,1);
        assert(present(2000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED);
        ShotRecord *r=record_for(7,SHOT_FIRE);assert(r && r==&shot_records[8] && play_args[3]==1);
        assert(!strcmp(played_cue,"Royal_Sceptre_fire"));
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,3,0,0,0,7,0,0,0); /* a VFX retire never matches a party fire */
        assert(present(2100) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && r->state==SHOT_PLAYING);
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,7,1,1,1);
        assert(present(2116) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && play_calls==2); /* no replay */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1,24,0,0,7,1,1,1); /* Elco's sequence 7 is another shot */
        assert(present(2120) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && play_calls==3);
        ShotRecord *elco=NULL;
        for(unsigned i=8;i<SHOT_RECORDS;++i)if(shot_records[i].state==SHOT_PLAYING && shot_records[i].character==1)elco=&shot_records[i];
        assert(elco && elco!=r);recycle(elco->cue.handle);assert(present(2130) && elco->state==SHOT_FREE);
        assert(present(3999) && r->state==SHOT_PLAYING);
        recycle(r->cue.handle);uint32_t stops=stop_calls;
        assert(present(4010) && r->state==SHOT_FREE && stop_calls==stops);assert_idle();
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,8,1,1,1);
        assert(present(10000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED);r=record_for(8,SHOT_FIRE);
        uint32_t handle=r->cue.handle;
        assert(present(14000) && r->state==SHOT_PLAYING);
        assert(present(14001) && r->state==SHOT_FREE && stop_calls==stops+1);
        recycle(handle);assert_idle();
        puts("PASS shot party fire: natural drain without a stop, VFX retire never matches, no replay, 4 s cap stops");
    }
    {   /* Local _2D rule (check #3): planner flag, proven local character and native leader. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        shot_local=3;shot_roster.leader_character=3;
        num(s_manager+0x130,48); /* a 2D cue needs no attachment cell */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,1,0,20,1,1,1);
        assert(present(20000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && play_args[3]==0);
        assert(!strcmp(played_cue,"Royal_Sceptre_fire_2D"));
        ShotRecord *r=record_for(20,SHOT_FIRE);assert(r && !*(void **)r->slot && !cells_used());
        recycle(r->cue.handle);assert(present(20100) && r->state==SHOT_FREE);num(s_manager+0x130,0);assert_idle();
        names("","Xurics_Shock_staff"); /* no _2D variant: base name, still 2D */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,17,1,0,21,1,1,1);
        assert(present(21000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && play_args[3]==0);
        assert(!strcmp(played_cue,"Xurics_Shock_staff"));
        r=record_for(21,SHOT_FIRE);recycle(r->cue.handle);assert(present(21100));assert_idle();
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        shot_roster.leader_character=1; /* native leader disagrees: positional */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,1,0,22,1,1,1);
        assert(present(22000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && play_args[3]==1);
        assert(!strcmp(played_cue,"Royal_Sceptre_fire"));
        r=record_for(22,SHOT_FIRE);recycle(r->cue.handle);assert(present(22100));assert_idle();
        shot_roster.leader_character=3;shot_local=2; /* proven local character disagrees */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,1,0,23,1,1,1);
        assert(present(23000) && play_args[3]==1 && !strcmp(played_cue,"Royal_Sceptre_fire"));
        r=record_for(23,SHOT_FIRE);recycle(r->cue.handle);assert(present(23100));assert_idle();
        names("Royal_Sceptre_projectile",""); /* empty cue: silent, nothing claimed */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,17,0,0,24,1,1,1);
        uint32_t plays=play_calls;assert(present(24000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && play_calls==plays);
        shot_roster.leader_character=2;
        puts("PASS shot local fire: _2D unpositioned only when planner, local character and native leader agree; base fallback; empty name silent");
    }
    {   /* World missile: separate fire and travel, one pairing, both stop on RETIRE (check #5). */
        names("kurianKnight_projectile","kurian_knight_fire_effect");
        unsigned calls=world_calls;
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,4,0,0,183,500,-93,8,-152);
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,4,0,0,183,500,-93,8,-152);
        assert(present(30000) && plan_commits==2 && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED &&
            plan_results[1]==SUDEKIMP_SHOT_AUDIO_PLAYED && world_calls==calls+1);
        ShotRecord *fire=record_for(500,SHOT_FIRE),*travel=record_for(500,SHOT_TRAVEL);
        assert(fire && travel && fire->world && travel->world && cells_used()==2 && u32(s_bank+0x18)==5);
        queue(SUDEKIMP_SHOT_AUDIO_MOVE,4,0,0,183,500,-90,8,-150);
        assert(present(30016) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED);
        assert(((float *)(fire->slot+4))[0]==-93 && ((float *)(travel->slot+4))[0]==-90); /* fire stays at launch */
        uint32_t f=fire->cue.handle,t=travel->cue.handle,stops=stop_calls;
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,4,0,0,183,500,0,0,0);
        assert(present(30100) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && stop_calls==stops+2);
        assert(fire->state==SHOT_FREE && travel->state==SHOT_FREE);recycle(f);recycle(t);assert_idle();
        /* The same instance number from a restarted VFX stream (new emitted
         * tick) is a new missile: paired afresh, never served from the cache. */
        queue_tick=2000;queue(SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,4,0,0,183,500,-93,8,-152);
        assert(present(30200) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && world_calls==calls+2);
        fire=record_for(500,SHOT_FIRE);f=fire->cue.handle;
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,4,0,0,183,500,0,0,0);assert(present(30216) && fire->state==SHOT_FREE);
        recycle(f);queue_tick=1000;assert_idle();calls=world_calls-1;
        cues_result=SUDEKIMP_SHOT_CUES_DEFER; /* a registry change mid-walk is not cached */
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,4,0,0,183,501,0,0,0);
        assert(present(31000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_DEFER && world_calls==calls+2);
        cues_result=SUDEKIMP_SHOT_CUES_SILENT;
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,4,0,0,183,501,0,0,0);
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,4,0,0,183,501,0,0,0);
        assert(present(31016) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && plan_results[1]==SUDEKIMP_SHOT_AUDIO_SILENT &&
            world_calls==calls+3);
        cues_result=SUDEKIMP_SHOT_CUES_OK;assert_idle();
        puts("PASS shot world missile: fire+travel as separate actions, one pairing per instance, both stop on retire, DEFER uncached");
    }
    {   /* Check #13: the idle sweep frees a finished travel attachment mid-flight. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,600,1,2,3);
        assert(present(40000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED);
        ShotRecord *r=record_for(600,SHOT_TRAVEL);uint8_t *attachment=*(uint8_t **)r->slot;
        typedef void (__attribute__((stdcall)) *Sweep)(void *manager);
        ((Sweep)(void *)(base+0x2897b0))(s_manager); /* retail sweep; retail status: no sound playing */
        assert(!*(void **)r->slot && !cells_used() && !u32(s_manager+0x130) && u32(attachment+4)==1);
        uint32_t moves=position_calls,releases=release_calls;
        queue(SUDEKIMP_SHOT_AUDIO_MOVE,3,12,0,44,600,7,8,9);
        assert(present(40016) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && position_calls==moves);
        recycle(r->cue.handle);uint32_t stops=stop_calls;
        assert(present(40032) && r->state==SHOT_FREE && stop_calls==stops && release_calls==releases);
        assert_idle();
        puts("PASS shot travel after the retail idle sweep: no move on an empty slot, positive retirement releases without a stop");
    }
    {   /* EmitterPlay failures. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        play_fail=1;queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,700,1,2,3);
        assert(present(50000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT);play_fail=0;assert_idle();
        play_attach_then_fail=1;uint32_t releases=release_calls;queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,701,1,2,3);
        assert(present(50016) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && release_calls==releases+1);
        play_attach_then_fail=0;assert_idle();
        play_status=1;uint32_t stops=stop_calls;queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,702,1,2,3);
        assert(present(50032) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_SILENT && stop_calls==stops+1);
        play_status=0;
        for(unsigned i=0;i<16;++i)if(s_entries[i*24]&63)recycle((u32(s_entries+i*24+8)<<8)|i);
        assert_idle();
        puts("PASS shot EmitterPlay failure: no handle releases (attachment too); a handle with a failure is stopped first");
    }
    {   /* Namespace change, NeedsService, Drain and Stop. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,800,1,2,3);
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,30,1,2,3);
        assert(present(60000));
        ShotRecord *travel=record_for(800,SHOT_TRAVEL),*fire=record_for(30,SHOT_FIRE);
        assert(travel && fire && SudekiMpStoryShotAudioRetains());
        assert(!SudekiMpStoryShotAudioNeedsService(60100) && SudekiMpStoryShotAudioNeedsService(60251));
        uint32_t t=travel->cue.handle,f=fire->cue.handle;
        shot_clock=60300;assert(!SudekiMpStoryShotAudioDrain(playback_exact,NULL));
        assert(travel->state==SHOT_FREE && fire->state==SHOT_PLAYING);recycle(t);
        recycle(f);assert(SudekiMpStoryShotAudioDrain(playback_exact,NULL) && !SudekiMpStoryShotAudioNeedsService(70000));
        assert_idle();
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,31,1,2,3);assert(present(80000));fire=record_for(31,SHOT_FIRE);
        f=fire->cue.handle;shot_clock=84001;assert(SudekiMpStoryShotAudioDrain(playback_exact,NULL) && fire->state==SHOT_FREE);
        recycle(f);assert_idle();
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,4,0,0,183,900,0,0,0);
        queue(SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,4,0,0,183,900,0,0,0);
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,32,1,2,3);
        assert(present(90000) && plan_commits==3);
        ShotRecord *wt=record_for(900,SHOT_TRAVEL),*wf=record_for(900,SHOT_FIRE);fire=record_for(32,SHOT_FIRE);
        uint32_t h1=wt->cue.handle,h2=wf->cue.handle,h3=fire->cue.handle;
        shot_namespace=8;assert(present(90016)); /* new namespace: host-visual cues stop, party fire drains */
        assert(wt->state==SHOT_FREE && wf->state==SHOT_FREE && fire->state==SHOT_PLAYING);
        admitted=FALSE;assert(!SudekiMpStoryShotAudioStop(playback_exact,NULL) && fire->state==SHOT_PLAYING);admitted=TRUE;
        uint32_t steps=plan_steps,stops=stop_calls;
        HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL);DWORD code=99;
        assert(thread && WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0 && GetExitCodeThread(thread,&code) && !code);
        CloseHandle(thread);assert(fire->state==SHOT_PLAYING && plan_steps==steps && stop_calls==stops);
        puts("PASS shot refusal before any native call: foreign thread (Present, Stop and Drain)");
        assert(SudekiMpStoryShotAudioStop(playback_exact,NULL) && fire->state==SHOT_FREE && !SudekiMpStoryShotAudioRetains());
        recycle(h1);recycle(h2);recycle(h3);assert_idle();
        puts("PASS shot teardown: namespace change, NeedsService after 250 ms, Drain stops travel and drains fire (4 s cap), Stop, foreign thread refused");
    }
    {   /* Refusals before any native call. */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        uint32_t steps=plan_steps;host_mode=TRUE;assert(!present(5000) && plan_steps==steps);host_mode=FALSE;
        puts("PASS shot refusal before any native call: host mode (no planner step)");
        admitted=FALSE;assert(!present(5000) && plan_steps==steps);admitted=TRUE;
        puts("PASS shot refusal before any native call: presentation not exact");
        num(s_bank+0x18,0);refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"bank+0x18 = 0 (never loads)");num(s_bank+0x18,1);
        s_bank[0x30]=1;refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"bank+0x30 set");s_bank[0x30]=0;
        s_banks[1]=s_bank2;num(s_bank2+0x14,SHOT_MISSILES_ID);num(s_manager+0x30,2);
        refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"duplicate missiles.xsb id");
        num(s_bank2+0x14,bank_name_id(s_bank2_name)); /* control: an unrelated second bank does not refuse */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,3999,1,2,3);
        assert(present(5050) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED);
        for(unsigned i=0;i<16;++i)if(s_entries[i*24]&63)recycle((u32(s_entries+i*24+8)<<8)|i);
        assert(present(5100));num(s_manager+0x30,1);assert_idle();
        memcpy(s_bank_name,"MISSILES.XSB",13);assert(bank_name_id(s_bank_name)==SHOT_MISSILES_ID);
        refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"bank name case mismatch");memcpy(s_bank_name,"missiles.xsb",13);
        names("","Not_A_Cue");refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"cue missing from missiles.xsb");
        names("","Unknown_wave_cue");refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"cue wave key missing");
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        s_categories[0x18]=1;refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"Master mute");s_categories[0x18]=0;
        num(s_pool+0xc,4);refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"pool units below reserve");num(s_pool+0xc,16);
        for(unsigned i=0;i<12;++i)s_entries[i*24]=1;
        refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"free pool entries below reserve");
        for(unsigned i=0;i<12;++i)s_entries[i*24]=0;
        num(s_manager+0x130,48);refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"manager+0x130 > 47");num(s_manager+0x130,0);
        ptr(s_camera_manager,base);refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"listener not exact");
        ptr(s_camera_manager,base+0x2c7b80);
        base[0x2893c0]^=1;refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"shot guard mismatch");base[0x2893c0]^=1;
        base[0x28aeb0]^=1;refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"character-audio guard mismatch");base[0x28aeb0]^=1;
        cues_image=FALSE;refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"resolver image mismatch");cues_image=TRUE;
        cues_result=SUDEKIMP_SHOT_CUES_DEFER;refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"resolver DEFER");
        cues_result=SUDEKIMP_SHOT_CUES_SILENT;refused_before_native(SUDEKIMP_SHOT_AUDIO_SILENT,"resolver SILENT");
        cues_result=SUDEKIMP_SHOT_CUES_OK;
        assert_idle();
    }
    {   /* Quarantine (pins only now, check #7), Retains and Initialize (check #6),
         * bank re-proof before the reference is dropped (check #8). */
        names("Royal_Sceptre_projectile","Royal_Sceptre_fire");
        assert(!shot_pinned);
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1000,1,2,3);assert(present(100000));
        ShotRecord *r=record_for(1000,SHOT_TRAVEL);uint32_t handle=r->cue.handle;
        num(s_bank+0x18,1); /* someone else dropped the record's reference */
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,3,0,0,44,1000,0,0,0);assert(present(100016));
        assert(r->state==SHOT_QUARANTINED && u32(s_bank+0x18)==1 && !cells_used() && shot_pinned);
        assert(!SudekiMpStoryShotAudioRetains() && SudekiMpStoryShotAudioStop(playback_exact,NULL));
        num(s_bank+0x18,2);recycle(handle);assert_idle();
        assert(SudekiMpStoryCharacterAudioInitialize((HMODULE)base,FALSE) && !host_mode && plan_resets);
        puts("PASS shot quarantine at bank+0x18 < 2: no decrement, module pinned only then, not retained, Initialize allowed");
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1001,1,2,3);assert(present(101000));
        r=record_for(1001,SHOT_TRAVEL);handle=r->cue.handle;
        shot_entry.release=(void *)release_fault;release_stub_result=0; /* answers 0 but leaves the cell */
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,3,0,0,44,1001,0,0,0);assert(present(101016));
        assert(r->state==SHOT_QUARANTINED && cells_used()==1 && u32(s_bank+0x18)==3);
        memset(s_manager+0x38,0,240);num(s_manager+0x130,0);num(s_bank+0x18,2);recycle(handle);assert_idle();
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1002,1,2,3);assert(present(102000));
        r=record_for(1002,SHOT_TRAVEL);handle=r->cue.handle;release_stub_result=0x80070057;
        queue(SUDEKIMP_SHOT_AUDIO_RETIRE,3,0,0,44,1002,0,0,0);assert(present(102016));
        assert(r->state==SHOT_QUARANTINED && *(void **)r->slot && u32(s_bank+0x18)==3);
        memset(s_manager+0x38,0,240);num(s_manager+0x130,0);num(s_bank+0x18,2);recycle(handle);assert_idle();
        shot_entry.release=(void *)release_counter;
        puts("PASS shot quarantine when the release leaves the cell or fails");
        /* Re-proof: the bank left the list, or its soundbank changed. */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1003,1,2,3);assert(present(103000));
        r=record_for(1003,SHOT_TRAVEL);recycle(r->cue.handle);
        s_banks[0]=s_bank2;assert(present(103016)); /* natural end -> release -> proof fails */
        assert(r->state==SHOT_QUARANTINED && !cells_used() && u32(s_bank+0x18)==2);
        s_banks[0]=s_bank;num(s_bank+0x18,1);assert_idle();
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1004,1,2,3);assert(present(104000));
        r=record_for(1004,SHOT_TRAVEL);recycle(r->cue.handle);
        ptr(s_bank+0x1c,s_sb2);assert(present(104016));
        assert(r->state==SHOT_QUARANTINED && u32(s_bank+0x18)==2);ptr(s_bank+0x1c,s_sb);num(s_bank+0x18,1);assert_idle();
        /* Transient: bank+0x30 set at release keeps the record retryable. */
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,3,12,0,44,1005,1,2,3);assert(present(105000));
        r=record_for(1005,SHOT_TRAVEL);recycle(r->cue.handle);s_bank[0x30]=1;
        assert(present(105016) && r->state==SHOT_RELEASING && u32(s_bank+0x18)==2 && SudekiMpStoryShotAudioRetains());
        assert(!SudekiMpStoryCharacterAudioInitialize((HMODULE)base,FALSE));
        assert(SudekiMpStoryShotAudioNeedsService(105300) && !SudekiMpStoryShotAudioStop(playback_exact,NULL));
        s_bank[0x30]=0;assert(SudekiMpStoryShotAudioStop(playback_exact,NULL) && r->state==SHOT_FREE);assert_idle();
        assert(SudekiMpStoryCharacterAudioInitialize((HMODULE)base,FALSE));
        puts("PASS shot bank re-proof at release: gone or changed bank quarantines without a write; transient retries and blocks Initialize");
        /* A FREE record whose slot a native cell already names. */
        ptr(s_manager+0x38+4*7,shot_records[8].slot);
        refused_before_native(SUDEKIMP_SHOT_AUDIO_DEFER,"slot already in a manager cell (record quarantined)");
        assert(shot_records[8].state==SHOT_QUARANTINED && !SudekiMpStoryShotAudioRetains());ptr(s_manager+0x38+4*7,NULL);
        queue(SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3,12,0,0,4100,1,2,3);
        assert(present(106000) && plan_results[0]==SUDEKIMP_SHOT_AUDIO_PLAYED && record_for(4100,SHOT_FIRE)==&shot_records[9]);
        recycle(shot_records[9].cue.handle);assert(present(106100));assert_idle();
        assert(records_in(SHOT_QUARANTINED)==6);
    }
    shot_entry.bank_set=shot_entry.position=shot_entry.detach=shot_entry.release=shot_entry.play=NULL;
    cue_stop_entry=NULL;shot_tick=NULL;
    ptr(base+0x409e58,NULL);ptr(base+0x409e8c,NULL);ptr(base+0x3c37d8,NULL);ptr(base+0x409d7c,NULL);
    free(s_backend);
}
int main(int argc,char **argv) {
    wchar_t path[32768];assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    base=map_image(path);assert(methods(base));
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        unsigned mutations=i<4?codes[i].size:1u;
        for(unsigned j=0;j<mutations;++j) {
            base[codes[i].rva+j]^=1;assert(!methods(base));base[codes[i].rva+j]^=1;assert(methods(base));
        }
        printf("Named native guard: %s PASS\n",codes[i].name);
    }
    for(unsigned i=0;i<sizeof(relocations)/sizeof(relocations[0]);++i) {
        base[relocations[i].rva]^=1;assert(!methods(base));base[relocations[i].rva]^=1;assert(methods(base));
    }
    assert(SudekiMpInstallInlineHook(&play_hook,base+0xfc200,base+0xfc200,7,play_detour));assert(methods(base));
    base[0xfc200]^=1;assert(!methods(base));base[0xfc200]^=1;
    assert(SudekiMpRestoreInlineHook(&play_hook));assert(methods(base));
    uint32_t handle=0,component=99;const char *name="BlockStart";
    invoke_play(native_play_stub,&component,name,&handle,1,0x3f800000,0);
    assert(handle==0x123400 && abi[0]==(uint32_t)(uintptr_t)&component &&
        abi[1]==(uint32_t)(uintptr_t)&handle && abi[2]==(uint32_t)(uintptr_t)name &&
        abi[3]==1 && abi[4]==0x3f800000 && abi[5]==0);
    play_hook.trampoline=native_play_stub;handle=0;
    invoke_play(play_detour,&component,name,&handle,0,0x3f000000,1);
    assert(handle==0x123400 && abi[3]==0 && abi[4]==0x3f000000 && abi[5]==1);
    memset(&play_hook,0,sizeof(play_hook));
    puts("PASS ActorPlayCue bridge and detour ECX/ESI/RET16 argument forwarding");
    uint8_t manager[0x15c]={0},pool[0x14]={0},bank[0x34]={0},sb[0x3c]={0},entries[48]={0};
    uint8_t actor[0x100]={0},sound[0xe0]={0},cue[0x30]={0},rows[24]={0},variants[16]={0},sounds[80]={0};
    uint8_t position[0xb8]={0};
    uint8_t *backend=calloc(1,0xafbc);assert(backend);
    uint8_t *banks[]={bank};char bank_name[96]="TalSoundScript.xsb",names[96]="BlockStart";
    ptr(manager,base+0x2e1dc0);ptr(manager+0x2c,banks);num(manager+0x30,1);ptr(manager+0x10,backend);
    ptr(backend,base+0x2e1e90);ptr(base+0x3c37d8,backend);ptr(base+0x409e58,manager);
    ptr(pool,base+0x2e1ee0);num(pool+4,100);num(pool+8,2);ptr(pool+0x10,entries);ptr(base+0x409e8c,pool);
    ptr(bank,base+0x2e1e40);ptr(bank+0x10,bank_name);num(bank+0x18,1);ptr(bank+0x1c,sb);
    ptr(sb,base+0x2e1f68);num(sb+4,1);memcpy(sb+8,"SSBN",4);num(sb+0x1c,1);num(sb+0x18,1);
    num(sb+0x2c,sizeof(names));ptr(sb+0x30,names);ptr(sb+0x38,rows);ptr(sb+0x34,sounds);
    num(rows+4,1);ptr(rows+0x10,variants);num(variants,7);num(sounds,7);
    ptr(actor+0x5c,sound);ptr(sound,base+0x2cd0ac);ptr(sound+0x10,actor);
    ptr(actor+0x44,position);ptr(position,base+0x2cdefc);ptr(position+0x10,actor);
    ptr(sound+0x4c,base+0x2e1db4);ptr(sound+0x50,bank);ptr(sound+0x54,sound+0x30);sound[0x5c]=1;
    entries[0]=1;num(entries+8,2);ptr(entries+0xc,cue);ptr(entries+0x10,bank);ptr(cue+8,sb);
    host_mode=TRUE;SudekiMpLanStoryNativeRoster roster={.epoch=1,.revision=1,.available_mask=4};roster.actors[2]=actor;
    SudekiMpStoryCharacterAudioBind(&roster);assert(bound);
    handle=512;observe_play(sound,names,&handle,1,0x3f800000,0);
    SudekiMpStoryAudio frame;assert(SudekiMpStoryCharacterAudioCapture(2,&frame));assert(frame.count==1 && frame.cues[0].id==handle);
    admitted=FALSE;assert(!SudekiMpStoryCharacterAudioCapture(2,&frame));admitted=TRUE;
    num(sound+0x10,0);assert(!SudekiMpStoryCharacterAudioCapture(2,&frame));ptr(sound+0x10,actor);
    ptr(entries+0x10,sb);assert(!SudekiMpStoryCharacterAudioCapture(2,&frame));ptr(entries+0x10,bank);
    num(entries+8,3);ptr(entries+0xc,(void *)1); /* no read of retired cue */
    assert(SudekiMpStoryCharacterAudioCapture(2,&frame) && !frame.count);
    entries[0]=0;assert(SudekiMpStoryCharacterAudioCapture(2,&frame) && !frame.count);
    puts("PASS native source actor/bank/handle ownership, unknown refusal and positive generation retirement");
    /* Execute all three retail bank-binding leaves, with no device/loading
     * stub: the actor already retains the bank, as it does in the live pair. */
    host_mode=FALSE;uint8_t fx[0xe0]={0};
    ptr(fx,base+0x2cd0ac);ptr(fx+0x4c,base+0x2e1db4);ptr(fx+0x54,fx+0x30);
    uint32_t bank_id=0;for(unsigned i=0;bank_name[i];++i) {
        uint8_t ch=bank_name[i];if(ch>='a' && ch<='z')ch-=32;
        bank_id=(i&1)?bank_id*ch:bank_id+ch;
    }
    num(bank+0x14,bank_id);unsigned refs=u32(bank+0x18);
    admitted=FALSE;assert(!SudekiMpStoryCharacterAudioBindEffect(&roster,2,fx));admitted=TRUE;
    assert(!*(void **)(fx+0x50) && u32(bank+0x18)==refs);
    assert(SudekiMpStoryCharacterAudioBindEffect(&roster,2,fx));
    assert(*(void **)(fx+0x50)==bank && fx[0x5c]==1 && u32(bank+0x18)==refs+1 && *(float *)(fx+0x80)==50);
    for(unsigned i=0;i<5;++i)assert(SudekiMpStoryCharacterAudioBindEffect(&roster,2,fx));
    assert(u32(bank+0x18)==refs+1); /* No duplicate references on retransmission. */
    uint8_t generic[0x34];memcpy(generic,bank,sizeof(generic));char generic_name[96]="sfx_generic.xsb";
    ptr(generic+0x10,generic_name);num(generic+0x14,bank_id+1);num(generic+0x18,2);
    void *both[]={bank,generic};ptr(manager+0x2c,both);num(manager+0x30,2);
    ptr(fx+0x50,generic);num(bank+0x18,refs); /* Isolated second fixture. */
    assert(SudekiMpStoryCharacterAudioBindEffect(&roster,2,fx));
    assert(u32(generic+0x18)==1 && u32(bank+0x18)==refs+1 && *(void **)(fx+0x50)==bank);
    ptr(fx+0x50,generic);num(bank+0x18,refs);
    assert(!SudekiMpStoryCharacterAudioBindEffect(&roster,2,fx) && u32(generic+0x18)==1);
    ptr(manager+0x2c,banks);num(manager+0x30,1);
    puts("PASS retail effect-bank ABI, initial and fallback binding, native references, radius, repeat and last-reference refusal");
    uint8_t camera_manager[0x60]={0},camera[0xbc]={0},camera_state[0xdc]={0};
    uint8_t categories[28]={0},parameters[24]={0},head[12]={0};char parameter_name[64]="Volume";
    ptr(camera_manager,base+0x2c7b80);ptr(camera_manager+0x20,camera);ptr(camera_manager+0x24,camera);
    ptr(camera,base+0x2cce5c);ptr(camera+0x34,camera_state);ptr(camera_state,base+0x2dd638);
    ptr(base+0x409d7c,camera_manager);*(uint16_t *)(manager+0x12c)=1;ptr(manager+0x128,categories);
    num(pool+0xc,2);num(base+0x3c37d4,1);ptr(base+0x3c2374,parameters);ptr(parameters+20,parameter_name);
    ptr(backend+0xafac,head);ptr(head,head);ptr(head+4,head);
    host_mode=FALSE;stub_handle=512;entries[0]=1;num(entries+8,2);ptr(entries+0xc,cue);
    AudioOwner a;assert(actor_owner(&roster,2,&a) && attachment_ready(&a) && play_available(&a) && listener_exact());
    /* Reproduce the live refusal through retail ActorPlayCue, replacing only
     * the lower audio-device call: the actor moved 400+ units but its paused
     * sound component still pointed at the original save position. */
    float presented_xyz[3]={-116,6,-68},stale_xyz[3]={-25,-4,349},radius=45;
    memcpy(position+0x18,presented_xyz,12);memcpy(camera_state+0xc0,presented_xyz,12);
    memcpy(sound+0x84,stale_xyz,12);memcpy(sound+0x34,stale_xyz,12);memcpy(sound+0x80,&radius,4);
    base[0x31f297]=1;
    uint8_t emitter_saved[5];memcpy(emitter_saved,base+0x28aeb0,5);
    base[0x28aeb0]=0xe9;*(int32_t *)(base+0x28aeb1)=(intptr_t)native_emitter_stub-(intptr_t)(base+0x28aeb5);
    assert(FlushInstructionCache(GetCurrentProcess(),base+0x28aeb0,5));
    handle=99;invoke_play(base+0xfc200,sound,names,&handle,1,0x3f800000,0);
    assert(!handle && !emitter_calls);
    admitted=FALSE;assert(!refresh_position(2,&a,playback_exact,NULL) && !memcmp(sound+0x84,stale_xyz,12));admitted=TRUE;
    ptr(position+0x94,actor);assert(!refresh_position(2,&a,playback_exact,NULL) && !memcmp(sound+0x84,stale_xyz,12));ptr(position+0x94,NULL);
    assert(refresh_position(2,&a,playback_exact,NULL) && !memcmp(sound+0x84,presented_xyz,12) && !memcmp(sound+0x34,presented_xyz,12));
    invoke_play(base+0xfc200,sound,names,&handle,1,0x3f800000,0);
    assert(handle==512 && emitter_calls==1);
    memcpy(base+0x28aeb0,emitter_saved,5);
    assert(FlushInstructionCache(GetCurrentProcess(),base+0x28aeb0,5) && methods(base));
    puts("PASS retail distance rejection with stale emitter, corrected replica origin and closed/foreign-owner refusal");
    /* A sound authored on a locator uses its current bone and body pose,
     * including body rotation; an invalid locator cannot mutate the emitter. */
    uint8_t wrapper[0x14]={0},object[0xd0]={0},renderer[0x10]={0},model[0x54]={0};
    uint8_t model_header[0x18]={0},locators[0xa0]={0};float poses[32]={0},origin[3];
    float *world=(float *)(object+0x90);
    ptr(position+0xb4,wrapper);ptr(wrapper+8,object);ptr(wrapper+0xc,renderer);ptr(wrapper+0x10,renderer);
    ptr(object,base+0x2dd700);ptr(object+0x14,renderer);ptr(renderer,base+0x2df8ec);
    ptr(renderer+8,model);ptr(renderer+0xc,poses);ptr(model+0x1c,model_header);ptr(model+0x50,locators);
    num(model_header+4,2);num(model_header+0x14,2);num(locators+0x50,1);
    poses[16]=poses[21]=poses[26]=poses[31]=1;poses[28]=2;poses[29]=3;poses[30]=4;
    world[2]=1;world[5]=1;world[8]=-1;world[15]=1;world[12]=10;world[13]=20;world[14]=30;
    num(sound+0xd4,1);
    assert(presented_origin(&a,origin) && origin[0]==6 && origin[1]==23 && origin[2]==32);
    num(locators+0x50,2);assert(!refresh_position(2,&a,playback_exact,NULL));
    assert(!memcmp(sound+0x84,presented_xyz,12));
    num(sound+0xd4,0);ptr(position+0xb4,NULL);
    puts("PASS authored sound locator follows presented bone/body pose; invalid locator leaves native state untouched");
    assert(SudekiMpInstallInlineHook(&play_hook,base+0xfc200,base+0xfc200,7,play_detour));
    void *saved_trampoline=play_hook.trampoline;play_hook.trampoline=native_play_stub;
    frame=(SudekiMpStoryAudio){.epoch=1,.revision=1,.tick=1000,.stream=1,.bank=digest(bank_name),.character=2,.count=1};
    frame.cues[0]=(SudekiMpStoryAudioCue){900,1000,digest(names),0x3f800000,0,1,0};
    uint32_t calls_before=stub_calls;
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==calls_before+1);
    /* A held shield must continue to follow its owner after starting. Run
     * the retail attachment position/velocity leaves on an empty owned list. */
    uint8_t attachment[0x54]={0},attached_head[12]={0};
    ptr(attachment,base+0x2e1f34);num(attachment+4,1);ptr(attachment+0x48,attached_head);
    ptr(attached_head,attached_head);ptr(attached_head+4,attached_head);
    ptr(sound+0x30,attachment);ptr(manager+0x38,sound+0x30);
    presented_xyz[0]+=5;memcpy(position+0x18,presented_xyz,12);
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==calls_before+1);
    assert(!memcmp(attachment+0xc,presented_xyz,12) && !memcmp(sound+0x84,presented_xyz,12));
    num(attachment+0x4c,1);presented_xyz[0]+=5;memcpy(position+0x18,presented_xyz,12);
    assert(!SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL));
    assert(memcmp(sound+0x84,presented_xyz,12));num(attachment+0x4c,0);
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL));
    assert(!memcmp(attachment+0xc,presented_xyz,12));
    ptr(sound+0x30,NULL);ptr(manager+0x38,NULL);
    puts("PASS active emitter follows movement without replay; foreign attachment list refuses before mutation");
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==calls_before+1);
    entries[0]=2;ptr(entries+0xc,(void *)1); /* finished locally; remote still owns cue */
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==calls_before+1);
    frame.count=0;assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL));
    assert(!SudekiMpStoryCharacterAudioRetains());
    local[2][0]=(LocalCue){.remote=901,.handle=512,.pool=pool,.bank=(void *)1,.cue=(void *)1};
    assert(SudekiMpStoryCharacterAudioStop(2,playback_exact,NULL) && !SudekiMpStoryCharacterAudioRetains());
    admitted=FALSE;frame.count=1;
    assert(!SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==calls_before+1);
    admitted=TRUE;
    puts("PASS replay deduplication, completed-local/active-host suppression, expired bank cleanup and closed admission");
    frame.count=0;frame.start_sequence=1;frame.start_tick=frame.tick;
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==1);
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==1);
    frame.start_sequence=2;frame.start_tick=frame.tick-251;
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==1);
    frame.start_tick=frame.tick;
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==1);
    frame.start_sequence=3;admitted=FALSE;
    assert(!SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==1);admitted=TRUE;
    assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && startup_calls==2);
    puts("PASS witnessed Spirit startup once per cast, retransmit, late-drop and closed admission");
    {   /* H1b door spot cue through the CSound spot emitter. */
        uint8_t csound[0x2c]={0},spot[0x14]={0};char cue[]="BlockStart";
        ptr(csound,base+0x2c6238);ptr(csound+8,base+0x2c6240);ptr(csound+0x18,spot);
        ptr(spot,base+0x2e1db4);ptr(spot+4,bank);spot[0x10]=1;
        ptr(base+0x408d40,csound);spot_pool=pool;spot_entry=(void *)spot_emitter_stub;
        snprintf(names,sizeof(names),"%s","BlockStart");
        unsigned generation=u32(pool+4);
        host_mode=TRUE;assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));host_mode=FALSE;
        ptr(csound,base);assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));ptr(csound,base+0x2c6238);
        ptr(csound+0x18,NULL);assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));ptr(csound+0x18,spot);
        spot[0x10]=0;assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));spot[0x10]=1;
        ptr(spot+8,spot);assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));ptr(spot+8,NULL);
        assert(!SudekiMpStoryCharacterAudioPlaySpot("generic_wooden_door")); /* not in this bank */
        assert(!SudekiMpStoryCharacterAudioPlaySpot("blockstart"));          /* exact case */
        categories[0x18]=1;assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));categories[0x18]=0;
        num(spot+0xc,2);assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));num(spot+0xc,0);
        base[0x280b90]^=1;assert(!SudekiMpStoryCharacterAudioPlaySpot(cue));base[0x280b90]^=1;
        assert(!spot_calls && u32(pool+4)==generation);
        assert(SudekiMpStoryCharacterAudioPlaySpot(cue) && spot_calls==1 && u32(pool+4)==generation+1);
        assert(spot_abi[0]==(uint32_t)(uintptr_t)spot && spot_abi[1]==(uint32_t)(uintptr_t)cue &&
            !spot_abi[2] && spot_abi[3]==1 && !spot_abi[4] && !spot_abi[5]);
        spot_entry=NULL;ptr(base+0x408d40,NULL);
        puts("PASS door spot cue: PlayCue's EmitterPlay ABI, spot emitter/bank/cue/capacity proofs and refusals before any call");
    }

    const char *banks_by_character[]={"BUKISoundScript.xsb","ELCOSoundScript.xsb","TalSoundScript.xsb","ALICESoundScript.xsb"};
    const char *shared_cues[]={"hit_sml","hit_med","hit_lrg","death"};
    const char *melee_cues[]={"attack_sml","attack_lrg","whooshmedium","BlockStart","BlockEnd"};
    for(unsigned c=0;c<4;++c) {
        memset(&roster,0,sizeof(roster));roster.epoch=2;roster.revision=2;
        roster.available_mask=1u<<c;roster.actors[c]=actor;
        snprintf(bank_name,sizeof(bank_name),"%s",banks_by_character[c]);
        unsigned total=4u+((c==0 || c==2)?5u:1u);
        for(unsigned j=0;j<total;++j) {
            const char *name=j<4?shared_cues[j]:(c==0 || c==2)?melee_cues[j-4]:
                c==3?"attack_sml":"whooshmedium";
            snprintf(names,sizeof(names),"%s",name);
            entries[0]=1;num(entries+8,2);ptr(entries+0xc,cue);
            host_mode=TRUE;SudekiMpStoryCharacterAudioBind(&roster);
            handle=512;observe_play(sound,names,&handle,1,0x3f400000,0);
            assert(SudekiMpStoryCharacterAudioCapture(c,&frame) && frame.count==1 && frame.character==c);
            assert(frame.bank==digest(bank_name) && frame.cues[0].hash==digest(names));
            /* Use a unique remote event while retaining this inert native slot. */
            frame.cues[0].id=1000+c*100+j;host_mode=FALSE;
            memcpy(sound+0x84,stale_xyz,12);memcpy(sound+0x34,stale_xyz,12);
            uint32_t before=stub_calls;
            assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==before+1);
            assert(abi[4]==0x3f400000 && !memcmp(sound+0x84,position+0x18,12));
            assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL) && stub_calls==before+1);
            entries[0]=2;frame.count=0;
            assert(SudekiMpStoryCharacterAudioPresent(&roster,&frame,playback_exact,NULL));
            host_mode=TRUE;assert(SudekiMpStoryCharacterAudioCapture(c,&frame) && !frame.count);host_mode=FALSE;
        }
        printf("PASS roster character %u bank identity, hurt/death%s, native gain and no duplicate playback\n",
            c,(c==0 || c==2)?", attacks and shield start/end":", action cues");
    }
    play_hook.trampoline=saved_trampoline;assert(SudekiMpRestoreInlineHook(&play_hook));
    log_enabled=getenv("SUDEKIMP_TEST_LOG")!=NULL;
    shot_tests();
    free(backend);VirtualFree(base,0,MEM_RELEASE);return 0;
}
