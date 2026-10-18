/* [SudekiMP] Shot-cue resolver on the exact image (sha256 8ceb1d3c...), mapped
 * at a foreign base with its relocations applied: ImageMatches holds, every
 * guarded code byte, vtable-slot byte, literal byte and identity header byte
 * refuses it, and an unrelocated operand refuses it. The item database,
 * string table and world registry globals are .bss (zero in the file), so
 * the real cue names are runtime data and cannot be read from the file; the
 * resolve paths run on synthetic objects that use this image's real vtable
 * identities, and one flipped guard byte silences them. No native routine
 * executes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_shot_cues.c"
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"

static IMAGE_NT_HEADERS32 *headers;
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);
    headers=(void *)(b+((IMAGE_DOS_HEADER *)b)->e_lfanew);
    return b;
}
static uint8_t *b;
static unsigned flips;
static void refuses_each_byte(uint32_t rva,size_t n) {
    for(size_t i=0;i<n;++i) {
        b[rva+i]^=0x01u;assert(!SudekiMpStoryShotCuesImageMatches((HMODULE)b));
        b[rva+i]^=0x81u;assert(!SudekiMpStoryShotCuesImageMatches((HMODULE)b));
        b[rva+i]^=0x80u;++flips;
    }
}
static void putp(void *p,const void *v) {memcpy(p,&v,sizeof(v));}
static void put32(void *p,uint32_t v) {memcpy(p,&v,4);}
static BOOL bss(uint32_t rva) {
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(headers);
    for(unsigned i=0;i<headers->FileHeader.NumberOfSections;++i)
        if(rva>=s[i].VirtualAddress && rva<s[i].VirtualAddress+s[i].Misc.VirtualSize)
            return rva-s[i].VirtualAddress>=s[i].SizeOfRawData;
    return FALSE;
}
int main(int argc,char **argv) {
    wchar_t path[1024];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    b=map_image(path);
    assert((uintptr_t)b!=0x400000u && SudekiMpStoryShotCuesImageMatches((HMODULE)b));
    /* Every guarded code byte, including each relocated operand byte. */
    unsigned reloc_slots=0;
    for(unsigned i=0;i<sizeof(contracts)/sizeof(contracts[0]);++i) {
        refuses_each_byte(contracts[i].rva,contracts[i].size);
        for(unsigned k=0;k<3u && contracts[i].reloc[k];++k) {
            /* The file's unrelocated operand must not pass at a foreign base. */
            uint8_t *slot=b+contracts[i].rva+contracts[i].reloc[k]-1u;uint32_t keep;memcpy(&keep,slot,4);
            put32(slot,keep-(uint32_t)((uintptr_t)b-0x400000u));assert(!SudekiMpStoryShotCuesImageMatches((HMODULE)b));
            memcpy(slot,&keep,4);++reloc_slots;
        }
    }
    for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i) refuses_each_byte(slots[i].slot,4);
    for(unsigned i=0;i<sizeof(literals)/sizeof(literals[0]);++i)
        refuses_each_byte(literals[i].rva,strlen(literals[i].text)+1u);
    refuses_each_byte((uint32_t)((uint8_t *)&headers->FileHeader.TimeDateStamp-b),4);
    refuses_each_byte((uint32_t)((uint8_t *)&headers->OptionalHeader.SizeOfImage-b),4);
    assert(SudekiMpStoryShotCuesImageMatches((HMODULE)b));

    /* The cue data is runtime state: the item database, string table and
     * world registry globals are uninitialised data in the file. */
    assert(bss(RVA_ITEM_DB) && bss(RVA_STRING_TABLE) && bss(RVA_WORLD_REGISTRY));
    assert(!ptr(b+RVA_ITEM_DB) && !ptr(b+RVA_STRING_TABLE) && !ptr(b+RVA_WORLD_REGISTRY));
    SudekiMpStoryShotCueNames out;float at[3]={-92.9f,8.0f,-152.5f};
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,&out)==SUDEKIMP_SHOT_CUES_SILENT && !out.travel[0] && !out.fire[0]);
    assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,183,at,&out)==SUDEKIMP_SHOT_CUES_SILENT);

    /* Synthetic objects with this image's identities (names as the game data
     * holds them; not read from the image). */
    static uint8_t db[0x1000],item12[0x130],item17[0x130],table[0x14],*array[4],entries[4][64];
    putp(db,b+VT_ITEM_DB);putp(db+12+12*4,item12);putp(db+12+17*4,item17);
    putp(item12,b+VT_WEAPON_ITEM);put32(item12+0x14,12);put32(item12+0x88,5);put32(item12+0x120,1);put32(item12+0x124,2);
    putp(item17,b+VT_WEAPON_ITEM);put32(item17+0x14,17);put32(item17+0x88,5);put32(item17+0x120,0xffffffffu);put32(item17+0x124,3);
    put32(entries[0],0x80000000u);
    put32(entries[1],0x80000000u);memcpy(entries[1]+4,"Royal_Sceptre_projectile",25);
    put32(entries[2],18u);putp(entries[2]+4,"Royal_Sceptre_fire");
    put32(entries[3],0x80000000u);memcpy(entries[3]+4,"Xurics_Shock_staff",19);
    for(unsigned i=0;i<4;++i) array[i]=entries[i];
    putp(table,b+VT_STRING_TABLE);put32(table+8,4);putp(table+0x10,array);
    putp(b+RVA_ITEM_DB,db);putp(b+RVA_STRING_TABLE,table);
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,&out)==SUDEKIMP_SHOT_CUES_OK &&
        !strcmp(out.travel,"Royal_Sceptre_projectile") && !strcmp(out.fire,"Royal_Sceptre_fire"));
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,17,&out)==SUDEKIMP_SHOT_CUES_OK &&
        !out.travel[0] && !strcmp(out.fire,"Xurics_Shock_staff"));
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,1,12,&out)==SUDEKIMP_SHOT_CUES_SILENT);
    uint8_t *e=calloc(1,0x1690),*p=calloc(1,0x30),*w=e+0x1210,*mm=e+0x1114,*combo=calloc(1,0xc4),*rows[1]={combo};
    static uint8_t manager[0x40];static void *registry[2];
    assert(e && p && combo);
    putp(e,b+VT_MONSTER);putp(e+8,b+VT_MONSTER_NODE);putp(e+0x44,p);putp(e+0xc0,w);putp(e+0xbc,mm);
    putp(p,b+VT_POSITION);putp(p+0x10,e);float pos[3]={-93.8f,8.0f,-152.45f};memcpy(p+0x18,pos,12);
    putp(w,b+VT_WEAPON);putp(w+0x10,e);w[0x14]=0x1f;
    put32(w+0x3bc,0x80000000u);memcpy(w+0x3c0,"kurianKnight_projectile",24);
    put32(w+0x3dc,0x80000000u);memcpy(w+0x3e0,"kurian_knight_fire_effect",26);
    putp(mm,b+VT_MISSILE_MANAGER);putp(mm+0x10,e);mm[0x14]=0x1e;put32(mm+0x44,1);putp(mm+0x4c,rows);
    putp(combo,b+VT_COMBO);putp(combo+0xc,b+VT_MISSILE_DATA);put32(combo+0x30,0xfa9u);put32(combo+0x34,0x75d53da9u);
    registry[0]=e;put32(manager+0x34,1);putp(manager+0x3c,registry);putp(b+RVA_WORLD_REGISTRY,manager);
    assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,183,at,&out)==SUDEKIMP_SHOT_CUES_OK &&
        !strcmp(out.travel,"kurianKnight_projectile") && !strcmp(out.fire,"kurian_knight_fire_effect"));
    assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,231,at,&out)==SUDEKIMP_SHOT_CUES_SILENT);
    /* The image check runs inside each resolve. */
    for(unsigned i=0;i<sizeof(contracts)/sizeof(contracts[0]);++i) {
        b[contracts[i].rva]^=1u;
        assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,&out)==SUDEKIMP_SHOT_CUES_SILENT && !out.travel[0] && !out.fire[0]);
        assert(SudekiMpStoryShotCuesForWorldMissile((HMODULE)b,183,at,&out)==SUDEKIMP_SHOT_CUES_SILENT && !out.travel[0]);
        b[contracts[i].rva]^=1u;
    }
    assert(SudekiMpStoryShotCuesForItem((HMODULE)b,3,12,&out)==SUDEKIMP_SHOT_CUES_OK);
    unsigned code_bytes=0;
    for(unsigned i=0;i<sizeof(contracts)/sizeof(contracts[0]);++i) code_bytes+=contracts[i].size;
    printf("StoryShotCuesImageTest: PASS (%u contracts / %u code bytes, %u relocated operands, %u vtable slots, "
        "%u literals, identity header; %u guarded bytes each refused; item db/string table/registry are .bss, "
        "so ForItem 12/17 names ran on synthetic objects with the real image identities; guard checked inside each resolve)\n",
        (unsigned)(sizeof(contracts)/sizeof(contracts[0])),code_bytes,reloc_slots,
        (unsigned)(sizeof(slots)/sizeof(slots[0])),(unsigned)(sizeof(literals)/sizeof(literals[0])),flips);
    VirtualFree(b,0,MEM_RELEASE);return 0;
}
