# Milestones

| Milestone | Status | Evidence |
| --- | --- | --- |
| Known GOG build | Complete | GOG build `50303954381148403`; installer and executable hashes recorded |
| Reproducible vanilla install | Complete | Offline Inno package installed in isolated Wine prefix |
| Hashed executable | Complete | `SUDEKI.exe` SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| Vanilla/working split | Complete | Both copies verify `394/394`; vanilla is read-only |
| Wine title-screen launch | Complete | Main menu captured at 1920×1080; post-launch hashes unchanged |
| Full gameplay baseline | Complete (user-confirmed) | User reported the vanilla baseline checklist is good on 2026-08-12 |
| Ghidra import and auto-analysis | Complete | Ghidra 12.1.2 project saved outside repo; hash-gated verification script passed |
| Quick Menu slowdown understood | Complete | Runtime confirmed mode `1` -> `0.07x` scaled simulation delta |
| Milestone 1: Quick Menu at normal world speed | Complete | Live-only mode `1` -> `0` experiment; menu active flags set while current/requested modes stayed `0`; user confirmed normal motion |
| Minimal `SudekiMP.dll` foothold | Complete | PE32 DLL loaded under Wine, exact build/signature checks passed, `status=ready` logged, no gameplay patch applied; user then closed the game normally without a crash |
| Version-gated Milestone 1 option | Complete | Disabled-by-default in-memory instruction change; enabled capture showed menu active, speed modes `0`, and normal-speed gameplay; disk executable untouched |
| Plasmatica activation path | In progress | Runtime-confirmed as `PC_Elco1__Skill|P`; two animation events gate `FireMissileScripted(10)`; no known damage method fires on any script thread, and static analysis follows the native missile collision path; exact damage resolution remains |
| Milestone 2: Plasmatica independent animation speed | Complete | Two 2.0x casts changed only Elco's validated `CNewMissileAimingGameModelAnimation` multiplier from `1.0` to `2.0` and restored it to `1.0`; event waits fell `201→101`, `249→125`, and `233→117` while normal world simulation remained enabled. Follow-up traces confirmed the collision-gated cinematic camera still runs, with its held window changing from about `11.59 s` to `5.92 s`. |
