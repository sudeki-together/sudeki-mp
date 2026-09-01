#include "hooks/lan_arena_campaign_guard.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

enum {
    RVA_SAVE_MENU_SHOW = 0x00084f10u,
    RVA_LOAD_GAME_SAVE = 0x00101690u,
    RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER = 0x00023f60u,
    RVA_GROUP_PLAYERS_NEXT_CHARACTER = 0x00024060u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_LOADING_FLAG = 0x00409df8u,
    SAVE_MENU_SHOW_HOOK_LENGTH = 5u,
    LOAD_GAME_SAVE_HOOK_LENGTH = 7u
};

static const uint8_t save_menu_show_stable_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x17u,
    0x8bu, 0x88u, 0x70u, 0x01u, 0x00u, 0x00u,
    0x85u, 0xc9u, 0x74u, 0x0du,
    0x8bu, 0x01u, 0x8bu, 0x50u, 0x2cu,
    0x6au, 0x00u, 0x6au, 0x00u, 0x6au, 0x1bu,
    0xffu, 0xd2u, 0xc3u
};
static const uint8_t group_players_character_switch_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x18u, 0x83u, 0xbeu, 0xccu, 0x00u, 0x00u, 0x00u, 0x01u
};

static SudekiMpInlineHook save_menu_show_hook;
static SudekiMpInlineHook load_game_save_hook;
static SudekiMpBytePatch previous_character_patch;
static SudekiMpBytePatch next_character_patch;
static uint8_t *game_base;
static BOOL save_block_logged;
static BOOL slot_block_logged;

static BOOL signatures_match(uint8_t *base) {
    uint32_t operand;
    if (base == NULL || base[RVA_SAVE_MENU_SHOW] != 0xa1u ||
        memcmp(base + RVA_SAVE_MENU_SHOW + 5u,
            save_menu_show_stable_tail,
            sizeof(save_menu_show_stable_tail)) != 0 ||
        memcmp(base + RVA_LOAD_GAME_SAVE, "\x80\x3d", 2u) != 0 ||
        memcmp(base + RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
            group_players_character_switch_entry,
            sizeof(group_players_character_switch_entry)) != 0 ||
        memcmp(base + RVA_GROUP_PLAYERS_NEXT_CHARACTER,
            group_players_character_switch_entry,
            sizeof(group_players_character_switch_entry)) != 0 ||
        base[RVA_LOAD_GAME_SAVE + 6u] != 0x00u) return FALSE;
    memcpy(&operand, base + RVA_SAVE_MENU_SHOW + 1u, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + RVA_UI_SCENE_GLOBAL)) {
        return FALSE;
    }
    memcpy(&operand, base + RVA_LOAD_GAME_SAVE + 2u, sizeof(operand));
    return operand == (uint32_t)(uintptr_t)(base + RVA_LOADING_FLAG);
}

static void __attribute__((cdecl)) block_save_menu_show(void) {
    if (!save_block_logged) {
        save_block_logged = TRUE;
        SudekiMpLogWrite(
            "lan_arena_campaign_guard event=save_menu status=blocked "
            "policy=ephemeral_cleanroom_no_campaign_read_or_write\r\n");
    }
}

static void __attribute__((cdecl)) block_load_game_save(int save_slot) {
    (void)save_slot;
    if (!slot_block_logged) {
        slot_block_logged = TRUE;
        SudekiMpLogWrite(
            "lan_arena_campaign_guard event=save_slot_operation status=blocked "
            "policy=ephemeral_cleanroom_no_campaign_read_or_write\r\n");
    }
}

BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module) {
    uint8_t expected_save[SAVE_MENU_SHOW_HOOK_LENGTH];
    uint8_t expected_slot[LOAD_GAME_SAVE_HOOK_LENGTH];
    uint8_t *base = (uint8_t *)game_module;
    if (base == NULL || game_base != NULL || !signatures_match(base)) {
        SetLastError(base == NULL || game_base != NULL ?
            ERROR_INVALID_PARAMETER : ERROR_INVALID_DATA);
        return FALSE;
    }
    memcpy(expected_save, base + RVA_SAVE_MENU_SHOW, sizeof(expected_save));
    memcpy(expected_slot, base + RVA_LOAD_GAME_SAVE, sizeof(expected_slot));
    if (!SudekiMpInstallInlineHook(
            &save_menu_show_hook, base + RVA_SAVE_MENU_SHOW,
            expected_save, sizeof(expected_save), block_save_menu_show) ||
        !SudekiMpInstallInlineHook(
            &load_game_save_hook, base + RVA_LOAD_GAME_SAVE,
            expected_slot, sizeof(expected_slot), block_load_game_save) ||
        !SudekiMpInstallBytePatch(
            &previous_character_patch,
            base + RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER, 0x55u, 0xc3u) ||
        !SudekiMpInstallBytePatch(
            &next_character_patch,
            base + RVA_GROUP_PLAYERS_NEXT_CHARACTER, 0x55u, 0xc3u)) {
        SudekiMpRestoreBytePatch(&next_character_patch);
        SudekiMpRestoreBytePatch(&previous_character_patch);
        SudekiMpRestoreInlineHook(&load_game_save_hook);
        SudekiMpRestoreInlineHook(&save_menu_show_hook);
        return FALSE;
    }
    game_base = base;
    save_block_logged = FALSE;
    slot_block_logged = FALSE;
    SudekiMpLogWrite(
        "lan_arena_campaign_guard event=install state=active "
        "character_switch=blocked roles=Tal_host_Ailish_client "
        "policy=fixed_role_tuple_no_native_F1_rotation\r\n");
    return TRUE;
}

void SudekiMpUninstallLanArenaCampaignGuard(void) {
    SudekiMpRestoreBytePatch(&next_character_patch);
    SudekiMpRestoreBytePatch(&previous_character_patch);
    SudekiMpRestoreInlineHook(&load_game_save_hook);
    SudekiMpRestoreInlineHook(&save_menu_show_hook);
    game_base = NULL;
    save_block_logged = FALSE;
    slot_block_logged = FALSE;
}
