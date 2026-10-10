#include "hooks/story_spawn_sigil.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const uint32_t unrelated[]={
        0xb51df833u, /* SFXHT000_HIT_EFFECT_NORMAL.HOM */
        0xe3293133u, /* SFXLU001_LEVEL_UP.HOM */
        0x4e2c1433u, /* SFXPU103_ITEMFX_COLLECT.HOM */
        0x3026f213u,0xda7c3643u, /* Other live scene effects. */
        0u,0x7ffffu,0xffffffffu,
        0xaf48a1bfu /* Another spawn-named resource is not an admitted sigil. */
    };
    assert(SudekiMpStoryIsSpawnSigilResource(0x29u,0x7e8a6f3fu));
    assert(SudekiMpStoryIsSpawnSigilResource(0x0fa9u,0x7e8a6f3fu));
    assert(SudekiMpStoryIsSpawnSigilResource(0x55554fa9u,0x7e8a6f3fu));
    for(uint32_t kind=0;kind<128u;++kind)
        assert(SudekiMpStoryIsSpawnSigilResource(kind,0x7e8a6f3fu)==(kind==0x29u));
    for(unsigned i=0;i<sizeof(unrelated)/sizeof(unrelated[0]);++i)
        assert(!SudekiMpStoryIsSpawnSigilResource(0x29u,unrelated[i]));
    for(unsigned bit=0;bit<32u;++bit)
        assert(!SudekiMpStoryIsSpawnSigilResource(0x29u,0x7e8a6f3fu^(UINT32_C(1)<<bit)));
    puts("Story spawn sigil predicate tests: PASS");
    return 0;
}
