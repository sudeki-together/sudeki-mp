/* Actual first-suspect logger ordering; synthetic linked memory, no native calls. */
#include "../src/cleanroom/engine.c"
#include <assert.h>
#include <stdarg.h>
static uint8_t node_storage[3][64];
static char expected[3][65];
static unsigned headers,rows;
void SudekiMpLogWrite(const char *s) {(void)s;assert(0);}
void SudekiMpLogFormat(const char *format,...) {
    char line[1024];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(strstr(line,"event=suspect")) {++headers;memset(node_storage,0xaa,sizeof(node_storage));}
    if(strstr(line,"event=node")) {assert(headers==1 && rows<3 && strstr(line,expected[rows]));++rows;}
}
int main(void) {
    uintptr_t object[2]={0};
    for(unsigned i=0;i<3;++i) {
        SudekiMpTPtrNode *node=(SudekiMpTPtrNode *)(node_storage[i]+16);
        *node=(SudekiMpTPtrNode){(uintptr_t)object,i?(uintptr_t)(node_storage[i-1]+16):1u,i<2?(uintptr_t)(node_storage[i+1]+16):0};
        for(unsigned j=0;j<32;++j)snprintf(expected[i]+j*2,3,"%02x",node_storage[i][8+j]);
    }
    object[1]=(uintptr_t)(node_storage[0]+16);
    SudekiMpCleanroomEngineSetTPtrAudit(FALSE);tptr_audit(object,NULL,"fixture");assert(!headers && !rows);
    SudekiMpCleanroomEngineSetTPtrAudit(TRUE);tptr_audit(object,NULL,"fixture");assert(headers==1 && rows==3);
    tptr_audit(object,NULL,"fixture");assert(headers==1 && rows==3);
    puts("TPtrCaptureTest PASS: every node captured before the first logger, one report");return 0;
}
