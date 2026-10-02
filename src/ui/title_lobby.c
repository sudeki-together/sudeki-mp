#include "ui/title_lobby.h"
#include "network/title_lobby.h"
#include "network/lan_arena_endpoint.h"
#include "engine/log.h"
#include "hooks/lobby_gameplay.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum Page { BROWSE, CREATE, JOIN, ROOM };
enum Action { FIND, HOST, DIRECT, BACK, NAME, ROOM_NAME, PORT, VISIBILITY,
    CREATE_ROOM, ADDRESS, LOCAL_ADDRESS, CONNECT, SEARCH, REFRESH, PREVIOUS,
    NEXT, SERVER, JOIN_SELECTED, READY, LEAVE, DESTINATION, START_GAME, NONE };
enum { PAGE_SIZE=6 };
static SudekiMpLobby *session;
static SudekiMpLobbyStatus status;
static SudekiMpLobbyView view;
static enum Page page, armed_page;
static enum Action actions[SUDEKIMP_PANEL_CONTROLS], armed_action;
static SudekiMpLobbyServer row_servers[SUDEKIMP_PANEL_CONTROLS], armed_server, chosen;
static unsigned browser_page;
static unsigned page_revision;
static char room[32]="Sudeki Together", player[32]="Player", port[6]="26770";
static char address[32], search[32], message[128];
static char *editing;
static unsigned edit_capacity;
static BOOL edit_release;
static DWORD edit_released_at;
static BOOL opened, advertised=TRUE, wanted_ready, focused, keys[256];
static unsigned logged_phase=~0u, logged_members=~0u, logged_ready=~0u;
static unsigned logged_advertised=~0u, logged_servers=~0u;
static unsigned logged_start=~0u;
static uint32_t seen_revision;
static BOOL launch_available, handoff_complete;

static BOOL start_busy(void) {
    return status.start.phase>=SUDEKIMP_LOBBY_START_PREPARE && status.start.phase<=SUDEKIMP_LOBBY_START_COMPLETE;
}
static unsigned bits(unsigned mask) {
    unsigned count=0; for (;mask;mask>>=1) count+=mask&1u; return count;
}
static void service_start(HWND window) {
    (void)window;
    if (handoff_complete) return;
    SudekiMpLobbyStart *p=&status.start;
    if (status.departure_safe && SudekiMpLobbyGameplayActive() && p->phase==SUDEKIMP_LOBBY_START_COMPLETE) {
        handoff_complete=TRUE;
        SudekiMpLobbyLeave(session); opened=focused=FALSE;
        SudekiMpLogWrite("title_lobby event=entered_testroom window=same\r\n");
        return;
    }
    if (!start_busy() || (status.phase!=SUDEKIMP_LOBBY_HOSTING && status.phase!=SUDEKIMP_LOBBY_CONNECTED)) {
        (void)SudekiMpLobbyGameplayCancel(); return;
    }
    if (!SudekiMpLobbyGameplayActive() && p->phase==SUDEKIMP_LOBBY_START_PREPARE && (!status.local_slot || p->port)) {
        SudekiMpLobbyLaunchPlan plan={.revision=p->revision,.generation=p->generation,
            .seat=status.local_slot,.members=p->members,.port=status.local_slot?p->port:0};
        memcpy(plan.nonce,p->nonce,sizeof(plan.nonce));
        snprintf(plan.host_ipv4,sizeof(plan.host_ipv4),"%s",status.host_ipv4);
        if (!launch_available || !SudekiMpLobbyGameplayPrepare(&plan)) {
            SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_FAILED,0);
            strcpy(message,"Could not prepare Test Room. Native setup was refused; see the game log.");
            return;
        }
        SudekiMpLogFormat("title_lobby event=in_place_prepare revision=%lu seat=%u members=%u\r\n",
            (unsigned long)p->revision,status.local_slot,p->members);
    }
    if (!SudekiMpLobbyGameplayActive()) return;
    if (!SudekiMpLobbyGameplayMatches(p->revision,p->generation)) {
        (void)SudekiMpLobbyGameplayCancel(); return;
    }
    unsigned port=0,phase=SudekiMpLobbyGameplayPoll(&port);
    if (phase==SUDEKIMP_LAUNCH_FAILED) {
        SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_FAILED,0);
        strcpy(message,"Test Room preparation or loading failed. Return to the lobby and try again.");
    } else if (p->phase==SUDEKIMP_LOBBY_START_PREPARE && phase>=SUDEKIMP_LAUNCH_PREPARED) {
        SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_PREPARED,status.local_slot?0:port);
    } else if (p->phase==SUDEKIMP_LOBBY_START_LOADING) {
        SudekiMpLobbyGameplayRequestStart();
        if (phase==SUDEKIMP_LAUNCH_LOADED)
            SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_LOADED,0);
    } else if (p->phase==SUDEKIMP_LOBBY_START_COMPLETE && phase==SUDEKIMP_LAUNCH_LOADED) {
        SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_COMPLETE,0);
    }
}

static void snapshot_keys(void) {
    for (unsigned i=0;i<256;++i) keys[i]=(GetAsyncKeyState((int)i)&0x8000)!=0;
}
static void change_page(enum Page next) {
    if (session) SudekiMpLobbyBrowse(session,next==BROWSE);
    page=next; editing=NULL; edit_release=FALSE; browser_page=0; message[0]=0;
    ++page_revision;
    memset(&chosen,0,sizeof(chosen)); snapshot_keys();
    SudekiMpLogFormat("title_lobby event=page page=%u actors=unassigned\r\n",(unsigned)page);
}
static BOOL contains(const char *text,const char *query) {
    for (;*text;++text) {
        unsigned i=0;
        while (query[i] && text[i] && tolower((unsigned char)query[i])==tolower((unsigned char)text[i])) ++i;
        if (!query[i]) return TRUE;
    }
    return !*query;
}
static BOOL valid_text(const char *text) {
    for (;*text;++text) if (*text!=' ') return TRUE;
    return FALSE;
}
static BOOL same_server(const SudekiMpLobbyServer *a,const SudekiMpLobbyServer *b) {
    return a->instance && a->instance==b->instance && a->port==b->port;
}
static unsigned control(enum Action action,SudekiMpPanelKind kind,float x,float y,
    float width,float height,const char *label,BOOL enabled,BOOL active) {
    unsigned row=view.count++;
    SudekiMpPanelControl *c=&view.text.controls[row];
    actions[row]=action;
    *c=(SudekiMpPanelControl){.x=x,.y=y,.width=width,.height=height,.kind=kind,.active=active};
    snprintf(c->label,sizeof(c->label),"%s",label);
    if (enabled) view.enabled|=1u<<row;
    return row;
}
static void field(enum Action action,const char *label,char *value,float x,float y,float width) {
    unsigned row=control(action,SUDEKIMP_PANEL_FIELD,x,y,width,38,label,TRUE,editing==value);
    snprintf(view.text.controls[row].value,sizeof(view.text.controls[row].value),"%s%s",value,editing==value?"_":"");
}
static void note(float x,float y,float size,float width,const char *text) {
    SudekiMpPanelText *t=&view.text.texts[view.text.text_count++];
    *t=(SudekiMpPanelText){.x=x,.y=y,.size=size,.width=width};
    snprintf(t->text,sizeof(t->text),"%s",text);
}
static void build_view(void) {
    memset(&view,0,sizeof(view)); memset(row_servers,0,sizeof(row_servers));
    view.text.panel=TRUE;
    strcpy(view.text.heading,"Multiplayer");
    strcpy(view.text.hint,"Gather your party");
    control(FIND,SUDEKIMP_PANEL_NAV,66,188,158,45,"Find a Game",page!=ROOM,page==BROWSE);
    control(HOST,SUDEKIMP_PANEL_NAV,66,244,158,45,"Host a Game",page!=ROOM,page==CREATE);
    control(DIRECT,SUDEKIMP_PANEL_NAV,66,300,158,45,"Direct Connect",page!=ROOM,page==JOIN);
    control(BACK,SUDEKIMP_PANEL_NAV,66,560,158,40,"Main Menu",page!=ROOM,FALSE);
    note(78,414,20,136,"Your party");
    note(78,445,16,136,"1 to 4 players");
    note(78,474,16,136,"Unclaimed heroes");
    note(78,498,16,136,"remain with AI.");
    strcpy(view.text.status,"Left click to select. Enter confirms. Escape goes back.");
    if (page==BROWSE) {
        unsigned filtered[SUDEKIMP_LOBBY_SERVERS],count=0;
        field(NAME,"Your name",player,278,202,265);
        field(SEARCH,"Filter rooms",search,561,202,283);
        note(290,274,16,264,"Room"); note(569,274,16,174,"Address"); note(768,274,16,66,"Players");
        const SudekiMpLobbyServer *selected_server=NULL;
        for (unsigned i=0;i<status.server_count;++i) {
            const SudekiMpLobbyServer *server=&status.servers[i];
            if (contains(server->name,search) || contains(server->ipv4,search)) {
                filtered[count++]=i;
                if (same_server(server,&chosen)) selected_server=server;
            }
        }
        if (browser_page*PAGE_SIZE>=count) browser_page=0;
        if (!selected_server) memset(&chosen,0,sizeof(chosen));
        for (unsigned row=0;row<PAGE_SIZE;++row) {
            unsigned index=browser_page*PAGE_SIZE+row;
            const SudekiMpLobbyServer *server=index<count?&status.servers[filtered[index]]:NULL;
            unsigned id=control(SERVER,SUDEKIMP_PANEL_SERVER,278,290+row*34,566,32,
                server?server->name:"",server!=NULL,server && same_server(server,&chosen));
            if (server) {
                row_servers[id]=*server;
                snprintf(view.text.controls[id].value,sizeof(view.text.controls[id].value),"%s:%u",server->ipv4,server->port);
                snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),"%u / 4",server->players);
            }
        }
        if (!count) {
            note(300,329,23,520,*search?"No rooms match this filter":"No discoverable rooms found");
            note(300,365,18,520,"Hosts must enable Discoverable on LAN.");
            note(300,394,18,520,"For unlisted rooms, use Direct Connect.");
        }
        char detail[128];
        snprintf(detail,sizeof(detail),"%u room%s  -  Page %u of %u",count,count==1?"":"s",
            browser_page+1,count?(count+PAGE_SIZE-1)/PAGE_SIZE:1);
        note(278,518,18,355,detail);
        control(PREVIOUS,SUDEKIMP_PANEL_BUTTON,650,502,90,32,"Previous",browser_page>0,FALSE);
        control(NEXT,SUDEKIMP_PANEL_BUTTON,754,502,90,32,"Next",(browser_page+1)*PAGE_SIZE<count,FALSE);
        control(REFRESH,SUDEKIMP_PANEL_BUTTON,278,568,165,38,"Refresh",TRUE,FALSE);
        control(JOIN_SELECTED,SUDEKIMP_PANEL_BUTTON,638,568,206,38,"Join Selected",
            selected_server && selected_server->players<4,FALSE);
        strcpy(view.text.hint,"Find games on this computer and your local network");
        snprintf(view.text.status,sizeof(view.text.status),"%s",*status.discovery_error?status.discovery_error:
            "Scanning automatically. Routed VPN and unlisted games can join by address.");
    } else if (page==CREATE) {
        field(NAME,"Your name",player,278,214,566);
        field(ROOM_NAME,"Room name",room,278,294,354);
        field(PORT,"Listening port",port,652,294,192);
        note(278,356,17,566,"Room visibility");
        control(VISIBILITY,SUDEKIMP_PANEL_BUTTON,278,376,566,40,
            advertised?"Discoverable on LAN":"Unlisted - join by address",TRUE,advertised);
        note(278,440,18,566,advertised?"Appears in Find a Game on this computer and LAN.":
            "Hidden from Find a Game. Share your address to invite players.");
        note(278,492,22,566,"Play with 1 to 4 people");
        note(278,523,18,566,"Unclaimed story companions remain AI. Empty slots are optional.");
        control(CREATE_ROOM,SUDEKIMP_PANEL_BUTTON,638,568,206,38,"Create Lobby",session!=NULL,FALSE);
        strcpy(view.text.hint,"Host a room - your player slot does not choose your character");
    } else if (page==JOIN) {
        field(NAME,"Your name",player,278,214,566);
        field(ADDRESS,"Host address (IPv4 or IPv4:port)",address,278,298,566);
        note(278,365,19,566,"Default port: 26770. Use the host's LAN or VPN address.");
        note(278,397,18,566,"For two games on this PC, connect to 127.0.0.1.");
        control(LOCAL_ADDRESS,SUDEKIMP_PANEL_BUTTON,278,426,240,38,"Use This Computer",TRUE,FALSE);
        note(278,505,18,566,"Unlisted rooms accept invitations by address.");
        note(278,532,18,566,"Both games must use a compatible build.");
        control(CONNECT,SUDEKIMP_PANEL_BUTTON,638,568,206,38,"Connect",session!=NULL,FALSE);
        strcpy(view.text.hint,"Connect directly to a friend's room");
    } else {
        BOOL connected=status.phase==SUDEKIMP_LOBBY_HOSTING || status.phase==SUDEKIMP_LOBBY_CONNECTED;
        BOOL host=status.phase==SUDEKIMP_LOBBY_HOSTING;
        BOOL busy=start_busy() || SudekiMpLobbyGameplayActive();
        BOOL testroom=status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM;
        static const char *const actors[]={"Buki","Elco","Tal","Ailish"};
        control(DESTINATION,SUDEKIMP_PANEL_BUTTON,278,176,566,34,
            testroom?"Destination: Test Room":"Destination: Choose Test Room",host && !busy,testroom);
        unsigned members=0,ready=0;
        for (unsigned i=0;i<4;++i) {
            const SudekiMpLobbyMember *m=&status.members[i];
            members+=m->present; ready+=m->present && m->ready;
            char label[80];
            if (m->present) snprintf(label,sizeof(label),"%s%s%s",m->name,i==0?" - Host":"",i==status.local_slot?" - You":"");
            else strcpy(label,"Open player slot - optional");
            unsigned id=control(NONE,SUDEKIMP_PANEL_MEMBER,278,244+i*61,566,55,label,FALSE,m->present);
            if (m->present) snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),"%s - %s",
                testroom?actors[i]:"Choose a destination",m->ready?"Ready":"Not ready");
            else snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),"%s - AI companion",actors[i]);
        }
        char roster[128];
        snprintf(roster,sizeof(roster),"%u / 4 connected - %u ready%s",members,ready,
            connected && ready==members?" - All connected players ready":"");
        note(278,228,18,566,roster);
        snprintf(view.text.heading,sizeof(view.text.heading),"%s",*status.room?status.room:"Connecting...");
        if (host) snprintf(view.text.hint,sizeof(view.text.hint),"Hosting on port %u - %s",status.port,
            status.advertised?"Discoverable on LAN":"Unlisted - direct address only");
        else strcpy(view.text.hint,connected?"Connected - the host chooses the destination and starts the game":"Waiting for the host...");
        if (host) control(VISIBILITY,SUDEKIMP_PANEL_BUTTON,278,506,566,32,
            status.advertised?"Visibility: Discoverable on LAN":"Visibility: Unlisted (click to advertise)",!busy,status.advertised);
        else note(278,522,18,566,"Unclaimed companions stay with AI; four players are not required.");
        control(READY,SUDEKIMP_PANEL_BUTTON,278,568,178,38,wanted_ready?"Not Ready":"Ready",connected && testroom && !busy,FALSE);
        control(START_GAME,SUDEKIMP_PANEL_BUTTON,470,568,186,38,busy?"Loading...":host?"Start Test Room":"Waiting for Host",
            host && testroom && !busy && launch_available && members && ready==members,FALSE);
        control(LEAVE,SUDEKIMP_PANEL_BUTTON,670,568,174,38,"Leave Lobby",TRUE,FALSE);
        strcpy(view.text.status,testroom?"Test Room: Buki hosts; Elco, Tal and Ailish fill joined slots. Empty slots use AI.":
            "Host: choose Test Room, then every connected player selects Ready.");
        if (status.start.phase==SUDEKIMP_LOBBY_START_PREPARE)
            snprintf(view.text.status,sizeof(view.text.status),"Preparing Test Room: %u of %u players. Leave cancels the start.",
                bits(status.start.prepared),bits(status.start.members));
        else if (status.start.phase==SUDEKIMP_LOBBY_START_LOADING)
            snprintf(view.text.status,sizeof(view.text.status),"Loading Test Room: %u of %u players.",
                bits(status.start.loaded),bits(status.start.members));
        else if (status.start.phase==SUDEKIMP_LOBBY_START_COMPLETE) strcpy(view.text.status,"Everyone loaded. Entering Test Room...");
        else if (status.start.phase==SUDEKIMP_LOBBY_START_ABORTED) strcpy(view.text.status,"Start cancelled or loading failed. Ready up again to retry.");
        if (!launch_available) strcpy(view.text.status,"Test Room start requires the matching native adapter.");
        if (status.phase==SUDEKIMP_LOBBY_ERROR) {
            strcpy(view.text.heading,"Connection ended");
            snprintf(view.text.hint,sizeof(view.text.hint),"%s",status.error);
        }
    }
    if (*message) snprintf(view.text.status,sizeof(view.text.status),"%s",message);
    if (editing) strcpy(view.text.status,"Type to edit. Ctrl+A clears; Enter or Escape finishes editing.");
}
void SudekiMpLobbyUiOpen(void) {
    opened=TRUE; focused=FALSE; wanted_ready=FALSE;
    launch_available=TRUE; handoff_complete=FALSE; seen_revision=0;
    if (!session) session=SudekiMpLobbyCreate();
    change_page(BROWSE);
    if (!session) strcpy(message,"Network initialization failed. Main Menu remains available.");
    SudekiMpLobbyStatusGet(session,&status); build_view();
}
void SudekiMpLobbyUiClose(void) {
    if (!opened) return;
    if (SudekiMpLobbyGameplayStarted()) { opened=focused=FALSE; editing=NULL; return; }
    (void)SudekiMpLobbyGameplayCancel();
    SudekiMpLobbyLeave(session); SudekiMpLobbyBrowse(session,FALSE);
    opened=focused=FALSE; editing=NULL;
}
BOOL SudekiMpLobbyUiDestroy(void) {
    if (!SudekiMpLobbyGameplayCancel()) return FALSE;
    if (!SudekiMpLobbyDestroy(session)) return FALSE;
    session=NULL; opened=focused=FALSE; editing=NULL; return TRUE;
}
const SudekiMpLobbyView *SudekiMpLobbyUiView(void) { return &view; }
BOOL SudekiMpLobbyUiEditing(void) { return editing!=NULL || edit_release; }
unsigned SudekiMpLobbyUiPageRevision(void) { return page_revision; }
void SudekiMpLobbyUiEndEdit(void) {
    editing=NULL; edit_release=FALSE; snapshot_keys(); build_view();
}
static void begin_edit(char *text,unsigned size) {
    editing=text; edit_capacity=size; message[0]=0; snapshot_keys();
}
static BOOL pressed(unsigned key) {
    BOOL down=(GetAsyncKeyState((int)key)&0x8000)!=0;
    BOOL edge=down && !keys[key]; keys[key]=down; return edge;
}
void SudekiMpLobbyUiPoll(HWND window,BOOL input) {
    if (!opened) return;
    SudekiMpLobbyStatusGet(session,&status);
    if (status.start.revision!=seen_revision) { seen_revision=status.start.revision; wanted_ready=FALSE; }
    service_start(window);
    if (logged_start!=status.start.phase) {
        SudekiMpLogFormat("title_lobby event=start phase=%u revision=%lu prepared=%u loaded=%u members=%u\r\n",
            status.start.phase,(unsigned long)status.start.revision,status.start.prepared,status.start.loaded,status.start.members);
        logged_start=status.start.phase;
    }
    unsigned members=0,ready=0;
    for (unsigned i=0;i<4;++i) { members|=(unsigned)status.members[i].present<<i; ready|=(unsigned)status.members[i].ready<<i; }
    if ((unsigned)status.phase!=logged_phase || members!=logged_members || ready!=logged_ready ||
        status.advertised!=logged_advertised || status.server_count!=logged_servers) {
        SudekiMpLogFormat("title_lobby event=roster phase=%u local_slot=%u members=%u ready=%u advertised=%u discovered=%u discovery_error=%u actors=unassigned\r\n",
            (unsigned)status.phase,(unsigned)status.local_slot,members,ready,(unsigned)status.advertised,
            status.server_count,*status.discovery_error?1u:0u);
        logged_phase=(unsigned)status.phase; logged_members=members; logged_ready=ready;
        logged_advertised=status.advertised; logged_servers=status.server_count;
    }
    BOOL active=input && window && GetForegroundWindow()==window;
    if (!active) { focused=FALSE; build_view(); return; }
    if (!focused) { focused=TRUE; snapshot_keys(); build_view(); return; }
    if (edit_release) {
        if ((GetAsyncKeyState(VK_RETURN) | GetAsyncKeyState(VK_ESCAPE)) & 0x8000) edit_released_at=0;
        else if (!edit_released_at) edit_released_at=GetTickCount();
        else if ((DWORD)(GetTickCount()-edit_released_at)>=150u) edit_release=FALSE;
        build_view(); return;
    }
    if (editing) {
        BOOL shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
        BOOL control_key=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
        for (unsigned key=8;key<255;++key) if (pressed(key)) {
            unsigned length=(unsigned)strlen(editing); char c=0;
            if (key==VK_RETURN || key==VK_ESCAPE) {
                editing=NULL; edit_release=TRUE; edit_released_at=0; break;
            }
            if (key=='A' && control_key) { editing[0]=0; continue; }
            if (control_key) continue;
            if (key==VK_BACK) { if (length) editing[length-1]=0; continue; }
            if (key==VK_DELETE) { editing[0]=0; continue; }
            if (key>='A' && key<='Z') c=(char)(shift?key:key+32);
            else if (key>='0' && key<='9') c=(char)key;
            else if (key>=VK_NUMPAD0 && key<=VK_NUMPAD9) c=(char)('0'+key-VK_NUMPAD0);
            else if (key==VK_OEM_PERIOD || key==VK_DECIMAL) c='.';
            else if (key==VK_OEM_1 && shift) c=':';
            else if (key==VK_OEM_MINUS) c=shift?'_':'-';
            else if (key==VK_SPACE) c=' ';
            if (editing==port && (c<'0' || c>'9')) c=0;
            if (editing==address && !((c>='0' && c<='9') || c=='.' || c==':')) c=0;
            if (c && length+1<edit_capacity) { editing[length]=c; editing[length+1]=0; browser_page=0; }
        }
    } else if (page==BROWSE) {
        if (pressed(VK_PRIOR)) { if (browser_page) --browser_page; }
        if (pressed(VK_NEXT)) ++browser_page;
    }
    build_view();
}
void SudekiMpLobbyUiArm(unsigned row) {
    armed_page=page; armed_action=row<view.count?actions[row]:NONE;
    memset(&armed_server,0,sizeof(armed_server));
    /* Pin the exact clicked/selected room across asynchronous discovery updates. */
    if (armed_action==SERVER) armed_server=row_servers[row];
    if (armed_action==JOIN_SELECTED) armed_server=chosen;
}
BOOL SudekiMpLobbyUiBack(unsigned *selection) {
    if (editing) { editing=NULL; build_view(); return FALSE; }
    if (page!=ROOM) return TRUE;
    (void)SudekiMpLobbyGameplayCancel();
    SudekiMpLobbyLeave(session); wanted_ready=FALSE;
    change_page(BROWSE); *selection=0; build_view(); return FALSE;
}
static void join_room(const SudekiMpLobbyServer *server) {
    if (!valid_text(player)) { strcpy(message,"Enter your player name first."); return; }
    wanted_ready=FALSE;
    if (SudekiMpLobbyJoin(session,server->ipv4,server->port,player)) change_page(ROOM);
    else { SudekiMpLobbyStatusGet(session,&status); snprintf(message,sizeof(message),"%s",
        *status.error?status.error:"Unable to start a connection."); }
}
BOOL SudekiMpLobbyUiCommit(unsigned *selection) {
    if (page!=armed_page) return FALSE;
    message[0]=0;
    switch (armed_action) {
    case FIND: case HOST: case DIRECT:
        if (page==ROOM) break;
        change_page(armed_action==FIND?BROWSE:armed_action==HOST?CREATE:JOIN);
        *selection=(unsigned)armed_action; break;
    case BACK: return page!=ROOM;
    case NAME: begin_edit(player,sizeof(player)); break;
    case ROOM_NAME: begin_edit(room,sizeof(room)); break;
    case PORT: begin_edit(port,sizeof(port)); break;
    case ADDRESS: begin_edit(address,sizeof(address)); break;
    case SEARCH: begin_edit(search,sizeof(search)); break;
    case LOCAL_ADDRESS: strcpy(address,"127.0.0.1"); break;
    case VISIBILITY:
        if (page==ROOM) {
            if (!SudekiMpLobbyAdvertise(session,!status.advertised))
                strcpy(message,"Discovery could not open its port. The lobby still accepts direct connections.");
        } else advertised=!advertised;
        break;
    case CREATE_ROOM: {
        char endpoint[48],ip[16]; uint16_t number=0;
        snprintf(endpoint,sizeof(endpoint),"127.0.0.1:%s",port);
        if (!valid_text(room) || !valid_text(player)) strcpy(message,"Enter a room and player name.");
        else if (!SudekiMpLanArenaParseEndpoint(endpoint,SUDEKIMP_LOBBY_PORT,ip,sizeof(ip),&number))
            strcpy(message,"Choose a port between 1024 and 65535.");
        else if (SudekiMpLobbyHost(session,room,player,number,advertised)) {
            wanted_ready=FALSE; change_page(ROOM); *selection=0;
        } else { SudekiMpLobbyStatusGet(session,&status); snprintf(message,sizeof(message),"%s",
            *status.error?status.error:"Unable to create a lobby."); }
        break;
    }
    case CONNECT: {
        SudekiMpLobbyServer target={0};
        if (!SudekiMpLanArenaParseEndpoint(address,SUDEKIMP_LOBBY_PORT,target.ipv4,sizeof(target.ipv4),&target.port))
            strcpy(message,"Enter a valid IPv4 address and optional port.");
        else { join_room(&target); if (page==ROOM) *selection=0; }
        break;
    }
    case REFRESH: SudekiMpLobbyBrowse(session,FALSE); SudekiMpLobbyBrowse(session,TRUE); browser_page=0; break;
    case PREVIOUS: if (browser_page) --browser_page; break;
    case NEXT: ++browser_page; break;
    case SERVER: chosen=armed_server; break;
    case JOIN_SELECTED:
        if (*armed_server.ipv4) { join_room(&armed_server); if (page==ROOM) *selection=0; }
        break;
    case READY: wanted_ready=!wanted_ready; SudekiMpLobbyReady(session,wanted_ready); break;
    case DESTINATION:
        if (SudekiMpLobbyDestination(session,status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM?
                SUDEKIMP_LOBBY_DEST_NONE:SUDEKIMP_LOBBY_DEST_TESTROOM)) wanted_ready=FALSE;
        break;
    case START_GAME:
        if (!launch_available || !SudekiMpLobbyStartGame(session))
            strcpy(message,"Choose Test Room and wait for all connected players to be ready.");
        break;
    case LEAVE: return SudekiMpLobbyUiBack(selection);
    case NONE: break;
    }
    SudekiMpLobbyStatusGet(session,&status); build_view(); return FALSE;
}

void SudekiMpLobbyUiBackground(void) {
    if (opened || handoff_complete || !SudekiMpLobbyGameplayActive()) return;
    SudekiMpLobbyStatusGet(session,&status); service_start(NULL);
}
