#!/usr/bin/env python3
"""Publish live agent markers for the local Graphify activity overlay.

The activity file deliberately lives under ``graphify-out`` and is hidden from
the source scan.  It is a small, local-only event bus for the companion server;
it is not part of the knowledge graph itself.

Examples::

    tools/graphify-live-agent.py start --agent codex \
        --node src/engine/player_combat_context.c \
        --state reading --detail "Tracing per-player targeting"
    tools/graphify-live-agent.py stop --agent codex
    tools/graphify-live-agent.py status
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import tempfile
import time
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GRAPH = REPO_ROOT / "graphify-out" / "graph.json"
DEFAULT_STATE = REPO_ROOT / "graphify-out" / ".agent-activity.json"
DEFAULT_COLOR = "#2f80ff"


def load_json(path: Path, fallback: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, OSError, json.JSONDecodeError):
        return fallback


def write_json_atomic(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2, ensure_ascii=False)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass


def node_degree(node_id: str, graph: dict[str, Any]) -> int:
    node = next(
        (item for item in graph.get("nodes", []) if item.get("id") == node_id),
        {},
    )
    if isinstance(node.get("degree"), int):
        return node["degree"]
    return sum(
        1
        for link in graph.get("links", [])
        if link.get("source") == node_id or link.get("target") == node_id
    )


def resolve_node(query: str | None, graph_path: Path) -> dict[str, Any] | None:
    if not query:
        return None
    graph = load_json(graph_path, {})
    nodes = graph.get("nodes", [])
    needle = query.casefold()

    def score(node: dict[str, Any]) -> tuple[int, int]:
        node_id = str(node.get("id", ""))
        label = str(node.get("label", ""))
        source = str(node.get("source_file", ""))
        if node_id == query:
            rank = 0
        elif label.casefold() == needle:
            rank = 1
        elif source.casefold() == needle:
            rank = 2
        elif needle in label.casefold():
            rank = 3
        elif needle in source.casefold():
            rank = 4
        else:
            rank = 99
        return rank, -node_degree(node_id, graph)

    matches = sorted((node for node in nodes if score(node)[0] < 99), key=score)
    return matches[0] if matches else None


def read_state(path: Path) -> dict[str, Any]:
    state = load_json(path, {})
    if not isinstance(state, dict):
        state = {}
    agents = state.get("agents")
    if not isinstance(agents, list):
        agents = []
    return {"version": 1, "updated_at": state.get("updated_at"), "agents": agents}


def save_state(path: Path, agents: list[dict[str, Any]]) -> None:
    write_json_atomic(
        path,
        {
            "version": 1,
            "updated_at": time.time(),
            "agents": agents,
        },
    )


def start_agent(args: argparse.Namespace) -> int:
    state = read_state(args.state)
    agents = [item for item in state["agents"] if item.get("id") != args.agent]
    node = resolve_node(args.node, args.graph)
    if args.node and node is None:
        raise SystemExit(f"No graph node matched: {args.node}")

    marker = {
        "id": args.agent,
        "label": args.label or args.agent,
        "state": args.activity_state,
        "kind": args.kind,
        "detail": args.detail or "",
        "color": args.color or DEFAULT_COLOR,
        "started_at": time.time(),
    }
    if node is not None:
        marker.update(
            {
                "node_id": node.get("id"),
                "node_label": node.get("label"),
                "source_file": node.get("source_file", ""),
            }
        )
    elif args.url:
        marker["url"] = args.url
    agents.append(marker)
    save_state(args.state, agents)
    print(json.dumps(marker, ensure_ascii=False))
    return 0


def stop_agent(args: argparse.Namespace) -> int:
    state = read_state(args.state)
    if args.agent:
        agents = [item for item in state["agents"] if item.get("id") != args.agent]
    else:
        agents = []
    save_state(args.state, agents)
    print(f"active agents: {len(agents)}")
    return 0


def status(args: argparse.Namespace) -> int:
    print(json.dumps(read_state(args.state), indent=2, ensure_ascii=False))
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--state", type=Path, default=DEFAULT_STATE)
    result.add_argument("--graph", type=Path, default=DEFAULT_GRAPH)
    commands = result.add_subparsers(dest="command", required=True)

    start = commands.add_parser("start", help="publish or replace one active marker")
    start.add_argument("--agent", default="codex")
    start.add_argument("--node", help="node id, label, or source-file path")
    start.add_argument("--label")
    start.add_argument("--state", dest="activity_state", default="working")
    start.add_argument("--kind", default="code", choices=("code", "docs", "web", "tool"))
    start.add_argument("--detail")
    start.add_argument("--url")
    start.add_argument("--color")
    start.set_defaults(handler=start_agent)

    stop = commands.add_parser("stop", help="remove one marker, or all markers")
    stop.add_argument("--agent")
    stop.set_defaults(handler=stop_agent)

    inspect = commands.add_parser("status", help="print the current activity feed")
    inspect.set_defaults(handler=status)
    return result


def main() -> int:
    args = parser().parse_args()
    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
