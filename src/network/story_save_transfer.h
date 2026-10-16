#ifndef SUDEKIMP_STORY_SAVE_TRANSFER_H
#define SUDEKIMP_STORY_SAVE_TRANSFER_H
#include <windows.h>
#include <stdint.h>
/* #72: send the host's saved-game pair to a joiner that does not have it.
 *
 * Policy-neutral transfer core: no sockets, no native game calls, no access
 * to the player's own save slots. The lobby (TCP, ordered, exact-build frame
 * header) carries the bodies built here; the caller decides when a transfer
 * may start and where a verified pair may be used.
 *
 * Protections enforced here (owner direction in #72):
 *  - only the two native save records travel, as one stream: sudeki.fish
 *    then fluffy.bunny. No names or paths ever cross the wire;
 *  - exact sizes: every native save observed has the same fixed record sizes
 *    (26 of 26 files in two prefixes, 2026-10-16); anything else is refused;
 *  - the offer must name exactly the content hashes the lobby already
 *    published for the selected save, and the received bytes must hash to
 *    them before the pair is released;
 *  - chunks are accepted strictly in order for the one active transfer id.
 * The staging writer stores a verified pair in a reserved SudekiMP folder,
 * never in a SAVESLOT folder, writes through temporary names, re-reads and
 * re-hashes, then renames. */
enum {
    SUDEKIMP_SAVE_TRANSFER_FISH_BYTES = 2621952u,
    SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES = 1536u,
    SUDEKIMP_SAVE_TRANSFER_TOTAL_BYTES = 2621952u + 1536u,
    SUDEKIMP_SAVE_TRANSFER_BODY_BYTES = 464u,  /* lobby frame body (WIRE 512 - 48) */
    SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES = 448u,
    SUDEKIMP_SAVE_TRANSFER_CHUNKS =
        (2621952u + 1536u + 448u - 1u) / 448u
};
typedef struct SudekiMpSaveTransferIdentity {
    uint8_t fish_sha256[32], bunny_sha256[32];
} SudekiMpSaveTransferIdentity;
typedef enum SudekiMpSaveTransferStatus {
    SUDEKIMP_SAVE_TRANSFER_REFUSED = 0,
    SUDEKIMP_SAVE_TRANSFER_PROGRESS,
    SUDEKIMP_SAVE_TRANSFER_COMPLETE
} SudekiMpSaveTransferStatus;
typedef enum SudekiMpSaveTransferRefusal {
    SUDEKIMP_SAVE_TRANSFER_OK = 0,
    SUDEKIMP_SAVE_TRANSFER_BAD_BODY,
    SUDEKIMP_SAVE_TRANSFER_BAD_SIZE,
    SUDEKIMP_SAVE_TRANSFER_WRONG_IDENTITY,
    SUDEKIMP_SAVE_TRANSFER_WRONG_TRANSFER,
    SUDEKIMP_SAVE_TRANSFER_OUT_OF_ORDER,
    SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH,
    SUDEKIMP_SAVE_TRANSFER_NO_MEMORY,
    SUDEKIMP_SAVE_TRANSFER_STATE
} SudekiMpSaveTransferRefusal;

/* SHA-256 (BCrypt). FALSE only when the provider fails. */
BOOL SudekiMpSaveTransferHash(const uint8_t *bytes, size_t size, uint8_t out[32]);

typedef struct SudekiMpSaveTransferSender {
    uint32_t transfer_id, next_chunk;
    uint8_t *stream; /* owned: fish then bunny */
    SudekiMpSaveTransferIdentity identity;
} SudekiMpSaveTransferSender;
/* Copies both records after proving exact sizes and that they hash to the
 * identity the lobby published (the host's files must not have changed).
 * transfer_id must be nonzero. */
BOOL SudekiMpSaveTransferSenderBegin(SudekiMpSaveTransferSender *sender, uint32_t transfer_id,
    const uint8_t *fish, size_t fish_size, const uint8_t *bunny, size_t bunny_size,
    const SudekiMpSaveTransferIdentity *published, SudekiMpSaveTransferRefusal *why);
void SudekiMpSaveTransferSenderEnd(SudekiMpSaveTransferSender *sender);
/* Offer body: id, both sizes, chunk geometry and both hashes. */
BOOL SudekiMpSaveTransferSenderOffer(const SudekiMpSaveTransferSender *sender,
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES]);
/* Next chunk body; *done TRUE after the last chunk was produced. */
BOOL SudekiMpSaveTransferSenderNext(SudekiMpSaveTransferSender *sender,
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES], BOOL *done);

typedef struct SudekiMpSaveTransferReceiver {
    uint32_t transfer_id, next_chunk;
    uint8_t *stream; /* owned while active */
    SudekiMpSaveTransferIdentity identity;
    BOOL complete;
} SudekiMpSaveTransferReceiver;
/* Accepts an offer only for exactly the published identity and the fixed
 * record sizes/geometry. A new offer replaces an unfinished one. */
BOOL SudekiMpSaveTransferReceiverOffer(SudekiMpSaveTransferReceiver *receiver,
    const uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES],
    const SudekiMpSaveTransferIdentity *published, SudekiMpSaveTransferRefusal *why);
/* In-order chunk for the active transfer. COMPLETE only after both records
 * hash to the offered (= published) identity; a mismatch refuses and drops
 * the received bytes. */
SudekiMpSaveTransferStatus SudekiMpSaveTransferReceiverChunk(SudekiMpSaveTransferReceiver *receiver,
    const uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES], SudekiMpSaveTransferRefusal *why);
/* Verified records of a COMPLETE receiver (borrowed until End). */
BOOL SudekiMpSaveTransferReceiverRecords(const SudekiMpSaveTransferReceiver *receiver,
    const uint8_t **fish, const uint8_t **bunny);
void SudekiMpSaveTransferReceiverEnd(SudekiMpSaveTransferReceiver *receiver);
/* 0..1000 progress for display. */
unsigned SudekiMpSaveTransferReceiverPermille(const SudekiMpSaveTransferReceiver *receiver);

/* Store a verified pair under root\<first 16 hex digits of the fish hash>\
 * as sudeki.fish / fluffy.bunny. root must already exist and must not be a
 * reparse point; the folder name is derived from content only. Files are
 * written as *.tmp (CREATE_NEW, write-through), re-read and re-hashed, then
 * renamed. An existing folder with the same verified content is reused.
 * out_dir receives the folder on success. */
BOOL SudekiMpSaveTransferStage(const wchar_t *root, const uint8_t *fish, const uint8_t *bunny,
    const SudekiMpSaveTransferIdentity *identity, wchar_t out_dir[MAX_PATH]);

/* Reserved native save folders for received pairs: SAVESLOT9900..9999 in
 * the game's own save directory (the only place the native loader reads),
 * each tagged by a marker file naming its content. The joiner's lobby finds
 * the pair by content hash like any local copy. A folder without the marker
 * is never written, reused or removed: the player's own saves stay untouched. */
enum { SUDEKIMP_SAVE_TRANSFER_FIRST_SLOT = 9900u, SUDEKIMP_SAVE_TRANSFER_LAST_SLOT = 9999u };
/* Reuse the reserved folder already holding this verified pair, or create
 * the first free reserved folder: marker, then both records through the
 * verified temporary-name writer. The small record stores its own folder
 * ("SAVESLOTdddd\\" in the 16 bytes at 0x200) and the native loader reads from that stored
 * folder, so the copy's stored folder is rewritten to the reserved one; the
 * marker keeps the original folder number. Refused when the received record
 * does not hold a well-formed stored folder. *slot receives the folder. */
BOOL SudekiMpSaveTransferImport(const wchar_t *save_root, const uint8_t *fish, const uint8_t *bunny,
    const SudekiMpSaveTransferIdentity *identity, unsigned *slot);
/* Find a reserved, marker-tagged folder holding the published pair: the main
 * record hashes to the published value, and the small record, with the
 * original stored folder put back, hashes to the published value. Read-only. */
BOOL SudekiMpSaveTransferFindImported(const wchar_t *save_root, const SudekiMpSaveTransferIdentity *identity, unsigned *slot);
/* Remove reserved, marker-tagged folders whose marker is older than
 * max_age_seconds: only the two records and the marker, then the folder if it
 * is empty. Unmarked folders are never touched. *removed counts folders. */
BOOL SudekiMpSaveTransferExpire(const wchar_t *save_root, uint64_t max_age_seconds, unsigned *removed);
#endif
