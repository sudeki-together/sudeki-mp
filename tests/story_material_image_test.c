/* Inert, hash-checked retail image and synthetic material/job graphs.
 * Exercises production validation and a test-owned Win32 critical section;
 * no retail resource, renderer, task, callback or gameplay method executes. */
#include "../src/hooks/lan_story_material.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *mapped;
static uint8_t child[0x2c],resource[0x30],manager[8],materials[2][0x40],description[0x14];
static uint8_t wrappers[2][8],backing_data[0x3c],jobs[2][0x1c],foreign_data[0x1000];
static void *originals[1],*overrides[1],*textures[2][1],*companion[2][1];
static uint8_t override_flags[1];
static CRITICAL_SECTION *queue_lock;
static HANDLE locked,release_lock;
void SudekiMpLogFormat(const char *format,...) { (void)format; }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
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
    free(raw); return base;
}

static void put(void *p,unsigned offset,void *value) { memcpy((uint8_t *)p+offset,&value,4); }
static void u32(void *p,unsigned offset,uint32_t value) { memcpy((uint8_t *)p+offset,&value,4); }
static void setup(BOOL with_texture,BOOL with_override) {
    memset(child,0,sizeof(child)); memset(resource,0,sizeof(resource)); memset(manager,0,sizeof(manager));
    memset(materials,0,sizeof(materials)); memset(description,0,sizeof(description));
    memset(wrappers,0,sizeof(wrappers)); memset(backing_data,0,sizeof(backing_data));
    memset(jobs,0,sizeof(jobs)); memset(mapped+0x363ee8,0,32*4);
    mapped[0x3c3137]=1;
    put(child,0,mapped+0x2dec74); put(child,8,mapped+0x2dedbc);
    put(child,0xc,mapped+0x2dedd8); put(child,0x10,resource); put(resource,0x2c,manager);
    if(!with_texture) return;
    u32(manager,0,1); put(manager,4,originals); originals[0]=materials[0];
    u32(description,4,with_override?2:1); u32(description,0xc,1);
    put(backing_data,4,foreign_data); u32(backing_data,0x20,with_override?0x20000:0x10000);
    u32(backing_data,0x24,with_override?2:1);
    for(unsigned i=0;i<2;++i) {
        put(materials[i],0,mapped+0x2deb7c); put(materials[i],4,description);
        put(materials[i],8,textures[i]); put(materials[i],0x10,companion[i]); u32(materials[i],0x14,1);
        textures[i][0]=wrappers[i]; put(wrappers[i],0,mapped+0x2dd80c); put(wrappers[i],4,backing_data);
    }
    if(with_override) {
        overrides[0]=materials[1]; override_flags[0]=2;
        put(child,0x20,overrides); put(child,0x24,override_flags);
    }
}
static void *foreign_bucket(unsigned bucket) {
    for(unsigned i=0;i<sizeof(foreign_data);++i)
        if(resource_bucket(foreign_data+i)==bucket) return foreign_data+i;
    assert(0); return NULL;
}
static void job(unsigned index,void *backing_pointer) {
    uint8_t *node=jobs[index]; memset(node,0,0x1c);
    put(node,0,mapped+0x2dd7ac); u32(node,0xc,1); put(node,0x10,backing_pointer);
    unsigned bucket=resource_bucket(backing_pointer);
    put(node,0x18,*(void **)(mapped+0x363ee8+bucket*4));
    put(mapped,0x363ee8+bucket*4,node);
}
static BOOL check(void) {
    return SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,child,resource);
}
static void refused(DWORD error) {
    assert(!check()); assert(GetLastError()==error);
    /* Every inspection releases its borrowed recursion level. */
    assert(queue_lock->RecursionCount==0);
}
static DWORD WINAPI hold_lock(void *unused) {
    (void)unused; EnterCriticalSection(queue_lock); assert(SetEvent(locked));
    assert(WaitForSingleObject(release_lock,5000)==WAIT_OBJECT_0);
    LeaveCriticalSection(queue_lock); return 0;
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused; assert(!check());
    assert(!SudekiMpLanStoryMaterialInitialize((HMODULE)mapped)); return 0;
}
static void *shader_allocations[128];
static unsigned shader_allocated,shader_freed;
static void * __attribute__((cdecl)) shader_allocate(size_t size) {
    void *p=calloc(1,size); assert(p && shader_allocated<128);
    shader_allocations[shader_allocated++]=p; return p;
}
static void __attribute__((cdecl)) shader_free(void *p) {
    if(!p) return;
    unsigned i=0; for(;i<shader_allocated && shader_allocations[i]!=p;++i) {}
    assert(i<shader_allocated); shader_allocations[i]=NULL; ++shader_freed; free(p);
}
static void allocator_jump(unsigned rva,void *target) {
    int32_t relative=(int32_t)((uintptr_t)target-(uintptr_t)(mapped+rva+5u));
    mapped[rva]=0xe9; memcpy(mapped+rva+1,&relative,4);
}
static void shader_outputs(void) {
    /* Execute the exact factory/getter/destructor on inert owned objects.
     * Only the process allocator is replaced by tracked test storage; no
     * renderer, gameplay, resource worker or network callback executes. */
    DWORD protect; assert(VirtualProtect(mapped,0x45f000,PAGE_EXECUTE_READWRITE,&protect));
    allocator_jump(0x2484fau,shader_allocate); allocator_jump(0x24844fu,shader_free);
    assert(FlushInstructionCache(GetCurrentProcess(),mapped,0x45f000));
    memset(mapped+0x36b8b0,0,256*4);
    put(mapped,0x3c3654,mapped+0x350040); put(mapped,0x350044,mapped+0x34fc00);
    put(mapped,0x34fc04,mapped+0x34fcb0); put(mapped,0x34fcb4,NULL);
    typedef uint8_t *(__attribute__((thiscall)) *Factory)(void *,uint32_t);
    typedef void *(__attribute__((thiscall)) *Property)(void *,unsigned,unsigned,uint32_t *);
    typedef void *(__attribute__((thiscall)) *Delete)(void *,unsigned);
    Factory factory=(Factory)(mapped+0x202a30); Property getter=(Property)(mapped+0x202e40);
    Delete destroy=(Delete)(mapped+0x2014b0);
    static const unsigned shifts[3]={2,6,8};
    static const unsigned orders[6][3]={{3,4,5},{3,5,4},{4,3,5},{4,5,3},{5,3,4},{5,4,3}};
    uint32_t seen[432]; unsigned seen_count=0;
    for(unsigned layout=0;layout<16;++layout) for(unsigned textured=0;textured<8;++textured) {
        setup(TRUE,TRUE); shader_allocated=shader_freed=0;
        uint32_t initial=0x80000001u | ((layout&1u)?0x10u:0u) |
            ((layout&2u)?0x400u:0u) | ((layout&4u)?0x800000u:0u) | ((layout&8u)?0x2000u:0u);
        for(unsigned i=0;i<3;++i) if(textured&(1u<<i)) initial|=1u<<shifts[i];
        uint8_t *original=factory(mapped+0x34fcb0,initial); assert(original);
        BOOL initial_seen=FALSE;
        for(unsigned i=0;i<seen_count;++i) if(seen[i]==initial) initial_seen=TRUE;
        if(!initial_seen) { assert(seen_count<432); seen[seen_count++]=initial; }
        u32(original,4,2); put(materials[0],4,original); put(materials[1],4,original);
        unsigned texture_count=*(uint32_t *)(original+0xc);
        uint8_t extra_wrappers[2][5][8]={0}; void *tex[2][6]={0},*aux[2][6]={0};
        u32(backing_data,0x20,(texture_count*2u)<<16); u32(backing_data,0x24,texture_count*2u);
        for(unsigned i=0;i<2;++i) {
            tex[i][0]=wrappers[i]; put(materials[i],8,tex[i]); put(materials[i],0x10,aux[i]);
            for(unsigned j=1;j<texture_count;++j) {
                tex[i][j]=extra_wrappers[i][j-1];
                put(tex[i][j],0,mapped+0x2dd80c); put(tex[i][j],4,backing_data);
            }
        }
        assert(sol_definition(original,FALSE) && check());
        assert(SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],3));
        assert(!SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],2));
        /* Invalid factory, cyclic cache or map is never a safe expansion. */
        void *old=*(void **)(mapped+0x350040); put(mapped,0x350040,mapped);
        if(textured!=7) assert(!SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],3));
        put(mapped,0x350040,old);
        put(original,0x24,original);
        assert(!SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],3));
        put(original,0x24,NULL);
        uint32_t *map=*(uint32_t **)(original+0x2c); map[1]=0;
        assert(!SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],3)); map[1]=UINT32_MAX;
        uint32_t final_flags=sol_color_flags(initial,7);
        if(final_flags!=initial) {
            uint8_t *precreated=factory(mapped+0x34fcb0,final_flags); assert(precreated);
            assert(sol_expansion_closure(initial)); /* Exact cached native reuse. */
            void *pre_map=*(void **)(precreated+0x28);
            put(precreated,0x28,*(void **)(original+0x28));
            assert(!SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],4));
            put(precreated,0x28,pre_map);
        }
        for(unsigned attempt=0;attempt<6;++attempt) {
            unsigned mask=0,colors=0; uint8_t *expanded=original;
            for(unsigned step=0;step<3;++step) {
                unsigned member=orders[attempt][step];
                assert(SudekiMpLanStoryMaterialShaderOutputExact((HMODULE)mapped,materials[1],materials[0],member));
                uint32_t flags=0; uint8_t before[0x40]; memcpy(before,materials[0],sizeof(before));
                void *constant=getter(materials[1],0,member,&flags);
                assert(!memcmp(before,materials[0],sizeof(before)));
                if(textured&(1u<<(member-3u))) {
                    assert(!constant && !flags); /* Native texture field is not converted. */
                } else {
                    assert(constant && flags==0x1010001u);
                    ++colors; mask|=1u<<(member-3u);
                    assert(!memcmp(constant,(float[4]){0,0,0,0},16));
                    float color[4]={(float)member,.5f,.75f,1}; memcpy(constant,color,16);
                }
                expanded=*(uint8_t **)(materials[1]+4u);
                assert(*(uint32_t *)(expanded+8)==sol_color_flags(initial,mask));
                assert(*(uint32_t *)(expanded+0x10)==colors && *(uint32_t *)(expanded+0xc)==texture_count);
                BOOL known=FALSE;
                for(unsigned i=0;i<seen_count;++i) if(seen[i]==*(uint32_t *)(expanded+8)) known=TRUE;
                if(!known) { assert(seen_count<432); seen[seen_count++]=*(uint32_t *)(expanded+8); }
                assert(check());
                if(colors) {
                    assert(expanded!=original && sol_derived(expanded,original));
                    assert(*(uint32_t *)(expanded+4)==1 && *(uint32_t *)(original+4)==1);
                    uint32_t material_flags=*(uint32_t *)(materials[1]+0xc);
                    u32(materials[1],0xc,material_flags|0x200u); assert(!check());
                    u32(materials[1],0xc,material_flags);
                    void *mapping=*(void **)(expanded+0x28); put(expanded,0x28,*(void **)(original+0x28));
                    assert(!check()); put(expanded,0x28,mapping); assert(check());
                    if(layout&1u) {
                        void *optional=*(void **)(expanded+0x20);
                        put(expanded,0x20,*(void **)(original+0x20)); assert(!check());
                        put(expanded,0x20,optional); assert(check());
                        uint32_t *words=optional; words[2]=2u; assert(!check());
                        words[2]=1u; assert(check());
                    }
                }
                for(unsigned prior=0;prior<=step;++prior) {
                    unsigned previous=orders[attempt][prior];
                    if(textured&(1u<<(previous-3u))) continue;
                    float expected[4]={(float)previous,.5f,.75f,1}; flags=0;
                    void *kept=getter(materials[1],0,previous,&flags);
                    assert(kept && flags==0x1000000u && !memcmp(kept,expected,16));
                }
                /* Expansion cannot replace or reorder any texture wrapper. */
                assert(*(void **)(materials[1]+8)==tex[1] && tex[1][0]==wrappers[1]);
                for(unsigned j=1;j<texture_count;++j) assert(tex[1][j]==extra_wrappers[1][j-1]);
            }
            if(expanded!=original) {
                shader_free(*(void **)(materials[1]+0x38)); put(materials[1],0x38,NULL);
                u32(expanded,4,0); destroy(expanded,1);
            }
            for(unsigned mask=1;mask<8;++mask) {
                uint32_t flags=sol_color_flags(initial,mask); if(flags==initial) continue;
                uint8_t *cached=(void *)1; assert(sol_cache(flags,&cached) && !cached);
            }
            put(materials[1],4,original); u32(original,4,2);
            assert(check() && sol_definition(original,FALSE));
        }
        /* Reject a different valid texture layout even if the count matches.
         * Both descriptors alone are native, but the pair is not a derivation. */
        if(textured==4) {
            uint8_t *wrong=factory(mapped+0x34fcb0,(initial&~0x3ccu)|0x4cu); u32(wrong,4,1);
            assert(sol_definition(wrong,FALSE) && !sol_derived(wrong,original));
            u32(wrong,4,0); destroy(wrong,1);
        }
        uint8_t *wrong_layout=factory(mapped+0x34fcb0,initial^0x800000u); u32(wrong_layout,4,1);
        assert(sol_definition(wrong_layout,FALSE) && !sol_derived(wrong_layout,original));
        u32(wrong_layout,4,0); destroy(wrong_layout,1);
        uint8_t *wrong_key=factory(mapped+0x34fcb0,initial^0x2000u); u32(wrong_key,4,1);
        assert(sol_definition(wrong_key,FALSE) && !sol_derived(wrong_key,original));
        u32(wrong_key,4,0); destroy(wrong_key,1);
        u32(original,4,0); destroy(original,1);
        assert(shader_allocated==shader_freed);
    }
    unsigned colors;
    assert(!sol_colors(0x80001001u,&colors)); /* Different descriptor family. */
    for(unsigned i=0;i<3;++i) assert(!sol_colors(0x80000001u|(2u<<shifts[i]),&colors));
    assert(!sol_colors(0x80000021u,&colors) && !sol_colors(0x80000c01u,&colors));
    assert(seen_count==432);
    puts("SOL members3..5: 128 authored layouts, all6 output orders,432 definitions, textures/colors/optional descriptors preserved, release/retry PASS");
}

int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    /* New synchronization/layout evidence participates in exact-build gating. */
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        mapped[codes[i].rva]^=1;
        assert(!SudekiMpLanStoryMaterialInitialize((HMODULE)mapped));
        assert(GetLastError()==ERROR_BAD_EXE_FORMAT && !verified_image);
        mapped[codes[i].rva]^=1;
    }
    static const struct {const char *name; unsigned rva,size;} shader_checks[]={
        {"ShaderConstantEnsure",0x2032c0u,457u},
        {"ShaderDefinitionLookup",0x1e76d0u,86u},
        {"SolDefinitionFactory",0x202a30u,501u},
        {"SolDefinitionDelete",0x2014b0u,90u},
        {"DefinitionUnlink",0x1e4080u,75u},
        {"BaseDefinitionFactory",0x1fb7c0u,147u},
        {"WaterDefinitionFactory",0x216630u,42u},
        {"TextureCount",0x201670u,83u},
        {"ConstantCount",0x2016d0u,86u},
        {"TextureMapping",0x201730u,148u},
        {"ConstantMapping",0x2017d0u,83u}
    };
    for(unsigned j=0;j<sizeof(shader_checks)/sizeof(shader_checks[0]);++j) {
        BOOL found=FALSE;
        for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
            if(codes[i].rva==shader_checks[j].rva && codes[i].size==shader_checks[j].size)
                found=code_exact(mapped,&codes[i]);
        assert(found);
        printf("Native byte check %s RVA %06x length %u: exact and mutation-rejected PASS\n",
            shader_checks[j].name,shader_checks[j].rva,shader_checks[j].size);
    }
    assert(SudekiMpLanStoryMaterialInitialize((HMODULE)mapped));
    queue_lock=(CRITICAL_SECTION *)(mapped+0x364148); InitializeCriticalSection(queue_lock);
    setup(FALSE,FALSE); assert(check());
    /* A successful walk never lends its memory permissions or graph result
     * to the next call, including failure/recovery and same-address reuse. */
    uint8_t *page=VirtualAlloc(NULL,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(page); memcpy(page,child,sizeof(child)); DWORD old_protect;
    assert(SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page,resource));
    put(page,0,NULL);
    assert(!SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page,resource));
    memcpy(page,child,sizeof(child));
    assert(VirtualProtect(page,4096,PAGE_READONLY,&old_protect));
    assert(!SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page,resource));
    assert(VirtualProtect(page,4096,PAGE_NOACCESS,&old_protect));
    assert(!SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page,resource));
    assert(VirtualProtect(page,4096,PAGE_READWRITE,&old_protect));
    assert(SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page,resource));
    memcpy(page+4096-16,child,sizeof(child));
    assert(VirtualProtect(page+4096,4096,PAGE_NOACCESS,&old_protect));
    assert(!SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,page+4096-16,resource));
    assert(!memory_scope && !memory_region_count);
    assert(VirtualFree(page,0,MEM_RELEASE));
    /* Empty graphs depend on no queue; textured graphs ignore unrelated jobs,
     * including a valid node in exactly the same hash bucket. */
    for(unsigned i=0;i<32;++i) {
        setup(FALSE,FALSE); job(0,foreign_bucket(i)); assert(check());
        setup(TRUE,FALSE); job(0,foreign_bucket(i)); assert(check());
        assert(queue_lock->RecursionCount==0);
    }
    setup(TRUE,TRUE); assert(check()); /* retirement preserves retained original */
    uint8_t saved_backing[sizeof(backing_data)],saved_materials[sizeof(materials)];
    memcpy(saved_backing,backing_data,sizeof(backing_data)); memcpy(saved_materials,materials,sizeof(materials));
    for(unsigned i=0;i<1000;++i) assert(check());
    assert(!memcmp(saved_backing,backing_data,sizeof(backing_data)));
    assert(!memcmp(saved_materials,materials,sizeof(materials)));
    setup(TRUE,FALSE); job(0,backing_data); refused(ERROR_IO_PENDING);
    /* Search past a hash collision to find an owned pending callback. */
    job(1,foreign_bucket(resource_bucket(backing_data))); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x14000); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x11000); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); put(backing_data,4,NULL); job(0,backing_data); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); assert(check()); /* next fresh inspection recovers */
    setup(TRUE,FALSE); put(backing_data,4,NULL); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x12000); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x24,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x24,0xffff); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x7fff0000); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(backing_data,0x24,1); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(backing_data,0x20,0x10000); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(description,4,1); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(description,4,UINT32_MAX); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); textures[1][0]=wrappers[0]; refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); overrides[0]=originals[0]; refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); put(backing_data,0xc,foreign_data); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); mapped[0x3c3137]=0; refused(ERROR_INVALID_STATE);
    setup(TRUE,FALSE); mapped[0x3c3137]=2; refused(ERROR_INVALID_STATE);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0,NULL); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x18,jobs[0]); refused(ERROR_INVALID_DATA); /* bounded cycle */
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x18,(void *)1); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x10,foreign_bucket((resource_bucket(backing_data)+1)&31)); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    u32(jobs[0],0xc,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE);
    locked=CreateEventW(NULL,TRUE,FALSE,NULL); release_lock=CreateEventW(NULL,TRUE,FALSE,NULL);
    assert(locked && release_lock);
    HANDLE worker=CreateThread(NULL,0,hold_lock,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(locked,5000)==WAIT_OBJECT_0);
    DWORD start=GetTickCount(); assert(!check() && GetLastError()==ERROR_IO_PENDING);
    assert(GetTickCount()-start<1000); /* cannot wait for the worker */
    assert(SetEvent(release_lock)); assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);
    CloseHandle(worker); CloseHandle(locked); CloseHandle(release_lock); assert(check());
    worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    assert(check() && queue_lock->RecursionCount==0);
    shader_outputs();
    DeleteCriticalSection(queue_lock); VirtualFree(mapped,0,MEM_RELEASE);
    puts("story material exact-image/resource-specific readiness tests passed (no native gameplay)");
    return 0;
}
