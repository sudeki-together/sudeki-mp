# SudekiMP 0.7.0 — story co-op update

Version 0.7.0 packages the accumulated saved-story co-op work, including the
castle/Brightwater gameplay checkpoint `d433688` and build-note/test-portability
follow-ups. It remains experimental: bug fixing and slowdown investigation
continue after this release.

## What changed

- Castle transitions handle larger actor rosters and bounded missing-pose gaps.
- Step-away/return handling, enemy target eligibility and companion defense
  policies have been revised. Enemy targeting gameplay tiers remain opt-in;
  the default observes native behavior.
- The researched castle arrival movie plays locally on each game under host
  skip/end control. Tal's connected reserved player controls him when the story
  forces him to lead, and both games use the authored room camera. A disconnected
  owner's character can be controlled temporarily by the host.
- Earlier saved-story work includes per-game loot/inventory handling, client
  menu/item use, join-time save import and scoped remote interactions. These
  remain experimental; complete per-player persistence is not implemented.

## Installation and opt-in behavior

Use the supported GOG Windows build of Sudeki and matching 0.7.0 builds
for every player. Download `sudekimp-windows-launcher-0.7.0.zip`, extract it, run `Launch SudekiMP.cmd`, and
select the game folder. For story co-op choose **Multiplayer (title menu lobby)**
or **Local host + client (two windows)**. See the included `README-Windows.txt`.

The castle arrival/lead-handoff behavior requires `ClientLeadSpectate=true` in
`[StoryAreas]` in both active INI files before launch. It remains off by default.
Other movies and arbitrary campaign leader changes are outside this scoped
acceptance. Host-only kick/body reassignment is not included.

## Evidence and limits

The owner completed the castle ↔ Brightwater loop with host Ailish and client
Tal on one Linux PC with two Wine windows. The test used party invulnerability
and a King's Road start. The live-tested cross-build DLL is:
`7d3596822dcf355694ef60a66660e6231ed46262b7a00732904183c24b9e7ae8`.

Recorded gameplay-candidate validation: 35/35 focused tests, 233/237 full-suite
invocations (four documented hardware/arguments/fixture cases), three native C11
checks and eight rejected scope/diagnostic mutants. These are not claims of
Windows gameplay testing. Native Windows build results and artifact hashes are recorded separately on
the release page; a successful build does not establish Windows gameplay acceptance.

Bugs and slowdowns remain under investigation. Frame-time budgets, multiple
machines, native Windows gameplay, broader routes, disconnect/rejoin and detailed
movie/camera recovery remain open. Join-time save import is not ongoing save
synchronization or the planned keep/discard/per-player-persistence flow.

SudekiMP uses only the user's installed game; the package does not
include Sudeki itself. The DLL patches memory rather than the game executable
or archives. Explicit fixture installation and multiplayer save import can write
save data. Preserve your existing saves when testing development builds.
