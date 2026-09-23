#include "engine/spirit_activation_abi.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct TestCharacter {
    uint8_t padding[0x2c];
    void **vtable;
} TestCharacter;

static int validation_result;
static int activation_result;
static int observed_strike;

static int __attribute__((thiscall)) test_resource_type(void *component) {
    (void)component;
    return 0x05;
}

static int __stdcall test_validate(void *manager, int strike_id) {
    CHECK(manager == (void *)(uintptr_t)0x4567u);
    observed_strike = strike_id;
    return validation_result;
}

static int __stdcall test_activate(void *manager, int strike_id) {
    CHECK(manager == (void *)(uintptr_t)0x4567u);
    observed_strike = strike_id;
    return activation_result;
}

static TestCharacter route_actors[2];
static BOOL route_idle=TRUE,deny_enter,deny_leave,null_manager,nest_validation;
static unsigned int route_calls,route_activations,route_count,route_owner[16];
static uint32_t route_serial,route_cookie[16];
static int route_errors;
static unsigned int recursive_queries;
static BOOL lose_owner_after_validation;
static BOOL route_idle_witness(void) { return route_idle; }
static void *route_manager(unsigned int actor) { return (void *)(uintptr_t)(0xa100u+actor*0x100u); }
static uint32_t route_enter(void *actor,int strike,BOOL activating,void **manager) {
    unsigned int index=strike==4 || strike==5 ? 0:1;
    (void)activating;
    if(deny_enter || route_count==16 || (actor ? actor!=&route_actors[index]:index!=0)) return 0;
    route_owner[route_count]=index; route_cookie[route_count]=++route_serial;
    *manager=null_manager ? NULL:route_manager(index);
    return route_cookie[route_count++];
}
static BOOL route_leave(uint32_t cookie) {
    if(deny_leave) return FALSE;
    if(!route_count || route_cookie[route_count-1]!=cookie) { ++route_errors; return FALSE; }
    --route_count; return TRUE;
}
static int __stdcall route_validate(void *manager,int strike) {
    unsigned int index=strike==4 || strike==5 ? 0:1;
    ++route_calls;
    if(manager!=route_manager(index) || !route_count || route_owner[route_count-1]!=index) ++route_errors;
    if(lose_owner_after_validation) deny_enter=TRUE;
    if(recursive_queries) {
        int result;
        --recursive_queries;
        result=SudekiMpInvokeSpiritValidate(&route_actors[index],manager,strike,route_validate);
        ++recursive_queries;
        return result;
    }
    return 0;
}
static int __stdcall route_activate(void *manager,int strike) {
    unsigned int index=strike==4 || strike==5 ? 0:1;
    ++route_activations;
    if(manager!=route_manager(index) || !route_count || route_owner[route_count-1]!=index) ++route_errors;
    if(nest_validation) {
        (void)SudekiMpInvokeSpiritValidate(&route_actors[1],manager,6,route_validate);
        if(!deny_leave && (route_count!=1 || route_owner[0]!=index)) ++route_errors;
    }
    SetLastError(1234);
    return 1;
}
static DWORD WINAPI wrong_thread(void *unused) {
    (void)unused;
    if(SudekiMpInvokeSpiritValidate(&route_actors[0],NULL,4,route_validate)!=2 ||
        SudekiMpInvokeSpiritActivate(&route_actors[0],NULL,4,route_activate)!=0 ||
        SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL) ||
        SudekiMpRetrySpiritActivationRoutingLeave()) ++route_errors;
    return 0;
}
static int routing_tests(void) {
    uint8_t *image=VirtualAlloc(NULL,0x45f000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    const uint8_t validate[]={0x83,0xec,0x2c,0x80,0x3d};
    const uint8_t suffix[]={0,0x55,0x8b,0x6c,0x24,0x38};
    const uint8_t activate[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,
        0xc4,0,0,0,0x53,0x56,0x8b,0x75};
    void *vtable[5]={0};
    SudekiMpSpiritActivationApi api={route_manager(1),route_validate,route_activate,test_resource_type};
    SudekiMpSpiritActivationResult result;
    SudekiMpSpiritQuickOptionList options;
    HANDLE thread;
    CHECK(image);
    memcpy(image+0x10940,validate,sizeof(validate));
    *(void **)(image+0x10945)=image+0x349570;
    memcpy(image+0x10949,suffix,sizeof(suffix));
    memcpy(image+0xfba0,activate,sizeof(activate));
    CHECK(SudekiMpInitializeSpiritActivationAbi((HMODULE)image));
    vtable[4]=(void *)test_resource_type; route_actors[0].vtable=vtable;
    CHECK(!SudekiMpSetSpiritActivationRouting(route_idle_witness,route_enter,NULL));
    route_idle=FALSE;
    CHECK(!SudekiMpSetSpiritActivationRouting(route_idle_witness,route_enter,route_leave));
    route_idle=TRUE;
    CHECK(SudekiMpSetSpiritActivationRouting(route_idle_witness,route_enter,route_leave));
    CHECK(!SudekiMpSetSpiritActivationRouting(route_idle_witness,route_enter,route_leave));
    CHECK(SudekiMpDescribeCharacterSpiritOptionsWithApi(&route_actors[0],&api,&options));
    CHECK(options.options[0].available && options.options[1].available && route_calls==2 && !route_count);
    result=SudekiMpActivateCharacterSpiritWithApi(&route_actors[0],2,&api);
    CHECK(result.status==SUDEKIMP_SPIRIT_ACTIVATION_STARTED && route_activations==1);
    CHECK(SudekiMpInvokeSpiritValidate(NULL,(void *)0x9999,4,route_validate)==0); /* Native Q resolves local owner. */
    CHECK(SudekiMpInvokeSpiritValidate(NULL,(void *)0x9999,6,route_validate)==2); /* Cannot select remote via Q ID. */
    CHECK(SudekiMpInvokeSpiritValidate(&route_actors[0],NULL,6,route_validate)==2);
    CHECK(SudekiMpInvokeSpiritValidate(&route_actors[1],NULL,6,route_validate)==0);
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[1],NULL,6,route_activate)==1);
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[1],NULL,8,route_activate)==0);
    deny_enter=TRUE;
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[0],route_manager(0),4,route_activate)==0);
    deny_enter=FALSE;
    CHECK(route_activations==2 && !route_count);
    lose_owner_after_validation=TRUE;
    result=SudekiMpActivateCharacterSpiritWithApi(&route_actors[0],1,&api);
    CHECK(result.validation_result==0 && result.status==SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED);
    CHECK(route_activations==2 && !route_count); /* Fresh authority checked again at native activation. */
    lose_owner_after_validation=FALSE; deny_enter=FALSE;
    {
        unsigned int before=route_calls;
        recursive_queries=16;
        CHECK(SudekiMpInvokeSpiritValidate(&route_actors[0],NULL,4,route_validate)==2);
        CHECK(route_calls==before+16 && !route_count && !route_errors);
        recursive_queries=0;
    }
    thread=CreateThread(NULL,0,wrong_thread,NULL,0,NULL); CHECK(thread);
    CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0); CloseHandle(thread);
    CHECK(!route_errors && route_activations==2 && !route_count);
    nest_validation=TRUE;
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[0],NULL,4,route_activate)==1);
    CHECK(GetLastError()==1234 && route_activations==3 && !route_count && !route_errors);
    deny_leave=TRUE;
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[0],NULL,4,route_activate)==1);
    CHECK(route_activations==4 && route_count==2 && !SudekiMpSpiritActivationRoutingHealthy());
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[0],NULL,4,route_activate)==0);
    CHECK(!SudekiMpDescribeCharacterSpiritOptionsWithApi(&route_actors[0],&api,&options));
    CHECK(!options.option_count);
    CHECK(!SudekiMpRetrySpiritActivationRoutingLeave() && route_count==2);
    CHECK(!SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL));
    SudekiMpResetSpiritActivationAbi(); CHECK(GetLastError()==ERROR_BUSY);
    CHECK(!SudekiMpInitializeSpiritActivationAbi((HMODULE)image)); /* Reset retained dependencies. */
    deny_leave=FALSE;
    CHECK(SudekiMpRetrySpiritActivationRoutingLeave() && !route_count && !route_errors);
    CHECK(route_activations==4 && !SudekiMpSpiritActivationRoutingHealthy());
    route_idle=FALSE; CHECK(!SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL));
    route_idle=TRUE; CHECK(SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL));
    CHECK(SudekiMpSpiritActivationRoutingHealthy());
    CHECK(SudekiMpSetSpiritActivationRouting(route_idle_witness,route_enter,route_leave));
    null_manager=TRUE;
    CHECK(SudekiMpInvokeSpiritActivate(&route_actors[0],NULL,4,route_activate)==0);
    CHECK(!route_count && !SudekiMpSpiritActivationRoutingHealthy() && route_activations==4);
    null_manager=FALSE;
    CHECK(SudekiMpSetSpiritActivationRouting(NULL,NULL,NULL));
    SudekiMpResetSpiritActivationAbi();
    CHECK(SudekiMpInitializeSpiritActivationAbi((HMODULE)image));
    SudekiMpResetSpiritActivationAbi();
    VirtualFree(image,0,MEM_RELEASE);
    return 0;
}

int main(void) {
    void *vtable[5];
    TestCharacter character;
    SudekiMpSpiritActivationApi api;
    SudekiMpSpiritActivationResult result;
    SudekiMpSpiritQuickOptionList options;
    int strike_id;

    CHECK(SudekiMpResolveSpiritStrikeId(0x23u, 1u, &strike_id) &&
        strike_id == 0);
    CHECK(SudekiMpResolveSpiritStrikeId(0x01u, 2u, &strike_id) &&
        strike_id == 3);
    CHECK(SudekiMpResolveSpiritStrikeId(0x05u, 1u, &strike_id) &&
        strike_id == 4);
    CHECK(SudekiMpResolveSpiritStrikeId(0x0eu, 2u, &strike_id) &&
        strike_id == 7);
    CHECK(!SudekiMpResolveSpiritStrikeId(0x99u, 1u, &strike_id));
    CHECK(!SudekiMpResolveSpiritStrikeId(0x23u, 0u, &strike_id));

    memset(&character, 0, sizeof(character));
    memset(vtable, 0, sizeof(vtable));
    vtable[4] = (void *)test_resource_type;
    character.vtable = vtable;
    memset(&api, 0, sizeof(api));
    api.manager = (void *)(uintptr_t)0x4567u;
    api.validate = test_validate;
    api.activate = test_activate;
    api.resource_type = test_resource_type;

    validation_result = 0;
    CHECK(SudekiMpDescribeCharacterSpiritOptionsWithApi(
        &character, &api, &options));
    CHECK(options.resource_type == 0x05u && options.option_count == 2u);
    CHECK(options.options[0].variant == 1u &&
        options.options[0].strike_id == 4 && options.options[0].available);
    CHECK(options.options[1].variant == 2u &&
        options.options[1].strike_id == 5 && options.options[1].available);

    validation_result = 9;
    CHECK(SudekiMpDescribeCharacterSpiritOptionsWithApi(
        &character, &api, &options));
    CHECK(!options.options[0].available &&
        options.options[0].validation_result == 9);

    validation_result = 0;
    activation_result = 1;
    observed_strike = -1;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 2u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_STARTED);
    CHECK(result.strike_id == 5 && observed_strike == 5);

    validation_result = 6;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 1u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED);
    CHECK(result.validation_result == 6);

    validation_result = 0;
    activation_result = 0;
    result = SudekiMpActivateCharacterSpiritWithApi(&character, 1u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED);
    CHECK(result.activation_result == 0);

    result = SudekiMpActivateCharacterSpiritWithApi(&character, 3u, &api);
    CHECK(result.status == SUDEKIMP_SPIRIT_ACTIVATION_INVALID_VARIANT);
    CHECK(routing_tests()==0);
    puts("spirit activation ABI tests passed");
    return 0;
}
