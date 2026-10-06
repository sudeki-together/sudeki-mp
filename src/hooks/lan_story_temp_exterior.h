#ifndef SUDEKIMP_LAN_STORY_TEMP_EXTERIOR_H
#define SUDEKIMP_LAN_STORY_TEMP_EXTERIOR_H
#include <windows.h>
#include <stdint.h>

/* Experimental separation of vanilla TEMP whole-exterior suspension.
 * Not installed by the ordinary story runtime.
 *
 * CWorld::EnterTemporaryZone suspends the current exterior and each listed
 * neighbor through 0x50B500 (entity pause/hide + terrain collision removal)
 * and stops their localized audio. ExitTemporaryZone performs the inverse
 * through 0x50B4B0 plus an inline neighbor re-enrollment/resume/audio loop.
 * Those helpers have no other callers in the supported image.
 *
 * This adapter owns exactly two relative calls: Enter 0x406527 and Exit
 * 0x40679C. When the attached consumer asks to keep a state-3 exterior live,
 * Enter skips the remaining exterior deactivation as one unit (main exterior
 * suspension, every neighbor's graphics release/suspension/audio stop) and
 * continues at the native TEMP selection. The main exterior's graphics release
 * at 0x40651F has already run and stays native. Exit then skips the whole
 * inverse unit for that exact descriptor and continues at native lead return.
 * Nothing is resumed that was not suspended: no pause counter is cleared,
 * underflowed or replayed.
 *
 * This is NOT a second simulation tick, collision filter, background-area
 * loader, native lifetime lease or client presentation path. TEMP foreground
 * selection (0x405B90) still runs natively and releases the exterior's
 * presentation resource. The exterior's AI/collision/audio continue under the
 * native shared scheduler while the TEMP is current; whether that is safe for
 * every area is the purpose of the live probe, not a property proven here. */

#define SUDEKIMP_STORY_TEMP_EXTERIOR_NAME 48u
#define SUDEKIMP_STORY_TEMP_EXTERIOR_TRACKED 64u
#define SUDEKIMP_STORY_TEMP_EXTERIOR_NEIGHBORS 16u

typedef struct SudekiMpLanStoryTempExteriorEntry {
    uint64_t ticket;
    char exterior[SUDEKIMP_STORY_TEMP_EXTERIOR_NAME];
    char destination[SUDEKIMP_STORY_TEMP_EXTERIOR_NAME];
    uint32_t neighbors,characters,tracked;
} SudekiMpLanStoryTempExteriorEntry;

enum {
    SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED=1,
    /* Skipped entry exists but exit saw another world/exterior/neighbor set.
     * Resume is still skipped: nothing was suspended by the skipped entry. */
    SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_MISMATCH=2
};

typedef struct SudekiMpLanStoryTempExteriorReceipt {
    uint64_t ticket;
    uint32_t result;
    char exterior[SUDEKIMP_STORY_TEMP_EXTERIOR_NAME];
    /* Copied diagnostics from the exterior character catalog. Positions are
     * native CPosition translations sampled at skipped entry and skipped exit.
     * A moved character is evidence the native scheduler advanced it; an
     * unmoved one may simply be idle. */
    uint32_t tracked,still_present,moved,unchanged,disable_changed,disable_raised,disable_lowered;
    uint32_t disable_entry_nonzero,disable_exit_nonzero;
    float max_displacement;
    uint32_t elapsed_ms;
    uint8_t terrain_present,terrain_enabled_entry,terrain_enabled_exit;
    uint32_t terrain_mask_entry,terrain_mask_exit;
} SudekiMpLanStoryTempExteriorReceipt;

typedef struct SudekiMpLanStoryTempExteriorConsumer {
    /* Called on the native thread inside EnterTemporaryZone, after the native
     * exterior graphics release and before suspension. Return TRUE to keep
     * this exterior live. Must not call native world functions. */
    BOOL (*keep_exterior)(void *context,const SudekiMpLanStoryTempExteriorEntry *);
    /* Called inside ExitTemporaryZone after the skipped resume decision. */
    void (*exit_skipped)(void *context,const SudekiMpLanStoryTempExteriorReceipt *);
    void *context;
} SudekiMpLanStoryTempExteriorConsumer;

typedef struct SudekiMpLanStoryTempExteriorStatus {
    BOOL installed,attached,outstanding,unknown;
    uint64_t native_entries,kept_entries,native_exits,skipped_exits,mismatched_exits;
} SudekiMpLanStoryTempExteriorStatus;

/* Exact-image installation. Must run before a TEMP transition can be active
 * (world current descriptor not state 4) or with no world yet. */
BOOL SudekiMpLanStoryTempExteriorInstall(HMODULE);
/* Consumer storage must outlive attachment. Attach before entry; detach is
 * refused while a kept exterior is outstanding. */
BOOL SudekiMpLanStoryTempExteriorAttach(const SudekiMpLanStoryTempExteriorConsumer *);
BOOL SudekiMpLanStoryTempExteriorDetach(void);
BOOL SudekiMpLanStoryTempExteriorStatusCopy(SudekiMpLanStoryTempExteriorStatus *);
/* Refused while an entry is outstanding, a callback is active or state is
 * unknown; restoration failure retains every dependency. */
BOOL SudekiMpLanStoryTempExteriorUninstall(void);
#endif
