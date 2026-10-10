# Paused story and cutscene effect clocks (#54)

The owner confirmed the corrected cutscene effect presentation in the two-instance
saved-story ambush replay on 2026-10-09 (owner local date): “yep that works.”
This resolves the reported animation-clock issue in that tested setup.
Evidence: **CONFIRMED_LIVE**, owner visual acceptance, on `ff2d280` plus the
uncommitted #54 prototype and the A1 corrections. See the
[plan progress log](coop-story-plan-54.md#9-progress-log) for the test scope.

## Reusable rule

Supply animation time on the first native renderer visit in a frame. Do not rely
on a second manual tick to recover a first visit with zero delta, and do not clear
the native frame stamp to force another update.

- **CONFIRMED_EXACT_IMAGE:** object update `0x5D64D0` compares the object's
  16-bit stamp at `+0x32` with the frame at `0x7C3150`. Even a zero-delta visit
  stamps the object; another visit in that frame skips its renderer callback.
- **CONFIRMED_STATIC + CONFIRMED_TEST:** `SudekiMpLanStoryWorldOwnedRenderers`
  had included SpecialEffectEntity renderers in the exclusions used by
  `lan_story_ambient.c`. Their animation is native, not a replicated actor pose.
  The correction leaves exact effect primary/update classes eligible for the
  ambient adapter's existing zero-delta substitution. Effects also no longer
  consume its bounded actor exclusion list. Unknown classes remain conservative.
- The existing ambient adapter owns renderer update `0x622B50` (RVA `0x222B50`).
  Reuse that owner; do not add a competing hook. Keep its active lease, frame
  clock limits and actor exclusions.
- A1 also corrects the paused effect update `0x5319B0`: its argument is the timing
  block `{0,0,0,dt}`, with a finite, positive, capped delta. The scheduler list
  referenced through GameUpdate is not that timing block.

For another cutscene, verify the exact effect/renderer identity, pause state,
first renderer visit and clock ownership before changing anything. A paused
world does not make an effect's animation host-replicated. Conversely, actors
whose poses are replicated must retain their exclusions.

## Validation and limits

The accepted dirty candidate passed `StorySpawnTimingImageTest` (native timing
ABI and frame-stamp behavior), `StoryEffectClockImageTest`,
`StoryRangedProjectionTest` (including 129 effects preceding a monster), and
`StoryInteractionGuardImageTest`. The ownership regression fails against the
pre-correction implementation. The frame-stamp fixture uses an inert renderer
callback and does not itself prove rendered gameplay.

Owner acceptance applies to the reported replay, not every cutscene. Keep the
host/client visual gate for new scenes. This finding does not establish a new
hit-contact protocol, proven-sigil-only hiding, or host-driven sigil termination;
those remain separate plan work. Native teardown behavior was not changed.
