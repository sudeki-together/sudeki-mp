# Live Graphify agent markers

`graphify-out/graph.html` is a static export: it contains the current nodes and
relationships, but it does not receive Codex activity events.  The local live
view adds a small, deliberately separate activity layer.

## Start the live view

From the repository root:

```bash
python3 tools/graphify-live-server.py --port 8765
```

Open:

```text
http://127.0.0.1:8765/graphify-out/graph.html
```

The browser page polls `graphify-out/.agent-activity.json` and places a blue,
pulsing triangle around the active agent's graph node.  The server injects the
overlay at request time, so Graphify can continue regenerating its normal
`graph.html` output without losing the feature.

## Publish activity

Start or replace a marker by node ID, label, or source file:

```bash
python3 tools/graphify-live-agent.py start \
  --agent codex \
  --node src/engine/player_combat_context.c \
  --state reading \
  --detail "Tracing per-player targeting"
```

Web research can be shown as activity without a graph node.  It appears in the
overlay's upper-left activity area:

```bash
python3 tools/graphify-live-agent.py start \
  --agent codex-web \
  --kind web \
  --label "Graphify documentation" \
  --url https://graphify.com/docs
```

Stop one agent or clear all markers:

```bash
python3 tools/graphify-live-agent.py stop --agent codex
python3 tools/graphify-live-agent.py stop
```

The activity feed is hidden under `graphify-out/`, is not source code, and is
not intended to become a knowledge-graph node.  It contains only local status;
no game files, credentials, or external network data are sent anywhere.
