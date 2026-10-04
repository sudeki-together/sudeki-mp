#include "ui/title_lobby.h"
#include "ui/save_catalog.h"
#include "network/title_lobby.h"
#include "network/lan_arena_endpoint.h"
#include "engine/log.h"
#include "hooks/lobby_gameplay.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum Page { BROWSE, CREATE, JOIN, ROOM, DESTINATIONS, SAVES, CONFIRM_SAVE };
enum Action { FIND, HOST, DIRECT, BACK, NAME, ROOM_NAME, PORT, VISIBILITY,
    CREATE_ROOM, ADDRESS, LOCAL_ADDRESS, CONNECT, SEARCH, REFRESH, PREVIOUS,
    NEXT, SERVER, JOIN_SELECTED, READY, LEAVE, DESTINATION, START_GAME,
    CHARACTER_BUKI, CHARACTER_ELCO, CHARACTER_TAL, CHARACTER_AILISH, LOCK_CHARACTER,
    RECONNECT, PICK_TESTROOM, PICK_SAVE, SAVE_ROW, SAVE_PREVIOUS, SAVE_NEXT,
    SAVE_REFRESH, REVIEW_SAVE, CONFIRM_YES, CONFIRM_NO, RETURN_ROOM, RETURN_DESTINATIONS, NONE };
enum { PAGE_SIZE=6 };
static SudekiMpLobby *session;
static SudekiMpLobbyStatus status;
static SudekiMpLobbyView view;
static enum Page page, armed_page;
static enum Action actions[SUDEKIMP_PANEL_CONTROLS], armed_action;
static SudekiMpLobbyServer row_servers[SUDEKIMP_PANEL_CONTROLS], armed_server, chosen;
static unsigned browser_page;
static unsigned page_revision;
static char room[32]="Sudeki Together", player[32], port[6]="26770";
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
static uint32_t seen_command;
static BOOL launch_available, handoff_complete;
static SudekiMpSaveCatalog saves;
static SudekiMpSaveFingerprint reviewed_save;
static unsigned save_page, selected_save=~0u, armed_save=~0u;
static unsigned row_saves[SUDEKIMP_PANEL_CONTROLS];
static BOOL save_reviewed;

static BOOL start_busy(void) {
    return !status.running && status.start.phase>=SUDEKIMP_LOBBY_START_PREPARE && status.start.phase<=SUDEKIMP_LOBBY_START_COMPLETE;
}
static BOOL destination_editable(void) {
    return status.phase==SUDEKIMP_LOBBY_HOSTING && !status.running &&
        !start_busy() && !SudekiMpLobbyGameplayActive();
}
static BOOL destination_page(void) { return page>=DESTINATIONS; }
static unsigned bits(unsigned mask) {
    unsigned count=0; for (;mask;mask>>=1) count+=mask&1u; return count;
}
static BOOL prepare_game(const SudekiMpLobbyLaunchPlan *plan) {
    if (status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM)
        return SudekiMpLobbyGameplayPrepare(plan);
    if (status.start.destination!=SUDEKIMP_LOBBY_DEST_SAVEDGAME) return FALSE;
    return SudekiMpLobbyGameplayPrepareSaved(plan,&status.saved_game);
}
static void service_running_join(void) {
    unsigned player_slot=status.local_slot;
    const SudekiMpLobbyAdmission *a=&status.admission[player_slot];
    if (status.phase!=SUDEKIMP_LOBBY_CONNECTED || !status.running) return;
    if (a->phase==SUDEKIMP_LOBBY_ADMISSION_NONE) return;
    if (a->phase==SUDEKIMP_LOBBY_ADMISSION_FAILED) {
        if (SudekiMpLobbyGameplayActive()) (void)SudekiMpLobbyGameplayCancel();
        strcpy(message,"Joining could not finish. The host can retry after native control is safe.");
        return;
    }
    /* Initial participants already have a native world. Their completed
     * initial admission is descriptive and must never trigger a new load. */
    if (a->phase==SUDEKIMP_LOBBY_ADMISSION_COMPLETE && SudekiMpLobbyGameplayActive()) {
        handoff_complete=TRUE; opened=focused=FALSE; return;
    }
    if (SudekiMpLobbyGameplayActive() &&
        !SudekiMpLobbyGameplayMatches(a->sequence,status.start.generation)) {
        /* A reconnect cannot reuse a controller/session from its old ticket.
         * Native cancellation positively drains before the same window loads
         * a replacement. Retain the old context while cancellation is unsafe. */
        if (!SudekiMpLobbyGameplayCancel()) return;
        handoff_complete=FALSE;
    }
    if (!SudekiMpLobbyGameplayActive()) {
        if (a->phase!=SUDEKIMP_LOBBY_ADMISSION_OFFERED || !a->ticket || !status.start.port) return;
        SudekiMpLobbyLaunchPlan p={.revision=a->sequence,.generation=status.start.generation,
            .seat=(uint8_t)player_slot,.port=status.start.port};
        for (unsigned i=0;i<4;++i) {
            const SudekiMpLobbyMember *m=&status.members[i];
            /* Membership identifies a connection, even when its player is
             * spectating. This client's concrete selection remains required. */
            if (m->present) p.members|=(uint8_t)(1u<<i);
            if (m->reserved && m->locked && m->character<4)
                p.reserved_mask|=(uint8_t)(1u<<i);
            p.character[i]=(p.reserved_mask&(1u<<i))?
                m->character:SUDEKIMP_LOBBY_NO_CHARACTER;
        }
        p.nonce[player_slot]=a->ticket;
        snprintf(p.host_ipv4,sizeof(p.host_ipv4),"%s",status.host_ipv4);
        if (!launch_available || !prepare_game(&p)) {
            SudekiMpLobbyAdmissionAck(session,a->sequence,a->ticket,SUDEKIMP_LOBBY_ACK_FAILED);
            strcpy(message,status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME?
                "Could not prepare the selected save. This game needs a matching local copy.":
                "The Test Room could not prepare this join. No character control was granted.");
            return;
        }
    }
    if (!SudekiMpLobbyGameplayMatches(a->sequence,status.start.generation)) return;
    unsigned phase=SudekiMpLobbyGameplayPoll(NULL);
    if (phase==SUDEKIMP_LAUNCH_FAILED) {
        SudekiMpLobbyAdmissionAck(session,a->sequence,a->ticket,SUDEKIMP_LOBBY_ACK_FAILED);
    } else if (phase>=SUDEKIMP_LAUNCH_PREPARED) {
        if (a->phase==SUDEKIMP_LOBBY_ADMISSION_OFFERED)
            SudekiMpLobbyAdmissionAck(session,a->sequence,a->ticket,SUDEKIMP_LOBBY_ACK_PREPARED);
        SudekiMpLobbyGameplayRequestStart();
        if (phase==SUDEKIMP_LAUNCH_LOADED && a->phase>=SUDEKIMP_LOBBY_ADMISSION_PREPARED)
            SudekiMpLobbyAdmissionAck(session,a->sequence,a->ticket,SUDEKIMP_LOBBY_ACK_LOADED);
    }
}
static void service_start(HWND window) {
    (void)window;
    SudekiMpLobbyStart *p=&status.start;
    /* A TCP error retains the last running/admission record for diagnosis.
     * Retire a saved spectator before those records can take the normal
     * running-session early return. Cancellation may need several native
     * frames to restore the camera and balance its owned pause; retry while
     * retaining the load evidence and session until that drain succeeds. */
    if (SudekiMpLobbyGameplaySavedGame() &&
        (status.phase==SUDEKIMP_LOBBY_ERROR || status.phase==SUDEKIMP_LOBBY_IDLE)) {
        if (SudekiMpLobbyGameplayCancel()) {
            SudekiMpLobbyLeave(session);
            handoff_complete=wanted_ready=FALSE;
            SudekiMpLogWrite("title_lobby event=story_session_ended native_cleanup=complete window=same\r\n");
        }
        return;
    }
    /* A running session retains its control service and initial start record.
     * A newcomer waits for a separate native admission, never replays Start. */
    if (status.running) {
        service_running_join();
        return;
    }
    if (handoff_complete) return;
    if (status.departure_safe && SudekiMpLobbyGameplayActive() && p->phase==SUDEKIMP_LOBBY_START_COMPLETE) {
        if (status.phase==SUDEKIMP_LOBBY_HOSTING && !SudekiMpLobbyHostRunning(session)) return;
        handoff_complete=TRUE;
        opened=focused=FALSE;
        SudekiMpLogFormat("title_lobby event=entered_game window=same destination=%u\r\n",p->destination);
        return;
    }
    if (!start_busy() || (status.phase!=SUDEKIMP_LOBBY_HOSTING && status.phase!=SUDEKIMP_LOBBY_CONNECTED)) {
        BOOL cancelled=SudekiMpLobbyGameplayCancel();
        /* An aborted initial load may still retain native actor owners.
         * Only confirmed cancellation releases its departed lobby members. */
        if(cancelled && status.phase==SUDEKIMP_LOBBY_HOSTING && !start_busy())
            (void)SudekiMpLobbyHostNativeDrained(session);
        return;
    }
    if (!SudekiMpLobbyGameplayActive() && p->phase==SUDEKIMP_LOBBY_START_PREPARE && (!status.local_slot || p->port)) {
        SudekiMpLobbyLaunchPlan plan={.revision=p->revision,.generation=p->generation,
            .seat=status.local_slot,.members=p->members,.port=status.local_slot?p->port:0};
        for (unsigned i=0;i<4;++i) {
            const SudekiMpLobbyMember *m=&status.members[i];
            if (m->reserved && m->locked && m->character<4)
                plan.reserved_mask|=(uint8_t)(1u<<i);
            plan.character[i]=(plan.reserved_mask&(1u<<i))?
                m->character:SUDEKIMP_LOBBY_NO_CHARACTER;
        }
        memcpy(plan.nonce,p->nonce,sizeof(plan.nonce));
        snprintf(plan.host_ipv4,sizeof(plan.host_ipv4),"%s",status.host_ipv4);
        if (!launch_available || !prepare_game(&plan)) {
            SudekiMpLobbyLoadAck(session,p->revision,p->generation,SUDEKIMP_LOBBY_ACK_FAILED,0);
            strcpy(message,p->destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME?
                "Could not prepare the selected save. Every player needs a matching local copy.":
                "Could not prepare Test Room. See the game log for details.");
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
        strcpy(message,"Game preparation or loading failed. Return to the lobby and try again.");
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
static void refresh_saves(void) {
    unsigned prior=selected_save<saves.count?saves.entries[selected_save].folder_slot:~0u;
    save_reviewed=FALSE;
    memset(&reviewed_save,0,sizeof(reviewed_save));
    if (!SudekiMpSaveCatalogRefresh(&saves))
        snprintf(message,sizeof(message),"%.127s",saves.error);
    selected_save=~0u;
    for (unsigned i=0;i<saves.count;++i)
        if (saves.entries[i].folder_slot==prior) { selected_save=i; break; }
    if (selected_save==~0u && saves.count) selected_save=0;
    save_page=selected_save<saves.count?selected_save/PAGE_SIZE:0;
    ++page_revision; /* Discard clicks belonging to the previous list. */
}
static void save_details(const SudekiMpSaveCatalogEntry *entry,float x,float y,float width) {
    char line[128];
    if (!entry) {
        note(x,y,23,width,"Choose a saved game");
        note(x,y+38,17,width,"Its details will appear here.");
        return;
    }
    note(x,y,24,width,entry->label);
    static const char *const leaders[]={"Buki","Elco","Tal","Ailish"};
    if(entry->party_known && entry->leader<4u)
        snprintf(line,sizeof(line),"Save %04lu - Leader: %s",
            (unsigned long)entry->folder_slot,leaders[entry->leader]);
    else snprintf(line,sizeof(line),"Save %04lu - Leader unknown",(unsigned long)entry->folder_slot);
    note(x,y+30,16,width,line);
    if (*entry->location) note(x,y+66,19,width,entry->location);
    snprintf(line,sizeof(line),"File updated: %s",entry->saved_at);
    note(x,y+102,16,width,line);
    if (*entry->play_time) {
        snprintf(line,sizeof(line),"Time played: %s",entry->play_time);
        note(x,y+132,18,width,line);
    }
    if (entry->metadata_known) for (unsigned i=0;i<4;++i)
        if (*entry->details[i]) note(x,y+173+i*28,16,width,entry->details[i]);
    if (!entry->metadata_known) {
        note(x,y+176,17,width,"Game details are unavailable for this save.");
        note(x,y+206,16,width,"The original save files remain unchanged.");
    }
}
static void build_destination_view(void) {
    view.text.full_width=TRUE;
    if (page==DESTINATIONS) {
        strcpy(view.text.heading,"Choose Destination");
        strcpy(view.text.hint,"Choose where your party will begin");
        unsigned id=control(PICK_TESTROOM,SUDEKIMP_PANEL_MEMBER,106,222,748,78,"Test Room",
            destination_editable(),status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM);
        strcpy(view.text.controls[id].detail,"Practice together with the full party.");
        id=control(PICK_SAVE,SUDEKIMP_PANEL_MEMBER,106,324,748,78,"Saved Game",
            destination_editable(),status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME);
        strcpy(view.text.controls[id].detail,"Browse your saves and choose one for this lobby.");
        note(106,455,19,748,"Your choice is remembered when you return to the lobby.");
        note(106,491,18,748,"Choosing a destination does not start the game.");
        control(RETURN_ROOM,SUDEKIMP_PANEL_BUTTON,106,559,224,42,"Back to Lobby",TRUE,FALSE);
        strcpy(view.text.status,"The host chooses the destination. Everyone sees the confirmed choice.");
    } else if (page==SAVES) {
        strcpy(view.text.heading,"Choose Saved Game");
        strcpy(view.text.hint,"Select a save to view its details");
        view.text.save_details=TRUE;
        for (unsigned i=0;i<PAGE_SIZE;++i) {
            unsigned index=save_page*PAGE_SIZE+i;
            const SudekiMpSaveCatalogEntry *entry=index<saves.count?&saves.entries[index]:NULL;
            unsigned id=control(SAVE_ROW,SUDEKIMP_PANEL_SAVE,76,206+i*51,380,47,
                entry?entry->label:"",entry!=NULL,entry && index==selected_save);
            row_saves[id]=index;
            if (entry) snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),
                "Save %04lu   %s",(unsigned long)entry->folder_slot,entry->play_time);
        }
        if (!saves.count) {
            note(93,251,22,344,"No saved games found");
            note(93,289,16,344,"Saves from this game profile appear here.");
        }
        save_details(selected_save<saves.count?&saves.entries[selected_save]:NULL,509,221,351);
        char count[128];
        snprintf(count,sizeof(count),"%u saves  -  %u / %u",saves.count,save_page+1,
            saves.count?(saves.count+PAGE_SIZE-1)/PAGE_SIZE:1);
        note(76,537,16,208,count);
        control(SAVE_PREVIOUS,SUDEKIMP_PANEL_BUTTON,286,521,80,32,"Previous",save_page>0,FALSE);
        control(SAVE_NEXT,SUDEKIMP_PANEL_BUTTON,376,521,80,32,"Next",(save_page+1)*PAGE_SIZE<saves.count,FALSE);
        control(RETURN_DESTINATIONS,SUDEKIMP_PANEL_BUTTON,76,570,190,38,"Back",TRUE,FALSE);
        control(SAVE_REFRESH,SUDEKIMP_PANEL_BUTTON,286,570,170,38,"Refresh",destination_editable(),FALSE);
        control(REVIEW_SAVE,SUDEKIMP_PANEL_BUTTON,608,570,252,38,"Choose This Save",
            destination_editable() && selected_save<saves.count,FALSE);
        strcpy(view.text.status,"Selecting a save opens a confirmation. It does not load the game.");
        if (saves.skipped || saves.truncated)
            snprintf(view.text.status,sizeof(view.text.status),"%u saves shown. Some entries could not be listed. Refresh to check again.",saves.count);
    } else {
        strcpy(view.text.heading,"Use This Saved Game?");
        strcpy(view.text.hint,"Remember this choice for the game you are hosting?");
        save_details(selected_save<saves.count?&saves.entries[selected_save]:NULL,236,207,488);
        note(128,517,18,704,"You will return to the lobby to finish setting up your party.");
        control(CONFIRM_NO,SUDEKIMP_PANEL_BUTTON,168,570,276,40,"No - Go Back",TRUE,FALSE);
        control(CONFIRM_YES,SUDEKIMP_PANEL_BUTTON,516,570,276,40,"Yes - Use This Save",
            destination_editable() && save_reviewed,FALSE);
        strcpy(view.text.status,SudekiMpLobbyGameplaySavedAvailable()?
            "Confirm the save, then choose characters and Ready in the lobby.":
            "This remembers your choice. Shared story start is not available yet.");
    }
}
static void build_view(void) {
    memset(&view,0,sizeof(view)); memset(row_servers,0,sizeof(row_servers));
    for (unsigned i=0;i<SUDEKIMP_PANEL_CONTROLS;++i) row_saves[i]=~0u;
    view.text.panel=TRUE;
    strcpy(view.text.heading,"Multiplayer");
    strcpy(view.text.hint,"Gather your party");
    if (page<ROOM) {
        control(FIND,SUDEKIMP_PANEL_NAV,66,188,158,45,"Find a Game",TRUE,page==BROWSE);
        control(HOST,SUDEKIMP_PANEL_NAV,66,244,158,45,"Host a Game",TRUE,page==CREATE);
        control(DIRECT,SUDEKIMP_PANEL_NAV,66,300,158,45,"Direct Connect",TRUE,page==JOIN);
        control(BACK,SUDEKIMP_PANEL_NAV,66,560,158,40,"Main Menu",TRUE,FALSE);
        note(78,414,20,136,"Your party");
        note(78,445,16,136,"1 to 4 players");
        note(78,474,16,136,"Unclaimed heroes");
        note(78,498,16,136,"remain with AI.");
    }
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
                snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),"%u/4 %s",server->players,
                    server->running?"Running":"Lobby");
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
    } else if (page==ROOM) {
        BOOL connected=status.phase==SUDEKIMP_LOBBY_HOSTING || status.phase==SUDEKIMP_LOBBY_CONNECTED;
        BOOL host=status.phase==SUDEKIMP_LOBBY_HOSTING;
        BOOL busy=start_busy() || SudekiMpLobbyGameplayActive();
        BOOL testroom=status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM;
        BOOL saved=status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME;
        const SudekiMpLobbyMember *local=&status.members[status.local_slot];
        BOOL can_select=connected && (testroom || saved) && !busy && (!status.running ||
            status.admission[status.local_slot].phase==SUDEKIMP_LOBBY_ADMISSION_NONE);
        static const char *const actors[]={"Buki","Elco","Tal","Ailish","No character"};
        /* The room uses both columns: party on the left, local setup on the
         * right. MEMBER rows need room for their two native-font text lines. */
        view.text.full_width=TRUE;
        char destination_label[80];
        snprintf(destination_label,sizeof(destination_label),"%s",status.running?"Test Room - In Progress":
            testroom?"Destination: Test Room":"Choose Destination");
        if (saved) snprintf(destination_label,sizeof(destination_label),"Destination: %s",status.saved_game.label);
        control(DESTINATION,SUDEKIMP_PANEL_BUTTON,76,208,388,38,destination_label,
            host && !busy && !status.running,testroom || saved);
        unsigned members=0,ready=0;
        for (unsigned i=0;i<4;++i) {
            const SudekiMpLobbyMember *m=&status.members[i];
            members+=m->present; ready+=m->present && m->ready;
            char label[80],fallback[32];
            snprintf(fallback,sizeof(fallback),"Player %u",i+1);
            if (m->reserved) snprintf(label,sizeof(label),"%s%s%s",*m->name?m->name:m->character<4?actors[m->character]:fallback,
                i==0?" - Host":"",i==status.local_slot?" - You":"");
            else strcpy(label,"Open slot");
            unsigned id=control(NONE,
                SUDEKIMP_PANEL_MEMBER,76,262+i*70,388,64,label,FALSE,m->present);
            if (m->reserved) {
                const char *state=!m->present?"Leaving...":
                    status.running?"Connected":m->ready?"Ready":m->locked?"Locked":"Choosing";
                snprintf(view.text.controls[id].detail,sizeof(view.text.controls[id].detail),"%s%s%s",
                    *m->name || m->character>=4?actors[m->character]:"",
                    *m->name || m->character>=4?" - ":"",state);
            } else strcpy(view.text.controls[id].detail,"Available for another player");
        }
        char roster[128];
        snprintf(roster,sizeof(roster),"Party - %u / 4 connected - %u ready",members,ready);
        note(76,193,17,388,roster);
        note(504,302,20,364,host && saved?"Host plays the saved leader":"Choose your character");
        for (unsigned character=0;character<4;++character) {
            BOOL available=TRUE;
            if(host && saved && !status.running && character!=status.saved_game.leader) available=FALSE;
            for (unsigned i=0;i<4;++i)
                if (i!=status.local_slot && status.members[i].reserved && status.members[i].locked &&
                    status.members[i].character==character) available=FALSE;
            char choice[32];
            snprintf(choice,sizeof(choice),"%s%s",actors[character],
                saved && !(status.saved_game.party_mask&(1u<<character))?" (later)":"");
            control((enum Action)(CHARACTER_BUKI+character),SUDEKIMP_PANEL_BUTTON,
                504+(character%2)*190,320+(character/2)*50,174,40,
                choice,can_select && available && !(host && saved && !status.running),local->character==character);
        }
        control(LOCK_CHARACTER,SUDEKIMP_PANEL_BUTTON,504,430,364,40,
            host && saved && !status.running?"Saved Leader Assigned":local->locked?"Unlock Character":"Lock Character",
            can_select && local->character<4 && !(host && saved && !status.running),local->locked);
        unsigned name_id=control(NAME,SUDEKIMP_PANEL_FIELD,504,208,364,38,"Your name (optional)",connected && !busy,editing==player);
        snprintf(view.text.controls[name_id].value,sizeof(view.text.controls[name_id].value),"%s%s",player,editing==player?"_":"");
        note(504,264,16,364,"Leave blank to use your character's name.");
        snprintf(view.text.heading,sizeof(view.text.heading),"%s",*status.room?status.room:"Connecting...");
        if (host) snprintf(view.text.hint,sizeof(view.text.hint),"Hosting on port %u - %s",status.port,
            status.advertised?"Discoverable on LAN":"Unlisted - direct address only");
        else strcpy(view.text.hint,!connected?"Waiting for the host...":status.running?
            "Join the party already in progress":"Choose a character, lock it, then select Ready");
        if (host) control(VISIBILITY,SUDEKIMP_PANEL_BUTTON,504,492,364,40,
            status.advertised?"Discoverable on LAN":"Unlisted - Join by Address",!busy,status.advertised);
        else note(504,512,17,364,"Unclaimed characters stay with AI.");
        control(READY,SUDEKIMP_PANEL_BUTTON,76,560,240,40,wanted_ready?"Not Ready":"Ready",
            connected && (testroom || saved) && !busy && !status.running && local->locked,FALSE);
        control(START_GAME,SUDEKIMP_PANEL_BUTTON,340,560,280,40,busy?"Loading...":status.running?"Game Running":host?"Start Game":"Waiting for Host",
            host && ((testroom && SudekiMpLobbyGameplayTestroomAvailable()) ||
                (saved && SudekiMpLobbyGameplaySavedAvailable())) &&
                !busy && !status.running && launch_available && members && ready==members,FALSE);
        control(status.phase==SUDEKIMP_LOBBY_ERROR?RECONNECT:LEAVE,SUDEKIMP_PANEL_BUTTON,644,560,224,40,
            status.phase==SUDEKIMP_LOBBY_ERROR?"Join Again":"Leave Lobby",TRUE,FALSE);
        strcpy(view.text.status,testroom?"Select and lock a character, then Ready. The host starts when everyone is ready.":
            saved?(SudekiMpLobbyGameplaySavedAvailable()?
                "Select and lock a character, then Ready. Unavailable characters spectate until they join the party.":
                "Save selected. Starting a shared story is not available yet."):
            "Choose a destination first; character choices follow its party.");
        if (status.running) strcpy(view.text.status,host?(saved?"The party is playing the selected save.":"The party is in the Test Room."):can_select?"Choose and lock an available character to join.":
            "Waiting for the host to finish synchronizing your game...");
        else if (status.start.phase==SUDEKIMP_LOBBY_START_PREPARE)
            snprintf(view.text.status,sizeof(view.text.status),"Preparing %s: %u of %u players. Leave cancels the start.",
                saved?"saved game":"Test Room",bits(status.start.prepared),bits(status.start.members));
        else if (status.start.phase==SUDEKIMP_LOBBY_START_LOADING)
            snprintf(view.text.status,sizeof(view.text.status),"Loading %s: %u of %u players.",
                saved?"saved game":"Test Room",bits(status.start.loaded),bits(status.start.members));
        else if (status.start.phase==SUDEKIMP_LOBBY_START_COMPLETE) strcpy(view.text.status,"Everyone loaded. Entering the game...");
        else if (status.start.phase==SUDEKIMP_LOBBY_START_ABORTED) strcpy(view.text.status,"Start cancelled or loading failed. Ready up again to retry.");
        if (!launch_available) strcpy(view.text.status,"Game start is unavailable in this build.");
        else if(testroom && !SudekiMpLobbyGameplayTestroomAvailable())
            strcpy(view.text.status,"Choose a saved game to start with this profile.");
        if (status.phase==SUDEKIMP_LOBBY_ERROR) {
            strcpy(view.text.heading,"Connection ended");
            snprintf(view.text.hint,sizeof(view.text.hint),"%s",status.error);
        }
    } else build_destination_view();
    if (*message) snprintf(view.text.status,sizeof(view.text.status),"%s",message);
    if (editing) strcpy(view.text.status,"Type to edit. Ctrl+A clears; Enter or Escape finishes editing.");
}
void SudekiMpLobbyUiOpen(void) {
    opened=TRUE; focused=FALSE; wanted_ready=FALSE;
    launch_available=TRUE; seen_revision=0; seen_command=0;
    if (!session) {
        session=SudekiMpLobbyCreate();
        (void)SudekiMpLobbyEnableSavedStart(session,SudekiMpLobbyGameplaySavedAvailable());
    }
    SudekiMpLobbyStatusGet(session,&status);
    if (SudekiMpLobbyGameplayActive()) SudekiMpLobbyGameplayService(session);
    if (!status.running) handoff_complete=FALSE;
    change_page(status.running?ROOM:BROWSE);
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
SudekiMpLobby *SudekiMpLobbyUiSession(void) { return session; }
BOOL SudekiMpLobbyUiEditing(void) { return editing!=NULL || edit_release; }
unsigned SudekiMpLobbyUiPageRevision(void) { return page_revision; }
void SudekiMpLobbyUiEndEdit(void) {
    if (editing==player && page==ROOM && !SudekiMpLobbySetName(session,player))
        strcpy(message,"Name update could not be queued. Wait for the previous choice and try again.");
    editing=NULL; edit_release=FALSE; snapshot_keys(); build_view();
}
static uint32_t seen_rejected_roster;
static void begin_edit(char *text,unsigned size) {
    editing=text; edit_capacity=size; message[0]=0; snapshot_keys();
}
static BOOL pressed(unsigned key) {
    BOOL down=(GetAsyncKeyState((int)key)&0x8000)!=0;
    BOOL edge=down && !keys[key]; keys[key]=down; return edge;
}
void SudekiMpLobbyUiPoll(HWND window,BOOL input) {
    if (!opened) return;
    if (SudekiMpLobbyGameplayActive()) SudekiMpLobbyGameplayService(session);
    SudekiMpLobbyStatusGet(session,&status);
    if (destination_page() && !destination_editable()) {
        save_reviewed=FALSE;
        change_page(ROOM);
    }
    if (status.command_sequence!=seen_command ||
        (status.command_rejected && status.roster_revision!=seen_rejected_roster)) {
        seen_command=status.command_sequence; seen_rejected_roster=status.roster_revision;
        if (status.command_rejected) strcpy(message,"The roster changed or that character is unavailable. Please choose again.");
    }
    if (page==ROOM && editing!=player && (status.phase==SUDEKIMP_LOBBY_HOSTING || status.phase==SUDEKIMP_LOBBY_CONNECTED))
        snprintf(player,sizeof(player),"%s",status.members[status.local_slot].name);
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
        SudekiMpLogFormat("title_lobby event=roster phase=%u local_slot=%u members=%u ready=%u advertised=%u discovered=%u discovery_error=%u\r\n",
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
                SudekiMpLobbyUiEndEdit(); edit_release=TRUE; edit_released_at=0; break;
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
    } else if (page==SAVES) {
        if (pressed(VK_PRIOR) && save_page) { --save_page; ++page_revision; }
        if (pressed(VK_NEXT) && (save_page+1)*PAGE_SIZE<saves.count) { ++save_page; ++page_revision; }
    }
    build_view();
}
void SudekiMpLobbyUiArm(unsigned row) {
    armed_page=page; armed_action=row<view.count?actions[row]:NONE;
    memset(&armed_server,0,sizeof(armed_server));
    /* Pin the exact clicked/selected room across asynchronous discovery updates. */
    if (armed_action==SERVER) armed_server=row_servers[row];
    if (armed_action==JOIN_SELECTED) armed_server=chosen;
    armed_save=armed_action==SAVE_ROW?row_saves[row]:selected_save;
}
BOOL SudekiMpLobbyUiBack(unsigned *selection) {
    if (editing) { SudekiMpLobbyUiEndEdit(); return FALSE; }
    if (destination_page()) {
        save_reviewed=FALSE;
        change_page(page==CONFIRM_SAVE?SAVES:page==SAVES?DESTINATIONS:ROOM);
        *selection=0; build_view(); return FALSE;
    }
    if (page!=ROOM) return TRUE;
    if (!SudekiMpLobbyGameplayCancel()) {
        strcpy(message,"Waiting for native loading or control to finish before leaving."); build_view(); return FALSE;
    }
    SudekiMpLobbyLeave(session); wanted_ready=FALSE;
    handoff_complete=FALSE;
    change_page(BROWSE); *selection=0; build_view(); return FALSE;
}
static void join_room(const SudekiMpLobbyServer *server) {
    wanted_ready=FALSE;
    handoff_complete=FALSE;
    if (SudekiMpLobbyJoin(session,server->ipv4,server->port,player)) change_page(ROOM);
    else { SudekiMpLobbyStatusGet(session,&status); snprintf(message,sizeof(message),"%s",
        *status.error?status.error:"Unable to start a connection."); }
}
BOOL SudekiMpLobbyUiCommit(unsigned *selection) {
    if (page!=armed_page) return FALSE;
    SudekiMpLobbyStatusGet(session,&status);
    if (destination_page() && !destination_editable()) {
        save_reviewed=FALSE; change_page(ROOM); *selection=0; build_view(); return FALSE;
    }
    message[0]=0;
    switch (armed_action) {
    case FIND: case HOST: case DIRECT:
        if (page==ROOM) break;
        change_page(armed_action==FIND?BROWSE:armed_action==HOST?CREATE:JOIN);
        *selection=(unsigned)armed_action; break;
    case BACK: return page<ROOM;
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
        if (!valid_text(room)) strcpy(message,"Enter a room name. Your player name is optional.");
        else if (!SudekiMpLanArenaParseEndpoint(endpoint,SUDEKIMP_LOBBY_PORT,ip,sizeof(ip),&number))
            strcpy(message,"Choose a port between 1024 and 65535.");
        else if (SudekiMpLobbyHost(session,room,player,number,advertised)) {
            wanted_ready=FALSE; handoff_complete=FALSE; change_page(ROOM); *selection=0;
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
    case CHARACTER_BUKI: case CHARACTER_ELCO: case CHARACTER_TAL: case CHARACTER_AILISH:
        if (!SudekiMpLobbySelectCharacter(session,(unsigned)(armed_action-CHARACTER_BUKI),FALSE))
            strcpy(message,"Character selection is waiting for the host or the previous request.");
        wanted_ready=FALSE; break;
    case LOCK_CHARACTER: {
        const SudekiMpLobbyMember *m=&status.members[status.local_slot];
        if (!SudekiMpLobbySelectCharacter(session,m->character,!m->locked))
            strcpy(message,"That character cannot be locked now. Wait for the host or choose another.");
        wanted_ready=FALSE; break;
    }
    case RECONNECT:
        if (!SudekiMpLobbyReconnect(session)) strcpy(message,"Unable to rejoin. Leave, then join again and choose an available character.");
        break;
    case DESTINATION:
        if (destination_editable()) {
            change_page(DESTINATIONS); *selection=0;
        }
        break;
    case PICK_TESTROOM:
        if (SudekiMpLobbyDestination(session,SUDEKIMP_LOBBY_DEST_TESTROOM)) {
            SudekiMpLobbyStatusGet(session,&status);
            wanted_ready=status.members[status.local_slot].ready!=0;
            change_page(ROOM); *selection=0;
        }
        break;
    case PICK_SAVE:
        change_page(SAVES); refresh_saves(); *selection=0;
        break;
    case SAVE_REFRESH:
        refresh_saves(); *selection=0;
        break;
    case SAVE_PREVIOUS:
        if (save_page) { --save_page; ++page_revision; }
        *selection=0; break;
    case SAVE_NEXT:
        if ((save_page+1)*PAGE_SIZE<saves.count) { ++save_page; ++page_revision; }
        *selection=0; break;
    case SAVE_ROW:
        if (armed_save<saves.count) selected_save=armed_save;
        save_reviewed=FALSE; break;
    case REVIEW_SAVE:
        save_reviewed=FALSE;
        if (armed_save<saves.count && SudekiMpSaveCatalogFingerprint(&saves,armed_save,&reviewed_save)) {
            selected_save=armed_save; save_reviewed=TRUE;
            change_page(CONFIRM_SAVE); *selection=0;
        } else snprintf(message,sizeof(message),"%.127s",*saves.error?saves.error:"Select a saved game first.");
        break;
    case CONFIRM_YES:
        if (save_reviewed && selected_save<saves.count) {
            if(!saves.entries[selected_save].party_known) {
                strcpy(message,"This save's party could not be identified. Choose another save.");
                break;
            }
            if (!SudekiMpSaveCatalogVerify(&saves,selected_save,&reviewed_save)) {
                save_reviewed=FALSE;
                snprintf(message,sizeof(message),"%.127s",saves.error);
                break;
            }
            SudekiMpLobbySavedGame save={.folder_slot=reviewed_save.folder_slot};
            memcpy(save.fish_sha256,reviewed_save.fish_sha256,sizeof(save.fish_sha256));
            memcpy(save.bunny_sha256,reviewed_save.bunny_sha256,sizeof(save.bunny_sha256));
            snprintf(save.label,sizeof(save.label),"%.47s",saves.entries[selected_save].label);
            save.party_count=saves.entries[selected_save].party_count;
            save.leader=saves.entries[selected_save].leader;
            save.party_mask=saves.entries[selected_save].party_mask;
            memcpy(save.party_order,saves.entries[selected_save].party_order,sizeof(save.party_order));
            if (!SudekiMpLobbySelectSavedGame(session,&save)) {
                strcpy(message,"The destination could not be changed. Return to the lobby and try again.");
                break;
            }
            SudekiMpLobbyStatusGet(session,&status);
            wanted_ready=status.members[status.local_slot].ready!=0;
            save_reviewed=FALSE; change_page(ROOM); *selection=0;
            SudekiMpLogFormat("title_lobby event=save_selected folder_slot=%lu load_requested=0\r\n",
                (unsigned long)save.folder_slot);
        }
        break;
    case CONFIRM_NO:
        save_reviewed=FALSE; change_page(SAVES); *selection=0;
        break;
    case RETURN_ROOM:
        save_reviewed=FALSE; change_page(ROOM); *selection=0;
        break;
    case RETURN_DESTINATIONS:
        save_reviewed=FALSE; change_page(DESTINATIONS); *selection=0;
        break;
    case START_GAME:
        if (!launch_available || !SudekiMpLobbyStartGame(session))
            strcpy(message,status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME && !SudekiMpLobbyGameplaySavedAvailable()?
                "Save selected. Starting a shared story is not available yet.":
                "Choose a destination and wait for all connected players to be ready.");
        break;
    case LEAVE: return SudekiMpLobbyUiBack(selection);
    case NONE: break;
    }
    SudekiMpLobbyStatusGet(session,&status); build_view(); return FALSE;
}

void SudekiMpLobbyUiBackground(void) {
    if (!session || opened || !SudekiMpLobbyGameplayActive()) return;
    SudekiMpLobbyGameplayService(session);
    SudekiMpLobbyStatusGet(session,&status); service_start(NULL);
}
