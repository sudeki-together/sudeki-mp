# Player statehood and shared-interaction design

## Purpose and boundary

Sudeki has one authoritative world, one active party inventory, and native UI
layers that were authored for one controller. SudekiMP may give several humans
independent character, input, camera, and viewport leases, but those leases do
not make every engine subsystem player-local. This document is the ownership
contract for extending world interaction without turning a Player 2 button
press into an unproven Player 1 action.

The central rule is:

> A human owns an actor lease, not an arbitrary write lease over global game
> state. Shared mutations are serialized and progression remains host-owned
> until an exact target-specific path proves otherwise.

The current controller router follows the shipped Xbox-style roles without
inventing Player 1 authority for another seat. A resolves a proven exact
interaction intent or falls back to native Weak; X submits native Strong; Y
exposes a per-seat Quick Menu intent whose native consumer is not connected;
B resolves modal Cancel or native combat Sweep; and the D-pad exposes per-seat
Quickshot intents whose consumers are not connected. The sticks remain on
their movement/camera paths. Button edges and reconnect-neutral fences are
stored independently for seats 0 through 3, although the present runtime
bridge supplies only P2.

This design describes one process and one loaded Sudeki world. Running two
independent exterior/interior worlds, player-local inventories, or player-local
save timelines would require a different engine architecture.

## Evidence scope

All addresses below are for the only supported image:

| Property | Value |
| --- | --- |
| Store/build | GOG offline build `50303954381148403` |
| Executable | `SUDEKI.exe`, PE32/x86 |
| SHA256 | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| Preferred image base | `0x00400000` |
| Static-analysis baseline | Ghidra 12.1.2 |

Addresses written as `VA` are preferred-image virtual addresses; `RVA` is
`VA - 0x00400000`. Runtime code must use the relocated module base and an exact
image/signature gate. Names inferred from exports, RTTI, and decompilation are
descriptive evidence, not a promise that the original engine exposed a modern
multiplayer abstraction.

## Ownership map

| System | Native shape | SudekiMP owner | Rule |
| --- | --- | --- | --- |
| Human seat | None | Per player | Presence and role selection persist separately from a live actor pointer. |
| Character actor | Party member object | Per-player lease | A lease is valid only for the published actor pointer and actor generation. Drop-out returns the actor to native AI. |
| Movement and combat input | P1 controller plus actor arbiters | Per player where proven | Submit to the leased actor's validated native path; never change the global lead merely to impersonate P2. |
| Camera and HUD cache | One native render camera plus cached views | Per viewport where virtualized | Presentation ownership never grants world-mutation authority. |
| World, scene, collision, AI, and scripts | One active simulation | Native global | All players occupy the same loaded world. Temporary-room transitions move the party atomically. |
| Interaction request | No native player identity | Per-player coordinator record | A request carries immutable provenance and does nothing authoritative until its policy permits commit. |
| Shop and blacksmith UI | Global layer/controller state | One serialized modal owner | Use full-width shared presentation until every mutable UI field is virtualized. |
| Merchant stock/catalog | `CShopInventory` singleton | Shared native state | Snapshot for display; re-resolve and revalidate before commit. Do not clone it as player inventory. |
| Items and money | `CInventory` singleton | Shared party state | Every successful transaction changes one party inventory/economy. |
| Equipped weapon definition | Character weapon object | Per character | The target character is explicit transaction provenance; the item/forge data it consumes remains shared. |
| Progression, travel, dialogue, quests, saves, cutscenes | Global script/world state | Host | Non-host input may request attention or vote, but cannot directly dispatch these paths. |
| Save payload | One party/world snapshot | Native global | Runtime leases, UI shadows, pointers, and interaction serials are never save data. |

### Proven native shop/economy seams

The exact-image static audit establishes why the initial design is serialized:

- `GetInventory` at VA `0x00422140` returns the one pointer stored at
  `0x00808D84`; money is the 32-bit field at `CInventory+0x134`.
- `GetShopInventory` at VA `0x00417D40` returns the one pointer stored at
  `0x00808D44`. Its current mode is at `+0x0C`, and the two mutable entry arrays
  begin at `+0x10` and `+0x20`.
- `ShopStart` at VA `0x0048D1A0` queues native UI mode `0x0C` through the global
  front-end controller. `IsShopActive` at `0x0048D1C0` reads that same global
  UI state. The shop and blacksmith layer pointer slots include `0x007C2F70`
  and `0x007C2F74`; there is no player-index parameter.
- `UIBlackSmithStart` at VA `0x00492C40` queues native UI mode `0x0D` through
  the global controller. `UIBlackSmithActive` at `0x00492C60` reads the active
  byte at `UILayerBlackSmith+0x29`.
- The shop buy path at VA `0x00489D70` calls `CInventory::AddItem`
  (`0x004217E0`) and then debits the same inventory. The sell path at
  `0x00489DF0` calls `CInventory::ReduceItem` (`0x00421AB0`) and then
  `CInventory::AddMoney` (`0x00421C50`). These are sequential native effects,
  not a re-entrant transaction object with rollback.
- The blacksmith confirmation path spans VA `0x004927C0`, `0x00530730`, and
  `0x004220C0`: it writes a shared inventory equipment-slot byte and then
  debits the party money. The final confirm path does not provide a safe
  concurrent compare-and-commit primitive.
- Blacksmith layer fields `+0x68` and `+0x6C` hold the target equipment item ID
  and selected forge component/service ID. `+0x2F0` is a target slot index, not
  a customer/actor pointer.
- A character weapon object carries its current item-definition ID at `+0x264`,
  resolved definition pointer at `+0x268`, and pending pointer at `+0x26C`.
  These fields make equipment character-specific, but do not make money,
  materials, or the native blacksmith layer character-specific.

Static analysis therefore supports one serialized global session. It does not
support two simultaneous native `UILayerShop`/`UILayerBlackSmith` instances.

### Default-off two-viewport blacksmith preview

The first Stage 3 experiment virtualizes presentation without cloning the
native layer. On the exact supported image, paired five-byte hooks at
`UIBlackSmithStart` and `UIBlackSmithActive` may replace one host-started native
blacksmith lifecycle with two mod-owned seat shadows. Sudeki's SOL bytecode
discards the Start result and polls Active, so the adapter reports one global
active lease until both panels close. Duplicate Start calls join that same
lease; they never create a second native or mod transaction.

The opt-in path requires the locked split roster, both current actor-generation
leases, a ready world, exclusive controller-bridge suppression, and readable
shared inventory/money. Each actor must equal its locked role pointer and occur
exactly once in the bounded active-party slots. It freezes both actors' gameplay
input, preserves both cached cameras, and renders independent P1/P2 equipment,
socket, rune, projected-stat, page, cursor, and session state from bounded
pointer-free snapshots. The money value remains one shared observation. A
mismatch or any lost prerequisite calls Sudeki's original exports unchanged or
closes the mod lease so the waiting script can resume.

This is deliberately not a forge implementation. The read adapter resolves
stable equipment/rune IDs, sockets, price, compatibility, projected stats, and
separate catalog/inventory/economy fingerprints, but `UIBlackSmithStart` still
supplies no merchant target identity. Confirm input only shows
`COMMIT DISABLED`; it never activates, clones, or commits through
`UILayerBlackSmith`. The remaining seam is target-specific pre-Start provenance
and live proof of the serialized native compare/revalidate/mutate/verify path.

## Interaction provenance tuple

Every request that could become authoritative must retain this immutable tuple:

```text
serial
player_index
actor
actor_generation
target
source_generation
kind
target_known
```

The tuple means:

- `serial` distinguishes a new press from an older request, even when all
  pointers happen to match.
- `player_index` identifies the human seat that made the request.
- `actor` and `actor_generation` identify the exact actor lease used at the
  source. A party rebuild, drop-out, reassignment, or load invalidates it.
- `target` identifies the exact usable, merchant, or other world object.
- `source_generation` identifies the world/interaction-source generation in
  which that target was resolved. It prevents a recycled pointer in a rebuilt
  zone from inheriting authority.
- `kind` selects the authority and presentation policy.
- `target_known` is a hard boundary. A generic attention request deliberately
  has no target and can never be promoted into a native action.

The coordinator moves through `IDLE -> REQUESTED -> ACTIVE`. A requested owner
whose lease changes is cancelled. An active owner whose lease changes becomes
`QUARANTINED` until the native modal is observed closed, because forgetting a
still-live global UI would be less safe than blocking a new request. Requests
expire after five seconds. Commit must compare the complete tuple again on the
game thread immediately before invoking native mutation.

The tuple is runtime-only. It must be cleared across process start, load, world
generation changes, party reconstruction, and any uncertain native-modal
lifecycle.

## Authority and presentation matrix

| Interaction kind | Authority | Current presentation | Commit rule |
| --- | --- | --- | --- |
| None / actor-local movement and combat | Local actor | Player viewport | Valid live actor lease and proven actor-specific native path. |
| Generic request | Request only | Research API only; no controller-created badge | Target is unknown and commit is forbidden. Ordinary controller input never creates one. |
| Shop | Serialized shared | Shared full width | One owner, known target/generation, then one atomic shared-inventory commit. |
| Blacksmith | Serialized shared | Default-off per-viewport read preview; native fallback is shared full width | One owner plus explicit merchant/character/equipment target; serialize shared money/material mutation. |
| Pickup | Serialized shared | World feedback | Revalidate target existence and inventory capacity; exactly one winner. |
| Chest | Serialized shared | World feedback | Revalidate closed/open state; exactly one winner. |
| Non-progression switch | Serialized shared | World feedback | Revalidate exact switch and world generation before dispatch. |
| Dialogue | Host only | Shared/native | Host dispatch until speaker, quest, script, and camera ownership are fully mapped. |
| Travel | Host only | Shared vote/transition UI | Consent may gate the host action; non-host never calls the late native transition path directly. |
| Quest | Host only | Shared/native | Global script and journal mutation. |
| Save | Host only | Shared/native | One global save payload and destructive load lifecycle. |
| Cutscene | Host only | Shared/native | Global scripts, cameras, control leases, and world time. |
| Unknown | Host only | None | Fail closed and instrument before classification. |

Shop and blacksmith presentation can become owner-viewport-local only after
their mutable UI state is independently virtualized. Until then the renderer
treats a known or uncertain shared modal as full-width and suppresses the P2
status and roaming-boundary overlays so split-only UI does not draw over it.

## Shared-inventory transaction rule

Money, item quantities, merchant stock, forge-slot bytes, and their save data
remain authoritative native shared state. Per-player work may shadow only
presentation/session state:

- owner player, actor lease, and explicit equipment character;
- merchant identity and catalog revision;
- inventory/augmentation and economy generations;
- stable item/service IDs rather than native entry pointers;
- menu page/mode, cursor, quantity, forge slot, preview, and confirmation;
- a read-only snapshot of price, affordability, capacity, and stock used to
  explain the pending choice.

A shadow is never authoritative. On confirm, one game-thread commit gate must:

1. Verify the complete interaction provenance tuple and modal ownership.
2. Re-resolve the merchant, item/service, target character, and forge slot by
   stable identity against the current world, catalog, inventory/augmentation,
   and economy generations.
3. Recheck stock, party quantity/capacity, price, money, and equipment
   compatibility from native state.
4. Execute the native effect sequence without yielding to another interaction
   commit.
5. Verify that both inventory/augmentation and economy generations advanced,
   then refresh every open shadow from native state. Catalog may legitimately
   remain unchanged after socketing. A partial or ambiguous result quarantines
   the session; it is never replayed blindly.

This allows multiple players to browse eventually, but not to race two native
confirm paths. The safe concurrency model is many read-only UI shadows and one
serialized shared mutation lane.

## Save/load and lifecycle hazards

The save path confirms that the economy is party-global. Money is packed at
save offset `+0x1E0`; item state begins at `+0x1E6`; inventory flags are packed
at `+0x16BA`. Two inventory equipment/forge byte regions are copied from
`CInventory+0x10` (length `0xA2`) to save `+0x1180`, and from
`CInventory+0xB2` (length `0x78`) to save `+0x1222`.

Consequences:

- No request serial, actor/target pointer, UI object, merchant-entry pointer,
  catalog pointer, or per-player shadow is persisted.
- Load is a generation boundary. Close/quarantine native modals, discard all
  shadows, increment actor/source/catalog generations, and reacquire seats from
  stable roster identity only after the new world is settled.
- Saving while a serialized commit is half-complete is forbidden. The commit
  lane and save/load lifecycle must be mutually exclusive.
- Per-character equipment is selected explicitly at commit, but the resulting
  shared inventory/equipment bytes are saved once with the party.

## Staged roadmap

### Stage 0 — targetless feedback experiment (retired)

- Stable P1/P2 actor leases remain part of the current coordinator.
- An earlier checkpoint converted controller X into a five-second targetless
  `GENERIC_REQUEST` and displayed `P2 INTERACT?` as attention feedback.
- That runtime path and badge are now removed. The generic coordinator API is
  retained for isolated research, but no controller action creates a targetless
  request or presents one as interaction success.

### Stage 1 — passive actor/target provenance, no authority

- The action router now reserves A for an exact interaction intent only when
  actor, actor generation, target, source generation, and kind are all known.
  With no exact tuple it resolves A to native Weak instead; with a tuple it
  currently reports intent only because native target dispatch is not connected.
- On the exact supported image, observe the native Select/OnAction source
  actor, at most 15 candidates, the native accepted-message path, and its
  same-thread SOL submission. Tie every observation to a nonzero zone source
  generation and invalidate it with that lifecycle.
- Keep a non-front/P2 candidate explicitly accepted-but-unvalidated and unable
  to authorize activation. The observer must not replay GUI Select, swap the
  global controller, bypass native eligibility, or invoke a world object.

### Stage 2 — authoritative P2 target validation

- Establish a native or safely equivalent P2 selection/eligibility seam that
  produces the same exact usable, actor, and source generation before any
  native side effect, without global controller swapping.
- Classify merchant, blacksmith, pickup, chest, benign switch, and host-only
  targets.
- Add target lifetime/generation checks and rejection logs. Unknown remains
  host-only.

### Stage 3 — one serialized owner session

- Permit either player's known shop/blacksmith request to acquire the sole
  session.
- Keep the native UI full-width and route its input only from the recorded
  owner.
- Make close, drop-out, transition, device loss, and load release or quarantine
  the owner deterministically.

This is the first practical independent-interaction milestone. It lets Ailish
open a merchant as P2 without pretending Tal pressed the button, while still
using Sudeki's one native shop/economy safely.

### Stage 4 — independent browsing, serialized commits

- Reverse and shadow every mutable shop/blacksmith UI field.
- Render an owner-viewport UI from stable IDs and catalog snapshots.
- Allow another player to continue roaming or browse a separate shadow, but
  serialize all shared inventory/economy confirmations through the commit gate.

### Stage 5 — actor-capable world interactions

- Extend the same provenance and winner rules to pickups, chests, and proven
  non-progression switches.
- Add actor-specific animations, reach/orientation validation, world feedback,
  and deterministic conflict handling.

### Stage 6 — host progression and consent

- Keep dialogue, quests, save/load, cutscenes, and travel host-owned.
- Add pre-action votes only at a reversible, target-specific seam; never veto
  after Sudeki has already started an opaque approach or transition script.
- Generalize the coordinator to P3/P4 and later network identities only after
  two-player lifecycle acceptance is stable.

## Acceptance invariants

- A generic research request can never be promoted into a world action, and
  ordinary controller input neither creates it nor lights a badge.
- A P2 action never changes the global lead to borrow P1 authority.
- At most one native shared modal and one shared-inventory commit exist at a
  time.
- No native pointer outlives its actor, world, catalog, or load generation.
- Drop-out never abandons a live global modal silently.
- A missing/uncertain modal detector fails closed to shared full-width
  presentation.
- UI failure cannot create an invisible gameplay constraint.
- Save/load never restores runtime interaction ownership.
