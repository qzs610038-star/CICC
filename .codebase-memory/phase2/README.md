# Phase 2 codebase-memory artifacts

> Generated: 2026-07-05
> Purpose: optional review graphs for official/preliminary reference material.

This directory stores selected Phase 2 codebase-memory-mcp artifacts. They are
kept separate from the root `.codebase-memory/graph.db.zst` so the default
collaboration graph remains focused on the Git working project.

## Artifacts

| Directory | Source scope | Project | Nodes / edges | Artifact |
|---|---|---|---:|---:|
| `official_demo/` | `赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3` | `D-cbm_phase2_stage_20260705-official_demo` | `628 / 962` | `67,195 bytes` |
| `prelim_src/` | `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/src` | `D-cbm_phase2_stage_20260705-prelim_src` | `438 / 615` | `46,245 bytes` |
| `prelim_sw/` | `初赛demo/2ChMIPICSI_2ChMIPIDSI_Demo_Test/sw` | `D-cbm_phase2_stage_20260705-prelim_sw` | `83 / 120` | `12,468 bytes` |

## Boundary

These graphs are for review acceleration only. They do not change the project
baseline:

- `final_project/` remains the formal development target.
- `初赛demo/` is an experience and issue reference, not a decision-code baseline.
- Any RTL/SoC/myCobot safety conclusion still requires checking real source,
  Efinity/ModelSim logs, and board behavior.

## Refresh Note

codebase-memory-mcp 0.8.1 writes persistent artifacts only when the indexed path
is a Git repository root. These Phase 2 artifacts were therefore generated from
temporary staging Git repositories copied from the selected source scopes, then
copied back into this directory.

