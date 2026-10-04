#include "ui/party_menu.h"
#include <stdio.h>
#include <string.h>

enum { HOME, CHARACTERS, PLAYERS, LEAVE_CONFIRM, REASSIGN };
enum { PAGE_HOME=100, PAGE_CHARACTERS, PAGE_PLAYERS, PAGE_LEAVE, CANCEL_LEAVE, PICK_RECIPIENT };
static const char *const characters[5]={"Buki","Elco","Tal","Ailish","Spectator"};
static SudekiMpPartyMenuState state;
static SudekiMpPartyMenuCommand command;
static SudekiMpTitleExtras panel;
static unsigned count, enabled, selected, page, recipient, actions[24], arguments[24];
static BOOL opened, focused, keys[256];
static HWND window;
static void keys_snapshot(void) {
    for(unsigned i=0;i<256;++i) keys[i]=(GetAsyncKeyState((int)i)&0x8000)!=0;
}
static BOOL pressed(unsigned key) {
    BOOL down=(GetAsyncKeyState((int)key)&0x8000)!=0, edge=down&&!keys[key];
    keys[key]=down; return edge;
}
static unsigned add(unsigned action,unsigned arg,SudekiMpPanelKind kind,
    float x,float y,float width,float height,const char *label,BOOL active) {
    if(count>=SUDEKIMP_PANEL_CONTROLS) return count;
    unsigned n=count++;
    SudekiMpPanelControl *c=&panel.controls[n];
    c->kind=kind; c->x=x; c->y=y; c->width=width; c->height=height; c->active=active;
    snprintf(c->label,sizeof(c->label),"%s",label);
    actions[n]=action; arguments[n]=arg;
    if(active) enabled|=1u<<n;
    return n;
}
static const char *player_name(unsigned p) {
    return *state.name[p]?state.name[p]:characters[state.character[p]<=4?state.character[p]:4];
}
static const char *player_status(unsigned p) {
    unsigned b=1u<<p;
    return !(state.connected&b)?((state.reserved&b)?"Releasing character...":"Open slot"):
        (state.away&b)?"Away; AI cover":(state.menu&b)?"In menu; AI cover":
        state.paused?"Paused":(state.input_ready&b)?"Playing":"Spectating / transfer pending";
}
static BOOL transfer_safe(unsigned target,unsigned character) {
    if(state.saved_story || !state.host || target>=4u || character>=4u || !(state.connected&(1u<<target)) ||
        state.combat || state.transition || state.paused ||
        (state.busy_characters&(1u<<character)) || state.character[target]==character ||
        (state.character[target]<4u && (state.busy_characters&(1u<<state.character[target]))))
        return FALSE;
    for(unsigned p=0;p<4u;++p) if((state.reserved&(1u<<p)) && state.character[p]==character)
        return !(state.controlling&(1u<<p)) &&
            (state.connected&(1u<<p)) && ((state.away|state.menu)&(1u<<p));
    return TRUE;
}
static void rebuild(void) {
    memset(&panel,0,sizeof(panel)); panel.panel=TRUE; count=enabled=0;
    if(state.saved_story && (page==CHARACTERS || page==REASSIGN)) page=HOME;
    snprintf(panel.heading,sizeof(panel.heading),"Sudeki Together");
    snprintf(panel.hint,sizeof(panel.hint),"%s",state.paused?"Party paused - the host controls resuming":
        "The world keeps moving while this menu is open");
    snprintf(panel.status,sizeof(panel.status),"%s",state.status);
    add(PAGE_HOME,0,SUDEKIMP_PANEL_NAV,82,176,152,40,"Session",TRUE);
    if(!state.saved_story)
        add(PAGE_CHARACTERS,0,SUDEKIMP_PANEL_NAV,82,228,152,40,"Characters",TRUE);
    add(PAGE_PLAYERS,0,SUDEKIMP_PANEL_NAV,82,state.saved_story?228:280,152,40,"Players",TRUE);
    unsigned self=state.local_player, bit=1u<<self;
    if(page==HOME) {
        if(state.saved_story) {
            add(SUDEKIMP_PARTY_MENU_CLOSE,self,SUDEKIMP_PANEL_BUTTON,
                278,182,566,42,"Return to Game",TRUE);
            unsigned row=add(0,0,SUDEKIMP_PANEL_MEMBER,278,258,566,88,
                player_name(self),FALSE);
            snprintf(panel.controls[row].detail,sizeof(panel.controls[row].detail),
                "%s - F1: switch to an unclaimed party member",state.story_detail[self]);
        } else {
            add((state.away&bit)?SUDEKIMP_PARTY_MENU_RETURN:SUDEKIMP_PARTY_MENU_CLOSE,self,
                SUDEKIMP_PANEL_BUTTON,278,182,566,42,"Return to Play",state.character[self]<4);
            add(SUDEKIMP_PARTY_MENU_AWAY,self,SUDEKIMP_PANEL_BUTTON,278,240,566,42,
                "Away - watch the party",!(state.away&bit));
            add(state.host?(state.paused?SUDEKIMP_PARTY_MENU_RESUME:SUDEKIMP_PARTY_MENU_PAUSE):
                SUDEKIMP_PARTY_MENU_REQUEST_PAUSE,0,SUDEKIMP_PANEL_BUTTON,278,310,566,42,
                state.host?(state.paused?"Resume Party":"Pause Party"):"Request Pause",TRUE);
            if(state.host) add(SUDEKIMP_PARTY_MENU_POLICY,!state.policy,SUDEKIMP_PANEL_BUTTON,
                278,368,566,42,state.policy?"Absences: Pause Party":"Absences: AI Cover",TRUE);
        }
        add(PAGE_LEAVE,0,SUDEKIMP_PANEL_BUTTON,278,520,566,42,
            state.host?"End Session...":"Leave Session...",TRUE);
    } else if(page==CHARACTERS) {
        for(unsigned c=0;c<4;++c) {
            unsigned owner=4;
            for(unsigned p=0;p<4;++p)
                if((state.reserved&(1u<<p))&&state.character[p]==c) owner=p;
            char label[80];
            snprintf(label,sizeof(label),"%s%s",characters[c],owner==self?" - Yours":owner<4?
                ((state.connected&(1u<<owner))?" - Assigned":" - Releasing..."):" - Available AI");
            BOOL safe=owner==4 && !state.combat && !state.transition && !state.paused &&
                !(state.busy_characters&(1u<<c));
            add(SUDEKIMP_PARTY_MENU_SWITCH,c,SUDEKIMP_PANEL_BUTTON,278,186+c*76,566,56,label,safe);
        }
        if(!*panel.status) snprintf(panel.status,sizeof(panel.status),"Switching is available outside combat and active abilities.");
    } else if(page==PLAYERS) {
        for(unsigned p=0;p<4;++p) {
            char label[80]; unsigned b=1u<<p;
            BOOL occupied=((state.connected|state.reserved)&b)!=0;
            BOOL host_action=!state.saved_story && state.host && (state.connected&b);
            snprintf(label,sizeof(label),"%s%s",occupied?player_name(p):"Open slot",
                occupied && p==0?" - Host":"");
            unsigned row=add(0,0,SUDEKIMP_PANEL_MEMBER,278,180+p*87,
                host_action?456:566,72,label,FALSE);
            if(occupied && state.saved_story)
                snprintf(panel.controls[row].detail,sizeof(panel.controls[row].detail),
                    "%s",state.story_detail[p]);
            else if(occupied) snprintf(panel.controls[row].detail,sizeof(panel.controls[row].detail),"%s - %s",
                characters[state.character[p]<=4?state.character[p]:4],player_status(p));
            else snprintf(panel.controls[row].detail,sizeof(panel.controls[row].detail),
                "%s",state.saved_story?"No player connected":"Available for another player");
            if(host_action) {
                BOOL assignable=FALSE;
                for(unsigned c=0;c<4u;++c) if(transfer_safe(p,c)) assignable=TRUE;
                add(PICK_RECIPIENT,p,SUDEKIMP_PANEL_BUTTON,750,194+p*87,94,42,"Assign",assignable);
            }
        }
        if(!*panel.status) snprintf(panel.status,sizeof(panel.status),
            "%s",state.saved_story?"Players and their current characters; F1 switches to free AI":
                "Leaving makes your character available to other players.");
    } else if(page==REASSIGN) {
        snprintf(panel.hint,sizeof(panel.hint),"Assign a character to %.31s",player_name(recipient));
        for(unsigned c=0;c<4u;++c) {
            unsigned owner=4u;
            for(unsigned p=0;p<4u;++p)
                if((state.reserved&(1u<<p)) && state.character[p]==c) owner=p;
            char label[80];
            snprintf(label,sizeof(label),"%s - %s",characters[c],owner==recipient?"Already assigned":
                owner<4u?((state.connected&(1u<<owner))?"Assigned character":"Releasing..."):"Available AI");
            add(SUDEKIMP_PARTY_MENU_REASSIGN,(recipient<<8)|c,SUDEKIMP_PANEL_BUTTON,
                278,186+c*76,566,56,label,transfer_safe(recipient,c));
        }
        if(!*panel.status) snprintf(panel.status,sizeof(panel.status),
            "Assign an available AI or a player's character while they are away or in the menu.");
    } else {
        snprintf(panel.hint,sizeof(panel.hint),"%s",state.host?
            "End the session for everyone and return to the intro":
            state.saved_story?"Leave this session and return to the intro":
                "Return to the intro; AI takes over your character");
        add(CANCEL_LEAVE,0,SUDEKIMP_PANEL_BUTTON,278,290,270,48,"Keep Playing",TRUE);
        add(SUDEKIMP_PARTY_MENU_LEAVE,0,SUDEKIMP_PANEL_BUTTON,574,290,270,48,
            state.host?"End Session":"Leave Session",TRUE);
    }
    if(selected>=count || !(enabled&(1u<<selected))) selected=0;
}
static void activate(unsigned row) {
    if(row>=count || !(enabled&(1u<<row))) return;
    unsigned action=actions[row];
    if(action==PICK_RECIPIENT) {
        recipient=arguments[row]; page=REASSIGN; selected=0; return;
    }
    if(action>=PAGE_HOME) {
        page=action==PAGE_CHARACTERS?CHARACTERS:action==PAGE_PLAYERS?PLAYERS:
            action==PAGE_LEAVE?LEAVE_CONFIRM:HOME;
        selected=0; return;
    }
    if(command && command((SudekiMpPartyMenuAction)action,arguments[row])) {
        if(action==SUDEKIMP_PARTY_MENU_REASSIGN) { page=PLAYERS; selected=0; }
        if(action==SUDEKIMP_PARTY_MENU_CLOSE || action==SUDEKIMP_PARTY_MENU_RETURN ||
            action==SUDEKIMP_PARTY_MENU_AWAY) { opened=FALSE; focused=FALSE; }
    }
}
void SudekiMpPartyMenuInitialize(SudekiMpPartyMenuCommand fn) {
    SudekiMpPartyMenuReset(); command=fn;
}
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *next) {
    if(!next || next->local_player>=4) return;
    state=*next;
    for(unsigned p=0;p<4;++p) {
        state.name[p][31]=0;
        state.story_detail[p][sizeof(state.story_detail[p])-1]=0;
    }
    state.status[sizeof(state.status)-1]=0;
    if(!state.active) opened=FALSE;
}
void SudekiMpPartyMenuToggle(void) {
    if(!state.active || !command) return;
    if(opened) {
        /* Closing a menu never silently clears an explicit Away state. */
        if(command(SUDEKIMP_PARTY_MENU_CLOSE,state.local_player)) opened=FALSE;
    } else if(command(SUDEKIMP_PARTY_MENU_OPEN,state.local_player)) {
        opened=TRUE; page=HOME; selected=0; focused=FALSE; keys_snapshot();
    }
}
BOOL SudekiMpPartyMenuCapturesInput(void) {
    unsigned b=1u<<state.local_player;
    if(state.saved_story) return state.active && opened;
    return state.active && (opened || state.paused || (state.away&b) || !(state.input_ready&b));
}
static BOOL render_roster(void *device) {
    SudekiMpTitleExtras overlay={0};
    overlay.overlay=TRUE;
    /* A compact strip below the whole native party HUD, not a second roster
     * over the playfield. These labels identify connected people; portrait
     * slot order remains entirely native. Details stay in Escape -> Players. */
    if(*state.status) {
        /* Existing timed join/leave notices and essential local state share
         * the strip. Wrap rather than shrinking a full sentence to tiny text. */
        const char *remaining=state.status;
        for(unsigned row=0;row<2u && *remaining;++row) {
            SudekiMpPanelText *line=&overlay.texts[overlay.text_count++];
            line->x=660; line->y=684+row*19; line->width=228; line->size=13;
            size_t length=strlen(remaining),take=length>38u?38u:length;
            if(!row && length>take) {
                size_t word=take;
                while(word && remaining[word]!=' ') --word;
                if(word) take=word;
            }
            if(row && length>take) {
                snprintf(line->text,sizeof(line->text),"%.*s...",35,remaining);
                break;
            }
            snprintf(line->text,sizeof(line->text),"%.*s",(int)take,remaining);
            remaining+=take;
            while(*remaining==' ') ++remaining;
        }
    } else {
        for(unsigned p=0;p<4u;++p) if(state.connected&(1u<<p)) {
            unsigned n=overlay.text_count++;
            SudekiMpPanelText *line=&overlay.texts[n];
            line->x=660+(n%2u)*118; line->y=684+(n/2u)*19;
            line->width=110; line->size=13;
            const char *character=characters[state.character[p]<=4?state.character[p]:4];
            if(*state.name[p] && strcmp(state.name[p],character))
                snprintf(line->text,sizeof(line->text),"%.16s - %s",player_name(p),character);
            else snprintf(line->text,sizeof(line->text),"%s%s",character,
                p==state.local_player?" (you)":"");
        }
    }
    if(!overlay.text_count) return TRUE;
    return SudekiMpTitleViewDraw(device,1,0,0,NULL,SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&overlay);
}
BOOL SudekiMpPartyMenuRender(void *device) {
    if(!SudekiMpTitleViewRestore()) return FALSE;
    if(!state.active) return TRUE;
    if(!SudekiMpTitleViewPrepare(device)) return FALSE;
    if(!opened) return render_roster(device);
    rebuild();
    BOOL active=window && GetForegroundWindow()==window;
    if(!active) focused=FALSE;
    else if(!focused) { focused=TRUE; keys_snapshot(); }
    else {
        unsigned hover;
        POINT cursor;
        if(SudekiMpTitlePanelHit(window,count,&panel,&hover,&cursor) && (enabled&(1u<<hover))) selected=hover;
        BOOL up=pressed(VK_UP),down=pressed(VK_DOWN);
        if(up || down) {
            for(unsigned i=0;i<count;++i) {
                selected=up?(selected+count-1)%count:(selected+1)%count;
                if(enabled&(1u<<selected)) break;
            }
        }
        BOOL enter=pressed(VK_RETURN),click=pressed(VK_LBUTTON);
        if(enter) activate(selected);
        else if(click && SudekiMpTitlePanelHit(window,count,&panel,&hover,&cursor)) activate(hover);
        if(!opened) return TRUE;
        rebuild();
    }
    return SudekiMpTitleViewDraw(device,count,selected,enabled,NULL,
        SUDEKIMP_TITLE_BUTTON_REST,0,1,&window,&panel);
}
void SudekiMpPartyMenuReset(void) {
    memset(&state,0,sizeof(state)); memset(keys,0,sizeof(keys));
    command=NULL; opened=focused=FALSE; window=NULL; page=HOME; selected=recipient=0;
}
