#include "engine/story_placement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static SudekiMpStoryPlacementCatalog catalog,before;
static SudekiMpStoryPlacement rows[SUDEKIMP_STORY_PLACEMENT_MAX];
static SudekiMpStoryPlacementScope scope(void) {
    SudekiMpStoryPlacementScope s={.save_identity={1},.visit=9};
    strcpy(s.world,"area_a"); return s;
}
static SudekiMpStoryPlacement placement(float x) {
    SudekiMpStoryPlacement p; float pos[3]={x,2,3},forward[3]={0,0,1};
    uint32_t anchor; memcpy(&anchor,&x,4); char name[40]; snprintf(name,sizeof(name),"Spawn%08lx",(unsigned long)anchor);
    assert(SudekiMpStoryPlacementMake(&p,1,anchor,name,123,"Barrel_Type_A",pos,forward)); return p;
}
static void matching(void) {
    SudekiMpStoryPlacementScope s=scope();
    rows[0]=placement(10); rows[1]=placement(20); rows[2]=placement(30);
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,3));
    before=catalog;
    SudekiMpStoryPlacement swap=rows[0]; rows[0]=rows[2]; rows[2]=swap;
    uint16_t map[3]={99,99,99};
    assert(SudekiMpStoryPlacementMatch(&catalog,&s,rows,3,map,3));
    assert(map[0]==2 && map[1]==1 && map[2]==0);
    for(unsigned i=0;i<3;++i) {
        uint64_t source=0; assert(SudekiMpStoryPlacementSource(&catalog,&rows[map[i]],&source));
        assert(source==i+1);
    }
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,3));
    assert(!memcmp(&before,&catalog,sizeof(catalog)));
    /* Removing a destroyed object cannot renumber the original identities. */
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2));
    assert(!SudekiMpStoryPlacementMatch(&catalog,&s,rows,2,map,3));
    assert(!memcmp(&before,&catalog,sizeof(catalog)));
    uint64_t source=0; assert(SudekiMpStoryPlacementSource(&catalog,&rows[2],&source) && source==1);
    SudekiMpStoryPlacementScope wrong=s; ++wrong.visit;
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&wrong,rows,3));
    assert(!SudekiMpStoryPlacementMatch(&catalog,&wrong,rows,3,map,3));
    wrong=s; wrong.save_identity[0]=2;
    assert(!SudekiMpStoryPlacementMatch(&catalog,&wrong,rows,3,map,3));
    wrong=s; strcpy(wrong.world,"area_b");
    assert(!SudekiMpStoryPlacementMatch(&catalog,&wrong,rows,3,map,3));
    wrong=s; strcpy(wrong.temporary,"interior");
    assert(!SudekiMpStoryPlacementMatch(&catalog,&wrong,rows,3,map,3));
    assert(!SudekiMpStoryPlacementMatch(&catalog,&s,rows,3,map,2));
    uint16_t saved[3]; memcpy(saved,map,sizeof(map));
    rows[0]=rows[1];
    assert(!SudekiMpStoryPlacementMatch(&catalog,&s,rows,3,map,3));
    assert(!memcmp(saved,map,sizeof(map)));
    rows[0]=placement(30.00001f);
    assert(!SudekiMpStoryPlacementMatch(&catalog,&s,rows,3,map,3)); /* no nearest fallback */
    source=999; assert(!SudekiMpStoryPlacementSource(&catalog,&rows[0],&source) && source==999);
    assert(!memcmp(&before,&catalog,sizeof(catalog)));
}
static void ambiguous_and_precision(void) {
    memset(&catalog,0,sizeof(catalog)); before=catalog;
    SudekiMpStoryPlacementScope s=scope();
    rows[0]=placement(1); rows[1]=rows[0];
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2));
    assert(!memcmp(&before,&catalog,sizeof(catalog)));
    rows[1]=placement(nextafterf(1,2));
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2)); /* distinct float, no rounding */
    memset(&catalog,0,sizeof(catalog));
    rows[1]=rows[0]; strcpy(rows[1].anchor_name,"Spawn_Hash_Alias");
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2)); /* same hash, different authored text */
    memset(&catalog,0,sizeof(catalog));
    rows[1]=rows[0]; strcpy(rows[1].name,"Barrel_Hash_Alias");
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2)); /* conflicting definition for one spawn */
    rows[1]=rows[0]; rows[1].position[0]=0x40000000u;
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2)); /* one anchor cannot move/rebind */
    rows[1]=rows[0]; ++rows[1].anchor; strcpy(rows[1].anchor_name,"Overlapping_But_Distinct_Spawn");
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,2)); /* exact authored identities resolve overlap */
    float pos[3]={-0.0f,2,3},forward[3]={-0.0f,0,1};
    SudekiMpStoryPlacement zero;
    assert(SudekiMpStoryPlacementMake(&zero,1,55,"Spawn55",123,"Barrel_Type_A",pos,forward));
    assert(!zero.position[0] && !zero.forward[0]);
    SudekiMpStoryPlacement saved=zero;
    pos[0]=NAN; assert(!SudekiMpStoryPlacementMake(&zero,1,55,"Spawn55",123,"Barrel_Type_A",pos,forward));
    assert(!memcmp(&saved,&zero,sizeof(zero)));
    pos[0]=0; forward[2]=0;
    assert(!SudekiMpStoryPlacementMake(&zero,1,55,"Spawn55",123,"Barrel_Type_A",pos,forward));
    zero=saved; zero.position[0]=0x80000000u; assert(!SudekiMpStoryPlacementValid(&zero));
    zero=saved; zero.name[sizeof(zero.name)-1]='X'; assert(!SudekiMpStoryPlacementValid(&zero));
}
static void bounds(void) {
    memset(&catalog,0,sizeof(catalog)); SudekiMpStoryPlacementScope s=scope();
    for(unsigned i=0;i<SUDEKIMP_STORY_PLACEMENT_MAX;++i) rows[i]=placement((float)i);
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,SUDEKIMP_STORY_PLACEMENT_MAX));
    uint16_t map[SUDEKIMP_STORY_PLACEMENT_MAX];
    assert(SudekiMpStoryPlacementMatch(&catalog,&s,rows,SUDEKIMP_STORY_PLACEMENT_MAX,map,SUDEKIMP_STORY_PLACEMENT_MAX));
    before=catalog;
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,SUDEKIMP_STORY_PLACEMENT_MAX+1));
    assert(!memcmp(&catalog,&before,sizeof(catalog)));
    memset(&catalog,0,sizeof(catalog));
    assert(SudekiMpStoryPlacementCatalogBind(&catalog,&s,NULL,0));
    assert(SudekiMpStoryPlacementMatch(&catalog,&s,NULL,0,NULL,0));
    assert(!SudekiMpStoryPlacementCatalogBind(&catalog,&s,rows,1)); /* empty is still frozen */
}
int main(void) {
    matching(); ambiguous_and_precision(); bounds();
    puts("story placement catalog tests passed (exact matching, immutable IDs, no native mutation)"); return 0;
}
