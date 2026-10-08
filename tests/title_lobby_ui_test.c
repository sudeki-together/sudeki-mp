/* Pure title-page fixture: plain lobby snapshots and captured commands only.
 * This does not prove native rendering, key dispatch, sockets or gameplay. */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static SHORT fixture_keys[256];
static SHORT fixture_key_state(int key) { return fixture_keys[key]; }
static HWND fixture_foreground(void) { return (HWND)(uintptr_t)1; }
#define GetAsyncKeyState fixture_key_state
#define GetForegroundWindow fixture_foreground
#include "../src/ui/title_lobby.c"
#undef GetAsyncKeyState
#undef GetForegroundWindow

static SudekiMpLobbyStatus fixture;
static unsigned selected_value, selected_calls, host_calls, start_calls;
static BOOL selected_lock, cancel_ok=TRUE;
static void refresh(void) { status=fixture; build_view(); }
static void reset_fixture(BOOL enabled) {
    memset(&fixture,0,sizeof(fixture));
    memset(fixture_keys,0,sizeof(fixture_keys));
    for (unsigned i=0;i<4;++i) fixture.members[i].character=SUDEKIMP_LOBBY_NO_CHARACTER;
    session=(SudekiMpLobby *)(uintptr_t)1;
    selected_calls=host_calls=start_calls=0;
    auto_enabled=auto_done=auto_started=auto_named=FALSE;
    auto_retry_at=0; cancel_ok=TRUE; message[0]=0;
    SudekiMpLobbyUiConfigureDevPlay(enabled,NULL);
    SudekiMpLobbyUiOpen();
}
static unsigned row_for(enum Action action) {
    for (unsigned i=0;i<view.count;++i) if (actions[i]==action) return i;
    return ~0u;
}
static void click(enum Action action) {
    unsigned row=row_for(action), selection=0;
    assert(row<view.count && (view.enabled&(1u<<row)));
    SudekiMpLobbyUiArm(row);
    assert(!SudekiMpLobbyUiCommit(&selection));
}
static void room_fixture(void) {
    fixture.phase=SUDEKIMP_LOBBY_HOSTING;
    fixture.start.destination=SUDEKIMP_LOBBY_DEST_SAVEDGAME;
    fixture.saved_game.leader=3;
    fixture.saved_game.party_mask=15;
    strcpy(fixture.saved_game.label,"Fixture save");
    fixture.members[0].present=fixture.members[0].reserved=1;
    strcpy(fixture.members[0].name,"Host");
    change_page(ROOM); refresh();
}
static void auto_tick(void) { auto_last=GetTickCount()-1000u; auto_step(); refresh(); }

SudekiMpLobby *SudekiMpLobbyCreate(void) { return (SudekiMpLobby *)(uintptr_t)1; }
BOOL SudekiMpLobbyDestroy(SudekiMpLobby *s) { (void)s; return TRUE; }
void SudekiMpLobbyStatusGet(SudekiMpLobby *s,SudekiMpLobbyStatus *out) { (void)s; *out=fixture; }
BOOL SudekiMpLobbySetMode(SudekiMpLobby *s,unsigned mode) {
    (void)s;
    if (fixture.phase!=SUDEKIMP_LOBBY_IDLE && fixture.phase!=SUDEKIMP_LOBBY_ERROR) return FALSE;
    fixture.mode=(uint8_t)mode; return TRUE;
}
void SudekiMpLobbyBrowse(SudekiMpLobby *s,BOOL active) { (void)s; (void)active; }
void SudekiMpLobbyLeave(SudekiMpLobby *s) { (void)s; fixture.phase=SUDEKIMP_LOBBY_IDLE; }
BOOL SudekiMpLobbyHost(SudekiMpLobby *s,const char *r,const char *n,uint16_t p,BOOL a) {
    (void)s; (void)n; (void)a; ++host_calls;
    fixture.phase=SUDEKIMP_LOBBY_HOSTING; fixture.port=p;
    snprintf(fixture.room,sizeof(fixture.room),"%s",r);
    fixture.members[0].present=fixture.members[0].reserved=1; return TRUE;
}
BOOL SudekiMpLobbyJoin(SudekiMpLobby *s,const char *ip,uint16_t p,const char *n) {
    (void)s; (void)ip; (void)p; (void)n; return FALSE;
}
BOOL SudekiMpLobbyReconnect(SudekiMpLobby *s) { (void)s; return FALSE; }
BOOL SudekiMpLobbySelectCharacter(SudekiMpLobby *s,unsigned c,BOOL lock) {
    (void)s; selected_value=c; selected_lock=lock; ++selected_calls;
    fixture.members[fixture.local_slot].character=(uint8_t)c;
    fixture.members[fixture.local_slot].locked=(uint8_t)lock; return TRUE;
}
BOOL SudekiMpLobbySetName(SudekiMpLobby *s,const char *name) { (void)s; (void)name; return TRUE; }
void SudekiMpLobbyReady(SudekiMpLobby *s,BOOL ready) { (void)s; fixture.members[fixture.local_slot].ready=(uint8_t)ready; }
BOOL SudekiMpLobbyAdvertise(SudekiMpLobby *s,BOOL e) { (void)s; fixture.advertised=(uint8_t)e; return TRUE; }
BOOL SudekiMpLobbyDestination(SudekiMpLobby *s,unsigned d) { (void)s; fixture.start.destination=(uint8_t)d; return TRUE; }
BOOL SudekiMpLobbySelectSavedGame(SudekiMpLobby *s,const SudekiMpLobbySavedGame *saved) { (void)s; (void)saved; return FALSE; }
BOOL SudekiMpLobbyEnableSavedStart(SudekiMpLobby *s,BOOL e) { (void)s; (void)e; return TRUE; }
BOOL SudekiMpLobbyStartGame(SudekiMpLobby *s) { (void)s; ++start_calls; return TRUE; }
BOOL SudekiMpLobbyHostRunning(SudekiMpLobby *s) { (void)s; return FALSE; }
BOOL SudekiMpLobbyHostNativeDrained(SudekiMpLobby *s) { (void)s; return TRUE; }
void SudekiMpLobbyLoadAck(SudekiMpLobby *s,uint32_t r,uint64_t g,unsigned a,unsigned p) { (void)s; (void)r; (void)g; (void)a; (void)p; }
void SudekiMpLobbyAdmissionAck(SudekiMpLobby *s,uint32_t q,uint64_t t,unsigned a) { (void)s; (void)q; (void)t; (void)a; }
BOOL SudekiMpLobbyGameplayPrepare(const SudekiMpLobbyLaunchPlan *p) { (void)p; return FALSE; }
BOOL SudekiMpLobbyGameplayPrepareSaved(const SudekiMpLobbyLaunchPlan *p,const SudekiMpLobbySavedGame *s) { (void)p; (void)s; return FALSE; }
BOOL SudekiMpLobbyGameplaySavedAvailable(void) { return TRUE; }
BOOL SudekiMpLobbyGameplayTestroomAvailable(void) { return TRUE; }
BOOL SudekiMpLobbyGameplaySavedGame(void) { return FALSE; }
BOOL SudekiMpLobbyGameplayActive(void) { return FALSE; }
BOOL SudekiMpLobbyGameplayStarted(void) { return FALSE; }
BOOL SudekiMpLobbyGameplayCancel(void) { return cancel_ok; }
BOOL SudekiMpLobbyGameplayMatches(uint32_t r,uint64_t g) { (void)r; (void)g; return FALSE; }
void SudekiMpLobbyGameplayService(SudekiMpLobby *s) { (void)s; }
void SudekiMpLobbyGameplayRequestStart(void) {}
unsigned SudekiMpLobbyGameplayPoll(unsigned *p) { (void)p; return SUDEKIMP_LAUNCH_FAILED; }
BOOL SudekiMpSaveCatalogRefresh(SudekiMpSaveCatalog *c) { (void)c; return FALSE; }
BOOL SudekiMpSaveCatalogFingerprint(SudekiMpSaveCatalog *c,unsigned i,SudekiMpSaveFingerprint *f) { (void)c; (void)i; (void)f; return FALSE; }
BOOL SudekiMpSaveCatalogVerify(SudekiMpSaveCatalog *c,unsigned i,const SudekiMpSaveFingerprint *f) { (void)c; (void)i; (void)f; return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
void SudekiMpLogWrite(const char *message_text) { (void)message_text; }

int main(void) {
    reset_fixture(FALSE);
    assert(page==BROWSE && !dev_play && row_for(MODE_DEV_PLAY)==~0u);
    unsigned selection=0;
    assert(SudekiMpLobbyUiBack(&selection));

    reset_fixture(TRUE);
    assert(page==MODES && view.count==3u);
    click(MODE_DEV_PLAY);
    assert(page==BROWSE && dev_play && fixture.mode==SUDEKIMP_LOBBY_MODE_DEV_PLAY);
    assert(!SudekiMpLobbyUiBack(&selection) && page==MODES);
    /* Rejected mode mutation must not change the UI's selected mode. */
    fixture.phase=SUDEKIMP_LOBBY_HOSTING;
    click(MODE_MULTIPLAYER);
    assert(page==MODES && dev_play);
    fixture.phase=SUDEKIMP_LOBBY_IDLE;
    click(MODE_MULTIPLAYER);
    assert(page==BROWSE && !dev_play);
    room_fixture();
    assert(row_for(CHOOSE_CHARACTER)==~0u);
    for (unsigned i=0;i<4;++i) assert(row_for((enum Action)(CHARACTER_BUKI+i))<view.count);

    reset_fixture(TRUE); click(MODE_DEV_PLAY); room_fixture();
    fixture.members[1].present=fixture.members[1].reserved=fixture.members[1].locked=1;
    fixture.members[1].character=SUDEKIMP_LOBBY_TALOS;
    refresh(); click(CHOOSE_CHARACTER);
    assert(page==PICKER && view.count==6u);
    for (unsigned i=0;i<5u;++i) {
        assert(actions[i]==PICK_CHARACTER && (view.enabled&(1u<<i)));
        assert(view.text.controls[i].kind==SUDEKIMP_PANEL_PORTRAIT);
        assert(view.text.controls[i].portrait_character==picker_characters[i]);
        assert(!view.text.controls[i].texture);
        assert(!strcmp(view.text.controls[i].label,actor_name(picker_characters[i])));
    }
    /* Arrow cycling uses the same focus row consumed by the native adapter. */
    SudekiMpLobbyUiPoll((HWND)(uintptr_t)1,TRUE);
    SudekiMpLobbyUiFocus(3);
    fixture_keys[VK_RIGHT]=(SHORT)0x8000;
    SudekiMpLobbyUiPoll((HWND)(uintptr_t)1,TRUE);
    assert(SudekiMpLobbyUiTakeSelection(&selection) && selection==4u);
    assert(!SudekiMpLobbyUiTakeSelection(&selection));
    fixture_keys[VK_RIGHT]=0; SudekiMpLobbyUiPoll((HWND)(uintptr_t)1,TRUE);
    fixture_keys[VK_RIGHT]=(SHORT)0x8000; SudekiMpLobbyUiPoll((HWND)(uintptr_t)1,TRUE);
    assert(SudekiMpLobbyUiTakeSelection(&selection) && selection==0u);
    fixture_keys[VK_RIGHT]=0;
    SudekiMpLobbyUiArm(4); assert(!SudekiMpLobbyUiCommit(&selection));
    assert(page==ROOM && selected_calls==1 && selected_value==SUDEKIMP_LOBBY_TALOS && selected_lock);
    unsigned indicators=0;
    for (unsigned i=0;i<view.count;++i) indicators+=view.text.controls[i].kind==SUDEKIMP_PANEL_CHARACTER;
    assert(indicators==4);
    fixture.members[0].ready=fixture.members[1].ready=1;
    refresh();
    assert(view.enabled&(1u<<row_for(START_GAME)));
    SudekiMpLobbyUiArm(row_for(START_GAME)); assert(!SudekiMpLobbyUiCommit(&selection));
    assert(start_calls==1); message[0]=0;
    fixture.members[0].character=fixture.members[1].character=3;
    refresh(); assert(view.enabled&(1u<<row_for(START_GAME))); /* extra hero spectates */
    fixture.members[1].character=2; fixture.members[0].character=1;
    refresh(); assert(view.enabled&(1u<<row_for(START_GAME))); /* present nonleader host */
    fixture.saved_game.party_mask=12;
    refresh(); assert(!(view.enabled&(1u<<row_for(START_GAME))) && strstr(view.text.status,"not in this save"));
    fixture.members[0].character=2;
    refresh(); assert(view.enabled&(1u<<row_for(START_GAME))); /* sparse present hero */
    fixture.saved_game.party_mask=15;
    fixture.members[0].character=3;
    refresh(); assert(view.enabled&(1u<<row_for(START_GAME)));
    fixture.members[0].character=SUDEKIMP_LOBBY_TALOS;
    fixture.members[1].character=SUDEKIMP_LOBBY_TALOS;
    refresh(); assert(view.enabled&(1u<<row_for(START_GAME)));
    fixture.members[1].character=3;
    refresh(); assert(!(view.enabled&(1u<<row_for(START_GAME))) && strstr(view.text.status,"stays with AI"));

    /* Both automation paths enter the same page actions; normal host remains regular. */
    SudekiMpLobbyAuto automated={.host=TRUE,.port=26770,.save_slot=~0u,.character=4,.min_players=1};
    strcpy(automated.room,"Fixture");
    reset_fixture(FALSE); SudekiMpLobbyUiConfigureAuto(&automated);
    auto_tick(); assert(page==CREATE); auto_tick(); assert(page==ROOM && host_calls==1 && !fixture.mode);
    reset_fixture(TRUE); automated.dev_play=TRUE; SudekiMpLobbyUiConfigureAuto(&automated);
    auto_tick(); assert(page==BROWSE && dev_play); auto_tick(); auto_tick();
    assert(page==ROOM && host_calls==1 && fixture.mode==SUDEKIMP_LOBBY_MODE_DEV_PLAY);
    reset_fixture(FALSE); SudekiMpLobbyUiConfigureAuto(&automated);
    auto_tick(); assert(auto_done && page==BROWSE && !host_calls && strstr(message,"Enable Dev Play"));

    /* Host and join automation relinquish Dev Play UI only after the host
     * confirms all requested values, then allow manual character changes. */
    for (unsigned client=0;client<2u;++client) {
        reset_fixture(TRUE); click(MODE_DEV_PLAY); room_fixture();
        fixture.phase=client?SUDEKIMP_LOBBY_CONNECTED:SUDEKIMP_LOBBY_HOSTING;
        fixture.local_slot=(uint8_t)client;
        SudekiMpLobbyMember *me=&fixture.members[client];
        me->present=me->reserved=me->locked=1;
        me->character=SUDEKIMP_LOBBY_TALOS; me->ready=1;
        me->name[0]=0;
        automated.host=!client; automated.join=client; automated.dev_play=TRUE;
        automated.character=SUDEKIMP_LOBBY_TALOS; automated.ready=TRUE;
        strcpy(automated.name,"Avatar owner");
        SudekiMpLobbyUiConfigureAuto(&automated);
        auto_named=TRUE; /* A name command was sent; no confirming state yet. */
        refresh(); auto_tick(); assert(!auto_done);
        strcpy(me->name,automated.name); me->ready=0; wanted_ready=TRUE;
        refresh(); auto_tick(); assert(!auto_done); /* Ready ACK still pending. */
        me->ready=1; refresh(); auto_tick(); assert(auto_done);
        if (client) assert(!strstr(view.text.hint,"lock it"));
        click(CHOOSE_CHARACTER);
        unsigned calls_before=selected_calls;
        auto_tick(); assert(page==PICKER && selected_calls==calls_before);
        SudekiMpLobbyUiArm(0); assert(!SudekiMpLobbyUiCommit(&selection));
        assert(page==ROOM && me->character==2u);
        auto_tick(); assert(page==ROOM && me->character==2u && selected_calls==calls_before+1u);
    }
    reset_fixture(FALSE); room_fixture();
    automated.host=TRUE; automated.join=FALSE; automated.dev_play=FALSE;
    automated.character=4; automated.ready=FALSE; automated.name[0]=0;
    SudekiMpLobbyUiConfigureAuto(&automated); auto_tick();
    assert(!auto_done); /* Regular automation retains its previous behavior. */

    wchar_t temporary[MAX_PATH], config[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,temporary));
    assert(GetTempFileNameW(temporary,L"sui",0,config));
    SudekiMpLobbyUiConfigureDevPlay(FALSE,config);
    assert(SudekiMpLobbyUiSetDevPlayEnabled(TRUE));
    assert(SudekiMpLobbyUiDevPlayEnabled() && GetPrivateProfileIntW(L"DevPlay",L"Enabled",0,config)==1);
    assert(SudekiMpLobbyUiSetDevPlayEnabled(FALSE));
    assert(!SudekiMpLobbyUiDevPlayEnabled() && GetPrivateProfileIntW(L"DevPlay",L"Enabled",1,config)==0);
    DeleteFileW(config);
    puts("PASS: hidden/enabled modes, settings persistence, five-choice picker, duplicate UI admission, focus cycling, avatar launch gate, normal/dev automation, configured Dev Play handback");
    return 0;
}
