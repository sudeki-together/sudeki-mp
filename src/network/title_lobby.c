#include <winsock2.h>
#include "network/title_lobby.h"
#include "engine/build_identity.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

enum { WIRE = 512, BODY = 48, HELLO = 1, STATE, READY, LOAD_ACK,
    SELECT_CHARACTER, SET_NAME, ADMISSION_ACK,
    QUERY = 10, OFFER, VERSION = SUDEKIMP_LOBBY_VERSION, POLL_LIMIT = 16, TIMEOUT = 8000 };
typedef struct Connection {
    SOCKET socket;
    unsigned rx_size, tx_size, tx_offset;
    uint8_t rx[WIRE], tx[WIRE];
    DWORD seen, sent;
    BOOL admitted;
    uint8_t player;
} Connection;
struct SudekiMpLobby {
    SRWLOCK lock;
    HANDLE worker, stop;
    SOCKET listener, discovery, browser;
    Connection connections[4];
    SudekiMpLobbyStatus status;
    uint8_t hash[32];
    BOOL browsing, connecting, desired_ready;
    DWORD query_at, discovery_window_at, connect_at;
    unsigned discovery_replies;
    uint64_t query_nonce, instance;
    DWORD start_at;
    uint32_t ready_revision;
    unsigned load_ack;
    char player[SUDEKIMP_LOBBY_NAME];
    uint8_t credentials[4][16], reconnect_credential[16];
    char reconnect_ipv4[16];
    uint16_t reconnect_port;
    uint32_t command_seen[4], next_command, admission_sequence;
    uint8_t command_rejected[4], command[WIRE];
    uint8_t native_members; /* Initial loading can own actors before running. */
    BOOL command_pending;
    DWORD admission_at[4];
    unsigned admission_ack;
    BOOL saved_start_enabled;
};

static void close_socket(SOCKET *socket) {
    if (*socket != INVALID_SOCKET) closesocket(*socket);
    *socket = INVALID_SOCKET;
}
static BOOL nonblocking(SOCKET socket) {
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}
static void put16(uint8_t *p, unsigned value) { p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }
static unsigned get16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
static void put32(uint8_t *p,uint32_t value) { for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(value>>(8*i)); }
static uint32_t get32(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put64(uint8_t *p,uint64_t value) { put32(p,(uint32_t)value); put32(p+4,(uint32_t)(value>>32)); }
static uint64_t get64(const uint8_t *p) { return get32(p)|(uint64_t)get32(p+4)<<32; }
static BOOL starting(SudekiMpLobby *s) {
    return !s->status.running && s->status.start.phase>=SUDEKIMP_LOBBY_START_PREPARE &&
        s->status.start.phase<=SUDEKIMP_LOBBY_START_COMPLETE;
}
static void member_clear(SudekiMpLobbyMember *m) {
    memset(m,0,sizeof(*m)); m->character=SUDEKIMP_LOBBY_NO_CHARACTER;
}
static BOOL random_bytes(void *out,unsigned size) {
    return BCryptGenRandom(NULL,(PUCHAR)out,size,BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0;
}
static void roster_changed(SudekiMpLobby *s) {
    ++s->status.roster_revision;
    if (!s->status.roster_revision) ++s->status.roster_revision;
}
static void unready(SudekiMpLobby *s) {
    for (unsigned i=0;i<4;++i) s->status.members[i].ready=0;
    s->desired_ready=FALSE; ++s->status.start.revision;
    if (!s->status.start.revision) ++s->status.start.revision;
}
static void abort_start(SudekiMpLobby *s) {
    if (!starting(s)) return;
    s->status.start.phase=SUDEKIMP_LOBBY_START_ABORTED;
    s->load_ack=0; unready(s);
}
static void release_member_locked(SudekiMpLobby *s,unsigned player) {
    member_clear(&s->status.members[player]);
    SecureZeroMemory(s->credentials[player],16);
    memset(&s->status.admission[player],0,sizeof(s->status.admission[player]));
    s->command_seen[player]=s->command_rejected[player]=0;
    s->admission_at[player]=0;
    s->native_members&=(uint8_t)~(1u<<player);
}
static void depart_member_locked(SudekiMpLobby *s,unsigned player) {
    if(!s->status.members[player].reserved) return;
    if(!s->status.running) { abort_start(s); unready(s); }
    s->status.members[player].present=s->status.members[player].ready=0;
    /* Never let the former credential reclaim a character during its drain.
     * Running joins may already have a native reservation before HostAdmit. */
    SecureZeroMemory(s->credentials[player],16);
    if(!s->status.running && !(s->native_members&(1u<<player)))
        release_member_locked(s,player);
    else if(s->status.admission[player].phase)
        s->status.admission[player].phase=SUDEKIMP_LOBBY_ADMISSION_FAILED;
    roster_changed(s);
}
static void advance_start(SudekiMpLobby *s) {
    SudekiMpLobbyStart *p=&s->status.start;
    if (p->phase==SUDEKIMP_LOBBY_START_PREPARE && p->prepared==p->members && p->port) {
        p->phase=SUDEKIMP_LOBBY_START_LOADING; s->start_at=GetTickCount();
    }
    if (p->phase==SUDEKIMP_LOBBY_START_LOADING && p->loaded==p->members) {
        p->phase=SUDEKIMP_LOBBY_START_COMPLETE; s->start_at=GetTickCount();
    }
    if (p->phase==SUDEKIMP_LOBBY_START_COMPLETE && p->completed==p->members)
        s->status.departure_safe=TRUE;
}
static BOOL apply_ack(SudekiMpLobby *s,unsigned slot,unsigned ack,unsigned port) {
    SudekiMpLobbyStart *p=&s->status.start;
    if (!(p->members&(1u<<slot)) || !starting(s)) return FALSE;
    if (ack==SUDEKIMP_LOBBY_ACK_FAILED) { abort_start(s); return TRUE; }
    if (ack==SUDEKIMP_LOBBY_ACK_PREPARED) {
        if (!slot) { if (!port || port>65535 || (p->port && p->port!=port)) return FALSE; p->port=(uint16_t)port; }
        else if (port) return FALSE;
        p->prepared|=(uint8_t)(1u<<slot);
    } else if (ack==SUDEKIMP_LOBBY_ACK_LOADED) {
        if (port || p->phase<SUDEKIMP_LOBBY_START_LOADING || !(p->prepared&(1u<<slot))) return FALSE;
        p->loaded|=(uint8_t)(1u<<slot);
    } else if (ack==SUDEKIMP_LOBBY_ACK_COMPLETE) {
        if (port || p->phase!=SUDEKIMP_LOBBY_START_COMPLETE || !(p->loaded&(1u<<slot))) return FALSE;
        p->completed|=(uint8_t)(1u<<slot);
    } else return FALSE;
    advance_start(s); return TRUE;
}
static BOOL zero(const void *raw, unsigned size) {
    const uint8_t *p=raw;
    for (unsigned i=0;i<size;++i) if (p[i]) return FALSE;
    return TRUE;
}
static BOOL valid_name(const char *name) {
    unsigned i;
    if (!name || !name[0]) return FALSE;
    for (i=0;i<SUDEKIMP_LOBBY_NAME && name[i];++i)
        if ((unsigned char)name[i]<32 || (unsigned char)name[i]>126) return FALSE;
    return i<SUDEKIMP_LOBBY_NAME;
}
static BOOL valid_player_name(const char *name) { return name && (!name[0] || valid_name(name)); }
static BOOL wire_name(const uint8_t *p) {
    return valid_name((const char *)p) &&
        zero(p+strlen((const char *)p),SUDEKIMP_LOBBY_NAME-(unsigned)strlen((const char *)p));
}
static BOOL wire_player_name(const uint8_t *p) {
    return !p[0]?zero(p,SUDEKIMP_LOBBY_NAME):wire_name(p);
}
static BOOL saved_game_valid(const SudekiMpLobbySavedGame *save) {
    if (!save || save->folder_slot>9999 || zero(save->fish_sha256,32) ||
        zero(save->bunny_sha256,32) || !save->label[0] ||
        !save->party_count || save->party_count>4u || save->leader>=4u ||
        save->leader!=save->party_order[0]) return FALSE;
    unsigned mask=0;
    for(unsigned j=0;j<4u;++j) {
        unsigned c=save->party_order[j];
        if(j>=save->party_count) { if(c!=4u) return FALSE; }
        else { if(c>=4u || (mask&(1u<<c))) return FALSE; mask|=1u<<c; }
    }
    if(mask!=save->party_mask) return FALSE;
    unsigned i;
    for (i=0;i<SUDEKIMP_LOBBY_SAVE_LABEL && save->label[i];++i)
        if ((unsigned char)save->label[i]<32 || (unsigned char)save->label[i]>126) return FALSE;
    return i<SUDEKIMP_LOBBY_SAVE_LABEL &&
        zero(save->label+i,SUDEKIMP_LOBBY_SAVE_LABEL-i);
}
static BOOL saved_game_equal(const SudekiMpLobbySavedGame *a,const SudekiMpLobbySavedGame *b) {
    return a->folder_slot==b->folder_slot && !memcmp(a->fish_sha256,b->fish_sha256,32) &&
        !memcmp(a->bunny_sha256,b->bunny_sha256,32) &&
        !memcmp(a->label,b->label,SUDEKIMP_LOBBY_SAVE_LABEL) &&
        a->party_count==b->party_count && a->leader==b->leader && a->party_mask==b->party_mask &&
        !memcmp(a->party_order,b->party_order,sizeof(a->party_order));
}
static void name_copy(char *out, const char *name) {
    memset(out,0,SUDEKIMP_LOBBY_NAME);
    memcpy(out,name,strlen(name));
}
static void packet(SudekiMpLobby *s, uint8_t *out, unsigned type) {
    memset(out,0,WIRE); memcpy(out,"SLB1",4);
    out[4]=VERSION; out[5]=(uint8_t)type; put16(out+6,WIRE);
    memcpy(out+8,"LB05",4); memcpy(out+12,s->hash,32);
}
static BOOL header(SudekiMpLobby *s, const uint8_t *p) {
    return !memcmp(p,"SLB1",4) && p[4]==VERSION && get16(p+6)==WIRE &&
        !memcmp(p+8,"LB05",4) && !memcmp(p+12,s->hash,32) && zero(p+44,4);
}
static void error(SudekiMpLobby *s, const char *text) {
    s->status.phase=SUDEKIMP_LOBBY_ERROR;
    strncpy(s->status.error,text,sizeof(s->status.error)-1);
    s->status.error[sizeof(s->status.error)-1]=0;
}
static void connection_close(Connection *c) {
    close_socket(&c->socket); memset(c,0,sizeof(*c)); c->socket=INVALID_SOCKET;
}
static void leave_locked(SudekiMpLobby *s) {
    close_socket(&s->listener); close_socket(&s->discovery);
    for (unsigned i=0;i<4;++i) connection_close(&s->connections[i]);
    for (unsigned i=0;i<4;++i) member_clear(&s->status.members[i]);
    s->status.phase=SUDEKIMP_LOBBY_IDLE;
    s->status.local_slot=0; s->status.advertised=0; s->status.room[0]=0;
    s->status.error[0]=0; s->status.port=0; s->connecting=s->desired_ready=FALSE;
    memset(&s->status.start,0,sizeof(s->status.start));
    memset(&s->status.saved_game,0,sizeof(s->status.saved_game));
    s->status.host_ipv4[0]=0; s->status.departure_safe=FALSE; s->load_ack=0; s->ready_revision=0;
    s->status.running=s->status.paused=s->status.absence_policy=0;
    s->status.roster_revision=s->status.command_sequence=s->status.command_rejected=0;
    memset(s->status.admission,0,sizeof(s->status.admission));
    memset(s->credentials,0,sizeof(s->credentials)); memset(s->reconnect_credential,0,16);
    memset(s->command_seen,0,sizeof(s->command_seen)); memset(s->command_rejected,0,sizeof(s->command_rejected));
    s->reconnect_ipv4[0]=0; s->reconnect_port=0;
    s->command_pending=FALSE; s->next_command=s->admission_sequence=s->admission_ack=0;
    s->native_members=0;
}
static SOCKET bound_socket(int type, uint16_t port, BOOL shared) {
    SOCKET sock=socket(AF_INET,type,type==SOCK_STREAM?IPPROTO_TCP:IPPROTO_UDP);
    struct sockaddr_in address;
    BOOL enabled=TRUE;
    if (sock==INVALID_SOCKET) return sock;
    memset(&address,0,sizeof(address)); address.sin_family=AF_INET;
    address.sin_port=htons(port); address.sin_addr.s_addr=htonl(INADDR_ANY);
    if (setsockopt(sock,SOL_SOCKET,shared?SO_REUSEADDR:SO_EXCLUSIVEADDRUSE,
            (const char *)&enabled,sizeof(enabled)) || !nonblocking(sock) ||
        bind(sock,(struct sockaddr *)&address,sizeof(address))) {
        closesocket(sock); return INVALID_SOCKET;
    }
    return sock;
}
static void enqueue(Connection *c, const uint8_t *data) {
    if (c->tx_size) return;
    memcpy(c->tx,data,WIRE); c->tx_size=WIRE; c->tx_offset=0;
}
static void state_packet(SudekiMpLobby *s, unsigned slot, uint8_t *out) {
    packet(s,out,STATE); uint8_t *body=out+BODY;
    body[0]=(uint8_t)slot; body[1]=s->status.advertised;
    memcpy(body+2,s->status.room,32);
    for (unsigned i=0;i<4;++i) {
        const SudekiMpLobbyMember *member=&s->status.members[i];
        body[34+i*34]=member->present; body[35+i*34]=member->ready;
        memcpy(body+36+i*34,member->name,32);
    }
    const SudekiMpLobbyStart *p=&s->status.start;
    body[170]=p->destination; body[171]=p->phase; body[172]=p->members;
    body[173]=p->prepared; body[174]=p->loaded; body[175]=p->completed;
    put32(body+176,p->revision); put64(body+180,p->generation); put16(body+188,p->port);
    /* Each peer receives only its own admission ticket. */
    put64(body+190+slot*8,p->nonce[slot]);
    body[222]=s->status.running; body[223]=s->status.absence_policy; body[224]=s->status.paused;
    put32(body+228,s->status.roster_revision);
    for (unsigned i=0;i<4;++i) {
        const SudekiMpLobbyMember *m=&s->status.members[i];
        body[232+i*4]=m->reserved; body[233+i*4]=m->locked; body[234+i*4]=m->character;
        const SudekiMpLobbyAdmission *a=&s->status.admission[i];
        put32(body+272+i*16,a->sequence); body[276+i*16]=a->phase;
        if (i==slot) put64(body+280+i*16,a->ticket);
    }
    memcpy(body+248,s->credentials[slot],16);
    put32(body+264,s->command_seen[slot]); body[268]=s->command_rejected[slot];
    if (p->destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME) {
        const SudekiMpLobbySavedGame *save=&s->status.saved_game;
        put32(body+336,save->folder_slot);
        memcpy(body+340,save->fish_sha256,32); memcpy(body+372,save->bunny_sha256,32);
        memcpy(body+404,save->label,SUDEKIMP_LOBBY_SAVE_LABEL);
        body[452]=save->party_count; body[453]=save->leader; body[454]=save->party_mask;
        memcpy(body+455,save->party_order,4);
    }
}
static BOOL selectable(SudekiMpLobby *s,unsigned player) {
    const SudekiMpLobbyMember *m=&s->status.members[player];
    return m->present && s->status.start.destination!=SUDEKIMP_LOBBY_DEST_NONE &&
        !starting(s) && (!s->status.running ||
        s->status.admission[player].phase==SUDEKIMP_LOBBY_ADMISSION_NONE ||
        s->status.admission[player].phase==SUDEKIMP_LOBBY_ADMISSION_FAILED);
}
static BOOL select_character(SudekiMpLobby *s,unsigned player,unsigned character,BOOL locked) {
    if (player>=4 || character>SUDEKIMP_LOBBY_NO_CHARACTER ||
        (locked && character==SUDEKIMP_LOBBY_NO_CHARACTER) || !selectable(s,player)) return FALSE;
    if(!player && !s->status.running && s->status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME &&
        (character!=s->status.saved_game.leader || !locked)) return FALSE;
    for (unsigned i=0;locked && i<4;++i)
        if (i!=player && s->status.members[i].reserved && s->status.members[i].locked &&
            s->status.members[i].character==character) return FALSE;
    SudekiMpLobbyMember *m=&s->status.members[player];
    if (m->character==character && m->locked==(locked?1:0)) return TRUE;
    m->character=(uint8_t)character; m->locked=locked?1:0; m->ready=0;
    roster_changed(s); if (!s->status.running) unready(s);
    return TRUE;
}
static BOOL rename_member(SudekiMpLobby *s,unsigned player,const char *name) {
    if (player>=4 || !s->status.members[player].present || !valid_player_name(name)) return FALSE;
    if (!strcmp(s->status.members[player].name,name)) return TRUE;
    name_copy(s->status.members[player].name,name); roster_changed(s);
    if (!s->status.running && !starting(s)) unready(s);
    return TRUE;
}
static BOOL admission_ack(SudekiMpLobby *s,unsigned player,uint32_t sequence,uint64_t ticket,unsigned ack) {
    SudekiMpLobbyAdmission *a=&s->status.admission[player];
    if (!s->status.running || !s->status.members[player].present || !sequence ||
        sequence!=a->sequence || !ticket || ticket!=a->ticket ||
        a->phase==SUDEKIMP_LOBBY_ADMISSION_NONE) return FALSE;
    /* An ACK already in flight cannot revoke a completed native handoff or
     * disconnect an otherwise healthy client after the host timed it out. */
    if (a->phase>=SUDEKIMP_LOBBY_ADMISSION_COMPLETE) return TRUE;
    if (ack==SUDEKIMP_LOBBY_ACK_FAILED) { a->phase=SUDEKIMP_LOBBY_ADMISSION_FAILED; return TRUE; }
    if (ack==SUDEKIMP_LOBBY_ACK_PREPARED && a->phase>=SUDEKIMP_LOBBY_ADMISSION_OFFERED) {
        if (a->phase==SUDEKIMP_LOBBY_ADMISSION_OFFERED) a->phase=SUDEKIMP_LOBBY_ADMISSION_PREPARED;
        return TRUE;
    }
    if (ack==SUDEKIMP_LOBBY_ACK_LOADED && a->phase>=SUDEKIMP_LOBBY_ADMISSION_PREPARED) {
        if (a->phase==SUDEKIMP_LOBBY_ADMISSION_PREPARED) a->phase=SUDEKIMP_LOBBY_ADMISSION_LOADED;
        return TRUE;
    }
    return FALSE;
}
static BOOL received(SudekiMpLobby *s, unsigned connection, const uint8_t *p) {
    Connection *c=&s->connections[connection]; const uint8_t *body=p+BODY;
    if (!header(s,p)) return FALSE;
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) {
        if (p[5]==HELLO && !c->admitted && !starting(s) && wire_player_name(body) &&
            zero(body+48,WIRE-BODY-48)) {
            unsigned player;
            if (zero(body+32,16)) {
                for (player=1;player<4 && s->status.members[player].reserved;++player) {}
                if (player==4 || !random_bytes(s->credentials[player],16) ||
                    zero(s->credentials[player],16)) return FALSE;
                member_clear(&s->status.members[player]);
                s->status.members[player].reserved=1;
                s->command_seen[player]=s->command_rejected[player]=0;
                memset(&s->status.admission[player],0,sizeof(s->status.admission[player]));
            } else {
                /* Departure invalidates this credential immediately. Kept
                 * for the existing wire contract, never an offline claim. */
                for (player=1;player<4;++player)
                    if (s->status.members[player].reserved && !s->status.members[player].present &&
                        !memcmp(body+32,s->credentials[player],16)) break;
                if (player==4) return FALSE;
            }
            name_copy(s->status.members[player].name,(const char *)body);
            s->status.members[player].present=1; s->status.members[player].ready=0;
            c->admitted=TRUE; c->player=(uint8_t)player;
            roster_changed(s); if (!s->status.running) unready(s);
            return TRUE;
        }
        if (!c->admitted || c->player<1 || c->player>3) return FALSE;
        unsigned player=c->player;
        if (p[5]==READY && body[0]<=1 && zero(body+1,3) && zero(body+8,WIRE-BODY-8)) {
            if (!starting(s) && !s->status.running && get32(body+4)==s->status.start.revision)
                s->status.members[player].ready=(uint8_t)(body[0] && s->status.members[player].locked);
        } else if (p[5]==LOAD_ACK && body[12]>=1 && body[12]<=4 && !body[13] &&
            zero(body+16,WIRE-BODY-16)) {
            if (!s->status.running && get32(body+8)==s->status.start.revision &&
                get64(body)==s->status.start.generation && !apply_ack(s,player,body[12],get16(body+14))) return FALSE;
        } else if (p[5]==SELECT_CHARACTER || p[5]==SET_NAME) {
            if (!get32(body) || (p[5]==SELECT_CHARACTER &&
                    (body[8]>4 || body[9]>1 || !zero(body+10,WIRE-BODY-10))) ||
                (p[5]==SET_NAME && (!wire_player_name(body+8) || !zero(body+40,WIRE-BODY-40)))) return FALSE;
            uint32_t sequence=get32(body);
            if (sequence<=s->command_seen[player]) return TRUE;
            s->command_seen[player]=sequence;
            BOOL okay=get32(body+4)==s->status.roster_revision;
            if (okay) okay=p[5]==SELECT_CHARACTER?select_character(s,player,body[8],body[9]):
                rename_member(s,player,(const char *)body+8);
            s->command_rejected[player]=okay?0:1;
        } else if (p[5]==ADMISSION_ACK && body[12]>=1 && body[12]<=4 &&
            zero(body+13,WIRE-BODY-13)) {
            /* A stale completion must not affect a newer admission. */
            if (get32(body)==s->status.admission[player].sequence &&
                !admission_ack(s,player,get32(body),get64(body+4),body[12])) return FALSE;
        } else return FALSE;
    } else {
        if (p[5]!=STATE || body[0]<1 || body[0]>3 || body[1]>1 || !wire_name(body+2) ||
            body[222]>1 || body[223]>SUDEKIMP_LOBBY_SHARED_PAUSE || body[224]>1 ||
            !zero(body+225,3) || !get32(body+228) || zero(body+248,16) || body[268]>1 ||
            !zero(body+269,3) || !zero(body+459,WIRE-BODY-459)) return FALSE;
        if (c->admitted && s->status.local_slot!=body[0]) return FALSE;
        SudekiMpLobbyMember members[4]; memset(members,0,sizeof(members));
        unsigned present=0,locked=0;
        for (unsigned i=0;i<4;++i) {
            const uint8_t *m=body+34+i*34,*extra=body+232+i*4;
            if (m[0]>1 || m[1]>1 || extra[0]>1 || extra[1]>1 || extra[2]>4 || extra[3] ||
                (m[0] && !extra[0]) || (m[1] && (!m[0] || !extra[1])) ||
                (extra[1] && (!extra[0] || extra[2]>=4)) ||
                (extra[0]?!wire_player_name(m+2):(!zero(m,34) || extra[1] || extra[2]!=4))) return FALSE;
            if (extra[1]) { if (locked&(1u<<extra[2])) return FALSE; locked|=1u<<extra[2]; }
            members[i].present=m[0]; members[i].ready=m[1]; members[i].reserved=extra[0];
            members[i].locked=extra[1]; members[i].character=extra[2]; memcpy(members[i].name,m+2,32);
            present|=(unsigned)m[0]<<i;
        }
        if (!members[0].present || !members[body[0]].present ||
            (!c->admitted && strcmp(members[body[0]].name,s->player))) return FALSE;
        SudekiMpLobbyStart start={0};
        start.destination=body[170]; start.phase=body[171]; start.members=body[172];
        start.prepared=body[173]; start.loaded=body[174]; start.completed=body[175];
        start.revision=get32(body+176); start.generation=get64(body+180); start.port=(uint16_t)get16(body+188);
        for (unsigned i=0;i<4;++i) {
            start.nonce[i]=get64(body+190+i*8);
            if (i!=body[0] && start.nonce[i]) return FALSE;
        }
        if (start.destination>SUDEKIMP_LOBBY_DEST_SAVEDGAME || start.phase>SUDEKIMP_LOBBY_START_ABORTED ||
            !start.revision || (start.prepared&~start.members) || (start.loaded&~start.prepared) ||
            (start.completed&~start.loaded) || start.members>15 ||
            (body[222] && (start.phase!=SUDEKIMP_LOBBY_START_COMPLETE || start.completed!=start.members))) return FALSE;
        if (start.phase>=SUDEKIMP_LOBBY_START_PREPARE && start.phase<=SUDEKIMP_LOBBY_START_COMPLETE) {
            if ((start.destination!=SUDEKIMP_LOBBY_DEST_TESTROOM &&
                    (start.destination!=SUDEKIMP_LOBBY_DEST_SAVEDGAME || !s->saved_start_enabled)) || !start.generation ||
                (!body[222] && start.members!=present) ||
                (start.phase>=SUDEKIMP_LOBBY_START_LOADING && (!start.port || start.prepared!=start.members)) ||
                (start.phase==SUDEKIMP_LOBBY_START_COMPLETE && start.loaded!=start.members) ||
                (((start.members>>body[0])&1u)!=(start.nonce[body[0]]!=0))) return FALSE;
        }
        SudekiMpLobbySavedGame saved_game={0};
        if (start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME) {
            saved_game.folder_slot=get32(body+336);
            memcpy(saved_game.fish_sha256,body+340,32); memcpy(saved_game.bunny_sha256,body+372,32);
            memcpy(saved_game.label,body+404,SUDEKIMP_LOBBY_SAVE_LABEL);
            saved_game.party_count=body[452]; saved_game.leader=body[453]; saved_game.party_mask=body[454];
            memcpy(saved_game.party_order,body+455,4);
            if (!saved_game_valid(&saved_game)) return FALSE;
            if(!body[222] && (!members[0].locked || members[0].character!=saved_game.leader)) return FALSE;
            if (start.phase==SUDEKIMP_LOBBY_START_IDLE &&
                (start.generation || start.port || start.members || start.prepared || start.loaded ||
                    start.completed || !zero(start.nonce,sizeof(start.nonce)) || body[222])) return FALSE;
        } else if (!zero(body+336,123)) return FALSE;
        if (c->admitted && s->status.start.revision==start.revision &&
            (s->status.start.destination!=start.destination ||
                !saved_game_equal(&s->status.saved_game,&saved_game))) return FALSE;
        SudekiMpLobbyAdmission admissions[4]; memset(admissions,0,sizeof(admissions));
        for (unsigned i=0;i<4;++i) {
            const uint8_t *a=body+272+i*16;
            admissions[i].sequence=get32(a); admissions[i].phase=a[4]; admissions[i].ticket=get64(a+8);
            if (a[4]>SUDEKIMP_LOBBY_ADMISSION_FAILED || !zero(a+5,3) ||
                (a[4] && (!body[222] || !members[i].reserved || !get32(a) || (i==body[0] && !get64(a+8)))) ||
                (!a[4] && !zero(a,16)) || (i!=body[0] && get64(a+8))) return FALSE;
        }
        if (s->status.start.revision!=start.revision) { s->desired_ready=FALSE; s->load_ack=0; }
        if (s->status.admission[body[0]].sequence!=admissions[body[0]].sequence) s->admission_ack=0;
        if (admissions[body[0]].phase>=SUDEKIMP_LOBBY_ADMISSION_COMPLETE) s->admission_ack=0;
        s->status.start=start;
        s->status.saved_game=saved_game;
        s->status.local_slot=body[0]; s->status.advertised=body[1];
        s->status.running=body[222]; s->status.absence_policy=body[223]; s->status.paused=body[224];
        s->status.roster_revision=get32(body+228); s->status.command_sequence=get32(body+264);
        s->status.command_rejected=body[268];
        if (s->command_pending && s->status.command_sequence>=get32(s->command+BODY)) s->command_pending=FALSE;
        if (s->next_command<s->status.command_sequence) s->next_command=s->status.command_sequence;
        memcpy(s->reconnect_credential,body+248,16);
        memcpy(s->status.admission,admissions,sizeof(admissions));
        memcpy(s->status.room,body+2,32); memcpy(s->status.members,members,sizeof(members));
        name_copy(s->player,members[body[0]].name);
        s->status.phase=SUDEKIMP_LOBBY_CONNECTED; c->admitted=TRUE;
    }
    return TRUE;
}
static BOOL pump(SudekiMpLobby *s, unsigned slot, DWORD now) {
    Connection *c=&s->connections[slot];
    if (c->tx_size) {
        int n=send(c->socket,(const char *)c->tx+c->tx_offset,(int)(c->tx_size-c->tx_offset),0);
        if (n>0) { c->tx_offset+=(unsigned)n; if (c->tx_offset==c->tx_size) {
            if (c->tx[5]==LOAD_ACK && c->tx[BODY+12]==SUDEKIMP_LOBBY_ACK_COMPLETE &&
                get64(c->tx+BODY)==s->status.start.generation) s->status.departure_safe=TRUE;
            c->tx_size=c->tx_offset=0;
        } }
        else if (!n || WSAGetLastError()!=WSAEWOULDBLOCK) return FALSE;
    }
    for (unsigned limit=0;limit<4;++limit) {
        int n=recv(c->socket,(char *)c->rx+c->rx_size,WIRE-(int)c->rx_size,0);
        if (!n) return FALSE;
        if (n<0) { if (WSAGetLastError()!=WSAEWOULDBLOCK) return FALSE; break; }
        c->rx_size+=(unsigned)n;
        if (c->rx_size==WIRE) {
            if (!received(s,slot,c->rx)) return FALSE;
            c->rx_size=0; c->seen=now;
        }
    }
    return (DWORD)(now-c->seen)<TIMEOUT;
}
static BOOL usable_source(const struct sockaddr_in *address) {
    uint32_t ip=ntohl(address->sin_addr.s_addr);
    return address->sin_family==AF_INET && ip && (ip>>24)<224 && ip!=0xffffffffu;
}
static void discovery_poll(SudekiMpLobby *s, DWORD now) {
    uint8_t bytes[WIRE+1]; struct sockaddr_in source; int length;
    /* Permit concurrent browsers, including local broadcast + loopback pairs.
     * A single global 25ms exclusion could repeatedly starve a second browser.
     * Keep a bounded aggregate reply budget without tying it to poll order. */
    if ((DWORD)(now-s->discovery_window_at)>=250u) {
        s->discovery_window_at=now; s->discovery_replies=0;
    }
    if (s->discovery!=INVALID_SOCKET && !starting(s)) for (unsigned i=0;i<POLL_LIMIT;++i) {
        length=sizeof(source);
        int n=recvfrom(s->discovery,(char *)bytes,sizeof(bytes),0,(struct sockaddr *)&source,&length);
        if (n<0) break;
        if (n!=WIRE || !usable_source(&source) || !header(s,bytes) || bytes[5]!=QUERY ||
            zero(bytes+BODY,8) || !zero(bytes+BODY+8,WIRE-BODY-8) ||
            s->discovery_replies>=POLL_LIMIT) continue;
        uint8_t out[WIRE]; packet(s,out,OFFER); memcpy(out+BODY,bytes+BODY,8);
        memcpy(out+BODY+43,&s->instance,8);
        put16(out+BODY+8,s->status.port); memcpy(out+BODY+11,s->status.room,32);
        for (unsigned j=0;j<4;++j) out[BODY+10]+=s->status.members[j].present;
        out[BODY+51]=s->status.running;
        sendto(s->discovery,(const char *)out,WIRE,0,(struct sockaddr *)&source,sizeof(source));
        ++s->discovery_replies;
    }
    if (s->browser==INVALID_SOCKET) return;
    if (s->browsing && (!s->query_at || (DWORD)(now-s->query_at)>=1000u)) {
        packet(s,bytes,QUERY); memcpy(bytes+BODY,&s->query_nonce,8);
        memset(&source,0,sizeof(source)); source.sin_family=AF_INET;
        source.sin_port=htons(SUDEKIMP_LOBBY_DISCOVERY_PORT);
        /* Prefer a local route when both copies are on this computer. */
        source.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        int local=sendto(s->browser,(const char *)bytes,WIRE,0,(struct sockaddr *)&source,sizeof(source));
        source.sin_addr.s_addr=htonl(INADDR_BROADCAST);
        int lan=sendto(s->browser,(const char *)bytes,WIRE,0,(struct sockaddr *)&source,sizeof(source));
        if (local!=WIRE && lan!=WIRE)
            strcpy(s->status.discovery_error,"Discovery could not send. Try Refresh or Direct Connect.");
        else s->status.discovery_error[0]=0;
        s->query_at=now;
    }
    for (unsigned i=0;i<POLL_LIMIT;++i) {
        length=sizeof(source);
        int n=recvfrom(s->browser,(char *)bytes,sizeof(bytes),0,(struct sockaddr *)&source,&length);
        if (n<0) break;
        if (n!=WIRE || !usable_source(&source) || !header(s,bytes) || bytes[5]!=OFFER ||
            memcmp(bytes+BODY,&s->query_nonce,8) || get16(bytes+BODY+8)<1024 ||
            bytes[BODY+10]<1 || bytes[BODY+10]>4 || !wire_name(bytes+BODY+11) ||
            zero(bytes+BODY+43,8) || bytes[BODY+51]>1 || !zero(bytes+BODY+52,WIRE-BODY-52)) continue;
        char address[16]; if (!InetNtopA(AF_INET,&source.sin_addr,address,sizeof(address))) continue;
        unsigned index;
        for (index=0;index<s->status.server_count;++index)
            if (!memcmp(&s->status.servers[index].instance,bytes+BODY+43,8) &&
                s->status.servers[index].port==get16(bytes+BODY+8)) break;
        if (index==SUDEKIMP_LOBBY_SERVERS) continue;
        if (index==s->status.server_count) ++s->status.server_count;
        SudekiMpLobbyServer *server=&s->status.servers[index];
        /* Do not replace an already-discovered same-PC route with a LAN
         * interface address from the second response to the same query. */
        BOOL retain_loopback=!memcmp(&server->instance,bytes+BODY+43,8) &&
            server->port==get16(bytes+BODY+8) && !strcmp(server->ipv4,"127.0.0.1");
        memset(server,0,sizeof(*server)); strcpy(server->ipv4,retain_loopback?"127.0.0.1":address);
        memcpy(&server->instance,bytes+BODY+43,8);
        memcpy(server->name,bytes+BODY+11,32); server->port=(uint16_t)get16(bytes+BODY+8);
        server->players=bytes[BODY+10]; server->running=bytes[BODY+51]; server->seen_at=now;
    }
    for (unsigned i=0;i<s->status.server_count;) {
        if ((DWORD)(now-s->status.servers[i].seen_at)>5000u) {
            memmove(&s->status.servers[i],&s->status.servers[i+1],
                (--s->status.server_count-i)*sizeof(s->status.servers[0]));
        } else ++i;
    }
}
static void poll_locked(SudekiMpLobby *s, DWORD now) {
    uint8_t bytes[WIRE];
    if (s->listener!=INVALID_SOCKET) for (unsigned limit=0;limit<4;++limit) {
        SOCKET socket=accept(s->listener,NULL,NULL);
        if (socket==INVALID_SOCKET) break;
        unsigned slot;
        for (slot=1;slot<4 && s->connections[slot].socket!=INVALID_SOCKET;++slot) {}
        if (slot==4 || starting(s) || !nonblocking(socket)) { closesocket(socket); continue; }
        s->connections[slot].socket=socket; s->connections[slot].seen=now;
    }
    if (s->connecting) {
        Connection *c=&s->connections[0]; fd_set write_set,errors;
        struct timeval instant={0,0}; FD_ZERO(&write_set); FD_ZERO(&errors);
        FD_SET(c->socket,&write_set); FD_SET(c->socket,&errors);
        int result=select(0,NULL,&write_set,&errors,&instant);
        if (result>0) {
            int code=0,size=sizeof(code);
            if (getsockopt(c->socket,SOL_SOCKET,SO_ERROR,(char *)&code,&size) || code || FD_ISSET(c->socket,&errors)) {
                connection_close(c); s->connecting=FALSE; error(s,"Unable to reach the host.");
            } else {
                s->connecting=FALSE; packet(s,bytes,HELLO); name_copy((char *)bytes+BODY,s->player);
                memcpy(bytes+BODY+32,s->reconnect_credential,16);
                enqueue(c,bytes); c->seen=now;
            }
        } else if (result<0 || (DWORD)(now-s->connect_at)>=TIMEOUT) {
            connection_close(c); s->connecting=FALSE; error(s,"Connection timed out.");
        }
    }
    for (unsigned slot=0;slot<4;++slot) {
        Connection *c=&s->connections[slot];
        if (c->socket==INVALID_SOCKET || (!slot && s->connecting)) continue;
        if (!pump(s,slot,now)) {
            unsigned player=c->player; BOOL admitted=c->admitted;
            connection_close(c);
            if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) {
                if (admitted && player>0 && player<4) depart_member_locked(s,player);
            }
            else {
                s->command_pending=FALSE; SecureZeroMemory(s->reconnect_credential,16);
                error(s,"Host closed, lobby full, or incompatible build.");
            }
            continue;
        }
        if (c->admitted && !c->tx_size && (DWORD)(now-c->sent)>=100u) {
            if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) state_packet(s,c->player,bytes);
            else if (s->command_pending) memcpy(bytes,s->command,WIRE);
            else if (s->admission_ack && s->status.running) {
                const SudekiMpLobbyAdmission *a=&s->status.admission[s->status.local_slot];
                packet(s,bytes,ADMISSION_ACK); put32(bytes+BODY,a->sequence);
                put64(bytes+BODY+4,a->ticket); bytes[BODY+12]=(uint8_t)s->admission_ack;
            }
            else if (s->load_ack && starting(s)) {
                packet(s,bytes,LOAD_ACK); put64(bytes+BODY,s->status.start.generation);
                put32(bytes+BODY+8,s->status.start.revision); bytes[BODY+12]=(uint8_t)s->load_ack;
            } else { packet(s,bytes,READY); bytes[BODY]=s->desired_ready?1:0; put32(bytes+BODY+4,s->ready_revision); }
            enqueue(c,bytes); c->sent=now;
        }
    }
    /* pump/apply_ack can advance the phase after the poll-entry timestamp.
     * Read time after that transition; subtracting the older now wraps. */
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING && starting(s) &&
        !s->status.departure_safe && (DWORD)(GetTickCount()-s->start_at)>90000u) abort_start(s);
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING && s->status.running)
        for (unsigned i=1;i<4;++i)
            if (s->status.admission[i].phase>=SUDEKIMP_LOBBY_ADMISSION_OFFERED &&
                s->status.admission[i].phase<=SUDEKIMP_LOBBY_ADMISSION_LOADED &&
                (DWORD)(GetTickCount()-s->admission_at[i])>90000u)
                s->status.admission[i].phase=SUDEKIMP_LOBBY_ADMISSION_FAILED;
    discovery_poll(s,now);
}
static DWORD WINAPI worker(void *raw) {
    SudekiMpLobby *s=raw;
    while (WaitForSingleObject(s->stop,10)==WAIT_TIMEOUT) {
        AcquireSRWLockExclusive(&s->lock); poll_locked(s,GetTickCount()); ReleaseSRWLockExclusive(&s->lock);
    }
    return 0;
}
SudekiMpLobby *SudekiMpLobbyCreate(void) {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2,2),&data)) return NULL;
    SudekiMpLobby *s=calloc(1,sizeof(*s));
    if (!s) { WSACleanup(); return NULL; }
    InitializeSRWLock(&s->lock);
    s->listener=s->discovery=s->browser=INVALID_SOCKET;
    for (unsigned i=0;i<4;++i) s->connections[i].socket=INVALID_SOCKET;
    const char *hex=SUDEKIMP_EXPECTED_SHA256;
    for (unsigned i=0;i<32;++i) {
        unsigned a=hex[i*2]<='9'?hex[i*2]-'0':hex[i*2]-'a'+10;
        unsigned b=hex[i*2+1]<='9'?hex[i*2+1]-'0':hex[i*2+1]-'a'+10;
        s->hash[i]=(uint8_t)(a*16+b);
    }
    s->stop=CreateEventW(NULL,TRUE,FALSE,NULL);
    if (s->stop) s->worker=CreateThread(NULL,0,worker,s,0,NULL);
    if (!s->worker) { if (s->stop) CloseHandle(s->stop); free(s); WSACleanup(); return NULL; }
    return s;
}
BOOL SudekiMpLobbyDestroy(SudekiMpLobby *s) {
    if (!s) return TRUE;
    SetEvent(s->stop);
    if (WaitForSingleObject(s->worker,1000)!=WAIT_OBJECT_0) return FALSE;
    leave_locked(s); close_socket(&s->browser);
    CloseHandle(s->worker); CloseHandle(s->stop); free(s); WSACleanup(); return TRUE;
}
BOOL SudekiMpLobbyHost(SudekiMpLobby *s,const char *room,const char *name,uint16_t port,BOOL advertised) {
    if (!s || !valid_name(room) || !valid_player_name(name) || port<1024) return FALSE;
    AcquireSRWLockExclusive(&s->lock); leave_locked(s);
    if (BCryptGenRandom(NULL,(PUCHAR)&s->instance,sizeof(s->instance),BCRYPT_USE_SYSTEM_PREFERRED_RNG) || !s->instance) goto fail;
    s->listener=bound_socket(SOCK_STREAM,port,FALSE);
    if (s->listener==INVALID_SOCKET || listen(s->listener,3)) goto fail;
    if (advertised) {
        s->discovery=bound_socket(SOCK_DGRAM,SUDEKIMP_LOBBY_DISCOVERY_PORT,TRUE);
        if (s->discovery==INVALID_SOCKET) goto fail;
    }
    s->status.phase=SUDEKIMP_LOBBY_HOSTING; s->status.advertised=advertised?1:0;
    s->status.port=port; name_copy(s->status.room,room);
    s->status.members[0].present=s->status.members[0].reserved=1;
    name_copy(s->status.members[0].name,name);
    s->status.start.revision=s->status.roster_revision=1;
    ReleaseSRWLockExclusive(&s->lock); return TRUE;
fail:
    leave_locked(s); error(s,"Could not host. Check the port is available.");
    ReleaseSRWLockExclusive(&s->lock); return FALSE;
}
static BOOL join_locked(SudekiMpLobby *s,const char *ipv4,uint16_t port,const char *name,const uint8_t credential[16]) {
    struct sockaddr_in address; memset(&address,0,sizeof(address)); address.sin_family=AF_INET;
    address.sin_port=htons(port);
    if (!s || !ipv4 || !valid_player_name(name) || port<1024 ||
        InetPtonA(AF_INET,ipv4,&address.sin_addr)!=1 || !usable_source(&address)) return FALSE;
    leave_locked(s); name_copy(s->player,name);
    if (credential) memcpy(s->reconnect_credential,credential,16);
    strcpy(s->reconnect_ipv4,ipv4); s->reconnect_port=port;
    Connection *c=&s->connections[0]; c->socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (c->socket==INVALID_SOCKET || !nonblocking(c->socket)) goto fail;
    int result=connect(c->socket,(struct sockaddr *)&address,sizeof(address));
    if (result && WSAGetLastError()!=WSAEWOULDBLOCK && WSAGetLastError()!=WSAEINPROGRESS) goto fail;
    s->status.phase=SUDEKIMP_LOBBY_CONNECTING; s->status.port=port;
    strcpy(s->status.host_ipv4,ipv4);
    s->connecting=TRUE; s->connect_at=c->seen=GetTickCount();
    return TRUE;
fail:
    connection_close(c); s->connecting=FALSE; error(s,"Unable to connect to that address.");
    return FALSE;
}
BOOL SudekiMpLobbyJoin(SudekiMpLobby *s,const char *ipv4,uint16_t port,const char *name) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=join_locked(s,ipv4,port,name,NULL);
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyReconnect(SudekiMpLobby *s) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_ERROR && s->reconnect_port &&
        s->reconnect_ipv4[0];
    if (okay) {
        char address[16],name[32]; uint16_t port=s->reconnect_port;
        strcpy(address,s->reconnect_ipv4); name_copy(name,s->player);
        okay=join_locked(s,address,port,name,NULL);
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
void SudekiMpLobbyLeave(SudekiMpLobby *s) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock); leave_locked(s); ReleaseSRWLockExclusive(&s->lock);
}
static BOOL begin_command(SudekiMpLobby *s,unsigned type) {
    if (s->status.phase!=SUDEKIMP_LOBBY_CONNECTED || s->command_pending ||
        s->next_command==UINT32_MAX) return FALSE;
    packet(s,s->command,type); put32(s->command+BODY,++s->next_command);
    put32(s->command+BODY+4,s->status.roster_revision); s->command_pending=TRUE;
    return TRUE;
}
BOOL SudekiMpLobbySelectCharacter(SudekiMpLobby *s,unsigned character,BOOL locked) {
    if (!s || character>SUDEKIMP_LOBBY_NO_CHARACTER || (locked && character==SUDEKIMP_LOBBY_NO_CHARACTER)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay;
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) okay=select_character(s,0,character,locked);
    else {
        okay=selectable(s,s->status.local_slot) && begin_command(s,SELECT_CHARACTER);
        if (okay) { s->command[BODY+8]=(uint8_t)character; s->command[BODY+9]=locked?1:0; }
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbySetName(SudekiMpLobby *s,const char *name) {
    if (!s || !valid_player_name(name)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay;
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) okay=rename_member(s,0,name);
    else { okay=begin_command(s,SET_NAME); if (okay) name_copy((char *)s->command+BODY+8,name); }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
void SudekiMpLobbyReady(SudekiMpLobby *s,BOOL ready) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock);
    if (!starting(s) && !s->status.running) {
        s->desired_ready=ready && s->status.members[s->status.local_slot].locked;
        s->ready_revision=s->status.start.revision;
        if (s->status.phase==SUDEKIMP_LOBBY_HOSTING)
            s->status.members[0].ready=s->desired_ready?1:0;
    }
    ReleaseSRWLockExclusive(&s->lock);
}
void SudekiMpLobbyBrowse(SudekiMpLobby *s,BOOL enabled) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock);
    s->browsing=enabled; s->query_at=0;
    s->status.discovery_error[0]=0;
    if (enabled && s->browser==INVALID_SOCKET) {
        BOOL broadcast=TRUE;
        s->browser=bound_socket(SOCK_DGRAM,0,FALSE);
        if (s->browser==INVALID_SOCKET || setsockopt(s->browser,SOL_SOCKET,SO_BROADCAST,(const char *)&broadcast,sizeof(broadcast)) ||
            BCryptGenRandom(NULL,(PUCHAR)&s->query_nonce,sizeof(s->query_nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG) || !s->query_nonce) {
            close_socket(&s->browser); s->browsing=FALSE;
            strcpy(s->status.discovery_error,"Discovery could not open. Try Refresh or Direct Connect.");
        }
    }
    if (!enabled) { close_socket(&s->browser); s->status.server_count=0; }
    ReleaseSRWLockExclusive(&s->lock);
}
BOOL SudekiMpLobbyAdvertise(SudekiMpLobby *s,BOOL enabled) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING;
    if (okay && enabled && s->discovery==INVALID_SOCKET) {
        s->discovery=bound_socket(SOCK_DGRAM,SUDEKIMP_LOBBY_DISCOVERY_PORT,TRUE);
        okay=s->discovery!=INVALID_SOCKET;
    }
    if (okay) {
        if (!enabled) close_socket(&s->discovery);
        s->status.advertised=enabled?1:0;
        s->discovery_replies=0; s->discovery_window_at=GetTickCount();
    }
    ReleaseSRWLockExclusive(&s->lock);
    return okay;
}
BOOL SudekiMpLobbyDestination(SudekiMpLobby *s,unsigned destination) {
    if (!s || destination>SUDEKIMP_LOBBY_DEST_TESTROOM) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s) && !s->status.running;
    if (okay && s->status.start.destination!=destination) {
        unready(s);
        for(unsigned i=0;i<4u;++i) {
            s->status.members[i].locked=0; s->status.members[i].character=4u;
        }
        roster_changed(s);
        uint32_t revision=s->status.start.revision;
        memset(&s->status.start,0,sizeof(s->status.start));
        memset(&s->status.saved_game,0,sizeof(s->status.saved_game));
        s->status.start.revision=revision; s->status.start.destination=(uint8_t)destination;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbySelectSavedGame(SudekiMpLobby *s,const SudekiMpLobbySavedGame *save) {
    if (!s || !save) return FALSE;
    SudekiMpLobbySavedGame candidate=*save;
    if (!saved_game_valid(&candidate)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s) && !s->status.running;
    if (okay && (s->status.start.destination!=SUDEKIMP_LOBBY_DEST_SAVEDGAME ||
            !saved_game_equal(&s->status.saved_game,&candidate))) {
        unready(s);
        /* Destination changes retire only lobby choices. No native game is
         * running here. Keep connections/names and require fresh Ready. */
        for(unsigned i=0;i<4u;++i) {
            s->status.members[i].locked=0; s->status.members[i].character=4u;
        }
        s->status.members[0].character=candidate.leader;
        s->status.members[0].locked=1;
        roster_changed(s);
        uint32_t revision=s->status.start.revision;
        memset(&s->status.start,0,sizeof(s->status.start));
        s->status.start.revision=revision; s->status.start.destination=SUDEKIMP_LOBBY_DEST_SAVEDGAME;
        s->status.saved_game=candidate;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyEnableSavedStart(SudekiMpLobby *s,BOOL enabled) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=!starting(s) && !s->status.running && !s->native_members;
    if (okay) s->saved_start_enabled=enabled;
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyStartGame(SudekiMpLobby *s) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s) && !s->status.running &&
        !s->native_members &&
        (s->status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM ||
            (s->saved_start_enabled && s->status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME &&
                saved_game_valid(&s->status.saved_game)));
    if(okay && s->status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME &&
        s->status.members[0].character!=s->status.saved_game.leader) okay=FALSE;
    unsigned mask=0;
    for (unsigned i=0;i<4;++i) if (s->status.members[i].present) {
        mask|=1u<<i; if (!s->status.members[i].ready || !s->status.members[i].locked) okay=FALSE;
    }
    SudekiMpLobbyStart plan={0};
    plan.destination=s->status.start.destination; plan.revision=s->status.start.revision;
    plan.phase=SUDEKIMP_LOBBY_START_PREPARE; plan.members=(uint8_t)mask;
    if (okay && (!mask || BCryptGenRandom(NULL,(PUCHAR)&plan.generation,sizeof(plan.generation),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) || !plan.generation)) okay=FALSE;
    for (unsigned i=0;okay && i<4;++i) if (mask&(1u<<i))
        if (BCryptGenRandom(NULL,(PUCHAR)&plan.nonce[i],sizeof(plan.nonce[i]),BCRYPT_USE_SYSTEM_PREFERRED_RNG) ||
            !plan.nonce[i]) okay=FALSE;
    if (okay) {
        s->status.start=plan; s->status.departure_safe=FALSE; s->load_ack=0; s->start_at=GetTickCount();
        s->native_members=(uint8_t)mask;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
void SudekiMpLobbyLoadAck(SudekiMpLobby *s,uint32_t revision,uint64_t generation,unsigned ack,unsigned port) {
    if (!s || ack<1 || ack>4) return;
    AcquireSRWLockExclusive(&s->lock);
    if (revision==s->status.start.revision && generation && generation==s->status.start.generation && starting(s)) {
        if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) (void)apply_ack(s,0,ack,port);
        else if (s->status.phase==SUDEKIMP_LOBBY_CONNECTED && !port) s->load_ack=ack;
    }
    ReleaseSRWLockExclusive(&s->lock);
}
void SudekiMpLobbyAbortStart(SudekiMpLobby *s) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock);
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) abort_start(s);
    else if (s->status.phase==SUDEKIMP_LOBBY_CONNECTED && starting(s)) s->load_ack=SUDEKIMP_LOBBY_ACK_FAILED;
    ReleaseSRWLockExclusive(&s->lock);
}
BOOL SudekiMpLobbyHostRunning(SudekiMpLobby *s) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && s->status.departure_safe &&
        s->status.start.phase==SUDEKIMP_LOBBY_START_COMPLETE &&
        s->status.start.completed==s->status.start.members;
    if (okay && !s->status.running) {
        for (unsigned i=0;i<4;++i) if (s->status.start.members&(1u<<i)) {
            SudekiMpLobbyAdmission *a=&s->status.admission[i];
            a->sequence=++s->admission_sequence; a->ticket=s->status.start.nonce[i];
            a->phase=SUDEKIMP_LOBBY_ADMISSION_COMPLETE;
        }
        s->status.running=1; s->load_ack=0;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyHostRuntimeState(SudekiMpLobby *s,unsigned policy,BOOL paused) {
    if (!s || policy>SUDEKIMP_LOBBY_SHARED_PAUSE) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING;
    if (okay) { s->status.absence_policy=(uint8_t)policy; s->status.paused=paused?1:0; }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyReflectAssignments(SudekiMpLobby *s,const uint8_t character[4],
    unsigned authoritative_members) {
    if (!s || !character || authoritative_members>15u || !(authoritative_members&1u)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && s->status.running;
    unsigned used=0;
    for (unsigned i=0;okay && i<4;++i) if(authoritative_members&(1u<<i)) {
        if (character[i]>SUDEKIMP_LOBBY_NO_CHARACTER ||
            (!s->status.members[i].reserved && character[i]!=SUDEKIMP_LOBBY_NO_CHARACTER)) okay=FALSE;
        else if (character[i]<4) {
            if (used&(1u<<character[i])) okay=FALSE;
            used|=1u<<character[i];
        }
    }
    if (okay) {
        BOOL changed=FALSE;
        for (unsigned i=0;i<4;++i) {
            SudekiMpLobbyMember *m=&s->status.members[i];
            if(authoritative_members&(1u<<i)) {
                if (m->character!=character[i] || m->locked!=(character[i]<4)) {
                    m->character=character[i]; m->locked=character[i]<4; m->ready=0; changed=TRUE;
                }
            } else if(m->character<4u && (used&(1u<<m->character))) {
                m->character=SUDEKIMP_LOBBY_NO_CHARACTER; m->locked=m->ready=0;
                s->command_rejected[i]=1; changed=TRUE;
                SudekiMpLobbyAdmission *a=&s->status.admission[i];
                if(a->phase && a->phase<SUDEKIMP_LOBBY_ADMISSION_COMPLETE)
                    a->phase=SUDEKIMP_LOBBY_ADMISSION_FAILED;
            }
        }
        if (changed) roster_changed(s);
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyDisconnectMember(SudekiMpLobby *s,unsigned player) {
    if(!s || player<1 || player>=4) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING &&
        s->status.members[player].reserved;
    if(okay && s->status.members[player].present) {
        for(unsigned i=1;i<4;++i)
            if(s->connections[i].admitted && s->connections[i].player==player)
                connection_close(&s->connections[i]);
        depart_member_locked(s,player);
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyHostNativeDrained(SudekiMpLobby *s) {
    if(!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING &&
        !s->status.running && !starting(s);
    if(okay) {
        BOOL changed=FALSE;
        for(unsigned player=1;player<4;++player)
            if(s->status.members[player].reserved && !s->status.members[player].present) {
                release_member_locked(s,player); changed=TRUE;
            }
        s->native_members=0;
        if(changed) { roster_changed(s); unready(s); }
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyReleaseReservation(SudekiMpLobby *s,unsigned player) {
    if (!s || player<1 || player>=4) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s) &&
        s->status.members[player].reserved && !s->status.members[player].present;
    if (okay) {
        release_member_locked(s,player);
        roster_changed(s); if (!s->status.running) unready(s);
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyHostAdmit(SudekiMpLobby *s,unsigned player) {
    if (!s || player<1 || player>=4) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    const SudekiMpLobbyMember *m=&s->status.members[player];
    SudekiMpLobbyAdmission *a=&s->status.admission[player];
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && s->status.running &&
        m->present && m->reserved && m->locked && m->character<4 &&
        (a->phase==SUDEKIMP_LOBBY_ADMISSION_NONE || a->phase==SUDEKIMP_LOBBY_ADMISSION_FAILED) &&
        s->admission_sequence<UINT32_MAX;
    uint64_t ticket=0;
    if (okay) okay=random_bytes(&ticket,sizeof(ticket)) && ticket && ticket!=a->ticket &&
        ticket!=s->status.start.nonce[player];
    if (okay) {
        a->sequence=++s->admission_sequence; a->ticket=ticket;
        a->phase=SUDEKIMP_LOBBY_ADMISSION_OFFERED; s->admission_at[player]=GetTickCount();
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
void SudekiMpLobbyAdmissionAck(SudekiMpLobby *s,uint32_t sequence,uint64_t ticket,unsigned ack) {
    if (!s || (ack!=SUDEKIMP_LOBBY_ACK_PREPARED && ack!=SUDEKIMP_LOBBY_ACK_LOADED &&
            ack!=SUDEKIMP_LOBBY_ACK_FAILED)) return;
    AcquireSRWLockExclusive(&s->lock);
    const SudekiMpLobbyAdmission *a=&s->status.admission[s->status.local_slot];
    if (s->status.phase==SUDEKIMP_LOBBY_CONNECTED && s->status.running && sequence &&
        a->sequence==sequence && ticket && a->ticket==ticket && a->phase &&
        a->phase<SUDEKIMP_LOBBY_ADMISSION_COMPLETE &&
        (ack!=SUDEKIMP_LOBBY_ACK_LOADED || a->phase>=SUDEKIMP_LOBBY_ADMISSION_PREPARED)) s->admission_ack=ack;
    ReleaseSRWLockExclusive(&s->lock);
}
BOOL SudekiMpLobbyHostAdmissionComplete(SudekiMpLobby *s,unsigned player,uint32_t sequence,uint64_t ticket,BOOL success) {
    if (!s || player<1 || player>=4) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    SudekiMpLobbyAdmission *a=&s->status.admission[player];
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && s->status.running && sequence &&
        a->sequence==sequence && ticket && a->ticket==ticket &&
        (success?(s->status.members[player].present &&
            (a->phase==SUDEKIMP_LOBBY_ADMISSION_LOADED || a->phase==SUDEKIMP_LOBBY_ADMISSION_COMPLETE)):
            a->phase!=SUDEKIMP_LOBBY_ADMISSION_NONE);
    if (okay) a->phase=success?SUDEKIMP_LOBBY_ADMISSION_COMPLETE:SUDEKIMP_LOBBY_ADMISSION_FAILED;
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
void SudekiMpLobbyStatusGet(SudekiMpLobby *s,SudekiMpLobbyStatus *out) {
    if (!out) return;
    memset(out,0,sizeof(*out));
    if (!s) return;
    AcquireSRWLockShared(&s->lock); *out=s->status; ReleaseSRWLockShared(&s->lock);
}
