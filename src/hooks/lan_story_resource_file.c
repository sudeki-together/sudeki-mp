#include "hooks/lan_story_resource_file.h"
#include "engine/build_identity.h"
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Saved-story file observations require the supported x86 layout"
#endif

enum { MAX_MOUNTS=64u, MAX_BUCKET_ROWS=4096u };
typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
    {0x1e3620u,226u,0xb87c2052u},{0x02ae00u,56u,0x35082095u},
    {0x1ba3d0u,151u,0xe6edfef4u},{0x052c50u,3u,0xbb96d90bu},
    {0x1bd560u,117u,0x005a2ea0u},{0x1be100u,126u,0xcbae234bu},
    {0x02ad50u,9u,0x1c4d5843u},{0x1bd790u,7u,0x72eb5e91u},
    {0x1bd290u,107u,0xa0e1c010u},{0x1da1a0u,10u,0x6c462d4bu},
    {0x1bd200u,15u,0x7c46f411u},{0x02ad90u,47u,0x68e5a63au},
    {0x1bd280u,16u,0xa30a5a93u},{0x1bd240u,49u,0x0ee2dff8u}
};
static const Relocation relocations[]={
    {0x1e3622u,0x3c3610u},{0x1e3654u,0x3c3210u},{0x1e36abu,0x2cd1f4u},
    {0x1e36dfu,0x3c3610u},{0x02ae05u,0x3c30d0u},{0x02ae2au,0x2c7380u},
    {0x1ba3dbu,0x34e6ccu},{0x1ba3e1u,0x29a068u},{0x1ba431u,0x29a088u},
    {0x1ba439u,0x34e6ccu},{0x1ba442u,0x29a064u},{0x1ba452u,0x34e6ccu},
    {0x1ba458u,0x29a064u},{0x1bd566u,0x3c30e4u},{0x1bd58fu,0x2dce88u},
    {0x1bd2aau,0x29a068u},{0x1bd2bfu,0x29a0b0u},{0x1bd2dau,0x29a0dcu},
    {0x1bd2eau,0x29a064u},{0x02ad98u,0x2c7380u},{0x02adaau,0x2c7350u},
    {0x1bd249u,0x2dce88u},{0x1bd25cu,0x2c82b4u}
};
static uint8_t *verified_image;

static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t u32(const void *p) { uint32_t v; memcpy(&v,p,4u); return v; }
static uint8_t *pointer(const void *p) { return (uint8_t *)(uintptr_t)u32(p); }
static BOOL slot(unsigned table,unsigned offset,unsigned method) {
    return readable(verified_image+table+offset,4u) &&
        pointer(verified_image+table+offset)==verified_image+method;
}
static BOOL methods(void) {
    return slot(0x2c739cu,4u,0x02ae00u) &&
        slot(0x2dce3cu,0x20u,0x052c50u) && slot(0x2dce3cu,0x10u,0x1bd560u) &&
        slot(0x2c7380u,0u,0x02ad90u) && slot(0x2c7380u,4u,0x02ad50u) &&
        slot(0x2c7380u,8u,0x1da1a0u) &&
        slot(0x2dce88u,0u,0x1bd790u) && slot(0x2dce88u,4u,0x1bd280u) &&
        slot(0x2dce88u,0x10u,0x1bd200u) && slot(0x2dce88u,0x14u,0x1bd240u) &&
        slot(0x2dce88u,0x18u,0x1bd290u);
}
static BOOL code_exact(uint8_t *image,const CodeIdentity *code) {
    if(!readable(image+code->rva,code->size)) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<code->size;++i) {
        uint8_t byte=image[code->rva+i];
        for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
            unsigned offset=code->rva+i-relocations[r].rva;
            if(offset>=4u) continue;
            if(u32(image+relocations[r].rva)!=(uintptr_t)image+relocations[r].target)
                return FALSE;
            byte=(uint8_t)((0x400000u+relocations[r].target)>>(offset*8u)); break;
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==code->hash;
}
BOOL SudekiMpLanStoryResourceFileInitialize(HMODULE module) {
    uint8_t *image=(uint8_t *)module;
    if(!image || (verified_image && verified_image!=image) ||
        !SudekiMpCheckLoadedExecutable(module))
        return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!code_exact(image,&codes[i])) { SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE; }
    verified_image=image;
    if(!methods()) { verified_image=NULL; SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE; }
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryResourceFileExact(uint32_t handle,uint32_t offset,uint32_t size) {
    if(!verified_image || handle==UINT32_MAX ||
        !size || !methods() || !readable(verified_image+0x3c3610u,4u) ||
        !readable(verified_image+0x3c30d0u,4u) || !readable(verified_image+0x408db0u,4u))
        return FALSE;
    /* Asset acquisition is rare. Recheck the complete dispatch closure for
     * this call rather than reuse an initialization-time code witness. */
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!code_exact(verified_image,&codes[i])) return FALSE;
    uint8_t *adapter=pointer(verified_image+0x3c3610u);
    uint8_t *owner=pointer(verified_image+0x408db0u);
    uint8_t *manager=pointer(verified_image+0x3c30d0u);
    if(adapter!=verified_image+0x349b00u || !readable(adapter,4u) ||
        pointer(adapter)!=verified_image+0x2c739cu || !readable(owner,0xb8u) ||
        pointer(owner)!=verified_image+0x2c732cu || manager!=owner+8u ||
        pointer(manager)!=verified_image+0x2c7334u || !readable(manager,0x28u)) return FALSE;
    unsigned count=u32(manager+0x1cu);
    uint8_t *mounts=pointer(manager+0x24u);
    if(!count || count>MAX_MOUNTS || !readable(mounts,count*4u)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *mount=pointer(mounts+i*4u);
        if(!mount) continue;
        if(!readable(mount,12u)) return FALSE;
        uint8_t *archive=pointer(mount+8u);
        /* Unknown preceding mounts could execute arbitrary virtual callbacks.
         * Require every mount native lookup visits to be this exact class. */
        if(!readable(archive,0x834u) || pointer(archive)!=verified_image+0x2dce3cu ||
            pointer(archive+4u)!=verified_image+0x2dce70u) return FALSE;
        unsigned bucket=handle&255u,rows=u32(archive+0x410u+bucket*4u);
        uint8_t *entries=pointer(archive+0x10u+bucket*4u);
        if(rows>MAX_BUCKET_ROWS || (rows && !readable(entries,rows*12u))) return FALSE;
        const uint8_t *match=NULL;
        uint32_t previous=0;
        for(unsigned j=0;j<rows;++j) {
            const uint8_t *entry=entries+j*12u;
            uint32_t key=u32(entry+8u);
            if((key&255u)!=bucket || (j && key<=previous)) return FALSE;
            if(key==handle) match=entry;
            previous=key;
        }
        if(!match) continue;
        uint32_t start=u32(match),length=u32(match+4u),file=u32(archive+0x814u);
        return file && file!=UINT32_MAX && length && start<=UINT32_MAX-length &&
            offset<=length && size<=length-offset;
    }
    return FALSE;
}
