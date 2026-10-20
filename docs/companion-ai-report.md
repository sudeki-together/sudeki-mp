# Companion A.I. Settings (Attack / Defend / Retreat): native rules, evidence, co-op proposals

Research and design report for the Codex enemy-targeting pass (issue #97,
`docs/enemy-ai-task.md`). Nothing here is an accepted decision. Scope: the
supported image (SHA256 8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94,
base 0x400000), read-only Ghidra decompilation and objdump, the SOLData.baf
resource parse, the repository source at fa50351 plus Codex's dirty work
(untouched), and private live logs read for conclusions only. No tracked file
was edited, nothing was built, launched or committed. The one tracked file this
task adds is this report; scratch files live under the session scratchpad
(`scratchpad/companion/<angle>/` and `scratchpad/companion/reviewer{1,2}/`:
Ghidra batch logs, objdump listings, helper scripts) and are not part of the
repository. Five research angles, one report writer and two adversarial
reviewers produced this; every reviewer refutation has been applied.

Evidence levels follow `docs/evidence-index.md`. Nothing about companion
stances has been observed live; the only live material is the handoff
refcount/mode behaviour in one private log, and its build identity was not
recorded, so it is cited as "private log" rather than CONFIRMED_LIVE.

## 1. What each native mode actually does

The Quick menu's A.I. Settings page stores one number per character (0, 1 or
2) in that character's stats block. That number is an index into the
character's authored list of four AI states, which for all four party members
is ordered [0] an attack-style state, [1] a defend-style state, [2] Retreat,
[3] Formation. A state becomes eligible only when the stored index points at
its own type, so the menu is a selector that the game re-reads every time it
weighs states, not a mode switch. All attack- and defend-style classes share
the same code (one melee base, one ranged base); they differ only in authored
parameters, chiefly the target-selection rule and a "counter chance".

- **Attack (row 0).** Tal: attack the enemy with the highest HP fraction. Buki,
  Ailish, Elco: attack the enemy nearest to the companion itself. Melee
  companions re-choose every tick while not in an attack (unit+0x44 bits 10/7
  clear; INFERENCE that these are the "mid-swing" bits: 0x4EAA40 maps them from
  arbiter 0x1000 and 0x4), so they switch targets freely. Ranged companions
  (Ailish, Elco) first look for an enemy within 4 units that is currently
  targeting them and shoot that one; otherwise the nearest enemy, keeping 2–5
  units away. No distance limit; boss-type AI units (type 3) are excluded; no
  attacker caps. The lead character plays no part in the choice.
- **Defend (row 1).** "Attacks enemies threatening the lead character" is
  implemented as: attack the enemy that is geometrically nearest to the party
  formation's slot-0 member, wherever that enemy is and whoever it is
  attacking. There is no check that the enemy threatens anyone, no aggro
  memory, no use of the lead's own target. Ranged Defend companions still give
  priority to an enemy within 4 units targeting themselves. Counter chance 1
  makes the companion write 1.0 into its counter weight every tick; what that
  does when it is hit is INFERENCE (section 2, "reaction") and differs between
  melee and ranged characters. The protection is of position (near slot 0),
  not of anyone's health.
- **Retreat (row 2).** The companion drops its target every tick and never
  attacks. It picks a new wander point only when enemies are counted as
  attacking it (its attacker nibbles are non-zero) or an enemy is within its
  current search radius (1–10 units, starting at 10); otherwise it only
  finishes its current move or stands. The point is a random walkable cell
  within the radius around its own position (the leader's position becomes
  the anchor only when the companion is more than 30 units from the leader).
  It never moves away from an attacker; enemy positions are not used. If two
  or more enemies are in melee contact with it, it issues a reaction command
  instead of moving that tick. When no living enemy is left in the area, or
  the stance changes, it hands back to Formation.
- **Formation (always present, not selectable).** The fallback whenever the
  selected stance has nothing to do: walk to a slot beside the formation
  leader (slot 0) and stand there. Two teleports exist: the 30-unit horizontal
  snap applies only to members whose current state is Formation/IdleFormation;
  a second, state-independent vertical warp in the unit tick moves any member
  whose height is more than 30 units below the leader's to its slot, stance
  or not.

What the player sees on the page is UNKNOWN: the row labels come from string
ids 0x135+type (Tal: 0x13C Juggernaut, 0x137 Bodyguard, 0x13D Retreat), so the
UI probably shows the per-character state names rather than the manual's
Attack / Defend / Retreat (INFERENCE; the localization table was not decoded).

## 2. Per-mode table

| Aspect | Attack (row 0) | Defend (row 1) | Retreat (row 2) | Formation (fallback) |
|---|---|---|---|---|
| State classes | Tal/Buki Juggernaut (melee base vtable 0x6DC674); Ailish/Elco Focused Attack (missile base 0x6DC604) | Tal Bodyguard, Buki Warrior (melee base); Ailish Hunter, Elco Support (missile base) | AiState_PCRetreat 0x6DC7E4, type 8 | AiState_Formation 0x6DC5D4, type 12 |
| Selection rule | Tal: Strongest = max HP/MaxHP (0x5B60B0). Buki/Ailish/Elco: Nearest = min xz distance² from self (0x5B5910). Ranged: an enemy within Threat range 4 whose +0xC0 target is me takes priority (self-defence request 0x5AE000, then the stance request 0x5ADF60) | Constraint Nearest (mode 9, 0x5B69B0): min over candidates of xz distance² to formation[0]'s entity (metric 0x5B6D70, constraint kind 3). Ranged: same 4-unit self-defence priority first | None: commits NULL every tick (0x4EA7C0(0,1) at 0x5AF52C); 0x5B7210 is used only as "is any enemy within my radius" | No target |
| Eligibility of enemies | Filter 0x5B6EC0: opposite side, same area, not AI unit type 3 (boss), flags +0x44 & 0x58 and & 0x1000 clear (a downed enemy drops out); attack range −1 and maxRange −1 so no distance limit; request caps −1 so the cap test is skipped (0x5B7183) | Same filter; leader-relative range test disabled (range −1) | n/a | n/a |
| Re-pick / retention | Melee: re-pick and commit every tick while unit+0x44 bits 10/7 are clear (tick 0x5AEAD0); no hysteresis, no damage memory. Ranged: re-pick every tick (0x5AE3F0). The selector is also called from the weight functions during group evaluation, so several selector calls per tick share one state request | Same as Attack (identical tick code) | Target cleared every tick | n/a |
| Reaction when attacked (self) | Ranged: the 4-unit self-defence pick (only enemies whose committed target is this companion; enemies attacking a human do not qualify). Melee: 0x4F0430 (≥2 enemies in melee contact targeting me, gap < 0.75) issues command 0x1A (→ 0x4DAC00(3,0)) every rand[min,max] cooldown; what action id 3 looks like is INFERENCE (guard/block). Counter weight unit+0x50 = 0, so the counter path never fires | Same two reactions, plus counter weight 1.0 each tick. INFERENCE on the effect: the roll 0x4ED920 runs with ECX = the attacker's committed target, so it fires only when the attacker has committed this companion; with weights 1/0/0 at +0x54/+0x58/+0x5C, 0x58EA40 queues command 0x1A only for a ranged party unit (+0x46 bit 0: Ailish/Elco), while a melee party unit gets command 0x20 (→ 0x4DACA0) gated by 0x4E35D0's action-table check. Live effect unobserved | 0x4F0430 true → command 0x1A instead of moving; non-zero attacker nibbles (unit+0x47, exposed as unit+0x44 bits 24–31) → re-search a point without the enemy-in-range gate; writes counter weight 1.0 (same INFERENCE as Defend) | None |
| Movement | Melee: move to target (0x4F5610) or face (0x4F56E0); in reach (+0xF2 bit 0: distance < target radius + unit+0x158) attack command with a random combo (0x58F350) and cooldown rand[1,2] s. Ranged: approach/back-off/strafe between fire offsets 2–5 (0x5B7A30), fire via 0x4EAA00 when +0xF2 bit 1 and off cooldown rand[0.2,0.6] s | Same; melee cooldown rand[2,3] (Tal) / [1,3] (Buki) | New point only when attacked or an enemy is within the radius: random walkable nav cell within ±radius of the anchor (0x4F1730 → 0x591080, ≤30 rings); radius starts at 10.0 (0x5AF4D0 overwrites the authored 5/10), +1 per success, −1 per failure, clamp [1,10]; anchor is self unless xz dist² to the leader > 900; move-to-point 0x4F56E0 with binary16 1.33 stored at control+0x2E (its consumer untraced: speed multiplier is INFERENCE) and gait bits 0x20000 | Walk to slot = leader + offset (0x4B2B10/0x4B3370); run when > 5 units (25.0 at 0x6E3C50) else stop/face; path masks 0x5A8DA0/0x5ADA60 |
| Fallback with no target | Weight 0 (0x5AE940/0x5AE280) → done flag → group 0x58E210 re-selects → Formation if its weight is 1, else no state | Same | Weight 0 when 0x4F0640 finds no opposite-side unit in the area with +0x44 bits 3/4 clear (INFERENCE that this means "living") → Formation | Weight 0 when no formation link, leader entity missing, leader == self, or leader arbiter+0x50 bits 7/20 set (0x5ADA90; those bits' meaning is UNKNOWN) |
| Lead / formation dependence | None for the pick. Hand-back to Formation depends on slot 0 | Total: the anchor is **(self+0x40) → formation entry 0 → +0x94 (slot 0 = first entity added, 0x4B2CB0; compaction on removal 0x4B2D50). A KO'd leader: no KO test in the metric, so companions keep anchoring on the body as long as it stays in entry 0 (whether a KO removes it is UNKNOWN 6). An absent leader: the metric-0 branch is practically unreachable (entries are dereferenced on add); the real absent path is unit removal 0x4EF4A0 → 0x4B2D50, which compacts so the next-added member, possibly the companion itself (→ nearest-to-self), becomes slot 0 | Only the >30-unit anchor swap and the hand-back to Formation | Everything keys on slot 0 |
| Caps and nibbles | Request caps −1: companions ignore authored attacker limits on their targets; they do count as attackers on their victim (0x4EA7C0 keeps the nibbles) | Same; all Defend companions converge on the one enemy nearest the leader (INFERENCE) | Holds no committed target, so it never occupies an attacker nibble on any enemy | n/a |
| Ends when | Stance index changes, no eligible enemy, or manager+0xA554 bit 7 set | Same | Same (plus 0x4F0640 == 0) | Leader invalid |

Authored parameters (resource parse, CONFIRMED_STATIC): PC_TAL.sol_35 [0]
Juggernaut Strongest cd 1–2 / [1] Bodyguard Constraint Nearest counter 1 cd
2–3 / [2] Retreat offset 10; PC_BUKI.sol_5 Juggernaut Nearest / Warrior
Constraint Nearest counter 1 cd 1–3 / Retreat 10; PC_AILISH.sol_1 Focused
Attack Nearest threat range 4 fire 2–5 cd 0.2–0.6 / Hunter Constraint Nearest
counter 1 / Retreat 5; PC_ELCO.sol_14 Focused Attack / Support / Retreat 10.
All: Weight 1, Timeout 0, Cooldown 0, attack range −1, caps −1, group
"Weighted Max", Formation last. The four non-playable PC resources
(PC_ALEXINE.sol_8, PC_CAFU.sol_10, PC_KAZEL.sol_11, PC_NICO.sol_9) carry the
same four-state layout; whether Dev Play's Talos/ally seat uses one of them is
UNKNOWN.

Threat is not aggro: the unit "Threat" value (unit+0x14C; Tal 1, Buki 1,
Ailish 0, Elco 0) is a static authored number read only by monster TargetModes
4/5; the companion "Threat range" (state+0x24 = 4) is a distance. Neither is
dynamic.

## 3. Evidence

Mechanism (CONFIRMED_STATIC unless noted):

- Quick menu category 4 = A.I. page: UILayerQuickMenu vtable 0x6CAF1C, category
  id at menu+0x204 (`src/hooks/lan_story_quick_menu.c:156`), confirm handler
  0x499320 case 4, category wrap 0x49A1D0/0x49A260; page build 0x49A2F0 (stage
  flag menu+0x1C8, slot menu+0x1CC, rows via 0x57D7A0, labels 0x52BB60 → string
  ids 0x135+type, row count 0x4EBF30 = states with type byte +0x19 in 1..11 =
  3 per party set).
- Stance storage: the dedicated setter 0x40B0C0 (its one direct caller is
  0x4997BC, reached from the category-4 confirm branch) writes (float)row to
  (entity+0x4C)+0x40 = CStats "AI", index 10 of the Number Value table 0x73CA28
  (serializer 0x4C0FC0, CStats vtable 0x6CC064). Other writers of that stat by
  index (the deserializer, script stat setters) were not excluded: UNKNOWN. HUD
  refresh 0x49D9B0 kind 10; 0x57EB70 fires "TutorialPCAIStateChanged" (0x6D8D3C).
- Stance consumption: 0x4EBFB0 truncates stats+0x40 (FISTP, round toward zero)
  into the state array (unit+0x3C: +4 array, +9 count) and returns +0x19;
  weights 0x5AE940 (melee), 0x5AE280 (missile), 0x5AF480 (Retreat), 0x5AF160
  (IronFortress) compare it with their own type and require manager
  (0x809DE4)+0xA554 bit 7 clear, then 0x5B81D0; group tick 0x58E210, chooser
  0x58E6C0 (strict max, lowest index wins), interruptible bit via 0x5B8110(1);
  Formation weight 0x5ADA90.
- Shared classes (raw vtables): 0x6DC604 = {0x5A98E0, 0x5AE280, 0x5AE310,
  0x5ACC20, 0x5AE3F0, 0x43B860, 0x5AE330, 0x5AE6A0, 0x43A390, 0x5AE0C0,
  0x5AE070, 0x5ADF60, 0x5AE000}; 0x6DC674 = {..., 0x5AE940, 0x5AE9F0, 0x5ACC20,
  0x5AEAD0, ..., 0x5A88F0, 0x5AE070, 0x5AE820}. PCBodyguard 0x6DC6A8, PCWarrior
  0x6DC880, PCJuggernaut 0x6DC7B0, PCChampion 0x6DC6DC, PCSwornBrother 0x6DC84C
  = melee base; PCFocusedAttack 0x6DC710, PCHunter 0x6DC748, PCSupport
  0x6DC814, PCAssassin 0x6DC63C = missile base; factory 0x5B03B0; name table
  0x728020 (1 Assassin … 7 Juggernaut, 8 Retreat, 9 Support, 11 Warrior, 12
  Formation).
- Request builders: 0x5AE820 (melee, request at state+0x38), 0x5ADF60
  (missile, state+0x44): +0x24 = TargetMode nibble (state+0x1C, parser
  0x5AE070, table 0x725B30) | 0x70 (bit 6 opposite side, bits 4–5 = constraint
  kind 3), +0x25 |= 4 (exclude type 3), caps +0x26/+0x27 = −1, attack range −1,
  maxRange −1.0f. 0x5AE000 (missile only): mode 0, +0x24 = 0x40, +0x25 |= 0xC
  (bit 3 = candidate +0xC0 must be self), caps 0xFF, maxRange = state+0x24.
- Selector and filter: 0x5B76C0 (case 9 call at 0x5B78FB → 0x5B69B0; case 0xB
  → 0x5B6B10 reads 0x808D94+0x90; case 0xC → 0x5B6C70); filter 0x5B6EC0; caps
  0x5B7120 (the two +0x16A compares are reached only for candidates whose type
  has bit 2 set, i.e. party/ally/NPC types 4–7; a companion's enemy candidates
  never reach them, and request caps −1 skip the test at 0x5B7183); metric
  0x5B6D70 kind 3 = distance² to **(self+0x40) → entry 0 → +0x94 → +0x10 →
  +0x74 → +0x44/+0x4C; mode handlers 0x5B5910 (Nearest), 0x5B60B0 (Strongest).
  The selector's calling convention is recorded as stdcall in the current
  brief and adapter (`docs/enemy-ai-task.md` S1); earlier research text said
  cdecl; the adapter's exact-image ABI test is the authority.
- Selector call sites per tick for a companion: the weight functions call it
  during group evaluation (0x5AE940 → slot 11 at 0x5AE9C3; 0x5AE280 → slots 11
  and 12 at 0x5AE2C6/0x5AE2DE), the state tick re-picks, and the tick's weight
  re-evaluation calls it again, all with the same state-local request pointer.
- Missile commit priority (asm 0x5AE452–0x5AE487): the tick calls slot 11
  (0x5ADF60) and slot 12 (0x5AE000), tests the self-defence result first and
  falls back to the stance result, then commits via 0x4EA7C0(eax,1).
- Melee tick 0x5AEAD0: re-pick at vtable+0x2C and commit while unit+0x44 bits
  10/7 clear; 0x4F0430 at 0x5AEBDA; command 0xD0 (= case 0x1A after the
  `>>3` decode in 0x4EC800) at 0x5AEBFD; movement 0x4F5610/0x4F56E0; attack
  0x58F350/0x4F5AE0.
- 0x4F0430: per opposite-side manager unit, counts (+0xC0 == me && +0xF2 & 1 &&
  binary16 +0xE6 < 0.75f at 0x759E18) or (+0xF4 == me && +0x126 & 1 && +0x11A
  < 0.75f).
- Counter path: 0x5AEAD0/0x5AE3F0 write the state's Counter chance into
  unit+0x50 and 1.0/0/0 into +0x54/+0x58/+0x5C each tick (AiAttackStyle block
  at unit+0x48, vtable 0x6D4FD8, serializer 0x58F3C0, export AiSetCounterWeights
  0x4F5E90). Roll 0x4ED920 (conditions +0x134 == 0, +0x140 == 0, +0x16A < 1,
  mode[0xB] != 0, random ≤ +0x50) is reached from 0x4EC0B0 (melee in) /
  0x4EC200 (missile in) with ECX = the attacker's committed target. Outcome
  0x58EA40: command 0x1A for a type-5 unit with +0x46 bit 0; command 0x20
  (→ 0x4DACA0) via 0x4E35D0 for melee party units; case 0x18 would commit the
  attacker as the victim's target when +0x5C != 0, which the ticks never set but
  the exported AiSetCounterWeights could. All of this past the roll's
  conditions is INFERENCE for the visible effect.
- Retreat: weight 0x5AF480 (type 8, 0x4F0640 > 0, bit 7); Enter 0x5AF4D0
  stores 10.0 into state+0x1C (the parser 0x5AF820 stores "Max retreat offset"
  at the same slot, so the authored value is dead); tick 0x5AF500: NULL commit
  0x5AF52C–0x5AF53D; nibble test 0x5AF577–0x5AF58F jumps to the point search;
  0x5B7210 gate at 0x5AF5A8 with maxRange = state+0x1C; otherwise je 0x5AF757
  (finish the current move at substate 0x1C/0x1D, 0x5AF77A–0x5AF7CC); anchor
  selection 0x5AF5BE–0x5AF66F (self vs leader, 900.0 at 0x6E3CB0); point
  search 0x4F1730/0x591080/0x432010 (random/spiral detail INFERENCE); radius
  ±1, clamp [1,10]; 0x4F0430 at 0x5AF75F, 0xD0 at 0x5AF768–0x5AF775; move
  0x4F56E0; 0x4F5DB0 stores binary16 1.33 into control+0x2E (consumer untraced).
- Formation: tick 0x5ADB40, slots 0x4B2B10/0x4B3370, 25.0 at 0x6E3C50; snap
  0x4B3690 (gate byte 0x7207E1, export 0x4B2C30, state id via 0x4EBF70 must be
  0x0C/0x11, 3D dist² > 900.0); the state-independent vertical warp in the unit
  tick 0x4EDE00 (member Y < leader Y − 30.0, double at 0x6E3BB0 → 0x4B2B10 →
  0x4F30A0 at 0x4EDF94–0x4EDFC9, no state check, no gate); formation layout
  ctor 0x4B2C40, add 0x4B2CB0, remove 0x4B2D50; writers found by direct call:
  0x4FDF40 / 0x4F6170 / 0x4F61D0 / 0x4EF4A0, plus the wrappers 0x4F2B00 (add)
  and 0x4F2B30 (remove) with no direct .text caller (probably script-bound);
  lead rotation 0x4237B0 → 0x4EF700 reads the slot point via 0x4B2B10 and
  resets the new lead through 0x4EC2D0 without changing membership.
- "Attack Style" (0x4DC3C0–0x4DC406) serializes string 0x6CC998 as a 4-entry
  enum (table 0x73A100: None/Melee/Missile/Hybrid) from bits 4–7 of this+0x58
  and writes the nibble back; it never touches +0x4C or stats+0x40, so it is
  not the stance (RETIRED as a stance claim). The owning object of that +0x58
  is UNKNOWN (the layout matches the arbiter nibble the brief uses as "ranged
  mode"). Save-file persistence of the AI stat is UNKNOWN (CStats reset
  0x4C0EF0 zeroes it; the serializer visits it; the save writer was not traced).
- KO: CauseConcussed 0x4BEC70 at 0x4D328A (death handler 0x4D31C0);
  ConcussedStatusEffect 0x4C0900/0x4C0AB0/0x4C0B10 write arbiter+0x50 bit
  0x10000, never stats+0x40 or the AI mode. Whether anything removes a KO'd
  member from the formation is UNKNOWN 6. Formation-weight bits 0x80/0x100000:
  writers 0x4BB790, 0x4D4DE0, 0x4CD1A0, 0x4CDBF0, 0x4C99D0; their meaning is
  UNKNOWN.
- Handoff: 0x4EC350 (from AiOverrideControl 0x4F60D0 / AiDefaultControl
  0x4F6100; 16 direct callers) clears +0xC0/+0xF4 via 0x4EA7C0/0x4EA940, counts
  +0x16A up/down, zeroes the queue heads +0x16C/+0x170, calls 0x4EC2D0(0) on
  first acquire (state Exit, index 0xFF), and on last release writes mode byte
  (unit+0x3C)[0xB] = 1 at 0x4EC522, except that a released unit whose entity
  is the CGroupPlayers (0x808DA4)+0x248 object with [entity+0xD8]+0x6C == 0 and
  [0x808D30]+0x5C == 0 (0x4EC4C8–0x4EC510) sets [group+0x84] = 1 and calls
  0x4290D0 without writing mode 1 (the controlled character's path). That its
  own body writes nothing to +0x4C, +0x48..+0x60 or the state array is
  CONFIRMED_STATIC; its callees 0x4DAD60, 0x4ED840 (writes unit+0x46 |= 0x80 and
  unit+0x164 = 1.0, or calls 0x4EBDE0), 0x4DC720, 0x4290D0/0x429410/0x41BE60
  were not traced. Mod side: `control_separation.c` ~7340 (party_ai_owned),
  ~7351 (HELD), ~7392 (party_call_ai), ~8188 (acquire), ~8515/~8594/~8634
  (drains); `lan_story_runtime.c` ~1938–2030 (service_story_controls); the
  client quick-menu adapter gives category 4 limit 0 (`lan_story_quick_menu.c`
  ~160–161; installed on clients only, `lan_story_runtime.c` ~3947–3950). No mod
  path writes CStats. Consequence: a client can never set its own character's
  stance; an away body runs on the host-side stat, which the CStats reset
  leaves at 0 = row 0 (Attack) unless the host set it.
- Private log (host/client runtime.log.011034, 2026-10-19, Ailish host / Tal
  client, dirty tree, build identity not recorded): 14 `host_control_drain
  reason=stepped_away` lines (17 drains in all), 17 ai_override 1→0 and 18
  0→1 transitions in Tal's party_state samples, consistent with the static
  handoff model; at least one nested native override episode (count 2) during
  which the lease refused to release (6 `drain_refused step=ai_owner`, 7
  `acquire_refused body_not_idle`, not analysed further); the host's own
  Ailish shows ai_override=1 ai_mode=0 in 11 samples (a native override on the
  controlled character). The r54 samplers never read stats+0x40, the mode byte
  or the active state, so stance preservation is UNKNOWN at live level. The
  absence of `story_quick_menu … category=4` lines says nothing about the host
  (the adapter is client-only and category 4 is refused there).
- Mod adapter (read-only, dirty/untracked): `src/hooks/story_enemy_targeting.c`
  `select_target` observes only when the self unit is a non-roster unit
  (monster/leader/boss); a party self passes through untouched, which is the
  accepted contract in `docs/enemy-ai-task.md` ("For every other self (PC
  companions, NPCs, allies) the detour returns the native result untouched").
  Seams: 0x5B76C0 prefix, the two 0x5B7120 compares, the shared 0x4D21D0
  observer; install/uninstall transactional. The current build logs
  `substitutions=0` (tier 0a only).

## 4. Explicit unknowns and the smallest observation for each

1. Row labels the player sees (ids 0x132 title, 0x136–0x140 per type):
   "Attack/Defend/Retreat" or state names. Open Q > A.I. Settings for Tal and
   Ailish on a host and read the three rows (or decode the localization table
   via 0x7C305C).
2. Live preservation of stats+0x40 across step-away/return. Extend
   `sample_party` in `src/hooks/lan_story_research54.c` (~L330; it already holds
   actor+0x4C and actor+0x94) with ai_stat = stats+0x40, mode =
   (*(ai+0x3C))[0xB], state index (*(ai+0x3C))[0x8] and its +0x19 type; set
   Tal to row 1, run three away/return cycles; any sample with ai_stat ≠ 1
   falsifies preservation.
3. Zone load and save/load persistence of the stat (0x4C0EF0 would reset it to
   0). Same sampler across one door transition and one save/quit/load.
4. Which body is formation slot 0 on a co-op host and whether a client join or
   zone load changes the add order. Read CAiManager (0x809DE4)+0xF4 entry 0 and
   +0x124 (count) and compare with CGroupPlayers (0x808D94)+0x90 slot 0 after
   load, after a Prev/Next rotation, after the client joins.
5. KO/revive effect on the stance and on an away body's AI mode. Same sampler
   at 4 Hz while an AI-driven companion in row 1 is knocked down and cured.
6. Whether a KO removes a member from the formation, and the script-bound
   formation writers (0x4F2B00/0x4F2B30, AiPCFormationAdd/DelGroup).
   `tools/inspect-sol-gex.pl` on SOLWORLDM.gex for those call sites; a
   hardware watch on formation+0x30 across one KO and one zone transition.
7. Meaning of manager+0xA554 bit 7 (set 0x4CEEBF, cleared 0x4F22EE/0x4F2983),
   which zeroes every stance weight. Decompile those two functions, or log the
   byte during a cutscene, a pause and a step-away.
8. The visible effect of the counter path and of command 0x1A / 0x20 (action
   id 3 via 0x4DAC00(3,0) → 0x4DB740 → 0x4D0730 → 0x4D0640(3); 0x4DACA0 via
   0x4E35D0). Decompile 0x4D0640 and 0x4DACA0, or log the action id and
   animation after a hit on a Defend companion.
9. Arbiter+0x50 bits 7 and 20 that gate the Formation weight. One log line per
   bit from the sampler during an interaction and during Elco's jetpack.
10. Live behaviour of all three stances: Defend convergence on the enemy
    nearest formation[0], the reaction posture, Retreat's wander. Watch-build
    line per selector call with a party self: mode nibble, chosen candidate,
    its distance to formation[0] and to self (section 6, step 0).
11. Whether companions never auto-target type-3 boss units live, or boss
    fights use scripted targeting. Log the candidate list for a companion in
    one boss encounter.
12. Whether a leased client body still reports state id 0x0C to 0x4EBF70 (snap
    eligibility). Log 0x4EBF70's result for the leased body once.
13. Which arbiter bits are hit-reaction/stagger and whether a stagger clears
    unit+0x44 bit 10/7. Log unit+0x44 and arbiter+0x50 around a received hit on
    a companion.
14. Setter of unit+0x46 bit 3 (Retreat's "arrived" advance); writers of
    unit+0x98 for a PC unit; whether the handoff's queue clears discard a
    queued companion action. Log those words before/after each `party_call_ai`.
15. The consumer of control+0x2E (Retreat "1.33"); random/spiral detail of
    0x591080; constraint kind 2 metric 0x5936D0; the HUD icon draw site; the
    owning class of the "Attack Style" +0x58 nibble. Low priority; one
    DecompileAtReport run each.

## 5. Recommended co-op behaviour

Kept apart: (A) verified native behaviour = sections 1–3; (B) accepted owner
decisions in `docs/enemy-ai-task.md`: cap parity for active clients (1),
nearest standing player as the enemy lead baseline with stickiness and
exclusions, with threat memory and alerts able to override (2), away keeps
normal player caps and no stance change (3), watch build first (7), the
direction to adapt companion behaviour without a chosen policy (9); not
accepted: any specific Defend replacement, blanket cap changes for unassigned
companions, the no-standing-human fallback (8). (C) Proposals below. Companion
choices never touch enemy attacker caps, enemy threat memory or the authored
Threat value.

Scenario baseline (native, from A): several separated humans → every Defend
companion attacks the enemy nearest formation slot 0 (probably the host,
UNKNOWN 4), however far; Attack companions ignore humans entirely; Retreat
companions wander near themselves when pressed. AI defending itself → ranged
stances already prioritise a 4-unit attacker; melee stances react when two
enemies are in contact; the counter path's effect is unobserved. Teammates
under pressure → nothing native reacts to pressure, only to distance from slot
0. Downed host → Defend keeps anchoring on the body while it stays in entry 0
(UNKNOWN 6), Formation keeps walking to it. Only AI members remaining → same
anchor; if slot 0 becomes the companion itself the metric becomes "nearest to
me".

**P1. Leave Attack and Retreat native.** Benefit: both are already
lead-agnostic for their pick (Retreat only swaps its wander anchor beyond 30
units). Failure cases: a Retreat away body wanders within 10 units of itself
when pressed; Attack-Tal's Strongest rule can send him across the area. Scope:
none. Owner decision: confirm "no change".

**P2. Defend anchor substitution** (the decision-9 replacement, proposal
only). Replace the anchor used by the companion's Constraint Nearest pick with
a co-op anchor while co-op is active. Recommended anchor: the standing human
(HUMAN_HOST / HUMAN_REMOTE seat kinds the policy already computes) nearest to
the companion, with the same stickiness shape as tier 0b (keep the current
anchor unless another is closer by max(fraction × d, units); 0.20 and 2 units
are tuning without evidence); DOWN/ABSENT seats never anchor. Variants: (b)
the human with the most enemies engaging them (the adapter's manager walk
already counts engaged per seat); (c) any living party body including self;
(d) self only. Fallback with no standing human: self-anchor (what native does
when slot 0 is the companion) or the nearest living AI/AWAY body, an owner
choice separate from enemy decision 8. Benefit: companions on Defend protect
the player near them instead of the single-player lead, which is what the
manual's wording promises; a downed host no longer pins companions to a body.
Failure cases: with two humans far apart, companions split and may leave one
unprotected; weak stickiness makes companions oscillate between humans
standing together; variant (b) flips with every engagement change; the
Formation follow still walks to slot 0, so a companion with nothing to attack
walks away from the human it was protecting (P3). Scope: a pure policy
function plus a companion branch in the existing detour (section 6), one ini
switch, pure and exact-image tests, a live gate. Owner decisions: anchor
variant, fallback, stickiness, switch default, and an amendment to the
adapter's accepted scope text.

**P3. Formation follow anchor in co-op.** The idle follow and both teleports
read formation[0] inside 0x5ADB40 / 0x4B3690 / 0x4EDE00, not through the
selector, so they cannot be changed at the 0x5B76C0 seam. Proposal: leave
native in this pass and record the mismatch (Defend anchor ≠ follow anchor) as
a known limitation. Benefit of deferring: no new seam. Failure case: companions
drift back to the host between fights. Owner decision: whether a later task
may own a formation-follow seam.

**P4. Self-defence and pressure: no change.** Native already gives ranged
companions a 4-unit self-defence priority in both stances and melee companions
the two-attacker reaction. A "protect the teammate under pressure" rule is
variant (b) of P2, not a separate mechanism.

**P5. Away bodies: no stance write** (decision 3). The mod must not write
stats+0x40. Note the native consequence: a client's character runs whatever
stance the host set for it (clients cannot set one), and the default is row 0
Attack. If observation 2 shows the stance is lost across a handoff, that
becomes an owner question, not an automatic carry.

**P6. Watch first** (decision 7): before any substitution, count companion
selector calls per stance and log the chosen candidate's distances (section
6, step 0), deduplicating the several selector calls per tick that share one
state request.

## 6. Proposed integration and validation plan for Codex's current pass

Constraints honoured: the existing adapter owns the 0x5B76C0 entry (S1) and
the two 0x5B7120 compares (S2); no second hook on 0x5B6D70, 0x5AE820/0x5ADF60,
the formation, or the stance stat; no bypass of 0x4EA7C0 (the native state
commits whatever `*out` holds); nothing written to
+0xC0/+0xF4/+0x46/+0x47/+0xBC; transport threads untouched; game thread only.

Where companion policy could plug in: the same detour, as a second self-class.
This contradicts the brief's current accepted contract ("a party self passes
through untouched"), so it needs an explicit owner amendment before Codex
builds it. If approved: add a companion branch when the self unit is a roster
actor (type 5) whose seat kind is AI or AWAY (ai+0x16A == 0 and mode byte +0xB
== 1 for a native companion; an away body after release looks the same), the
request is readable, its mode nibble is 9 with constraint bits (request+0x24
>> 4) & 3 == 3, and the request pointer lies inside that unit's current state
(state+0x38 melee / state+0x44 missile) rather than unit+0x98. For such a call
the selector has rebuilt 0x7D5540/0x7C30AC (only mode 0xE skips that), so the
policy chooses argmin over this tick's list of xz distance² to the chosen
anchor and replaces `*out`; an empty answer leaves the native result. The same
request is presented up to three times per tick (weight path, tick re-pick,
weight re-evaluation): answer identically for all of them and count once per
tick. Never answer for modes 0/3 (Attack), for the self-defence request
0x5AE000 (constraint bits 0 and +0x25 bit 3 set: the companion protecting
itself), for Retreat (no selector call) or Formation. The S2 compares are not
reached for a companion's enemy candidates (types 1–3), so no companion-scope
guard is needed there. The request builders and the stance setting are
rejected as plug-in points: the builders would need a second seam and alter
the request block; the stance stat is a native CStats write with unknown
persistence and visible menu/HUD effects; reordering the formation moves the
follow target, both teleports and the enemies' mode 0xC.

Switches and logging: a separate ini key (`[StoryAreas]
CompanionDefend=native|watch|nearest_human|…`, default `native`) and a separate
log event (`event=companion_substitution`), so companion behaviour cannot be
enabled by the enemy master switch. Rollback is hook removal; the next tick
re-selects natively.

Tests: pure (`tests/story_threat_test.c`): anchor choice per seat kind with
DOWN/ABSENT skipped, stickiness margin, fallback variants, inert when co-op is
inactive, no answer for a non-type-5 self, for mode ≠ 9, for the self-defence
request, or when `CompanionDefend=native`; identical answers for repeated
calls on one request within a tick. Exact-image
(`tests/story_enemy_targeting_image_test.c`): a synthetic mode-9 request with a
party self and a list whose nearest-to-anchor differs from nearest-to-slot-0 →
`*out` replaced; mode 0/3 party self → pass-through; monster self unchanged;
request outside the state block → pass-through; install/rollback and
changed-byte refusal unchanged. Host fixture: the substituted companion pick
flows through to the 0x4EA7C0 commit.

Live observations to run first (watch only, no gameplay change): (0a) the
sampler fields ai_stat/mode/state (unknown 2) with Tal set to row 1, three
away/return cycles; (0b) formation slot-0 identity after load, rotation and
client join (unknown 4); (0c) companion watch lines per selector call: stance
index, mode nibble, candidate chosen, distance to formation[0] and to self,
and whether a candidate nearer to any standing human existed; (0d) the Q > A.I.
page labels (unknown 1). Then the live gate for P2 (castle-road ambush, Ailish
host / Tal client, Buki/Elco as AI companions on row 1): with the client nearer
to the enemies, the companions attack the enemy nearest the client; with the
host down and the client standing, companions anchor on the client; the stance
is unchanged after every cycle; teardown/reinstall mid-fight leaves no stale
pick.

## 7. Remaining owner questions

1. Decision 9 policy: which Defend anchor (nearest standing human to the
   companion, most-pressured human, any living member including self, or self
   only); stickiness; whether DOWN seats are always skipped; the fallback when
   no standing human exists (kept separate from enemy decision 8); and the
   amendment to the adapter's accepted scope text that a companion branch
   requires.
2. Whether the Formation follow / teleport anchor (slot 0) may be changed
   later through its own seam, or stays native (P3).
3. Whether the mod may ever write the stance stat (stats+0x40), for example to
   carry it across a handoff if observation 2 shows loss, or to let a client
   choose its own character's stance (today only the host can, and the default
   is Attack); decision 3 currently forbids changing the stance.
4. Whether companion substitution gets its own switch and default (`native`
   proposed) rather than riding the enemy master switch.
5. Whether to decode the localization to settle the row labels, or accept the
   manual's Attack/Defend/Retreat names with the row-order inference.
6. Whether a downed body remaining the Formation anchor (companions walking to
   and fighting around it) is acceptable for now.
7. Whether companions' native exclusion of boss-type units needs any co-op
   treatment (none proposed).
8. Whether the watch observations in section 6 should be run before Codex's
   current cap-parity pass is extended, given decision 7.

## Files added or modified by this task

- Added (tracked): `docs/companion-ai-report.md` (this file).
- Modified (tracked): none.
- Scratch (outside the repository, session scratchpad
  `scratchpad/companion/`): `ai-settings-mapping/`, `attack/`, `defend/`,
  `retreat/`, `stance-persistence/`, `reviewer1/`, `reviewer2/` with Ghidra
  batch logs, objdump listings and helper scripts (`rtti_vtables.py`,
  `tables.py`, `xref.py`, `dump.py`, `StanceXrefReport.java`), plus
  `companion-ai.json` and `companion-report-draft.md`.
