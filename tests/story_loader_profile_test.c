/* The generated include contains the unmodified title-profile branch and
 * boolean readers from dllmain.c. Native installs are spies; INI IO is real.
 * This tests launch routing, not hook installation or native gameplay. */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "loader/fixed_three_profile.h"
#include "ui/save_catalog.h"
#include "network/lan_party_session.h"
#define SUDEKIMP_INIT_OK 0u
#define SUDEKIMP_INIT_BAD_CONFIG 6u
#define SUDEKIMP_INIT_CLEANROOM_MENU_FAILED 15u
static unsigned setter_calls,title_calls;
static BOOL lead_value,title_value;
static BOOL native_ok(void) { return TRUE; }
static void set_lead(BOOL enabled) { ++setter_calls;lead_value=enabled; }
static BOOL install_title(void) { ++title_calls;title_value=lead_value;return TRUE; }
#define SudekiMpLanStoryRuntimeSetClientLeadSpectate set_lead
#define SudekiMpInstallTitleMultiplayer(...) install_title()
#define SudekiMpInitializeSkillActivationAbi(...) native_ok()
#define SudekiMpInitializeSpiritActivationAbi(...) native_ok()
#define SudekiMpInitializeWeaponActivationAbi(...) native_ok()
#define SudekiMpCleanroomEngineInitialize(...) native_ok()
#define SudekiMpInstallControlSeparation(...) native_ok()
#define SudekiMpInstallLanArenaWindowPolicy(...) native_ok()
#define configure_background_audio(...) native_ok()
#define SudekiMpLanArenaStartupMovieSkipIntroPoem(...) native_ok()
#define SudekiMpInstallLanArenaStartupMovieSkip(...) native_ok()
#define read_config_float(...) native_ok()
#define SudekiMpSaveSearchSpeedInstall(...) native_ok()
#define SudekiMpLogSetResearch(...) native_ok()
#define SudekiMpCleanroomEngineSetTPtrAudit(...) native_ok()
#define SudekiMpLanStoryRuntimeSetStepAway(...) native_ok()
#define configure_test_start(...) ((void)0)
#define SudekiMpLanStoryMenuTestInvulnerable(...) native_ok()
#define SudekiMpLanStoryNameTagsInstall(...) native_ok()
#define configure_research_markers(...) ((void)0)
#define SudekiMpLanStoryAnimTraceInstall(...) native_ok()
#define SudekiMpLogResearchEnabled(...) native_ok()
#define SudekiMpLanStoryResearch54Install(...) native_ok()
#define SudekiMpResourceSwapInstall(...) native_ok()
#define SudekiMpLanStoryAreaFadeEnable(...) native_ok()
#define SudekiMpLanStoryNpcGestureEnable(...) native_ok()
#define SudekiMpLanStoryAmbientInstall(...) native_ok()
#define SudekiMpInstallLobbyGameplay(...) native_ok()
#define SudekiMpInstallLobbyInstance(...) native_ok()
#define configure_profile_folder(...) native_ok()
#define SudekiMpLanStoryTaskTraceInstall(...) native_ok()
#define SudekiMpInstallLanStoryLoad(...) native_ok()
#define SudekiMpLobbyGameplayEnableTestroom(...) native_ok()
#define SudekiMpLobbyGameplayEnableSaved(...) native_ok()
#define SudekiMpSharedReadFilesInstall(...) native_ok()
#define configure_lobby_auto(...) ((void)0)
#define SudekiMpInstallLanStoryRuntime(...) native_ok()
#define SudekiMpTitleMultiplayerQueueSavedLoadProbe(...) native_ok()
#define uninstall_runtime_hooks(...) native_ok()
#define SudekiMpLogFormat(...) ((void)0)
#define SudekiMpLogWrite(...) ((void)0)
#define decode_sha256_text(...) native_ok()
#include "story_loader_profile_generated.h"
int main(void) {
    wchar_t dir[MAX_PATH],ini[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,dir));assert(GetTempFileNameW(dir,L"slp",0,ini));
    assert(WritePrivateProfileStringW(L"TitleMenu",L"Enabled",L"true",ini));
    const wchar_t *scopes[]={L"saved-story",L"entry",L"saved-load-observe"};
    const wchar_t *values[]={NULL,L"false",L"true",L"on",L"invalid"};
    const wchar_t *hash=L"0000000000000000000000000000000000000000000000000000000000000000";
    assert(WritePrivateProfileStringW(L"TitleMenu",L"SaveSlot",L"11",ini));
    assert(WritePrivateProfileStringW(L"TitleMenu",L"FishSha256",hash,ini));
    assert(WritePrivateProfileStringW(L"TitleMenu",L"BunnySha256",hash,ini));
    for(unsigned profile=0;profile<3;++profile) for(unsigned value=0;value<5;++value) {
        assert(WritePrivateProfileStringW(L"TitleMenu",L"Scope",scopes[profile],ini));
        assert(WritePrivateProfileStringW(L"StoryAreas",L"ClientLeadSpectate",values[value],ini));
        setter_calls=title_calls=0;lead_value=title_value=FALSE;
        assert(title_profile_start(ini,FALSE)==SUDEKIMP_INIT_OK);
        assert(title_calls==1);
        assert(setter_calls==(profile==0));
        assert(lead_value==(profile==0 && (value==2 || value==3)));
        assert(title_value==lead_value); /* Before the lobby can start a session. */
    }
    assert(WritePrivateProfileStringW(L"TitleMenu",L"Scope",L"saved-story",ini));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"ClientLeadSpectate",L"true",ini));
    setter_calls=title_calls=0;
    assert(title_profile_start(ini,TRUE)==99 && !setter_calls && !title_calls);
    assert(WritePrivateProfileStringW(L"TitleMenu",L"Enabled",L"false",ini));
    assert(title_profile_start(ini,FALSE)==99 && !setter_calls && !title_calls);
    assert(DeleteFileW(ini));
    puts("StoryLoaderProfileTest PASS: saved-story INI true/false/missing/invalid, entry and load-only exclusion, setter before lobby");
    return 0;
}
