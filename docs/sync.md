# Private development mirror

The project has a private NetBird mirror for reproducing the full development
environment on a second machine. It exports the source tree, the user-owned
Sudeki installation/working copies, analysis data, and the Wine prefixes used
by the launchers. These binaries and prefixes are intentionally not tracked by
Git and must not be redistributed.

The source machine currently serves NetBird peer `100.95.174.52` from
`100.95.93.91` using rsync port `18730` and the handoff text on HTTP port
`18731`. The service configuration and destination pull script are in
`tools/sync/`.

The mirror is deliberately narrower than a home-directory backup: it excludes
credentials, Codex state, browser profiles, and unrelated games. The live Codex
conversation is not portable; use `RECOVER.txt`, `README.md`,
`docs/project-status.md`, `docs/milestones.md`, and `docs/research-log.md` as
the durable context on the second machine.
