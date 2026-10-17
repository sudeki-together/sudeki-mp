/* H1b / D11: a host player's shopkeeper conversation is sent to clients as
 * the idle presentation record. Pure: keeper table, privacy step, record. */
#include "hooks/lan_story_shopkeepers.h"
#include "network/lan_story_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Matches() is not used here; the party frame codec stays out of this test. */
BOOL SudekiMpLanStoryFrameValid(const SudekiMpLanStoryFrame *frame) { (void)frame; return FALSE; }

static void keeper_table(void) {
    assert(SudekiMpLanStoryShopkeeperIs("NPC_Kamo_NB") && SudekiMpLanStoryShopkeeperIs("npc_kamo_nb"));
    assert(!SudekiMpLanStoryShopkeeperIs("NPC_Kamo_NB2") && !SudekiMpLanStoryShopkeeperIs("Kamo") &&
        !SudekiMpLanStoryShopkeeperIs("NB_Kamo_Shop_Entry_Trig") && !SudekiMpLanStoryShopkeeperIs("") &&
        !SudekiMpLanStoryShopkeeperIs(NULL));
    const SudekiMpLanStoryShopkeeper *k=SudekiMpLanStoryShopkeeperOf("lnbr_kamo_shop");
    assert(k && !strcmp(k->npc,"NPC_Kamo_NB"));
    assert(!SudekiMpLanStoryShopkeeperOf("LNBr_other") && !SudekiMpLanStoryShopkeeperOf(NULL));
    puts("Shopkeeper table whole-name and zone lookup: PASS");
}
static void privacy_steps(void) {
    BOOL p=FALSE;
    p=SudekiMpLanStorySpeechPrivateStep(p,TRUE,TRUE,TRUE);   assert(p);    /* Kamo's line */
    p=SudekiMpLanStorySpeechPrivateStep(p,TRUE,FALSE,FALSE); assert(p);    /* question waits */
    /* An unobserved frame is not a step: the caller keeps p. */
    p=SudekiMpLanStorySpeechPrivateStep(p,TRUE,TRUE,TRUE);   assert(p);    /* farewell line */
    p=SudekiMpLanStorySpeechPrivateStep(p,FALSE,FALSE,FALSE); assert(!p);  /* conversation over */
    p=SudekiMpLanStorySpeechPrivateStep(p,TRUE,TRUE,FALSE);  assert(!p);   /* story line */
    p=SudekiMpLanStorySpeechPrivateStep(p,TRUE,FALSE,FALSE); assert(!p);
    /* A story line straight after a shopkeeper's, with no idle frame. */
    p=SudekiMpLanStorySpeechPrivateStep(TRUE,TRUE,TRUE,FALSE); assert(!p);
    /* Narration (no speaker) is classified non-keeper by the caller. */
    p=SudekiMpLanStorySpeechPrivateStep(FALSE,TRUE,TRUE,FALSE); assert(!p);
    puts("Speech privacy: keeper line private, question kept, idle or other speaker public: PASS");
}
static void idle_record(void) {
    static SudekiMpLanStoryPresentation f,d;
    static uint8_t bytes[SUDEKIMP_STORY_PRESENTATION_MAX_SIZE];
    size_t n=0;
    f.epoch=6; f.revision=7; f.host_tick=99; f.sequence=12; f.conversation_serial=3; f.line_serial=9;
    f.elapsed_ms=400; f.duration_ms=3000; f.letterbox=.1f; f.letterbox_bottom=.1f;
    f.flags=SUDEKIMP_STORY_DIALOGUE_ACTIVE|SUDEKIMP_STORY_LINE_ACTIVE|SUDEKIMP_STORY_SPEAKER_PRESENT|
        SUDEKIMP_STORY_CUE_PRESENT|SUDEKIMP_STORY_LETTERBOX_PRESENT;
    f.subtitle[0]='W'; f.subtitle[1]='e'; f.speaker[0]='K'; strcpy(f.cue,"kamo_2837");
    assert(SudekiMpLanStoryPresentationValid(&f));
    SudekiMpLanStoryPresentationClearDialogue(&f);
    assert(SudekiMpLanStoryPresentationValid(&f) && !f.flags && !f.letterbox && !f.letterbox_bottom &&
        !f.elapsed_ms && !f.duration_ms && !f.subtitle[0] && !f.speaker[0] && !f.cue[0]);
    assert(f.epoch==6 && f.revision==7 && f.host_tick==99 && f.sequence==12 &&
        f.conversation_serial==3 && f.line_serial==9);
    assert(SudekiMpLanStoryPresentationEncode(&f,bytes,sizeof(bytes),&n) && n==SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE);
    assert(SudekiMpLanStoryPresentationDecode(bytes,n,&d) && !memcmp(&d,&f,sizeof(f)));
    SudekiMpLanStoryPresentationClearDialogue(NULL);
    puts("Cleared shopkeeper frame is the valid idle record, identity and serials kept: PASS");
}
int main(void) {
    keeper_table(); privacy_steps(); idle_record();
    puts("StoryPresentationPrivacyTest: PASS");
    return 0;
}
