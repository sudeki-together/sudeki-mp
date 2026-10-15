# Codex brief for #54 saved-story co-op — handoff 2026-10-15

This is the owner's handoff to the executing agent (Codex). It summarises what
is accepted, what is open, and the next step (D6 loot rule). The binding
plan is `docs/coop-story-plan-54.md` (decisions §1, phases §3–§7, progress
log §9, remaining work §10). Read `AGENTS.md` first; evidence words follow
`docs/evidence-index.md`. Addresses below are RVAs on the supported image
(VA = RVA + 0x400000).

## Working rules (unchanged)

- Branch `codex/shared-simulation`; HEAD is `8f7ec42` (clean tree). Commit
  identity `heywander <127889726+heywander@users.noreply.github.com>`, one
  commit per completed step with the issue number, **no push unless the owner asks**.
- One adapter per native seam, exact-byte guards before every patch, transactional
  install / reverse rollback, native objects only on the game thread.
- Never mark a step working from compilation or hook install. Live proof is an
  **owner gate**: build, run the Wine suites, write the exact procedure into §9, stop
  and ask the owner. Record `CONFIRMED_LIVE` only with the owner's words.
- Build: `flatpak run --command=sh --filesystem=host org.freedesktop.Sdk//25.08 -c
  'export PATH=/usr/lib/sdk/mingw-w64/bin:$PATH; cmake --build build/mingw32 --target <t> -j6'`.
  Tests run under GE-Proton Wine; image tests take `Z:\...\SUDEKI.exe`.
- Stage scripts, logs and the live pair are private (`.agent-local/`, Git-ignored).
  Never copy their contents into tracked files. After a code change run
  `graphify update .`.
- Always check that the staged candidate hash changed before asking the owner to test.
  Logs append across launches: rotate `runtime.log` on every relaunch.

## Accepted live (owner, Ailish host / Tal client, castle road ↔ New Brightwater)

Commits `ff94f5a`, `4ed4f14` (merged to main), `19931be` (README), `8f7ec42`:

- Phases A, B, C and D1–D5: remote control parity, host-authored spawns, drops
  on every screen, damage/death presentation, XP popups, house travel, zone follow.
- Doors and chests follow the host (#66, #67), host door camera, client native
  Exploration camera (#71), chest glow and door effects whitelisted.
- Client Main Menu scoped to the owned character (#75): cycle/open/equip/use seams
  (`src/hooks/lan_story_main_menu.c`, 12 guarded seams, image suite), M/Esc/F3
  polled while the menu owns the frame, menu pauses skipped on the client.
- Weapon equip relay (EQUIP_WEAPON request; host grants a missing weapon once and
  equips through the menu's own core). Armour is **not** relayed (#40, #74).
- Item use (#75, #76): Items confirm → framed yes/no prompt (D10, self only) →
  USE_ITEM request; Q-menu items (category 5) send the same request; the host runs
  the native can-use (0x12E1E0) and then the character's own use action
  (0xDC110: arbiter, definition, party slot; RET 0xC). Animation, keyframe apply
  (0x58A480) and the SFXM005_HEAL effect are native and reach the client.
  Remote move path holds the body with no native submission while arbiter 0x20 is set.

## Open (issues on Gitea `sudeki-together/sudeki-mp`)

- **D6 loot rule — next (see below).**
- #73 void fall when the host streams the area out; #70 departure hold; #69 slope
  slide; #71 camera walls; #72 host→joiner save transfer; #40/#74 armour relay.
- Client menu: weapon 3D preview stale after a relayed equip; F-keys unreachable.
- Phases E (missiles, Mungo), F (spectating, cluster follow), G (lead vs ownership),
  H1 (remote door/chest/talk/shop use), I (saves), J (new game): not started.
- Bosses, full campaign, two-machine and native-Windows runs: untested.

## Next step: D6 loot rule (owner decision D9)

**Rule.** Items picked up from the ground, from enemies and from chests are cloned
to every player's inventory. Items a player buys in a shop stay with that player.
Nothing else is shared. (Open owner question: whether shop purchases should also
spend a per-player wallet now, using the existing `personal_wallet`, or later.)

**Design (host-authoritative, no new trust).**

1. *Static first.* Find the host pickup paths that end in `CInventory::AddItem`
   (0x217E0, thiscall(inventory, id, qty, show); exact entry guard already in
   `SudekiMpGrantCharacterWeaponItem`, `src/engine/weapon_activation_abi.c`):
   dropped-item pickup (DroppedItem, vtable 0x2D3E68, definition at +0x44 ↔
   `world_drop()` in `src/hooks/lan_story_world_visuals.h`), chest treasure
   (Treasure component state machine 0x1849D0 / use entry 0x184EC0), enemy drop
   collection, and the merchant purchase path (0x489DF0 region: ReduceItem +
   AddMoney). Record each call site, its source and its argument contract with
   the graph-findings workflow. Do **not** hook AddItem itself (shops share it).
2. *Host.* Observe the loot call sites only (relative-call hooks with exact bytes,
   one adapter) and emit a **grant** record {serial, source, item id, quantity}
   into the existing story presentation stream, with the same fence/serial
   discipline as USE_ITEM results so a lost or repeated packet never double-grants.
   Florins picked up are loot (I315_MONEY / I320_MONEYBAG, already whitelisted as
   drop models).
3. *Client.* On a new grant serial, run AddItem locally through the same exact-image
   guard, log `story_loot event=granted …`, and refresh the Items page if it is the
   active UI (`SudekiMpLanStoryMainMenuRefreshItems`). Never grant from a shop
   source. Keep the client's own ReduceItem on accepted USE_ITEM as is.
4. *Pure tests.* Grant codec + dedupe (transport suite pattern in
   `tests/story_action_transport_test.c`); source classification table.
   *Image test.* Every hooked loot site guarded like `tests/story_main_menu_image_test.c`.
5. *Owner gates.* (a) Ailish picks up an ointment → Tal's count rises on his screen
   without a menu reopen; (b) a chest item appears once in both inventories;
   (c) Tal buys an ointment → Ailish's count does not move; (d) florins from a
   drop appear for both. Use `[StoryAreas] TestStartLeaderHealth=0.5` /
   `TestStartFollowerHealth=0.5` to start hurt when a heal gate is needed.

**Pitfalls seen this week.** A client whose runtime log stops after `status=ok`
with ~1.5 % CPU is waiting in winex11 `set_focus → wait_grab_pointer`; a real click
at its window centre releases it (the placement script now retries). Two inline-asm
blocks must not share an input register across a native call (host crash
2026-10-15). `&&` chains stop before later edits when an early grep fails.
