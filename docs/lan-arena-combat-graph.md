# LAN arena combat synchronization graph

This document is the cross-module edge that an AST-only call graph cannot
infer through serialized packets. It records the required proof chain for
every combat feature added to LAN arena mode.

## Proven Tal melee path

| Stage | Evidence / implementation |
| --- | --- |
| Physical input | Controller actions Weak `0x2C`, Strong `0x2D`, Sweep `0x2E` populate the native action records. |
| Actor authority | Controller consumer RVA `0x000286C0` resolves Tal's arbiter and calls per-arbiter submission RVA `0x000DB0E0`. |
| Native admission | RVA `0x000DAC00` admits attack kinds; combo dispatcher `0x000D0730` appends the accepted kind. |
| Authored transition | Lookup `0x000D04F0` searches the bounded history and gate `0x000D13E0` checks timing, target distance, and direction. Commit `0x000D14D0` records only an accepted result. |
| Host observation | `host_actor_native_action_variant()` reads the resulting Tal renderer selector and calls `SudekiMpLanArenaTalActionFromNativePresentation()`. It never manufactures an action from the input edge. |
| Wire identity | `host_track_actor_action_sequence()` appends the actor-local semantic result to the bounded action journal and LA15 carries the host-observed action phase plus canonical-simulation input acknowledgement; `SudekiMpLanArenaValidateSnapshot()` rejects invalid state/variant/phase pairs. |
| Client replay | The replica drains journal events in sequence, interpolates the current host phase, and `apply_actor_presentation()` calls `SudekiMpLanArenaTalActionToNativePresentation()` for Tal's independently leased renderer. No client damage or attack execution occurs. |

The semantic key must preserve the native result, not merely the input
history. In particular, `WSS` selected both renderer `69` and renderer `70`
under different native gates. Both are valid strong-finisher presentation
identities in protocol `LA15`.

## Reused co-op foundations

- Character control is acquired through the existing actor-pointer and
  generation leases; LAN code does not retarget Sudeki's global controller.
- Native per-arbiter submission, validator, targeting, animation, hit, and
  damage paths run once in the shared simulation, as they do in the proven
  local co-op path. Sudeki's world trigger—not a player role—authors the combat
  flag, and snapshots distribute that observed state to every client.
- `lan_arena_shared_simulation` distinguishes the canonical native-world node
  from player roles. It requires a session-token-exact native combat
  observation before committing a canonical frame; replicas can only accept
  authenticated newer frames. This is the migration seam for a later
  dedicated simulation process.
- The native-world observation also replaces both players' HP/SP and the
  complete bounded enemy set. Tal and Ailish presentation/transforms arrive as
  separate actor observations, and the reducer composes a fresh frame instead
  of trusting a monolithic candidate. Actor input and presentation adapters
  therefore cannot manufacture damage, healing, resource use, enemy identity,
  spawn/despawn, or incapacitation.
- Player input crosses a separate actor-scoped contribution boundary. Socket
  receipt alone is not acknowledged: the canonical reducer must validate and
  admit a monotonically newer Tal or Ailish input before its sequence can be
  projected into a snapshot. Contributions never carry combat, enemy, damage,
  resource, or match-state authority.
- Cleanup follows the established camera/control/AI reverse-release order.
- Renderer readiness is actor-local. Ailish's unavailable first-person/world
  graph cannot suppress Tal, and Tal readiness cannot authorize Ailish.

## Rules for future graph additions

Every new action needs all four connected proofs before it is enabled:

1. input or request source;
2. native host admission and observable accepted result;
3. bounded protocol identity plus authority validation;
4. actor-local client presentation or replicated consequence.

An input binding by itself is not an accepted action. An animation selector by
itself is not authority. A position or HP snapshot is not proof that the
matching hit, projectile, target, or combo branch was represented.

## Research queue

| Priority | Missing graph edge | Required evidence |
| --- | --- | --- |
| 1 | Tal combo candidate identity beyond three inputs | Enumerate the native six-entry history table and record accepted fourth-through-sixth transitions; rejected late inputs must remain absent from the journal. |
| 2 | Tal hit consequences | Trace accepted combo result to target choice, lunge/root motion, hit window, damage, stagger/knockback, and enemy animation; decide which consequences are snapshots versus explicit events. |
| 3 | Ailish ranged authority | Connect first-person aim, hold/repeat cadence, native projectile creation, ammunition/resource change, impact, and observer presentation. Strong input is not available in her ranged arbiter path. |
| 4 | Per-actor weapon/combat readiness | Replace remaining transition-wide assumptions with Tal-world, Ailish-world, and Ailish-first-person leases and explicit timeout/error states. |
| 5 | NPC/enemy replication | Identify stable actor identity, animation-bank identity, native action result, target, root motion, HP/status, and spawn/despawn generation. Do not generalize Tal selector numbers to NPC renderers. |
| 6 | Host-routed menus and skills | Follow each category from request through native validation, target/global arbitration, execution, resource effects, and remote presentation before enabling client confirmation. |

Repeatable evidence belongs in exact-hash Ghidra reports under
`tools/ghidra/`, pure translation/validation modules under `src/network/`,
and focused tests under `tests/`. Live-only observations must be recorded in
`docs/research-log.md` with the exact protocol/build and fail-closed behavior.
