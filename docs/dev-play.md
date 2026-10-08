# Dev Play

Dev Play is an opt-in experimental lobby, separate from regular multiplayer.
The owner-selected design (2026-10-07) allows all players, including the host,
to pick Tal, Ailish, Elco, Buki or Talos; duplicate choices are allowed.
Regular multiplayer retains unique hero choices and the saved leader rules.

## Current boundary

The setting, mode selection, room indicators, native portrait picker and
version-12 lobby protocol are implemented candidates. Dev Play offers saved
games only. The labelled New Brightwater run with both players choosing Talos
has two independently identified native avatars in each process. The latest
native-HUD candidate is dirty `2854c47fb544`, DLL prefix `85a275e`. Both players'
input became ready; bounded movement was exercised independently on the earlier
`834a` candidate with the same party/control code and was reflected on the other
peer. Camera framing remains poor and readiness is intermittent under the
observed low client frame rate. Camera behavior is not accepted.

All five native portraits have loaded as 64-by-64 GPU textures and have been
visually inspected in the title picker, including
`SUI_PORTRAIT_BOSS_TALOSMERGED`. Native loader names are extensionless: its SQX
decoder appends `.sqx`. Each wrapper is explicitly owned and released, and no
extracted game pixels are distributed.

Duplicate hero choices preserve the first player's native assignment; extra
copies join as spectators with an explicit lobby note. Native hero cloning is
not enabled. A present nonleader host hero has a tested native-rotation startup
candidate; absent host heroes and a remote saved-leader choice under a Talos
host remain outside the current launch boundary.

The owner superseded the separate-card design: the current candidate reuses the
four native party HUD widgets for player portraits, names and HP/SP. The custom
stats-card draw path has been removed. Host-authoritative status records still
expire after 250 ms and carry scene, player, generation and sequence; native
widget presentation consumes those values without changing gameplay stats.
Exact-image tests cover native portrait ownership, bar/text ABI, stale rows and
retryable restoration. In the latest labelled pair, screenshots show two real
Talos portraits in the original bars, local-first player names, and 8000 HP /
999 SP on the main row. Read-only inspection confirms resident portraits and
hidden unused rows. Owner acceptance, changing values and four-player live
presentation remain open. See [the HUD contract](dev-play-native-hud-contract.md).

The initial all-Talos/None route also replaces the saved native party. The save
loader first constructs its saved heroes; the adapter waits for native avatar
completion, adds the actual local Talos as native leader and deletes the
unselected heroes through native group operations. It does not edit the save
or hide a borrowed hero. See [the native party contract](dev-play-native-party-contract.md).
Mixed selected heroes and story-script recruitment after replacement are not
covered by this initial adapter.

Both peers loaded on the first Start. The native input-filter request now waits
for its later native commit before changing the party. Live registry inspection
on the preceding `834a` candidate found only the two Talos actors and no canonical
hero actors; the latest candidate again reached the zero-hero party with its
actual local Talos as sole native member. The save loader still constructs saved
heroes briefly before this handoff; constructor suppression is not implemented.

Normal host End Session on `85a275e` returned both processes to the visible native
title, with verified native world exit, balanced client pause ownership and
completed runtime cleanup. Same-process re-entry is still pending. These results
do not establish campaign transitions, combat, mixed hero/Talos selections or
camera quality.

## Configuration

The SudekiMP Settings page persists the Dev Play toggle in the active sidecar:

```ini
[DevPlay]
Enabled=true
```

When disabled, Multiplayer opens the existing browser directly. When enabled,
it first offers Multiplayer and Dev Play. The existing Talos co-op tuning rows
remain available in the Talos Co-op Settings submenu.

Scripted tests can select the mode and character through the existing lobby
automation. Host/join, save, name, ready and start retain their normal checks:

```ini
[Lobby]
Mode=DevPlay
Character=Talos
```

`Mode=DevPlay` requires the Dev Play setting. The new lobby does not translate a
Talos choice into the former leader-borrowing `AllySeatPlayer`/`ClientAlly`
experiment. That older implementation remains research code, not the new
avatar control path.

## Protocol and launch

Lobby protocol version 12 carries the mode in STATE byte 225, HELLO byte 48
and discovery OFFER byte 52. Other reserved bytes remain zero and malformed
modes are rejected. Direct joins and discovery are isolated by mode. Character
5 is Talos; character 4 remains none. Talos, duplicate selections and independent
host selection are admitted only in Dev Play. Older protocol versions refuse
the new messages. Local launcher IPC version 4 carries the mode as well.

`SudekiMpLobbyLaunchProjectNative` projects a validated plan into the unique
native party: the first player selecting a hero receives that hero; later
copies and Talos remain outside this projection. Projection grants no spawn,
AI, camera or input authority. The lobby's original choices remain intact.

## Remaining work and acceptance

1. Obtain owner acceptance of the native HUD and improve camera framing, handoff
   delay and intermittent readiness on the labelled host/client pair.
2. Verify same-process re-entry after the completed-party exit, client melee and
   the supported camera-state transitions. Unknown camera modes retain ownership
   and close input; they do not authorize restoring through a foreign owner.
3. Exercise duplicate spectators, nonleader native-host startup and mixed
   selected heroes; these are not established by the all-Talos replacement run.
4. Verify changing HP/SP and four-player visibility live. Four-player transport
   and UI isolation have fixture coverage.
5. Resume native item/equip compatibility and compare Tal and Talos on the same
   target through the receiver chain, then verify target HP decreases. Damage
   research remains on hold during the HUD/party correction.
6. Implement initial-plan cases still excluded above and Dev Play late joins;
   ordinary hero admission does not admit newly configured Talos avatars.

Each gameplay claim requires the labelled host/client live acceptance and
owner confirmation described in the request. Compilation and protocol tests
do not establish those results. No Talos damage fix is established by the
lobby, spawn or native-HUD implementation.

The owner also asked to retain these alternatives for later consideration:
unique characters only; at most one Talos; Talos clients only with the host
remaining the save leader. They are not the selected design.

Native findings and the opt-in, observe-only damage probe are recorded in
[the native contract checkpoint](dev-play-native-research.md). In particular,
`FUN_004d92d0` is not the proposed item equip writer, and a null weapon item
can still use native fallback damage. Neither finding establishes live damage.
The subsequent [equip contract investigation](dev-play-talos-equip-contract.md)
records the observed zero fallback/attack inputs and the remaining native
item-model and handle-lifetime constraints.
