#include <winsock2.h>
#include "network/title_lobby.h"
#include "engine/build_identity.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

enum { WIRE = 320, BODY = 48, HELLO = 1, STATE, READY, LOAD_ACK,
    QUERY = 10, OFFER, VERSION = 2, POLL_LIMIT = 16, TIMEOUT = 8000 };
typedef struct Connection {
    SOCKET socket;
    unsigned rx_size, tx_size, tx_offset;
    uint8_t rx[WIRE], tx[WIRE];
    DWORD seen, sent;
    BOOL admitted;
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
    return s->status.start.phase>=SUDEKIMP_LOBBY_START_PREPARE &&
        s->status.start.phase<=SUDEKIMP_LOBBY_START_COMPLETE;
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
static BOOL wire_name(const uint8_t *p) {
    return valid_name((const char *)p) &&
        zero(p+strlen((const char *)p),SUDEKIMP_LOBBY_NAME-(unsigned)strlen((const char *)p));
}
static void name_copy(char *out, const char *name) {
    memset(out,0,SUDEKIMP_LOBBY_NAME);
    memcpy(out,name,strlen(name));
}
static void packet(SudekiMpLobby *s, uint8_t *out, unsigned type) {
    memset(out,0,WIRE); memcpy(out,"SLB1",4);
    out[4]=VERSION; out[5]=(uint8_t)type; put16(out+6,WIRE);
    memcpy(out+8,"LB02",4); memcpy(out+12,s->hash,32);
}
static BOOL header(SudekiMpLobby *s, const uint8_t *p) {
    return !memcmp(p,"SLB1",4) && p[4]==VERSION && get16(p+6)==WIRE &&
        !memcmp(p+8,"LB02",4) && !memcmp(p+12,s->hash,32) && zero(p+44,4);
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
    memset(s->status.members,0,sizeof(s->status.members));
    s->status.phase=SUDEKIMP_LOBBY_IDLE;
    s->status.local_slot=0; s->status.advertised=0; s->status.room[0]=0;
    s->status.error[0]=0; s->status.port=0; s->connecting=s->desired_ready=FALSE;
    memset(&s->status.start,0,sizeof(s->status.start));
    s->status.host_ipv4[0]=0; s->status.departure_safe=FALSE; s->load_ack=0; s->ready_revision=0;
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
}
static BOOL received(SudekiMpLobby *s, unsigned slot, const uint8_t *p) {
    Connection *c=&s->connections[slot]; const uint8_t *body=p+BODY;
    if (!header(s,p)) return FALSE;
    if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) {
        if (p[5]==HELLO && !c->admitted && !starting(s) && wire_name(body) && zero(body+32,WIRE-BODY-32)) {
            name_copy(s->status.members[slot].name,(const char *)body);
            s->status.members[slot].present=1; c->admitted=TRUE;
            unready(s);
        } else if (p[5]==READY && c->admitted && body[0]<=1 && zero(body+1,3) && zero(body+8,WIRE-BODY-8)) {
            if (!starting(s) && get32(body+4)==s->status.start.revision) s->status.members[slot].ready=body[0];
        } else if (p[5]==LOAD_ACK && c->admitted && body[12]>=1 && body[12]<=4 && !body[13] &&
            zero(body+16,WIRE-BODY-16)) {
            if (get32(body+8)==s->status.start.revision && get64(body)==s->status.start.generation &&
                !apply_ack(s,slot,body[12],get16(body+14))) return FALSE;
        } else return FALSE;
    } else {
        if (p[5]!=STATE || body[0]<1 || body[0]>3 || body[1]>1 || !wire_name(body+2) ||
            !zero(body+222,WIRE-BODY-222)) return FALSE;
        if (c->admitted && s->status.local_slot!=body[0]) return FALSE;
        SudekiMpLobbyMember members[4]; memset(members,0,sizeof(members));
        for (unsigned i=0;i<4;++i) {
            const uint8_t *m=body+34+i*34;
            if (m[0]>1 || m[1]>1 || (m[0]?!wire_name(m+2):!zero(m,34))) return FALSE;
            members[i].present=m[0]; members[i].ready=m[1]; memcpy(members[i].name,m+2,32);
        }
        if (!members[0].present || !members[body[0]].present ||
            strcmp(members[body[0]].name,s->player)) return FALSE;
        SudekiMpLobbyStart start={0};
        start.destination=body[170]; start.phase=body[171]; start.members=body[172];
        start.prepared=body[173]; start.loaded=body[174]; start.completed=body[175];
        start.revision=get32(body+176); start.generation=get64(body+180); start.port=(uint16_t)get16(body+188);
        unsigned present=0;
        for (unsigned i=0;i<4;++i) { start.nonce[i]=get64(body+190+i*8); present|=(unsigned)members[i].present<<i; }
        if (start.destination>SUDEKIMP_LOBBY_DEST_TESTROOM || start.phase>SUDEKIMP_LOBBY_START_ABORTED ||
            !start.revision || (start.prepared&~start.members) || (start.loaded&~start.prepared) ||
            (start.completed&~start.loaded) || start.members>15) return FALSE;
        if (start.phase>=SUDEKIMP_LOBBY_START_PREPARE && start.phase<=SUDEKIMP_LOBBY_START_COMPLETE) {
            if (start.destination!=SUDEKIMP_LOBBY_DEST_TESTROOM || !start.generation || start.members!=present ||
                (start.phase>=SUDEKIMP_LOBBY_START_LOADING && (!start.port || start.prepared!=present)) ||
                (start.phase==SUDEKIMP_LOBBY_START_COMPLETE && start.loaded!=present)) return FALSE;
            for (unsigned i=0;i<4;++i) if ((i==body[0])!=(start.nonce[i]!=0)) return FALSE;
        }
        if (s->status.start.revision!=start.revision) { s->desired_ready=FALSE; s->load_ack=0; }
        s->status.start=start;
        s->status.local_slot=body[0]; s->status.advertised=body[1];
        memcpy(s->status.room,body+2,32); memcpy(s->status.members,members,sizeof(members));
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
            zero(bytes+BODY+43,8) || !zero(bytes+BODY+51,WIRE-BODY-51)) continue;
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
        server->players=bytes[BODY+10]; server->seen_at=now;
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
            connection_close(c);
            if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) {
                if (s->status.start.phase!=SUDEKIMP_LOBBY_START_COMPLETE) {
                    abort_start(s); memset(&s->status.members[slot],0,sizeof(s->status.members[slot])); unready(s);
                } else { s->status.start.completed|=(uint8_t)(1u<<slot); advance_start(s); }
            }
            else { memset(s->status.members,0,sizeof(s->status.members)); error(s,"Host closed, lobby full, or incompatible build."); }
            continue;
        }
        if (c->admitted && !c->tx_size && (DWORD)(now-c->sent)>=(starting(s)?100u:500u)) {
            if (s->status.phase==SUDEKIMP_LOBBY_HOSTING) state_packet(s,slot,bytes);
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
    if (!s || !valid_name(room) || !valid_name(name) || port<1024) return FALSE;
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
    s->status.members[0].present=1; name_copy(s->status.members[0].name,name);
    s->status.start.revision=1;
    ReleaseSRWLockExclusive(&s->lock); return TRUE;
fail:
    leave_locked(s); error(s,"Could not host. Check the port is available.");
    ReleaseSRWLockExclusive(&s->lock); return FALSE;
}
BOOL SudekiMpLobbyJoin(SudekiMpLobby *s,const char *ipv4,uint16_t port,const char *name) {
    struct sockaddr_in address; memset(&address,0,sizeof(address)); address.sin_family=AF_INET;
    address.sin_port=htons(port);
    if (!s || !ipv4 || !valid_name(name) || port<1024 ||
        InetPtonA(AF_INET,ipv4,&address.sin_addr)!=1 || !usable_source(&address)) return FALSE;
    AcquireSRWLockExclusive(&s->lock); leave_locked(s); name_copy(s->player,name);
    Connection *c=&s->connections[0]; c->socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (c->socket==INVALID_SOCKET || !nonblocking(c->socket)) goto fail;
    int result=connect(c->socket,(struct sockaddr *)&address,sizeof(address));
    if (result && WSAGetLastError()!=WSAEWOULDBLOCK && WSAGetLastError()!=WSAEINPROGRESS) goto fail;
    s->status.phase=SUDEKIMP_LOBBY_CONNECTING; s->status.port=port;
    strcpy(s->status.host_ipv4,ipv4);
    s->connecting=TRUE; s->connect_at=c->seen=GetTickCount();
    ReleaseSRWLockExclusive(&s->lock); return TRUE;
fail:
    leave_locked(s); error(s,"Unable to connect to that address.");
    ReleaseSRWLockExclusive(&s->lock); return FALSE;
}
void SudekiMpLobbyLeave(SudekiMpLobby *s) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock); leave_locked(s); ReleaseSRWLockExclusive(&s->lock);
}
void SudekiMpLobbyReady(SudekiMpLobby *s,BOOL ready) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock);
    if (!starting(s)) { s->desired_ready=ready; s->ready_revision=s->status.start.revision; }
    if (!starting(s) && s->status.phase==SUDEKIMP_LOBBY_HOSTING)
        s->status.members[s->status.local_slot].ready=ready?1:0;
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
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s);
    if (okay && s->status.start.destination!=destination) {
        unready(s);
        uint32_t revision=s->status.start.revision;
        memset(&s->status.start,0,sizeof(s->status.start));
        s->status.start.revision=revision; s->status.start.destination=(uint8_t)destination;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLobbyStartGame(SudekiMpLobby *s) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL okay=s->status.phase==SUDEKIMP_LOBBY_HOSTING && !starting(s) &&
        s->status.start.destination==SUDEKIMP_LOBBY_DEST_TESTROOM;
    unsigned mask=0;
    for (unsigned i=0;i<4;++i) if (s->status.members[i].present) {
        mask|=1u<<i; if (!s->status.members[i].ready) okay=FALSE;
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
void SudekiMpLobbyStatusGet(SudekiMpLobby *s,SudekiMpLobbyStatus *out) {
    if (!out) return;
    memset(out,0,sizeof(*out));
    if (!s) return;
    AcquireSRWLockShared(&s->lock); *out=s->status; ReleaseSRWLockShared(&s->lock);
}
