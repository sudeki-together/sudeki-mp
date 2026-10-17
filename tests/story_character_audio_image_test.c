#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "hooks/lan_story_character_audio.c"
void SudekiMpLogFormat(const char *format,...) {(void)format;}
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
    free(backend);VirtualFree(base,0,MEM_RELEASE);return 0;
}
