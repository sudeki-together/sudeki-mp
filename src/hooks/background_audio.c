#include "hooks/background_audio.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

/* `mov dword [esp+disp],0x000100A0`: the DSBUFFERDESC.dwFlags store before
 * each CreateSoundBuffer; byte 5 is bits 8..15 of the immediate. */
static const struct { uint32_t rva; uint8_t disp; } flag_sites[]={
    {0x28230bu,0x34u},{0x2824d3u,0x28u},{0x282989u,0x34u},
};
/* WM_KILLFOCUS: `test eax,eax / je / push 1 / push 0xFF / push eax / call
 * 0x2853B0` (XACT pause, all categories) and `test eax,eax / je / push 1 /
 * push eax / call [BinkPause]`. The pause argument is the byte after 6A. */
static const uint8_t xact_pause_site[]={0x85u,0xc0u,0x74u,0x0du,0x6au,0x01u,0x68u,0xffu,0x00u,0x00u,0x00u,0x50u,0xe8u};
static const uint8_t bink_pause_site[]={0x85u,0xc0u,0x74u,0x09u,0x6au,0x01u,0x50u,0xffu,0x15u};
enum { FLAG_SITES=sizeof(flag_sites)/sizeof(flag_sites[0]), FLAGS_BYTE=5u, NATIVE_FLAGS_BYTE=0x00u, GLOBALFOCUS_BYTE=0x80u,
    RVA_XACT_PAUSE_SITE=0x28d75du, RVA_XACT_PAUSE=0x2853b0u, RVA_BINK_PAUSE_SITE=0x28d773u, RVA_BINK_PAUSE_IAT=0x29a274u,
    PAUSE_ARG=5u, PATCHES=FLAG_SITES+2u };
static SudekiMpBytePatch patches[PATCHES];
static BOOL installed;

static BOOL readable(const uint8_t *p,size_t n) { return p && !IsBadReadPtr(p,n); }
static BOOL flag_site_exact(const uint8_t *b,unsigned i) {
    const uint8_t expected[8]={0xc7u,0x44u,0x24u,flag_sites[i].disp,0xa0u,NATIVE_FLAGS_BYTE,0x01u,0x00u};
    return readable(b+flag_sites[i].rva,sizeof(expected)) && !memcmp(b+flag_sites[i].rva,expected,sizeof(expected));
}
static BOOL pause_sites_exact(const uint8_t *b) {
    const uint8_t *x=b+RVA_XACT_PAUSE_SITE,*k=b+RVA_BINK_PAUSE_SITE;
    if(!readable(x,sizeof(xact_pause_site)+4u) || memcmp(x,xact_pause_site,sizeof(xact_pause_site)) ||
        !readable(k,sizeof(bink_pause_site)+4u) || memcmp(k,bink_pause_site,sizeof(bink_pause_site))) return FALSE;
    int32_t rel; memcpy(&rel,x+sizeof(xact_pause_site),4);
    const uint8_t *call_end=x+sizeof(xact_pause_site)+4u;
    uint32_t iat; memcpy(&iat,k+sizeof(bink_pause_site),4);
    return call_end+rel==b+RVA_XACT_PAUSE && iat==(uint32_t)(uintptr_t)(b+RVA_BINK_PAUSE_IAT);
}
static BOOL image_exact(const uint8_t *b) {
    for(unsigned i=0;i<FLAG_SITES;++i) if(!flag_site_exact(b,i)) return FALSE;
    return pause_sites_exact(b);
}
BOOL SudekiMpBackgroundAudioInstalled(void) { return installed; }
BOOL SudekiMpInstallBackgroundAudio(HMODULE game_module) {
    uint8_t *b=(uint8_t *)game_module;
    if(installed) return TRUE;
    if(!b || !SudekiMpCheckLoadedExecutable(game_module) || !image_exact(b)) {
        SudekiMpLogWrite("background_audio event=refused reason=image\r\n");
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    uint8_t *targets[PATCHES]; uint8_t expected[PATCHES],replacement[PATCHES];
    for(unsigned i=0;i<FLAG_SITES;++i) {
        targets[i]=b+flag_sites[i].rva+FLAGS_BYTE; expected[i]=NATIVE_FLAGS_BYTE; replacement[i]=GLOBALFOCUS_BYTE;
    }
    targets[FLAG_SITES]=b+RVA_XACT_PAUSE_SITE+PAUSE_ARG; targets[FLAG_SITES+1u]=b+RVA_BINK_PAUSE_SITE+PAUSE_ARG;
    expected[FLAG_SITES]=expected[FLAG_SITES+1u]=1u; replacement[FLAG_SITES]=replacement[FLAG_SITES+1u]=0u;
    for(unsigned i=0;i<PATCHES;++i)
        if(!SudekiMpInstallBytePatch(&patches[i],targets[i],expected[i],replacement[i])) {
            while(i--) (void)SudekiMpRestoreBytePatch(&patches[i]);
            SudekiMpLogWrite("background_audio event=install_failed\r\n"); return FALSE;
        }
    installed=TRUE;
    SudekiMpLogWrite("background_audio event=installed buffer_sites=3 flag=DSBCAPS_GLOBALFOCUS focus_loss_pause=off\r\n");
    return TRUE;
}
BOOL SudekiMpUninstallBackgroundAudio(void) {
    if(!installed) return TRUE;
    BOOL ok=TRUE;
    for(unsigned i=PATCHES;i--;) if(!SudekiMpRestoreBytePatch(&patches[i])) ok=FALSE;
    if(ok) installed=FALSE;
    return ok;
}
