# Stage 1.1 castle reference audit — 2026-10-20

Scope: saved-story co-op on dirty `27f7296`, supported GOG image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
This is a source/exact-image audit and an opt-in diagnostic, not a crash fix or
live validation. The owner-reported crash and two non-crashing away switches
are the observations recorded in [the task brief](step-away-stage1.1-task.md#part-e--castle-crash-audit-and-diagnostic-only-decision-6).

| Path | Audit evidence and lifetime implications |
| --- | --- |
| Whole-world release | **CONFIRMED_STATIC:** observer `zone_set`, `zone_enter`, and `zone_main` invoke the runtime's before-world-change hook before native entry. The runtime closes READY and calls `ReleaseBeforeWorldChange`; release requires the same actor/group/controller/AI/mode, stopped movement, body idle and `AiDefaultControl` verification. A refusal retains the lease for retry. |
| Castle background switch | **CONFIRMED_STATIC:** `zone_switch` requests a background load and deliberately does **not** call before-world-change or bump the epoch. Therefore the preload-release path cannot be assumed to have run merely from `zone_switch_requested`. `LoadDrain` is release-only when the runtime subsequently observes a load/drain condition; an unchanged, still-exact READY lease can remain controlled while the background loader runs. This distinction fits the owner's controlled-versus-away difference without proving a culprit. |
| Release/reoffer | **CONFIRMED_STATIC:** `party_call_ai` receives the existing stable party-slot TPtr; the C lease stores ordinary actor/AI/mode pointers, not a newly linked stack TPtr. Acquire re-proves native identity before overriding AI. `story_control_forget` records the released key; stage 1.1 preserves that receipt over an empty epoch rebind. Monotonic generations prevent matching it to a new lease. The new test exercises later Drain and LoadDrain of the earlier key. |
| Input and policy | **CONFIRMED_STATIC:** `lan_story_input.c` samples on its established native/window thread; `party_ownership.c` operates on plain records and does not dereference native actors. Neither creates an intrusive party-actor reference. Socket/session workers sequence copied records; the runtime owns native application on the controller dispatch. No new worker-native access is introduced. |
| `entity_from_gel_pointer` | **CONFIRMED_STATIC:** GetPC/GEL resolution constructs this frame's three-word `tracked_entity`; cleanup unlinks it before the GEL deleting destructor runs. `split_screen_render.c` has a separate synchronous GEL-group resolver and cleanup. These stack temporaries must not outlive their frame. The opt-in audit samples the party entity list immediately before both cleanup and destruction in the cleanroom resolver. |
| Direct weak assignment | **CONFIRMED_STATIC:** `lan_arena_spirit_visual_host.c`, `lan_arena_spirit_vfx.c`, `lan_party_jetpack.c`, and `lan_story_gates.h` use the native `0x401750` weak assignment. Their persistent observers live in owner records; validation, release and retained teardown are in their respective owners. `lan_party_projectile_lifetime.c` uses native `0x4015B0` copy and destructor contracts with stable observer records. These are weak links, not actor lifetime ownership. |
| Stack arguments to natives | **CONFIRMED_STATIC:** VFX replay and shop dialogue use synchronous three-word actor arguments. The VFX `PlaySfx` argument is initialized as an unlinked object-only record; the call's native descendants still merit investigation if the detector identifies that frame. Native control passes the actual party-slot record, not a fabricated copy. Spirit manager participants retain their native TPtr identity. No assumption that an arbitrary object-only argument is safe to unlink was added. |
| Remote chest use (`lan_story_remote_use.c`) | **CONFIRMED_STATIC:** 16 native `CALL 0x4015B0` front-handle copies redirect to the opener's party-slot handle within the remote-use scope. Start is gated by the runtime's in-control `admitted` call to RemoteUseTry. Lifetime verdict: control-gated start, but the Open record alone reopens scope in update_wrapper (native treasure update and host-frame RemoteUseService) and remote_use_finish_enter after drain. No step-away/drain cancels it; RemoteUseActive only zeros buttons. Redirects continue until state 0/8, state-1 abort after ABORT_MS=5 s, or OPEN_LIMIT_MS=20 s release, irrespective of subsequent control state. A released control lease does not prove this reference traffic ended. |
| Armour preview (`armour_menu.c:refresh_preview`) | **CONFIRMED_STATIC:** native 0x4015B0 creates a stack TPtr passed by value to UIModelManager::Setup at 0x560F90 (RET 0x10, releases reference). Lifetime verdict: synchronous call-owned temporary, not a retained control lease; native release must complete before the stack frame ends. |
| AI MOVE/SNAP (`story_ai_movement.c`) | **CONFIRMED_EXACT_IMAGE** (reviewer's supported-image objdump recorded in the castle/stage-1.1 brief): AI_MOVE_FRAME 0x4F4BB0 copies controller+0x248 into a stack local using 0x4015B0 and ends it with 0x4015E0 before the patched compare at 0x4F4CB3. Lifetime verdict: balanced native temporary on every AI tick, regardless of gate result; not exclusive to in-control play. |
| Per-tick target refresh (`control_separation.c`) | **CONFIRMED_STATIC:** `SudekiMpLanStoryControlMove` reaches `SudekiMpLanPartyControlStoryRefreshTarget` only on an exact-lease move tick with no actor+0x2B pause reference, no native item-use hold (arbiter+0x50 mask 0x20), and `ordinary_world(TRUE)`. `SudekiMpStoryTargetRefresh` reaches acquire RVA 0xBA1C0 only with native auto-target enabled (targeter+0x84 mask 2), not ranged/first-person (arbiter+0x58), arbiter+0x50 & 0x0041c410 clear, and arbiter+0x60 mask 4 clear; commit through RVA 0xB9D20 is conditional on this admitted path. Whether these gates are open during the castle switch window is **UNKNOWN**. Lifetime verdict: additional in-control reference-list traffic, using the exact actor/targeter lease; release stops this control path but does not establish that other native users stopped. |
| GetPC callbacks | **CONFIRMED_STATIC:** the retained roster proof still performs both original actor-name lookups. No replacement lookup, register-saving bridge, hook on GetPC's vtable[3] call at `0x504025`, or hook on GetName at `0x401E35` was introduced. A nested native callback or a pre-existing hook remains a possible writer; source absence is not proof that retail or another DLL cannot write. |
| Retail loader workers | The brief's **CONFIRMED_STATIC** workers at `0x5BB100`/`0x5BB1E0` and suspend/resume around `0x43E840` (`0x5BD140`/`0x5BD190` from `0x556160`) are not a global synchronization guarantee for every GetPC invocation. **UNKNOWN:** whether a retail worker, native callback, or existing reference user made the corrupt link during the failing lookup window. The diagnostic deliberately does not inspect other threads' stacks. |

**CONFIRMED_EXACT_IMAGE contract:** `StorySkyLightImageTest` checks the full
supported file identity and hashes original, pre-relocation bytes at
`0x401B30` (31 bytes), `0x401B50` (167), `0x4015E0` (54), `0x401750` (85),
and `0x5BF4E0` (160, including alignment). The destructor's store at
`0x401B9E` writes the predecessor into the next node. This substantiates the
possible intrusive-list write mechanism; it does not identify the bad node.

The refined saved-register evidence and competing hypotheses are recorded
below. The corrupting writer, node origin and any other thread's involvement
remain **UNKNOWN**; neither source review nor balanced mod lookups resolves them.

Enable only for the owner gate with `[StoryAreas] TPtrAudit=true`. It defaults
to false through `read_config_boolean`, independently of ResearchDiagnostics.
The walker checks object identity, predecessor links, cycles, a 64-node bound,
and foreign nodes in the current NT_TIB stack interval, exempting only this
frame's own tracked record. The first suspect walk logs node addresses, up to
32 readable bytes around each, and thread ID. The CAS winner now copies
all node bytes into static storage before the first log call, avoiding the
logger's 1 KB local buffer and an extra capture array on the audit stack.
The audit's own frame and readable_memory's VirtualQuery calls can still
overwrite dead nodes near SP before they are read. The walker cannot tell a
live caller's linked node from a dead stack node. Live research rule: a stack
node that persists longer than about 5 ms across lookups is dangling. It never repairs links, skips
cleanup, changes native control, or replaces a name lookup. A clean walk proves
nothing about other frames or threads. Logs remain local diagnostic material.

Owner gate: Tal controlled through the castle tunnel and back twice, recording
no crash and control on each arrival, with default-off `[StoryAreas] TPtrAudit=true`
`story_tptr event=switch_host` / `switch_client` rows in each 20 s post-switch
window. `tptr_audit` reports once per process: its CAS latch resets only through
`SudekiMpCleanroomEngineSetTPtrAudit` at startup, and the first lookup of every
run consumes it. Do not expect another suspect report at the tunnel; no
per-switch rearming was added. The decisive data remain the owner's 1 ms list
walk and conditioned hardware watchpoint below. Stepping away first remains
the brief's provisional workaround, based on two runs.

## 2026-10-20 castle crash research refinement (Part C; no fix)

Research concerns the 2026-10-19 crash on DLL prefix `cfa8f432`. The evidence
below is the sanitized owner/reviewer record in
[the castle/stage-1.1 brief](castle-and-stage11-fixes-task.md#part-c--castle-crash-research-record-only-no-fix-in-this-task),
not a new live observation by this implementation pass.

- **CONFIRMED_LIVE** crash registers plus **CONFIRMED_STATIC** lookup path:
  the second ActorEntity(TAL) lookup inside roster_identity_locked returned
  the correct entity with ESI=0. **CONFIRMED_STATIC:** entity_from_gel_pointer
  restores ESI from its saved slot, so that slot was overwritten with zero
  while the frame was live.
- **CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** the only main-thread store
  in that lookup window into memory it does not own is GEL destructor unlink
  at 0x401B9E, `[next+4] = node.prev`, zero for the head node. 0x401B50 is the
  destructor body; 0x401B30 is its deleting wrapper.
- **INFERENCE, conditional on a main-thread bad write:** Tal's list head
  then was a dead stack node on that frame's saved-register slots. The node
  address is not observed: Wine's stack dump begins at ESP.
  **CONFIRMED_STATIC:** the first TAL lookup at the same ESP in the same call
  did not clobber. **INFERENCE:** the bad node became head between the two
  calls. **UNKNOWN:** newly created node versus one previously deeper in the
  list.
- **HYPOTHESIS H1:** a non-main-thread mutation races on the unlocked
  intrusive list. No source or live evidence yet shows any non-main thread
  linking/unlinking a party actor's TPtr. The background loader workers
  0x5BB100 / 0x5BB1E0 execute queued jobs through virtual calls; this alone
  does not establish a party-reference writer. **CONFIRMED_STATIC:**
  link-at-head copy 0x4015B0, assignment 0x401750, unlink 0x4015E0 and
  destructor 0x401B50 take no lock. **CONFIRMED_STATIC:** target refresh adds
  list traffic (acquire 0xBA1C0, commit 0xB9D20) only on the audit-row path;
  whether those gates are open in the castle switch window is **UNKNOWN**.
  **CONFIRMED_STATIC:** the native-control lease's own calls (acquire/release
  via `party_call_ai`, per-tick Move 0xDAE80 / blend-stop 0xDB070 and dodge)
  are not bounded by the target-refresh gates: `SudekiMpLanStoryControlMove`
  can still dispatch dodge under `block_world_ready` outside `ordinary_world`,
  or submit x=z=0 through `SudekiMpLanPartyControlStoryMove` and
  `SudekiMpStoryMovementSubmit` to blend-stop; whether these native-control
  calls link or unlink party TPtrs is **UNKNOWN**. The MOVE/SNAP copy/end runs
  on every AI tick irrespective of control. H1 is not upgraded to INFERENCE.
- **HYPOTHESIS H2:** a leaked main-thread node. **CONFIRMED_LIVE** owner
  research on October 20: mod lookups balanced 25 GEL creations / 25
  destructions, recycling the same object. Tal and Ailish lists each held
  about 47,000 nodes and grew about 43 per second while stationary, from a
  deep native call chain; the origin is **UNKNOWN**, not the mod lookup
  path. TPtrAudit fires on the first lookup of every run because legitimate
  live caller frames also hold linked nodes. A first-suspect line alone
  cannot distinguish H1, H2 or a healthy live caller.
  Evidence: 2026-10-20, deployed stage-1.1 candidate
  `dd917ee5601fc2a479c2eb22329355e009247933ceb4d381f4ce1500f0e5b80d`,
  dirty `27f7296`, `[StoryAreas] TPtrAudit=true`, live-mp70 pair (Ailish host /
  Tal client), countryside castle-road start. The owner measured with a
  read-only `/proc/<pid>/mem` list walk and gdb hardware breakpoints on the
  host. The ~47,000-node count is not TPtrAudit output (its walker is capped
  at 64). Private evidence identifier: owner's session log, 2026-10-20.
  This cites prior owner research, not a live run by this pass; teardown and
  tunnel success are not established by these stationary measurements.

Owner's next live probe after Part A: a read-only 1 ms `/proc/<pid>/mem`
walk of Tal's list during an in-control crossing, then a hardware watchpoint
on Tal+4 conditioned on a non-main-thread stack. Non-main-thread writes
support H1; only main-thread writes support H2. This pass neither implements
that probe nor changes native cleanup, lookup, control or the crash mechanism.

The same default-off `[StoryAreas] TPtrAudit` now enables separate switch
telemetry on the game-thread story-service seam, outside Research54's shared
line budget. The observer exposes a 20 s background-switch window. Host rows
are at most once per second per member: native position, pause references,
AI override and mode, with checked actor/AI/position links and readable mode.
Failed proofs log known=0. Client rows record the last successfully presented
party positions while spectator state is not 4, on state change or once per
second otherwise. There is a separate 64-sample / at-most-256-row bound per
switch; no render-frame or worker logging. These remain diagnostics, not
proof of progress or permission to alter native objects.
