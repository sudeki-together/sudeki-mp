# Review follow-ups: tiers 1, 1.5 and 2 implementation (2026-10-19)

**Where to work.** The implementation of [enemy-ai-tier12-task.md](enemy-ai-tier12-task.md)
sits uncommitted in the `shared-simulation` worktree of `codex/shared-simulation`
(dirty on `fa50351`, DLL `bbf23b04…`). Work there; do not commit, stash,
reset or reformat; other dirty files belong to the owner.

**Verdict of the review** (five area readers, two skeptics per finding):
no high finding. The policy and adapter match the brief; tier 0b behaviour
is unchanged; no native write beyond `*out`, no global-list read outside
modes 0/0xB, no retained pointer, no cross-thread access was found. What
survived is one low adapter defect and a set of tests that do not
discriminate the rule they name, plus doc sentences that claim more than
their test proves. None of it blocks the owner's live gates, which run on
the current DLL. Fix all of it in one pass; every item below names the
mutation the new or changed test must fail.

## Code (one defect)

1. `src/hooks/story_enemy_targeting.c` `walk()` (~981): a leased, live unit
   whose `position()` read fails is left out of the unit table while still
   counted in `seat_frames[c].engaged`, and `table_complete` stays TRUE. If
   that unit is the pushed enemy targeting its own seat, `frame_of()` finds
   no entry, `engaged_by_other` is not reduced, and the push ends with
   `engaged` on its own engagement. Give such a unit a table entry with
   `position_unknown` set (x/z invalid) so `engaged_by_other` can subtract
   it; distance-based uses (responder distance, `close`, Aware push search)
   skip entries with unknown positions. Image test: the pushed enemy with a
   broken position back-pointer keeps its push until `until`; reverting the
   entry must fail it.

## Tests that do not discriminate (fix each; the brief's rule is right)

2. `tests/story_threat_test.c` `clock_wrap_tests` (~418): every deadline is
   queried only after both `now` and the deadline crossed the boundary, so a
   plain `now >= until` in `threat_due` passes. Add cases that straddle the
   boundary: `now` just before 2^32 (and 2^31) with the deadline just after,
   and the reverse; assert not-due then due. Then testing.md's "signed/
   unsigned clock wrap" claim is true.
3. `tests/story_threat_test.c` Pick (~197): the only current-below-floor case
   uses current .01 vs best .2, a gap already beyond the stickiness margin.
   Add current in (.04, .05] and best in (.05, current × 1.25]: with the
   floor clause the result is best; without it, current. Deleting
   `scores[current] > floor` in `story_threat.c` must fail.
4. `tests/story_enemy_targeting_image_test.c` alert hold (~841): ten selects
   at one tick with no `Service` pass before the forced recheck failure. Run
   selects interleaved with `Service` passes through an active alert and
   assert the responder stays recruited and substituting on the success
   path (a mutation that sets `recheck_failed` on success must fail), then
   the forced failure releases it.
5. Image test push retirement (~910): no test retires a pushed enemy through
   the walk. Remove the pushed enemy from the manager (and separately mark
   its lease departed) during an active push; assert `push_end
   reason=invalid` from `!pushed_exists`. Replacing the adapter's `r!=NULL`
   argument with `1` must fail.
6. Image test aware/excluded filters (~975): the "mutation per filter" cases
   write `u->aware` / `u->excluded` into the derived table. Add fixtures the
   walk derives itself: a native non-aware unit (+0xF4 empty, and separately
   +0xC0 set) and a name-excluded and a resource-excluded unit at tier 2;
   assert none is pushed or recruited. Reverting the walk's derivation
   (`aware = !target && +0xF4`, `excluded_unit`) must fail.
7. Image test hit mapping (~879): only far/unengaged are checked. Add: a
   non-ranged hit (no missile match, arbiter mode ≠ 2) adds threat and no
   sniper; a Spirit hit (`spirit_known=1, spirit=1`) adds threat and no
   sniper; `spirit_known=0` adds no sniper; a `firing=0` hit adds no sniper —
   asserting the sniper value each time. Constant `ranged` / `hit_all` /
   `firing` arguments in `memory_hit` must fail.
8. `same_group` (adapter ~532): move the rule into a pure helper
   `SudekiMpStoryThreatSameGroup(a, b)` (both non-zero and equal), test it
   (0/0, 0/7, 7/7, 7/8), and add an image fixture with group-id-0 leases so
   dropping the non-zero check fails the responder order.
9. `tests/story_threat_test.c` fade / orphan fade / push end (~266, ~309,
   ~361): each case has one applicable reason, so the documented precedence
   (level, down, stop, close, left, expired; level, down, invalid, engaged,
   expired) is untested. Add cases with two reasons applicable asserting the
   first; swapping any two checks must fail.
10. `tests/story_threat_test.c` commit (~264): the `count == 0` case also
    passes NULL serials, NULL records and an active alert, each of which
    refuses on its own. Use non-NULL arrays and an idle record with
    `count = 0`; deleting `!count ||` must fail.
11. `tests/story_threat_test.c` Decide (~385): the lower-level fall-through
    is checked only at TIER1. Add an active push decided at TIER15 → falls
    through; weakening the push gate to TIER15 must fail.
12. `tests/story_split_control_test.c` Part 0: no menu-open assertion uses an
    unclaimed companion (every one uses character 2, the away reservation).
    Add `available|=1; menu_captures=TRUE; assert(!CompanionSeatExact(0))`
    (and restore), so moving the menu clause below `if(p>=4) return TRUE;`
    fails. Then testing.md ~716 is true as written.

## Documentation

13. `docs/enemy-ai-task.md` ~427: the Tests bullet says the exact-image
    fixture covers "mode-0xE passthrough without a list read or direct
    filter call". The 0xffffffff list poison is rejected by the adapter's
    own `n > 32` checks and no mode-14 call asserts the filter call count,
    so neither is proved. Either add a test (a valid-count stale list whose
    entries are unreadable, with a filter-call counter, during mode-14
    calls) or reword to what holds: mode-14 calls pass through unchanged
    (CONFIRMED_TEST); the no-read rule is CONFIRMED_STATIC by code reading.
14. `docs/enemy-ai-task.md` decision 11 (~580): "after the active one ends
    and that enemy's cooldown passes" reads as the original alerter's
    cooldown. Write: "… and the cooldown of the enemy that raises the next
    one has passed", matching O2 and the code.
15. `src/network/story_threat.h` ~54: the decision-10 note says "only
    HOST/REMOTE may be named above tier0"; add "AI/AWAY bodies remain
    reachable only through the tier 0b lead branch (Decide (d))", which is
    what the code does.
16. `docs/testing.md`: after items 2, 7, 12 and 13, re-read the section's
    CONFIRMED_TEST sentences and keep only those a listed assertion backs.

## Acceptance

Build; `git diff --check`; native C11 `story_threat_test.c`; Wine
`StoryThreatTest`, `StoryEnemyTargetingImageTest <supported SUDEKI.exe>`,
`StorySplitControlTest`, `LanArenaHitFeedbackTest`; the full suite with the
known non-pass set unchanged; testing.md addendum with the new DLL hash;
report file by file; no commit; no live claim. Keep the current DLL
(`bbf23b04…`) as the owner's live-gate artifact unless item 1 changes
behaviour the owner is exercising — say so explicitly if it does.
