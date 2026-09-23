#include "engine/player_combat_context.h"
#include "engine/skill_activation_abi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
void *seen_availability_data;
void *seen_availability_context;
uint8_t availability_result = 1u;
static void *seen_validate_skill;
static int seen_validate_slot;
static int validate_result;
static const uint8_t *validate_host_approval_flag;
static uint8_t seen_validate_host_approval;
static void *seen_use_skill;
static int seen_use_slot;
static uint8_t seen_use_host_approval;
static uint8_t use_result = 1u;
static unsigned int use_calls;

static uint8_t routed_skills[2][0x78];
static BOOL route_idle=TRUE,route_deny_enter,route_deny_leave,route_nested,route_lose_owner;
static unsigned int route_depth,route_stack[16],route_calls,route_uses;
static BOOL route_idle_witness(void) { return route_idle; }
static uint32_t route_enter(void *skill,int slot,BOOL using_skill) {
    unsigned int i=skill==routed_skills[0] ? 0:1;
    (void)slot; (void)using_skill;
    if(route_deny_enter || route_depth==16 || skill!=routed_skills[i]) return 0;
    route_stack[route_depth++]=i;
    return route_depth;
}
static BOOL route_leave(uint32_t cookie) {
    if(route_deny_leave || cookie!=route_depth || !route_depth) return FALSE;
    --route_depth; return TRUE;
}

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
static int __attribute__((regparm(2))) routed_validate(void *skill,int slot) {
    unsigned int i=skill==routed_skills[0] ? 0:1;
    check(slot==2 && route_depth && route_stack[route_depth-1]==i,
        "native skill validation sees its actor context");
    ++route_calls;
    if(route_lose_owner) route_deny_enter=TRUE;
    return 0;
}
static uint8_t __attribute__((fastcall)) routed_use(void *skill,void *edx,int slot) {
    unsigned int i=skill==routed_skills[0] ? 0:1;
    check(edx==(void *)0x5678 && slot==2 && route_depth && route_stack[route_depth-1]==i,
        "native Use retains ECX actor, EDX and slot within its own scope");
    ++route_uses;
    if(route_nested) {
        check(SudekiMpInvokeSkillValidate(routed_skills[1-i],2,routed_validate)==0,
            "nested other-owner query succeeds");
        check(route_depth==1 && route_stack[0]==i,"nested query restores outer skill owner");
    }
    SetLastError(1234); return 1;
}
static DWORD WINAPI route_wrong_thread(void *unused) {
    (void)unused;
    check(SudekiMpInvokeSkillValidate(routed_skills[0],2,routed_validate)==4,
        "off-thread skill validation rejected");
    check(SudekiMpInvokeSkillUse(routed_skills[0],(void *)0x5678,2,routed_use)==0,
        "off-thread skill Use rejected before mutation");
    check(!SudekiMpSetSkillActivationRouting(NULL,NULL,NULL),"off-thread unregister rejected");
    return 0;
}
static void test_owner_routing(void) {
    uint8_t *image=VirtualAlloc(NULL,0x45f000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    const uint8_t use_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,
        0xcc,0,0,0,0x53,0x56,0x8b,0x75};
    const uint8_t avail_tail[]={0,0x53,0x8b,0x58,0x0c},validate_tail[]={0,0x75,6,0xb8,5};
    HANDLE thread;
    unsigned int before;
    check(image!=NULL,"routing image allocated");
    if(!image) return;
    image[0xda2a0]=image[0xb4bc0]=0x80; image[0xda2a1]=image[0xb4bc1]=0x3d;
    *(void **)(image+0xda2a2)=image+0x3c2fd9;
    *(void **)(image+0xb4bc2)=image+0x34a8b0;
    memcpy(image+0xda2a6,avail_tail,sizeof(avail_tail));
    memcpy(image+0xb4bc6,validate_tail,sizeof(validate_tail));
    memcpy(image+0xb4810,use_entry,sizeof(use_entry));
    check(SudekiMpInitializeSkillActivationAbi((HMODULE)image),"routing image ABI initialized");
    check(!SudekiMpSetSkillActivationRouting(route_idle_witness,route_enter,NULL),"partial router rejected");
    check(SudekiMpSetSkillActivationRouting(route_idle_witness,route_enter,route_leave),"routing registered idle");
    route_nested=TRUE;
    check(SudekiMpInvokeSkillUse(routed_skills[0],(void *)0x5678,2,routed_use)==1 &&
        GetLastError()==1234 && !route_depth,"full native Use routed and restored exactly once");
    route_nested=FALSE;
    check(SudekiMpInvokeSkillUse(routed_skills[1],(void *)0x5678,2,routed_use)==1,
        "second actor uses the same routing path");
    before=route_uses;
    route_lose_owner=TRUE;
    check(SudekiMpInvokeSkillValidate(routed_skills[0],2,routed_validate)==0,"query initially eligible");
    check(!SudekiMpInvokeSkillUse(routed_skills[0],(void *)0x5678,2,routed_use) && route_uses==before,
        "Use revalidates ownership after eligibility query");
    route_lose_owner=FALSE; route_deny_enter=FALSE;
    thread=CreateThread(NULL,0,route_wrong_thread,NULL,0,NULL);
    check(thread!=NULL,"off-thread fixture launched");
    if(thread) { check(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,"off-thread fixture finished"); CloseHandle(thread); }
    check(route_uses==before,"off-thread path did not invoke Use");
    route_deny_leave=TRUE;
    check(SudekiMpInvokeSkillUse(routed_skills[0],(void *)0x5678,2,routed_use)==1 && route_uses==before+1,
        "restore failure preserves already-executed native Use result");
    check(!SudekiMpSkillActivationRoutingHealthy() && route_depth==1,"failed scope retained");
    check(!SudekiMpSetSkillActivationRouting(NULL,NULL,NULL),"live failed scope blocks unregister");
    SudekiMpResetSkillActivationAbi();
    check(!SudekiMpInvokeSkillUse(routed_skills[0],(void *)0x5678,2,routed_use) && route_uses==before+1,
        "ABI reset cannot remove retained routing or replay charged action");
    route_deny_leave=FALSE;
    check(SudekiMpRetrySkillActivationRoutingLeave() && !route_depth,"restore retry unwinds scope only");
    route_idle=FALSE;
    check(!SudekiMpSetSkillActivationRouting(NULL,NULL,NULL),"asynchronous owner prevents unregister");
    route_idle=TRUE;
    check(SudekiMpSetSkillActivationRouting(NULL,NULL,NULL),"idle owner unregisters");
    check(SudekiMpSkillActivationRoutingHealthy(),"full idle retirement clears routing fault");
    SudekiMpResetSkillActivationAbi();
    VirtualFree(image,0,MEM_RELEASE);
}

__attribute__((naked, noinline))
static uint8_t availability_mock(void) {
    __asm__ volatile(
        "movl %eax, _seen_availability_data\n\t"
        "movl %esi, _seen_availability_context\n\t"
        "movzbl _availability_result, %eax\n\t"
        "ret\n\t"
    );
}

static int __attribute__((regparm(2))) validate_mock(
    void *skill,
    int slot
) {
    seen_validate_skill = skill;
    seen_validate_slot = slot;
    seen_validate_host_approval = validate_host_approval_flag != NULL ?
        *validate_host_approval_flag : 0u;
    return validate_result;
}

static uint8_t __attribute__((fastcall)) use_mock(
    void *skill,
    void *ignored_edx,
    int slot
) {
    (void)ignored_edx;
    ++use_calls;
    seen_use_skill = skill;
    seen_use_slot = slot;
    seen_use_host_approval = validate_host_approval_flag != NULL ?
        *validate_host_approval_flag : 0u;
    if (use_result != 0u) {
        *((uint8_t *)skill + 0x6cu) = 1u;
        *(int *)((uint8_t *)skill + 0x70u) = slot;
    }
    return use_result;
}

int main(void) {
    uint8_t character[0x100];
    uint8_t skill[0x80];
    uint8_t skill_data[6][0xa0];
    uint8_t skill_context[4];
    uint8_t include_unavailable = 0u;
    SudekiMpSkillActivationApi api;
    SudekiMpSkillActivationResult result;
    SudekiMpSkillQuickSkillList list;
    SudekiMpSkillQuickSkillRow row;
    SudekiMpCharacterSkillState state;
    unsigned int index;

    ZeroMemory(character, sizeof(character));
    ZeroMemory(skill, sizeof(skill));
    ZeroMemory(skill_data, sizeof(skill_data));
    ZeroMemory(skill_context, sizeof(skill_context));
    ZeroMemory(&api, sizeof(api));
    *(void **)(character + 0xd4u) = skill_context;
    *(void **)(character + 0xd8u) = skill;
    *(void **)(skill + 0x10u) = character;
    for (index = 0u; index < 6u; ++index) {
        *(void **)(skill + 0x3cu + index * sizeof(void *)) =
            skill_data[index];
        *(unsigned int *)(skill + 0x54u + index * sizeof(unsigned int)) =
            index;
        skill_data[index][0x08u] = 1u;
        *(int *)(skill_data[index] + 0x0cu) = (int)(10u + index);
        *(uint32_t *)(skill_data[index] + 0x94u) = 20u + index;
    }
    api.availability_target = availability_mock;
    api.validate = validate_mock;
    api.use = use_mock;
    api.include_unavailable_skills = &include_unavailable;
    validate_host_approval_flag = &include_unavailable;

    check(SudekiMpDescribeCharacterQuickSkillsWithApi(
              character, &api, &list) && list.row_count == 6u,
        "custom-menu Skills snapshot uses the native filtered CSkill order");
    check(list.rows[2].ordinal == 2u && list.rows[2].slot == 12 &&
            list.rows[2].cost == 22u && list.rows[2].available != 0u,
        "custom-menu Skills row retains native execute ordinal, slot, cost, and availability");

    SudekiMpCombatContextsReset();
    SudekiMpCombatContextSetCharacter(1u, character);
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 2u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED,
        "third available skill starts through native ABI surface");
    check(result.skill == skill && result.skill_data == skill_data[2] &&
        result.slot == 12, "ordered skill data maps to its native slot");
    check(seen_availability_data == skill_data[2] &&
        seen_availability_context == skill_context,
        "availability bridge supplies EAX skill data and ESI context");
    check(seen_validate_skill == skill && seen_validate_slot == 12,
        "native validator receives CSkill and selected slot");
    check(seen_use_skill == skill && seen_use_slot == 12,
        "native Use receives CSkill and selected slot");

    skill[0x6cu] = 0u;
    ZeroMemory(&row, sizeof(row));
    check(SudekiMpDescribeCharacterSkillSlotWithApi(
              character, 14, &api, &row),
        "exact actor-local skill slot can be described for LAN transport");
    check(row.slot == 14 && row.cost == 24u && row.available != 0u,
        "exact slot description retains native cost and availability");
    result = SudekiMpActivateCharacterSkillSlotWithApi(
        character, 14, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED &&
            result.skill_data == skill_data[4] && result.slot == 14,
        "exact actor-local slot starts through the native validator and Use");
    check(seen_validate_slot == 14 && seen_use_slot == 14,
        "slot activation does not reinterpret the wire value as a UI ordinal");
    check(SudekiMpObserveCharacterSkillWithApi(character, &api, &state) &&
            state.active != 0u && state.skill == skill &&
            state.slot == 14 && state.cost == 24u,
        "host observation exposes only the exact active slot and authored cost");
    skill[0x6cu] = 0u;
    check(SudekiMpObserveCharacterSkillWithApi(character, &api, &state) &&
            state.active == 0u && state.slot == -1,
        "inactive actor skill state is explicit and pointer-local");

    *(int *)(skill_data[5] + 0x0cu) = 14;
    check(!SudekiMpDescribeCharacterSkillSlotWithApi(
              character, 14, &api, &row),
        "duplicate native slot identities are rejected instead of guessed");
    result = SudekiMpActivateCharacterSkillSlotWithApi(
        character, 14, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_INVALID_CONTEXT,
        "duplicate slot activation fails closed");
    *(int *)(skill_data[5] + 0x0cu) = 15;

    skill[0x6cu] = 0u;
    validate_result = 3;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
        result.validation_result == 3,
        "validation rejection is returned without bypass");

    validate_result = 0;
    use_result = 0u;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_USE_REJECTED,
        "native Use rejection is returned without patching state");

    use_result = 1u;
    availability_result = 0u;
    result = SudekiMpActivateCharacterQuickSkillWithApi(character, 0u, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE,
        "unavailable authored skills remain unavailable");
    result = SudekiMpActivateCharacterSkillSlotWithApi(character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_ORDINAL_UNAVAILABLE,
        "exact slot activation also preserves native availability rejection");

    skill[0x6cu] = 0u;
    availability_result = 0u;
    skill_data[0][0x08u] = 0u;
    validate_result = 0;
    use_result = 1u;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_STARTED &&
            result.skill_data == skill_data[0] && result.slot == 10,
        "host-approved replay resolves the stable native slot despite a different client unlock state");
    check(skill_data[0][0x08u] == 0u,
        "host-approved replay restores the client's authored enable byte");
    check(seen_validate_host_approval == 1u &&
            seen_use_host_approval == 1u && include_unavailable == 0u,
        "host-approved replay scopes and restores the native unavailable/no-SP admission byte");
    check(seen_validate_slot == 10 && seen_use_slot == 10,
        "host-approved replay retains the native validator and Use path");

    skill[0x6cu] = 0u;
    validate_result = 3;
    use_calls = 0u;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
            result.validation_result == 3 && use_calls == 0u,
        "host-approved replay never bypasses an invalid local actor state");
    check(skill_data[0][0x08u] == 0u && include_unavailable == 0u,
        "rejected host-approved replay restores the client's authored skill bytes");

    skill[0x6cu] = 0u;
    validate_result = 4;
    result = SudekiMpReplayHostApprovedCharacterSkillSlotWithApi(
        character, 10, &api);
    check(result.status == SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED &&
            skill_data[0][0x08u] == 0u && include_unavailable == 0u,
        "host-approved replay fails closed and restores local state when the native task validator rejects");

    test_owner_routing();
    if (failures != 0) {
        fprintf(stderr, "%d skill activation ABI test(s) failed\n", failures);
        return 1;
    }
    puts("skill activation ABI tests passed");
    return 0;
}
