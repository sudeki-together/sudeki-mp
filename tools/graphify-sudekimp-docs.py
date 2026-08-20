#!/usr/bin/env python3
"""Build the deterministic SudekiMP documentation layer for Graphify.

Graphify's AST extractor owns code structure. This companion keeps every
project document and heading navigable, adds only explicit code references,
and supplies a small curated set of cross-document project concepts.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "graphify-out"
DETECT = OUT / ".graphify_detect.json"
AST = OUT / ".graphify_ast.json"

HEADING_RE = re.compile(r"^(#{1,4})\s+(.+?)\s*$")
PATH_RE = re.compile(
    r"(?<![A-Za-z0-9_.-])((?:src|tools|tests|config|docs|research)/"
    r"[A-Za-z0-9_./-]+(?:\.[A-Za-z0-9_+-]+)?)"
)
BACKTICK_RE = re.compile(r"`([^`\n]+)`")

CONCEPTS = {
    "known_gog_build": (
        "Known GOG Build and Vanilla Baseline",
        ("offline gog build", "installer and vanilla", "identity", "current milestone"),
    ),
    "exact_build_safety": (
        "Exact-Build Safety and Reversible Hooks",
        ("safety gates", "ghidra rule", "repository policy", "runtime and validation"),
    ),
    "quick_menu_time": (
        "Quick Menu Simulation Time",
        ("quick menu slowdown", "milestone 1", "game-speed path"),
    ),
    "mod_foothold": (
        "PE32 DLL and Launcher Foothold",
        ("minimal mod foothold", "wine suspended-process", "preflight and launch"),
    ),
    "plasmatica_flow": (
        "Plasmatica Native Skill Flow",
        ("elco plasmatica", "plasmatica native", "skill activation path", "confirmed chain"),
    ),
    "direct_combat_actions": (
        "Direct Real-Time Combat Actions",
        ("native real-time quickskill", "native spirit strike", "real-time multiplayer skill"),
    ),
    "character_control": (
        "Independent Character and AI Control",
        ("native character-control", "character-switching", "per-character combat-input"),
    ),
    "player_two_input": (
        "Linux Player Two Input Bridge",
        ("player movement submission", "split-screen player 2", "current milestone"),
    ),
    "camera_ownership": (
        "Gameplay and Render Camera Ownership",
        ("gameplay camera target", "named native cameras", "camera target structures"),
    ),
    "split_screen": (
        "Clean Dual-Viewport Compositor",
        ("d3d9 frame and split-screen", "split-screen player 2", "dual-viewport"),
    ),
    "viewport_hud": (
        "Viewport-Owned HUD Presentation",
        ("viewport hud character ownership", "split-screen player 2", "current validation state"),
    ),
    "cleanroom": (
        "Native Cleanroom Test Harness",
        ("cleanroom", "training loadout", "menu", "native test-arena harness"),
    ),
    "native_title_menu": (
        "Native Title and Front-End Menu State Machine",
        ("native title/front-end menu", "title/menu state machine", "native menu population", "front-end action dispatcher"),
    ),
    "ranged_presentation": (
        "Viewport-Owned Ranged Presentation",
        ("viewport-owned ranged", "render-only ranged", "native ranged-presentation"),
    ),
    "spirit_temporal_history": (
        "Spirit Strike Temporal History Isolation",
        ("pure land temporal", "live-history isolation", "completion-preserving pure land"),
    ),
    "shared_resources": (
        "Global Multiplayer Resource Constraints",
        ("multiplayer integration constraints", "multiplayer resource and camera"),
    ),
    "sudeki_forge": (
        "SudekiForge Model and World Tooling",
        ("sudekiforge", "model and world-authoring"),
    ),
}


def slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def stem_for(path: Path) -> str:
    return slug(str(path.relative_to(ROOT).with_suffix("")))


def node(node_id: str, label: str, file_type: str, source: Path, line: int | None):
    return {
        "id": node_id,
        "label": label,
        "file_type": file_type,
        "source_file": str(source),
        "source_location": f"L{line}" if line else None,
        "source_url": None,
        "captured_at": None,
        "author": None,
        "contributor": None,
    }


def edge(source: str, target: str, relation: str, source_file: Path,
         confidence: str = "EXTRACTED", score: float = 1.0,
         line: int | None = None):
    return {
        "source": source,
        "target": target,
        "relation": relation,
        "confidence": confidence,
        "confidence_score": score,
        "source_file": str(source_file),
        "source_location": f"L{line}" if line else None,
        "weight": 1.0,
    }


def main() -> int:
    if not DETECT.exists() or not AST.exists():
        print("Run Graphify detection and AST extraction first.", file=sys.stderr)
        return 2

    detected = json.loads(DETECT.read_text(encoding="utf-8"))
    ast = json.loads(AST.read_text(encoding="utf-8"))
    documents = [
        Path(value)
        for value in detected.get("files", {}).get("document", [])
        if "/.codex/skills/graphify/" not in value
    ]

    nodes = []
    edges = []
    node_ids = set()
    heading_nodes: list[tuple[str, str]] = []
    document_roots: dict[Path, str] = {}

    def add_node(value):
        if value["id"] not in node_ids:
            nodes.append(value)
            node_ids.add(value["id"])

    ast_by_file = {}
    ast_by_label = {}
    for value in ast.get("nodes", []):
        source = value.get("source_file") or ""
        if source:
            ast_by_file.setdefault(source, value["id"])
        clean_label = (value.get("label") or "").removesuffix("()")
        if len(clean_label) >= 5:
            ast_by_label.setdefault(clean_label, value["id"])

    for path in documents:
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        stem = stem_for(path)
        root_id = f"{stem}_document"
        document_roots[path] = root_id
        add_node(node(root_id, str(path.relative_to(ROOT)), "document", path, 1))

        parents: list[tuple[int, str, str]] = []
        current_heading = root_id
        seen_explicit: set[tuple[str, str]] = set()
        for line_number, raw in enumerate(text.splitlines(), 1):
            match = HEADING_RE.match(raw)
            if match:
                level = len(match.group(1))
                title = match.group(2).strip()
                while parents and parents[-1][0] >= level:
                    parents.pop()
                hierarchy = [item[1] for item in parents] + [title]
                heading_id = f"{stem}_{slug('_'.join(hierarchy))}"
                add_node(node(heading_id, title, "document", path, line_number))
                parent_id = parents[-1][2] if parents else root_id
                edges.append(edge(parent_id, heading_id, "references", path, line=line_number))
                parents.append((level, title, heading_id))
                heading_nodes.append((heading_id, title.lower()))
                current_heading = heading_id

            for code_path in PATH_RE.findall(raw):
                target = ast_by_file.get(code_path)
                key = (current_heading, target or "")
                if target and key not in seen_explicit:
                    edges.append(edge(current_heading, target, "references", path, line=line_number))
                    seen_explicit.add(key)

            for quoted in BACKTICK_RE.findall(raw):
                target = ast_by_label.get(quoted.removesuffix("()"))
                key = (current_heading, target or "")
                if target and key not in seen_explicit:
                    edges.append(edge(current_heading, target, "references", path, line=line_number))
                    seen_explicit.add(key)

    source = ROOT / ".codex/skills/sudekimp-research/references/project-map.md"
    project_id = f"{stem_for(source)}_sudekimp_project_knowledge"
    add_node(node(project_id, "SudekiMP Project Knowledge", "concept", source, 1))
    for root_id in document_roots.values():
        edges.append(edge(project_id, root_id, "references", source))

    concept_ids = {}
    for key, (label, needles) in CONCEPTS.items():
        concept_id = f"{stem_for(source)}_{key}"
        concept_ids[key] = concept_id
        add_node(node(concept_id, label, "concept", source, 34))
        edges.append(edge(project_id, concept_id, "references", source))
        for heading_id, heading_label in heading_nodes:
            if any(needle in heading_label for needle in needles):
                edges.append(edge(
                    concept_id,
                    heading_id,
                    "conceptually_related_to",
                    source,
                    confidence="INFERRED",
                    score=0.95,
                ))

    hyperedges = [
        {
            "id": "sudekimp_exact_build_research_safety",
            "label": "Exact-Build Research Safety",
            "nodes": [concept_ids[k] for k in ("known_gog_build", "exact_build_safety", "mod_foothold")],
            "relation": "form",
            "confidence": "EXTRACTED",
            "confidence_score": 1.0,
            "source_file": str(source),
        },
        {
            "id": "sudekimp_local_coop_architecture",
            "label": "Local Co-op Architecture",
            "nodes": [concept_ids[k] for k in ("character_control", "player_two_input", "camera_ownership", "split_screen", "viewport_hud")],
            "relation": "participate_in",
            "confidence": "EXTRACTED",
            "confidence_score": 1.0,
            "source_file": str(source),
        },
        {
            "id": "sudekimp_realtime_combat_presentation",
            "label": "Real-Time Combat Presentation",
            "nodes": [concept_ids[k] for k in ("quick_menu_time", "plasmatica_flow", "direct_combat_actions", "ranged_presentation", "spirit_temporal_history")],
            "relation": "participate_in",
            "confidence": "EXTRACTED",
            "confidence_score": 1.0,
            "source_file": str(source),
        },
    ]

    result = {
        "nodes": nodes,
        "edges": edges,
        "hyperedges": hyperedges,
        "input_tokens": 0,
        "output_tokens": 0,
    }
    output = OUT / ".graphify_semantic.json"
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"SudekiMP docs: {len(documents)} files, {len(nodes)} nodes, "
        f"{len(edges)} edges, {len(hyperedges)} hyperedges"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
