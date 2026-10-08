#ifndef SUDEKIMP_SAVE_CATALOG_H
#define SUDEKIMP_SAVE_CATALOG_H

#include <windows.h>
#include <stdint.h>

#define SUDEKIMP_SAVE_CATALOG_MAX_ENTRIES 128u

/* Local file identity only. Folder suffixes are NOT native LoadGameSave IDs:
 * the native picker builds and sorts a separate catalog. */
typedef struct SudekiMpSaveFileIdentity {
    uint32_t volume, index_high, index_low;
    uint64_t size;
    FILETIME created, modified;
} SudekiMpSaveFileIdentity;

typedef struct SudekiMpSaveCatalogEntry {
    uint32_t folder_slot;
    /* All display fields are printable ASCII for the current renderer.
     * saved_at is always the fish file's last-write time in UTC. */
    char label[64], location[64], play_time[32], saved_at[32];
    char details[4][96];
    SudekiMpSaveFileIdentity fish, bunny;
    uint8_t metadata_known; /* Validated saved title, not complete native state. */
    /* Saved party order, mapped from native portrait IDs to SMP4 identities.
     * Unknown/NPC records leave party_known false; never infer a hero from
     * display text. The first member is the saved native leader. This is a
     * preview; native load completion must independently confirm the roster. */
    uint8_t party_known, party_count, party_mask, leader;
    uint8_t party_order[4];
    /* Display-order keys only, never native catalog/load identifiers. */
    FILETIME sort_start, sort_directory_modified;
    float sort_elapsed;
    uint8_t sort_known;
} SudekiMpSaveCatalogEntry;

typedef struct SudekiMpSaveCatalog {
    unsigned count, skipped;
    BOOL truncated;
    char error[160];
    wchar_t directory[MAX_PATH];
    SudekiMpSaveFileIdentity directory_identity;
    SudekiMpSaveCatalogEntry entries[SUDEKIMP_SAVE_CATALOG_MAX_ENTRIES];
} SudekiMpSaveCatalog;

typedef struct SudekiMpSaveFingerprint {
    uint32_t folder_slot;
    uint8_t fish_sha256[32], bunny_sha256[32];
} SudekiMpSaveFingerprint;

/* Explicit bounded refresh only; no worker, native loader or save writes.
 * A successful empty catalog is valid. Error text is safe to display. */
BOOL SudekiMpSaveCatalogRefresh(SudekiMpSaveCatalog *catalog);
/* Review selection: require the listed identities/sizes/timestamps, open both
 * files without write/delete sharing and hash their actual bytes. Listing does
 * not claim a byte baseline for fish; the fingerprint establishes that baseline. */
BOOL SudekiMpSaveCatalogFingerprint(SudekiMpSaveCatalog *catalog,
    unsigned index, SudekiMpSaveFingerprint *fingerprint);
/* Confirmation: rehash the same selected pair and compare with Review. */
BOOL SudekiMpSaveCatalogVerify(SudekiMpSaveCatalog *catalog,
    unsigned index, const SudekiMpSaveFingerprint *fingerprint);

/* Native loading keeps a reviewed pair read-only and denies write/delete
 * sharing until the synchronous native file read has positively returned.
 * The opaque lease owns no native pointers and never invokes the game. */
typedef struct SudekiMpSaveLease SudekiMpSaveLease;
BOOL SudekiMpSaveCatalogAcquire(SudekiMpSaveCatalog *catalog, unsigned index,
    const SudekiMpSaveFingerprint *fingerprint, SudekiMpSaveLease **lease);
BOOL SudekiMpSaveLeaseMatchesRoot(const SudekiMpSaveLease *lease,
    const wchar_t *native_directory);
BOOL SudekiMpSaveLeaseRecord(const SudekiMpSaveLease *lease,
    uint8_t record[0x2c8], unsigned *folder_slot);
/* Called only by the exact native reader adapter. TRUE means this is one of
 * the pinned paths (possibly with INVALID_HANDLE_VALUE and a Win32 error).
 * FALSE means unrelated: the caller must preserve its original file open.
 * The successful read-only handle is identity-checked and permits only reads. */
BOOL SudekiMpSaveLeaseOpenNativeRead(const SudekiMpSaveLease *lease,
    const wchar_t *path, HANDLE *opened);
void SudekiMpSaveLeaseRelease(SudekiMpSaveLease **lease);

/* Roaming AppData folder to use instead of the Windows profile's (NULL or
 * empty = the profile's own). Set once at DLL start-up, before any catalogue. */
void SudekiMpSaveCatalogSetAppDataOverride(const wchar_t *path);
#endif
