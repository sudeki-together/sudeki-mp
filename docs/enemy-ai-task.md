# Task brief: co-op enemy targeting (issue #97), saved-story co-op #54

Branch `codex/shared-simulation` (worktree `shared-simulation`, base fa50351;
the step-away stage 1.1 brief may land first; coordinate on
`lan_story_runtime.c`). Read `AGENTS.md` first: exact-image guards, one adapter
per native seam, transport threads never touch native objects, lifetime and
teardown rules in `runtime-lifecycle.md`, evidence vocabulary in
`evidence-index.md`, never write "works" for something that only compiled or
passed a pure test. Enemy AI runs only on the host; nothing here needs a
network change (clients already receive monster forward/clip/missile state,
INFERENCE until the live gate). Single player and the LAN arena stay vanilla:
every rule below is active only on the saved-story host while co-op is active.
"Co-op active" starts with the runtime's own `remote` computation in
`service_story_controls`: a peer OBSERVING with confirmed transport that
drives a character and is not away, or still holds its native key token.
Owner decision 3 extends enemy-policy admission to a connected, reserved away
seat after its native input lease has fully drained. The runtime proves that
ownership and connection through `SudekiMpLanStoryRuntimeAwaySeatExact`;
unknown, disconnected or transitioning seats do not extend admission. The
dead-lead hold retains its original `remote` computation.

## What the game does (CONFIRMED_STATIC unless marked; research 2026-10-19)

- Each AI unit re-picks its target several times a second in its state tick:
  selector 0x5B76C0 (stdcall: request*, out CAiUnit**, self CAiUnit*, float
  maxRange; prologue 55 8B EC 83 E4 F8 83 EC 5C 53 56; it returns its `out`
  argument in EAX; 24 state callers, including the PC companion request
  builders 0x5AE820/0x5ADF60/0x5AE000 and ally states) builds a filtered
  candidate list (filter 0x5B7120, stdcall RET 0x10: request, self, float
  range, candidate; every AI unit in the manager 0x809DE4; max 32 entries in
  the global array 0x7D5540 / count 0x7C30AC, entry dword = CAiUnit+4), then
  dispatches on the request's TargetMode nibble (request+0x24 & 0xF): 0
  Nearest, 1 Furthest, 2 Weakest, 3 Strongest, 4/5 Threat (static authored
  unit+0x14C), 6/7 Level, 8 Random, 9/10 Constraint, 0xB Lead PC (0x5B6B10,
  party slot 0 only; call at 0x5B791B), 0xC Leader's Target (0x5B6C70; for
  monsters it degenerates to Nearest because they have no formation link, and
  it still passes the follower's own filter), 0xE internal lock (written by the
  six Aware hit paths and unconditionally by AwareForceIn 0x5AA7D0). For mode
  0xE the selector returns from its prologue 0x5B76CE–0x5B76FF after
  re-validating self+0xC0 through 0x5B7120, **without rebuilding
  0x7D5540/0x7C30AC** (they still hold the previous unit's list); once that
  validation fails the unit gets NULL, +0xC0 and +0xF4 empty, the state ends
  on its done flag and Idle Enter/Exit 0x5A8A60/0x5A8B40 restore the authored
  mode within a few ticks; only an authored state timeout/cooldown or a boss
  state holds it longer. The pick is committed to unit+0xC0 (0x4EA7C0, which
  also keeps the attacker nibbles, the CTargeter entity+0xAC +0x54 mirror and
  the geometry block consistent); the nearest valid unit (0x5B7210, caps
  ignored, NULL in mode 0xE) is stored as the aware target unit+0xF4 by
  0x4EA940, which also mirrors it into CTargeter+0x54 while +0xC0 is empty.
  Combat needs +0xC0; Aware needs +0xF4 and no +0xC0; Idle needs neither.
- Attacker caps (0x5B7120): a party-side candidate that is AI-overridden
  (unit+0x16A > 0) or AI-driven gets melee and ranged caps forced to 1 (the two
  `66 83 B9 6A 01 00 00 00` = `CMP word [ECX+0x16A],0` instructions at
  0x5B7169 and 0x5B71B7, each followed by JG and the (ai+0x3C)+0xB check); the
  host's character keeps its authored caps. SudekiMP leases a client's body
  with AiOverrideControl (+0x16A == 1 under a held lease), so clients are
  capped like companions. Attacker counters are nibbles at unit+0x46/+0x47
  (melee = +0x47 & 0xF, ranged = (+0x46 >> 12) & 0xF; 0x4EA3C0 increments and
  wraps at 16; 0x4EA410 skips the decrement at 0). The MonsterEntity
  destructor path 0x53A1B0 unlinks +0xC0/+0xF4 without 0x4EA7C0, so a despawn
  without death leaves the victim's nibble incremented (native; nothing here
  may write the nibbles, so it is measured, not fixed).
- No aggro table, no damage memory. A hit-locked Aware enemy locks onto the
  nearest PC, not its attacker.
- There is no native monster group or leader link (CAiUnit+0x40 is the PC
  formation back-pointer only; spawn groups do not back-link from units). The
  module must own its group notion.
- Every damaging hit on a monster, ally, boss or party member passes through
  the HP receiver 0x4D21D0 (thiscall: ECX = victim CCombat, vtable RVA
  0x2C8754, +0x10 = victim entity; [ESP+4] = DamageStructure*, 0x68 bytes,
  vtable RVA 0x2D9148; prologue 55 8B EC 83 E4 F8). Its reference graph is
  closed: callers 0x4DAB91 (dispatcher 0x4DAB50, itself called from HitTarget
  0x4D1B00, reflect 0x4D3BA0, status tick 0x4BFA10, CCollisionDamage 0x538870),
  0x4D3B61 (DoDirectDamage, source == victim) and 0x4D3EDA (kill helper
  0x4D3EA0, which builds its own 1e7-damage packet with source = its killer
  argument; it is reached only from the NULL-packet branch of 0x5045A0, i.e.
  the Kill*/script routines, and that routine substitutes party slot 0 for a
  NULL killer). Spirit Strikes also go through 0x5045A0 (StartSpiritStrikeAttack
  0x4CC080 passes the hit-all flag to 0x4CC150, call sites 0x4CC3B7/0x4CC3C2)
  but as ordinary hit-all packets via HitTarget 0x4D1B00 → 0x4DAB50 → 0x4DAB91,
  with packet+0x30 = the casting character, delivered to every active monster
  and boss at any range. Packet+0x30 is the source entity as an intrusive TPtr
  (set via 0x401750); the attack-area path 0x4CC150 stores the attacking
  character, and missile init 0x586E10 re-sets it to the firing entity at
  0x58729A–0x5872AB, so missile hits carry their owner (resolves research gap
  G7). Amount = stats (+0x4C, vtable 0x2CC064) +0x2C before minus after
  (packet+0x14 is pre-mitigation binary16 and rewritten on overkill). Hit
  result byte +0x61 (1 blocked, 2 normal, 3 critical, 4 none), attack class
  +0x64 (3 seen once live for a Spirit Strike, INFERENCE as a discriminator),
  flags +0x67: opaque classifier inputs. The dispatcher's return address cannot
  tell contact, hit-all and suicide apart: classify by packet fields and
  source.
- Live (69 host logs, Ailish host / Tal client): enemies pick the host 78%,
  the client 22%; with the client nearer still the host 59%; with the host
  nearer the client 6%. The Nearest-mode types (Shooter, Grotesque, Venom
  Gnat, Scarecrow) make up most of "host picked while client nearer": the cap,
  not the lead rule. Host down and held in slot 0: no decision in 86.8% of
  samples (41.8% with both up); only 44 of 225 enemies ever switched to the
  client, median 7 s. Lead-PC types on the route (resource parse; per-type
  mode is not live-proven): Kurian Trooper and Levy (2/3 roll) and
  MON_Grotesque_Trooper (1/2; the live MON_Grotesque is authored Nearest).
  Lead-PC-only types elsewhere: Wyrd, Elite Illumina Archer, RoboArchanite,
  RobotGuard Z3000, ShadowOrb, FarexLore. Behemoth's boss attacks hard-target
  slot 0 (0x5A75D4, 0x5A7C24, 0x5A85B4; out of scope). No 0x4D21D0 observer
  has ever run on a story host (0 lines in 65 logs).
- World unit scale: about 1 unit = 1 m (INFERENCE from run speed 6.4 units/s,
  eye offset 1.55, melee offsets 1.5–3, formation snap 30). Confirm in the
  watch build before tuning distances; keep every distance in the ini table.

## Design thesis

All tiers reduce to one thing: **substituting the selector's result** for an
enemy, from a host-side pure policy, at one seam (the entry of 0x5B76C0), plus
**feeds** that the policy consumes (hits, shots, positions, engagement). The
state machine then does the rest natively: a substituted target makes an Aware
enemy go to Combat on its own; an empty policy answer leaves native behaviour.
No writes to unit+0xC0, +0xF4, +0x46/+0x47 or the request mode nibble (+0xBC)
at all, from any thread or seam; the only sanctioned native write is replacing
`*out` inside the 0x5B76C0 detour so the calling state commits it through
0x4EA7C0. Never set the lock nibble 0xE; never use TargetMode 0xC or a fake
formation link at +0x40.

**Scope of "enemy".** The policy answers only when `self` is a monster,
leader monster or boss (CAiUnit+0x148 == 1, 2 or 3) and the candidate is a
party character (type 5), never an ally or NPC (types 4 and 6, including
ALLY_Talos and the gate guards). For every other `self` (PC companions, NPCs,
allies) the enemy policy returns the native result untouched. Accepted decision 9
adds a distinct Defend companion branch in this same adapter; the current
candidate implements it without broadening enemy-tier admission. **Modes.** Enemy
substitutions use only current request modes Nearest (0) and Lead PC (0xB).
Furthest, Weakest, Strongest, Random, Constraint, Threat, Level and all other
modes retain their native results. Mode 0xE is never substituted, even with an
empty native result; its early return does not rebuild the global list, which
must not be read there. A live-designated unit calling in 0xE increments
`lock_blocked` only if the native lock is not already on its designated seat.
Below, "the list" means this call's 0x7D5540/0x7C30AC list for modes 0/0xB;
every replacement also passes a direct 0x5B7120 recheck under the same scope.
**Per-type exclusion.** Units whose entity name (entity+0x38, as
`lan_story_world.c`'s `entity_name` reads it) or lease resource matches
`[StoryAreas] EnemyTargetingExcludeTypes` (ini list, default
`MON_GuardSparPartner_Melee`, the weapon-training sparring partner, #80) never
get a substitution from any tier; the watch build logs the type of every
Lead-PC unit so the owner can extend the list.

Modules: `src/hooks/story_enemy_targeting.c/.h` (host-only adapter: seams,
feeds, per-tick service) and `src/network/story_threat.c/.h` (pure policy:
plain data in and out, callers pass `now`). The adapter owns exactly: the
0x5B76C0 entry detour, the two +0x16A compare instructions inside 0x5B7120,
and a consumer slot on the shared 0x4D21D0 observer. It never patches 0x5B6EC0
(two Talos modules own inline hooks there), 0x5B791B, the 0x5B7120 call sites
0x5B76F8/0x5B779E, the Idle request writers 0x5A8A60/0x5A8B40, the evict
routine 0x5B73B0, the death-hold sites or the Aware evict paths. Transactional
install with exact-byte and rel32 checks, reverse rollback, refuse on any
other image.

### Pure policy (`story_threat`)

- **Identity.** Enemies are keyed by (host epoch, spawn serial) from the
  spawn-event lease, re-resolved each tick through the registry and the
  GateWeak-exact actor, never by raw pointer across ticks; the lease
  `resource` hash and the entity name are carried for logs and per-type rules;
  party members by character 0..3. Records retire on the lease's `departed`
  rule, on lease clear, on epoch or area change, and when the manager no
  longer lists the unit; an unreadable registry is unknown, not absence.
- **Seat kinds** (computed once per native frame from sources the runtime
  already proves; precedence, first match wins): DOWN/ABSENT (DOWN =
  arbiter+0x50 & 0x10000 or ai+0x44 & 0x58; ABSENT = scene inside_mask bit
  set, in a shop, parked outside a story house, or not in the lead's area),
  then AWAY (ownership away bit set), then AI (native companion: ai+0x16A == 0
  and mode+0xB == 1, or ownership control AI, or a connected human seat whose
  last packet receipt is older than `SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS`,
  or that is draining or without a proven native control lease), then
  HUMAN_REMOTE (peer OBSERVING with confirmed transport,
  `host_control[p].ready && !draining`, a proven native control lease,
  ownership control HUMAN, away bit clear, fresh `heard_at` packet receipt;
  sentinel 1 encodes receipt 0 and stays stale), then HUMAN_HOST (c ==
  leader_character). Host-side native busy-body/steering holds do not demote
  a human seat. The client movement heartbeat is sent every client frame;
  `heard_at` stays fresh during those holds. The observer's background-load
  hold, including its 1500 ms grace, stops packet intake, so a seat can become
  stale there; that pre-existing behavior is unchanged.
  HUMAN_* are active players. Owner decision 3 additionally keeps an AWAY
  player's body under normal player attacker pressure while native AI drives it;
  stepping away must not introduce reduced attacker caps. Ordinary unassigned
  AI companions retain vanilla treatment. DOWN/ABSENT is never chosen or boosted
  by the policy. The current tier 0 candidate includes the AWAY extension;
  its live behavior remains unverified.
- **Tier 0a, cap parity.** A proven HUMAN_REMOTE or connected AWAY player's
  body takes the authored-cap branch, bypassing both the native override-count
  and AI-mode reductions. Ordinary unassigned AI companions and ABSENT seats
  receive no boost. The runtime keeps enemy-policy admission active for an
  exactly proven away reservation after the input lease drains. The host's
  Esc menu is not an AwaySeatExact refusal; CompanionSeatExact retains its own
  menu guard for Defend-anchor reset. Named `away_refused` diagnostics report
  other failing proof clauses, once per reason change per seat, at most 64
  per namespace. The menu-open/closed live rerun remains required. Native
  non-cap filters, authored limits, existing-target discounts and counter math
  remain intact. Cap parity is a prerequisite for tiers 1, 1.5 and 2: a
  substituted player body must still pass the native caps.
- **Tier 0b, lead rule.** When the enemy's mode is Lead PC and the native result
  is empty or is the host while a nearer player exists: the nearest standing
  player (HUMAN_*) in this tick's native list, with stickiness: keep the
  current target (self+0xC0, read at the seam) unless
  `d_current − d_best > max(lead_margin_fraction × d_current,
  lead_margin_units)` (defaults 0.20 and 2 units). Fallback when no standing
  player is in the list: the nearest AI/AWAY body in the list (owner decision
  8), then the native result. Owner decision 2 accepts `always`: this baseline
  applies while the host is standing too, with the documented stickiness and
  exclusions. It does not prevent the higher tiers below from choosing a more
  distant player because of threat or an active ranged-attacker alert.
- **Tier 1, threat memory.** Per enemy, per character, `threat` accumulates
  applied damage divided by victim max HP (stats +0x30), including hit-all
  damage. It decays exponentially with `threat_half_life` (default 4 s) and
  scores as `decayed_threat / (1 + d/d0)` (`d0=8`). All timestamps use uint32
  GetTickCount milliseconds; seconds-valued settings are converted to ms,
  negative signed elapsed time clamps to zero and deadlines are wrap-safe.
  A pick requires a distance-weighted score strictly above `threat_floor`
  (0.05): at distance zero this means 5% of max HP; at `d=d0` it means 10%.
  Only native-list HUMAN_HOST/HUMAN_REMOTE seats qualify (decision 10).
  Keep the current target only if it is above the floor and no other score
  exceeds it by more than `threat_margin` (25%); otherwise choose the best.
  AI/AWAY hits still accrue threat, but higher tiers never pick those bodies.
  They remain reachable through tier 0b's decision-8 fallback.
- **Tier 1.5, group alert.** A human shooter's ranged hit (missile match or
  arbiter ranged mode, known not hit-all) adds `damage / victim max HP` to
  the victim's decaying `sniper` memory only while FIRING, UNENGAGED and FAR.
  FIRING includes the firing signal or a recent shot (`fire_recent=1` s).
  UNENGAGED requires zero melee-partition targeters, known positions, and
  the nearest targeter from either partition beyond `melee_reach=3`.
  FAR uses `distance > far_distance=12`, not the journal's melee-distance
  `is_far` bit. Above `alert_threshold=0.10`, an admitted tier-1.5 enemy may
  alert on that shooter. Decision 11 permits one active alert per shooter
  per live/shadow set, including orphan responder copies; a refused second
  raise leaves the refusing enemy's cooldown untouched.

  Candidates are leased, live enemies within `alert_radius=25` of the origin;
  the alerting enemy is an ordinary candidate at distance zero. Prefer equal
  non-zero spawn groups, then nearest within that preference. Fill ranged
  first (`alert_ranged=2`), then melee (`alert_melee=1`), clamping the total
  to three and the output capacity even though ini parsing accepts 0–32
  for each key. Ranged capability uses only AI+0x46 bit 0, with no state-vtable
  scan. No stealing: a candidate's current target's raw decayed threat must
  not exceed the origin's raw threat for the shooter; ties qualify, including
  zero versus zero. No target or already targeting this shooter passes zero.
  Excluded, designated, non-substitutable or freshly known unlisted units
  are skipped; unknown list membership is admitted and logged. Each unit
  belongs to at most one alert or push within each set.

  The shooter is reconsidered on every mode-0/0xB call during `alert_hold=6`
  seconds. Copies carry the origin serial and raise sequence; an old alert
  cannot release a copy recruited by a newer alert. A post-raise list lacking
  the shooter or a failed direct recheck releases that responder. End reasons
  are checked in order: level/co-op loss, DOWN/ABSENT, no firing or attributed
  hit for more than `fade_seconds=3`, any remaining responder within
  `close_distance=6`, no belonging responders, then hold expiry. AI/AWAY
  shooters are temporarily ineligible without an immediate forced fade.
  On end, the origin starts `alert_cooldown=10` seconds. Origin retirement
  leaves other copies as orphans; they fade independently and the last logs
  the alert's end. Namespace/level changes clear all designations together.
  Any attributed hit by the shooter refreshes every matching alert/copy,
  even when its victim is unrelated. No audible bark is added.
- **Tier 2, pressure on idle players.** Track each seat's uninterrupted time
  with known zero live targeters, regardless of whether it is firing, even
  during a push or cooldown. Engagement, an incomplete/non-admitted frame or
  a service gap over 250 ms discards idle time. After `idle_seconds=5`, a
  firing HUMAN_HOST/HUMAN_REMOTE seat may receive one push. Choose the nearest
  leased, live Aware enemy within `idle_radius=20`: no current target, a
  non-empty aware target, admitted type, not excluded/designated and last
  observed mode 0/0xB (or never observed). Reapply the seat on every eligible
  selector call for `push_hold=8` seconds. End on level/co-op loss, DOWN/ABSENT,
  missing pushed identity, failed direct recheck, post-start list exclusion,
  `engaged_by_other > 0`, or expiry. The pushed enemy's own engagement does
  not end its hold; an unleased second engager does. Cooldown starts at push
  creation and lasts `push_cooldown=15` seconds. No candidate means no push
  or cooldown. AwareForceIn weights and the lock nibble remain native.
- **Tier 3:** decided against (owner decision 6); no scaling or switch.
- **Switches and table.** `[StoryAreas]
  EnemyTargeting=off|watch|tier0|tier1|tier15|tier2` is cumulative, with default
  `watch`. All listed gameplay tiers are implemented in the dirty candidate;
  each still needs its ordered owner live gate. `EnemyTargetingCapParity=on|off`
  defaults on inside gameplay tiers; watch retains native caps.
  `EnemyTargetingLeadRule=always|host_down` defaults to accepted `always`;
  `EnemyTargetingExcludeTypes` retains the sparring-partner default.
  Higher tiers require parity: with it off, effective level clamps to tier 0
  and logs `level_clamped` once per installation. `event=level effective=...`
  replaces the retired `owner_gate` record. Effective-level changes clear
  alert/push state with `reason=level`.

  Memory is shared; live and hypothetical tier-2 shadow alert/push state are
  independent. Shadow evaluation applies the same parity clamp, never makes
  a direct filter call and never writes `*out`. Per-resource/mode selection
  records expose `would_lead`, `would_threat`, `would_alert`, `would_push`,
  comparing shadow picks with final output. At watch these use the native-cap
  list; compare counters at tier 0 before higher-tier live gates. New event
  vocabulary is `alert`, `alert_end`, `alert_retired`, `responder_released`,
  `push`, `push_end` (64 each per namespace, live and shadow combined;
  shadow records carry `watch=1`). `threat_substitution rule=threat|alert|push`
  logs proposed target changes at most once per second and 128 per namespace.
  Status includes substitutions, alerts, empty recruit searches, pushes,
  blocked native locks and unslotted monster/boss hits. Every named threshold
  uses `EnemyTargeting` + CamelCase (e.g. `EnemyTargetingThreatHalfLife=4`),
  with defaults above, read at install and on a one-second timer.

### Feeds (host adapter, game thread, after the runtime's exact roster proof)

- **Hits** (victim, attacker seat, amount, lethal, far/near, ranged, hit-all,
  class bytes): a consumer on the shared 0x4D21D0 observer. Generalize the
  existing inline hook in `src/hooks/lan_arena_hit_feedback.c` into a
  publish-to-consumers observer (the arena/party feedback and this journal
  subscribe); the story runtime installs the consumer on the host seat only
  (never on clients, whose receiver calls are HP replays with source ==
  victim); `talos_defense_trace.c` stays the opt-in exception dllmain already
  excludes. The generalized observer must take its game-thread binding from
  the story runtime's verified native callback (`runtime_thread`,
  `lan_story_runtime.c` ~3371), not from `SudekiMpLanHitHostSnapshot`, which
  the story runtime never calls; refuse consumer registration until the
  thread is bound. The module's popup call hook 0x12969E comes with the
  install (read-only amount/colour capture for the arena); the story journal
  uses the stats delta and ignores it. Victim identity: prove the CCombat
  vtable and back-pointer, classify by entity vtable (monster 0x2D5FA0, ally
  0x2D55D4, boss 0x2D69EC, party = roster actor), keep the spawn identity,
  never a raw pointer across frames. Attacker: packet+0x30; source == victim
  means unattributable; a roster actor means that seat (Spirit Strike packets
  carry the caster there like any other attack-area hit). Hit-all tag: a new
  read-only per-seat query in `lan_story_cast` ("Spirit Strike in flight for
  character c"); log +0x64 alongside it in watch mode to confirm a packet-side
  discriminator before relying on it. Far/near: XZ distance between source
  and victim positions against `melee_reach` (default 3). Ranged: the attacker
  has a missile the projectile lifetime module currently observes
  (`SudekiMpLanPartyProjectileLifetimeObserve` entries; missile+0x3C =
  MissileData, a new read) whose MissileData matches packet+0x3C in either
  form (packet+0x3C == MD+0x58 as an address when MD+0x54 bit 31 is set, else
  packet+0x3C == *(uint32*)(MD+0x58)), or the attacker arbiter +0x58 bits 4–7
  == 2 (ranged mode; the nibble `control_separation.c` ~8469 already uses;
  `structures.md` marks +0x58 provisional). Hit-all packets never count as
  far/ranged. In watch mode log all three classifications and the hits that
  were FAR but not ranged. Log `GetAiDamageMultiplier` 0x4D17C0 / [0x73AFF0]
  once at install.
- **Shots:** the host shot journal (`lan_story_shots.c`: new read-only
  accessor for journal[c].latest sequence/host_tick, game thread only) for
  party ranged fire; projectile ownership by
  `SudekiMpLanPartyProjectileTag.character`.
- **Firing signal:** HUMAN_HOST: arbiter+0x50 & 0x00400000 (593 of 599 live
  host shots preceded by it). HUMAN_REMOTE: the latest accepted input's
  `ranged_first_person_active` plus weak attack, fresh within the input max
  age (the native bit is unproven on a leased body); plus the journal advance.
- **Positions, ranges, enemies:** enumerate CAiManager 0x809DE4 (count +0xE8,
  12-byte entries at +0xF0, entry[0] = CAiUnit+4), skipping +0x3B != 0 and
  +0x44 & (0x58|0x10|0x1000), keeping types 1–3 (+0x148). Position =
  *(*(unit+0x10)+0x74) +0x44/+0x4C; zone +0x3C; sector +0x40; range +0x4C;
  current target +0xC0 (dword − 4), aware target +0xF4.
- **Engagement:** count every live monster/boss whose +0xC0 names the seat,
  including unleased enemies and both partitions. AI+0x46 bit 0 distinguishes
  ranged versus melee; +0xF2 bit 0 and +0x44 & 0x480 are logged separately as
  in-melee and attacking. Nearest distance includes either partition. A bad
  targeter position still counts as engagement and marks that seat's position
  unknown, as does a bad seat position. The attack nibbles are diagnostic;
  counts above the live walk emit `stale_count`. `unleased`, `position_unknown`
  and `table_complete` make observation limits visible. Manager/lease failure
  or table overflow blocks new alerts/pushes and table-dependent fades/picks.
- **Groups:** read-only spawn-lease observations supply serial, group id/name,
  resource and call-local actor identity. Same-group preference requires both
  ids non-zero and equal. Only serials/plain data survive a frame. Unleased
  units contribute engagement, but cannot be designated; no zone/XZ clustering
  fallback is implemented.

## Native seams

ABI correction (CONFIRMED_EXACT_IMAGE, watch implementation 2026-10-19):
selector 0x5B76C0 uses stdcall, correcting this brief's original cdecl label.
All three exits use `RET 0x10`; see [watch evidence](testing.md#native-contract-correction).

- **S1 Selector entry detour at 0x5B76C0** (stdcall, 4 args; prologue
  55 8B EC 83 E4 F8, the same relocatable 6-byte prefix shape as the 0x4D21D0
  hook; validate the prologue bytes and that the 24 callers are relative
  calls). The detour calls the original (and returns its return value, the
  `out` argument, unchanged), then asks the policy for this unit (self
  identity and type, mode nibble, the native result). For modes 0 and 0xB the
  policy chooses only from this tick's list 0x7D5540/0x7C30AC (built by
  0x5B7120 inside this call: membership plus a direct 0x5B7120 recheck
  under the same selector scope is the validity proof). A mode-0xE request
  returns natively without rebuilding the list: never read that global list
  or substitute in mode 0xE, including an empty native result. Track only
  last-observed mode and a blocked live designation. A substitution
  replaces `*out` (the raw CAiUnit*, as the native resolvers store it; 0x5B7995
  then calls 0x4EE760 whose first instruction is `MOV EAX,[EAX]` on the out
  slot). The native state commits through 0x4EA7C0, so counters and mirrors
  stay consistent; teardown is hook removal (the next tick re-selects
  natively).
- **S2 Cap parity inside 0x5B7120:** own the two `CMP word [ECX+0x16A],0`
  instructions at 0x5B7169 and 0x5B71B7 the way `src/hooks/story_player_turn.c`
  owns its `CMP word [EAX+0x16A],0` (same pattern, different base register:
  modrm B9/ECX here, B8/EAX there). Validate the following JG and
  (ai+0x3C)+0xB checks as part of the exact filter body. An unapproved path
  replays `CMP word [ECX+0x16A],0`; an exactly proven HUMAN_REMOTE or AWAY
  player body resumes at the authored-cap branch (0x5B7183 / 0x5B71D1),
  bypassing both reductions. Native cap math stays untouched. Keep both
  continuation sets alive after any failed restoration.
- **S3 Shared hit observer at 0x4D21D0** (see Feeds); one inline hook owner,
  consumers registered on the game thread; the story runtime installs the
  consumer on the host seat and removes it at teardown; no native write.
- **Not used:** 0x5B791B (covered by S1), TargetMode 0xC, a +0x40 formation
  stub, 0x5B6EC0, the 0x5B7120 call sites 0x5B76F8/0x5B779E (S2 patches
  inside 0x5B7120 instead), the Idle request writers 0x5A8A60/0x5A8B40, the
  evict routine 0x5B73B0, the lock nibble, any write to
  +0xC0/+0xF4/+0x46/+0x47/+0xBC.

## Tests (one mutation per rule or guard)

- Pure (`tests/story_threat_test.c`): seat classification with precedence and
  the stale-input case; a type-6 self (ally) with a player in its list gets no
  substitution; mode gating (0/0xB only, never 0xE); per-type exclusion; cap
  parity per seat kind; lead rule nearest-player with the margin formula and
  fallbacks, in both `always` and `host_down` variants; threat accumulation
  (HP-ratio units), decay and distance weighting; sticky switching; hit-all
  excluded from sniper but counted as threat; alert raise (ranged, FIRING,
  UNENGAGED, FAR), responder choice (caps, same group first, no stealing a
  higher-threat target, ranged first), hold re-applied every call, fade (stop /
  close / leave / until), cooldown; idle push held for `push_hold`, once per
  cooldown, cancelled when another enemy engages; every rule inert when co-op
  is inactive, including the away-peer-without-native-key case; record
  retirement on departure, lease clear, epoch/area change and manager absence;
  ini table parsing, key names, defaults and the level clamp when cap parity is
  off.
- Exact-image (`tests/story_enemy_targeting_image_test.c`): S1 prologue and
  caller bytes, the stdcall ABI, the return value and the `*out` contract with a
  synthetic request, list and manager; mode-0xE unchanged passthrough
  (**CONFIRMED_TEST**). The no-list-read/no-direct-filter rule for mode 0xE
  is **CONFIRMED_STATIC** by adapter code inspection. S2 instruction bytes
  (ECX form) and the JG / +0xB follow-ups, authored-cap continuation only for an
  exact HUMAN_REMOTE lease or connected, fully released AWAY reservation; S3 consumer
  registration refused before the thread is bound; transactional install,
  reverse rollback, refusal on a changed byte, refusal to install twice, busy
  and wrong-thread teardown, failed restoration retaining dependencies.
- Host fixture: synthetic manager, candidate list and roster; the adapter's
  substitution reaches `*out` only after the real S2 filter with synthetic
  non-cap world filtering; this is CONFIRMED_TEST, not a native state-commit
  or live fighting claim. Native results pass through on no opinion; mode 0xE
  never substitutes. The tier-1/1.5/2 brief supplies the detailed guard cases.

## Live gates (owner, castle-road ambush, Ailish host / Tal client)

Levels are cumulative: gates 2–4 run with tier 0 (including cap parity)
already accepted.

0. **Watch build** (`EnemyTargeting=watch`): per enemy type, the mode used,
   how often Lead PC came back empty, how often a HUMAN_REMOTE seat was refused
   only by the cap (counted at S2, per request; the dominant host-down case
   exits with an empty list and never reaches the dispatch), the type of every
   Lead-PC unit, plus the hit journal: attribution per seat, far/near and the
   ranged classifications' agreement, hit-all tagging against +0x64, stale
   counts, the AI damage multiplier, and the unit-scale check (Tal's run speed
   and melee reach in units). Nothing changes in play.
1. **Tier 0** (with `EnemyTargetingCapParity=on`): Shooters and Troopers split
   between Ailish and Tal by distance;
   Tal takes several attackers at once; with Ailish down the enemies engage
   Tal instead of idling (undecided samples fall; Levy's authored caps keep
   some idle); no ping-pong between two players standing together.
2. **Tier 1:** Tal hits a Trooper that is on Ailish and it turns on Tal; Ailish
   and Tal then stop adding threat. After decay, the enemy follows the tier 0b
   distance rule again, which may keep it on Tal when he remains nearest.
3. **Tier 1.5:** Ailish snipes from range while Tal fights: within a few seconds
   up to two Shooters and one melee turn on Ailish within `alert_hold`;
   the switch is noticeable on her screen. They return when she closes in,
   stops firing or the hold expires.
4. **Tier 2:** a free, firing player is engaged by one Aware enemy after
   `idle_seconds` and it stays on them for the hold.
5. **Exclusions and absent seats:** the sparring partner stays on the trainee
   (#80; the deny-list makes the module inert for it; owner decisions 2 and
   8); an ABSENT seat (shop,
   parked outside a story house, or outside the lead area) is never chosen or
   boosted by the policy; the watch log counts native picks of such a body (zone sameness of
   a remote shop visitor's body and exterior AI ticking during a story house
   are UNKNOWN) and the Kamo-shop and wait-outside observations are reported
   to the owner rather than passed/failed; a despawned attacker's stale nibble
   is logged (`stale_count`), not fixed.
6. **Inert:** saved-story host alone (no peer OBSERVING): the adapter logs
   installed=yes active=no, zero substitutions, S2 reads the real word, and
   Shooter/Grotesque lead-following matches the single-player baseline. LAN
   arena: the arena chain does not install the adapter; expect no
   `story_enemy_targeting` lines and unchanged `lan_hit` feedback.
7. **Step-away:** owner decision 3 supersedes the original reduced-cap gate.
   Handing the body to native AI must preserve normal player attacker limits;
   returning must also preserve parity. Verify this with the last remote player
   away after its native control lease has been released, with the host Esc
   menu open and closed, as well as with other active peers present. Record
   any named `away_refused` clause; source/test success is not live acceptance.
8. **Teardown:** client disconnect mid-fight, leave to title, rejoin, then
   uninstall and reinstall: no stale substitution, attacker nibbles match the
   manager walk (no module-caused `stale_count` event), native selection
   resumes next tick; a failed restoration is reported, not hidden.

## Owner decisions

1. **ACCEPTED — 2026-10-19:** Tier 0a cap parity. Active human clients use
   the same authored melee/ranged attacker limits as the host. The owner accepts
   several enemies engaging a client at once so both players participate in the
   same fight, rather than enemies standing idle around the client. This enables
   cap parity by default within gameplay tiers; the master default remains
   `watch`, which changes no gameplay. This acceptance does not settle the lead
   rule, away-body treatment, fallback rule, or higher tiers below.
2. **ACCEPTED — 2026-10-19:** Always use the nearest standing player as the
   tier 0b baseline, including while the host is up, retaining stickiness and
   exclusions. This also changes the Wyrd / Elite Illumina Archer / RobotGuard
   Z3000 / FarexLore lead preference; the sparring partner remains excluded.
   The owner confirmed this in the context of the planned pursuit of distant
   shooters: nearest is the baseline, with threat memory and ranged-attacker
   alerts able to choose a farther player. This does not approve decision 4's
   responder counts, thresholds, durations, or bark. The current candidate
   implements Lead-PC tier 0b with fallback (8); policy and bounded adapter
   fixtures pass, but live selection and native state commits remain unverified.
3. **ACCEPTED — 2026-10-19:** No special reduced attacker caps for away
   characters. Away hands control to the native AI while the character remains
   exposed to normal player combat pressure. The owner explicitly rejected
   reduced caps for this handoff. This does not authorize changing the selected
   native Attack/Defend/Retreat stance. The current candidate implements this
   with runtime ownership checks and both native authored-cap branches tested.
   Live away/return and transition validation remains required.
4. **ACCEPTED — 2026-10-19:** Tier 1.5 responders are up to 2 ranged-capable
   enemies plus 1 melee enemy (`alert_ranged=2`, `alert_melee=1`), ranged
   first so the shooter sees missiles coming. The thresholds, hold, fade,
   cooldown and distance defaults above are accepted as starting values to be
   tuned live through the ini keys after the watch pass has reported the
   world scale; none of them is a live-validated number. An audible bark is
   deferred: the native turn/close-in/fire clips are the cue for now, and a
   client-audible bark needs a wire record, tracked as its own issue.
5. **ACCEPTED — 2026-10-19:** Tier 2 keeps `idle_seconds=5`, `push_hold=8`
   and `push_cooldown=15` as starting values, tunable live like the rest.
6. **DECIDED — 2026-10-19:** No tier 3 scaling. The owner's reasoning: more
   human players do not make the party stronger; every character keeps the
   same stats and levels up the same way, so enemy HP/damage has no reason to
   scale with player count. The targeting tiers spread pressure instead. A
   future proposal must come back as a new decision with evidence.
7. **ACCEPTED — 2026-10-19:** Watch build first (original task instruction);
   continue through the owner decisions before enabling each subsequent tier.
8. **ACCEPTED — 2026-10-19:** When no standing human is eligible, enemies
   choose the nearest eligible living AI/AWAY party character, retaining native
   filters and attacker limits. A player returning from away resumes that same
   reserved body and remains attackable; the handoff grants no immunity or
   special reduction in pressure. This does not force every enemy to switch to
   the returning player: normal eligibility, stickiness and target selection
   still apply. No eligible living body means no policy substitution.
9. **DEFEND POLICY ACCEPTED — 2026-10-19:** Adapt party AI
   behavior for co-op so it does not privilege the single-player lead. The
   owner requested research into all three companion stances and returned
   [the companion report](companion-ai-report.md) for this pass. Its native
   findings are incorporated below; its recommendations are not approvals.
   The owner subsequently selected the report's minimum-change proposal:
   protect a nearby standing human, using that player's position as Defend's
   anchor. This does not change the host's network simulation authority. Native
   Defend chooses enemies nearest formation slot 0, not enemies proven to be
   attacking that character. Replacing that position with a nearby standing
   human is the accepted change; detecting attacks on any living party member
   (including self) is not selected. Use the nearest standing human to the
   companion, retaining the native eligible enemy list and ranged self-defense
   priority. If no standing human is available, defend the nearest other
   eligible living AI/AWAY companion, using self only when no other eligible
   living companion exists. The owner selected mutual protection over the
   proposed self-first fallback; this does not transfer character ownership.
   Enemy fallback decision 8 is separate. The owner subsequently accepted
   anchor stickiness: retain an eligible anchor until another is closer by
   more than `max(0.20 * current_distance, 2 units)`; replace an ineligible
   anchor immediately. Enable this Defend policy alongside enemy tier 0,
   retaining observation-only behavior in `watch`; do not add a separate
   default-native companion setting. Attack/Retreat, formation behavior and stance ownership
   retain their current behavior; expanding those changes requires a further
   owner decision. Companion substitutions and bounded watch observations are
   implemented in the current candidate, with pure-policy, runtime-admission
   and native-image fixtures passing. Live stance preservation and gameplay
   remain unverified; see the [Defend evidence](testing.md#companion-defend-with-tier-0--2026-10-19).

10. **ACCEPTED — 2026-10-19:** Tier 1 threat never picks an AWAY body, and an
    AWAY body's hits never raise a tier 1.5 alert. Decision 3 covers attacker
    caps on an away body, not aggro drawn by it. Tiers 1–2 name only
    HUMAN_HOST / HUMAN_REMOTE seats; AI and AWAY bodies remain reachable
    only through tier 0b's decision-8 fallback.
11. **ACCEPTED — 2026-10-19:** At most one active tier 1.5 alert per shooter
    at a time (2 ranged + 1 melee in total), rather than one per sniped
    enemy. While an alert on a shooter is active, other enemies hit by that
    shooter raise none and keep their own cooldown untouched; the next alert
    can start only after the active one ends and the cooldown of the enemy
    that raises the next one has passed. A unit still belongs to at most one
    alert or push at a time.
    Implementation enforces this independently for live and shadow state;
    an orphan responder copy keeps that shooter's alert active until it ends.
    These decisions were already accepted before this implementation pass.
    Implementation brief: [enemy-ai-tier12-task.md](enemy-ai-tier12-task.md).

### Seat ownership and host moderation clarification — 2026-10-19

Owner intent: away AI does not make a player's character free for another
player to claim. Ordinary switching may select an available unclaimed
character; combat continuation does not require automatic switching to someone
else's reserved body. CONFIRMED_STATIC: `poll_story_swap` skips claimed
characters and `SudekiMpPartySwapRequest` refuses occupied targets; away retains
the reservation. `tests/party_ownership_test.c` covers away/control handoff and
reservation conflicts as pure policy, not live gameplay. The AI pass preserves
these ownership rules.

The owner also wants host kick support. Current source has disconnect and
post-drain reservation-release primitives (`SudekiMpLobbyDisconnectMember`,
`SudekiMpLobbyReleaseReservation`, `SudekiMpLanPartyDisconnect`), but no
host-facing kick action was found. Saved-story command admission currently
allows only swap and remote presence, not the general RELEASE/REASSIGN commands.
This is groundwork, not a completed saved-story kick feature. Record that
requested follow-up separately from AI targeting; no player is disconnected
and no claim is released by this pass.

## Docs to correct in the same task

With CONFIRMED_STATIC citations from the research: `docs/combat.md` ~189–191
(0x432A80 → 0x538870 is boss contact damage, not the missile path) and
`docs/coop-story-research-54.md` ~998 (gap G7: the missile packet source is
the firing entity, packet+0x30).

## Out of scope

Behemoth and other boss slot-0 attacks; the LAN arena; AI companions' and
allies' own targeting except the owner-requested Defend investigation and
adaptation and Attack/Retreat research under decision 9; any client-side change;
monster barks on clients;
AwareForceIn weights; fixing stale attacker nibbles; the +0x46 writers outside
the AI code (0x586E51 missile init and others) beyond noting them.

## Companion AI settings checked — 2026-10-19

CONFIRMED_STATIC (documented game behavior): the installed PC manual,
*Sudeki - Manual.pdf*, printed page 14 (PDF page 8), "Setting Character
Behaviour", lists three per-character Quick menu > A.I. Settings choices:
Attack prioritizes offense; Defend engages threats to the lead character;
Retreat avoids combat. One stance is selected per character. The inspected
manual SHA256 is
`3fc2fd081bf8d2e5df24e73feb31d35a4f88af670edb05ca7fe4e3a2f23d32f2`.
The manual is user-supplied game material and is not added to this repository.

CONFIRMED_STATIC (source path): `service_story_controls` in
`src/hooks/lan_story_runtime.c` drains a seat for `stepped_away`;
`story_control_drain` in `src/hooks/control_separation.c` calls
`party_call_ai(FALSE, slot)`, which uses the native `AiDefaultControl` entry.
The mod's handoff releases control to the native AI; it does not select an
Attack/Defend/Retreat stance. UNKNOWN: persistence of each selected stance
through the complete live away/return cycle, and actual formation membership
when the host is held down. The report below establishes Defend's static
selection rule; these remaining live questions still need observation. Enemy
targeting policy and companion stance are separate controls.

CONFIRMED_STATIC (supported executable, read-only disassembly/decompilation):
`tools/ghidra/CompanionAiPolicyReport.java` gates the executable SHA256 and
reports the native stance/request paths. Selector `0x5B76C0` mode `0xC` calls
`0x5B6C70`, which reads the supplied actor's AI target (`actor+0x94`, then
`AI+0xC0`) and returns it only if it is in the current candidate list. The
caller obtains that actor through the formation link, not an unconditional
read of party slot 0. This helper copies a chosen target; it does not itself
identify enemies attacking the lead. Party request builders `0x5ADF60` and
`0x5AE820` use their state's configured target mode; `0x5AE070` parses that
setting. The initial idea that Defend might use mode `0xC` is RETIRED by the
report's resource/state mapping: the four party resources use mode 9,
constraint kind 3. Do not equate Defend with copying the lead's target, or
claim body-blocking or damage interception from this evidence.

The report can be rerun without arguments for the initial paths, or with
explicit hexadecimal function addresses for focused follow-up. It changes no
program data. It was executed successfully against the supported image;
this is static research, not a live companion-behavior test.

## Companion report incorporated into this pass — 2026-10-19

Primary research: [companion-ai-report.md](companion-ai-report.md), especially
sections 2–4 for the per-character rules, native/resource evidence and unknowns.
The supported-image static findings below replace the manual-only account as
the implementation reference. No stance behavior is CONFIRMED_LIVE; the report's
unidentified-build private log is not promoted to live acceptance. The report
itself is preserved as supplied.

| Stance / path | CONFIRMED_STATIC finding from the report | Consequence for this pass |
| --- | --- | --- |
| Setting | Quick-menu row 0/1/2 is stored at `CStats+0x40`, indexing each character's authored state array; Formation is the fourth, non-selectable fallback | Preserve the chosen row; do not implement a stance by writing the stat or reordering formation membership. Exact visible localized labels remain UNKNOWN |
| Attack | Tal uses Strongest (highest HP fraction); Buki/Ailish/Elco use nearest to self. Ranged states first prefer a nearby enemy already targeting self, within authored threat range 4 | The attack pick has no lead anchor to replace. Attack changes require a separate gameplay decision |
| Defend | All four use Constraint Nearest (mode 9 / kind 3): enemy distance to formation slot 0, with no test that the enemy attacks that member. Ranged Defend retains the same self-defense priority | A different protected position preserves the native rule's shape; responding to actual attacks on teammates would be a new rule. Formation slot 0 must not be assumed to equal the network host's character |
| Retreat | Clears its target each tick; when pressured, searches a walkable point near self, using the leader as anchor beyond 30 units. Enemy positions do not determine a direction away from danger | Native Retreat is not deliberate escape from attackers. Improving it needs movement research and an owner choice; a target substitution alone cannot implement it |
| Formation | Idle follow and both snap/warp paths depend on formation slot 0 outside the shared selector | A Defend-only change can leave companions returning to a different character between fights. Matching follow behavior needs separate seam/lifetime evidence and an explicit scope choice |

Native companion requests exclude AI type-3 boss candidates and use unlimited
request attacker caps; preserve these filters while investigating. This is
separate from the number of enemies admitted to attack a player. The visible
counter/guard effects, formation membership through KO/rotation/load, and stance
preservation through away/return, KO and save/load remain UNKNOWN at live level.

CONFIRMED_STATIC (current mod source, checked during incorporation): the client
`input_present` path in `src/hooks/lan_story_quick_menu.c` gives category 4 no
admitted rows. There is no supported client stance-selection request through
that menu; an away character uses the host simulation's setting. This does not
prove its current row, or save/load and handoff persistence. Adding client choice
would be separate UI/request scope. `sample_party` in
`src/hooks/lan_story_research54.c` currently does not record the stance stat,
state-array index/type or AI-mode byte, so existing samples cannot prove those
values were preserved.

### Updated implementation and validation plan

1. Keep the accepted enemy and companion branches separate: away cap parity and Lead-PC
   replacement with decision 8's fallback are implemented in the current
   candidate, with bounded tests recorded in `testing.md`. Decision 9 enables
   the implemented Defend branch with tier 0. Tiers 1/1.5/2 now have pure
   policy and adapter fixtures; decisions 10/11 were already accepted.
   Live gates remain open and ordered after tier 0; tier 3 is declined.
2. Before changing companion selection, extend observation on existing owned
   game-thread seams: stance row, native-control mode, evaluated/active state,
   actual formation leader identity, native selected enemy, and distances to
   self/formation leader/eligible party members. Prove the roster, component
   ownership, state array bounds and exact layout before reading; observation
   failures remain UNKNOWN. Preserve native results and bound the log volume.
   Implemented: once-per-second `companion_state` observations record row,
   AI mode, active index/type and actual formation slot 0; bounded
   `companion_selection` records include evaluated state, native target
   resource, distance to self/formation leader, selected anchor and substitution.
   Bad layouts report unknown or leave native results unchanged. No live
   observations have been collected from this candidate.
3. With decision 9 settled, use a distinct companion branch of
   the existing `story_enemy_targeting` selector adapter, never a second hook.
   Party-self calls now enter only that separate companion branch. Require
   a positively proven AI-controlled party actor (including an away body only
   after native-control release), mode 9/kind 3, and a request belonging to the
   evaluated state in that unit's owned state array. Weight evaluation can run
   before that state becomes active; checking only the current active-state
   pointer is insufficient. Preserve the ranged self-defense request and its
   native priority, Attack, Retreat, Formation and all unrelated self classes.
4. Use this invocation's native eligible list. Deduplicate watch records across
   repeated weight/tick calls without assuming the candidate list is unchanged
   between them. Revalidate a chosen result for each invocation; never reuse an
   earlier global list. Leave native target commits and counters to the state
   machine. Retain S2's enemy-only cap admission. No request, stance, formation,
   target-field or counter writes are authorized by this plan.
5. Nearest-standing-human anchoring, nearest-other-living-companion fallback,
   the 20%/2-unit switching margin and enablement alongside tier 0 are accepted.
   The independent tuning keys are `CompanionDefendMarginFraction=0.20` and
   `CompanionDefendMarginUnits=2` in `[StoryAreas]`; they do not enable the
   policy in watch mode. Anchor memory contains character indices only and
   survives normal per-frame admission suspension; invalid/expired observations,
   roster namespace changes and watch/off clear it. Every pick rechecks current
   eligibility and never caches an enemy pointer or native candidate list.
   The SudekiMP Esc menu / tools capture or an in-progress swap clears the
   anchor, allowing one non-sticky re-pick afterward; the vanilla Q menu does not.
   Companion watch/admission must reuse the enemy adapter's proven away-seat
   extension when the last away seat releases its native lease and drops the
   runtime `remote` flag. Do not alter the dead-lead hold's admission.
6. First live observations: row 1 through three away/return cycles; actual
   formation slot 0 after join/rotation/load and KO; native picks for all four
   characters; visible menu labels and self-defense reactions. Record exact
   build/profile. Any later substitution needs policy tests, exact-image ABI,
   unchanged-path and install/rollback coverage, and live separated-player,
   downed-lead, all-away, transition and teardown checks. Static research alone
   does not pass these gates.

The owner selected the minimum-change nearest-standing-human Defend anchor.
Native Attack/Retreat and formation behavior are retained in this scope; the
follow-anchor mismatch remains a known limitation, not a claim of complete
removal of lead dependence. Looking up localized labels and verifying native
behavior are research tasks, not new gameplay decisions.
