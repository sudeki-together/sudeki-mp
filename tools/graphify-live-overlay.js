/*
 * Live agent activity overlay for graphify-live-server.py.
 *
 * The normal Graphify export remains untouched.  The companion server injects
 * this script into the page and exposes the current activity file at
 * /graphify-out/.agent-activity.json.
 */
(function graphifyLiveActivityOverlay() {
  "use strict";

  const graph = document.getElementById("graph");
  if (!graph) return;

  const overlay = document.createElement("div");
  overlay.id = "graphify-live-agent-overlay";
  overlay.setAttribute("data-graphify-live-overlay", "1");
  overlay.innerHTML = [
    '<div class="graphify-live-status" aria-live="polite">LIVE · waiting for agents</div>',
    '<div class="graphify-live-markers"></div>'
  ].join("");
  graph.appendChild(overlay);

  const style = document.createElement("style");
  style.textContent = `
    #graphify-live-agent-overlay {
      position: absolute;
      inset: 0;
      z-index: 20;
      pointer-events: none;
      overflow: hidden;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    .graphify-live-status {
      position: absolute;
      left: 14px;
      top: 14px;
      padding: 5px 9px;
      border: 1px solid rgba(47, 128, 255, .45);
      border-radius: 999px;
      background: rgba(15, 15, 26, .74);
      color: #8bbcff;
      font-size: 11px;
      letter-spacing: .04em;
      text-transform: uppercase;
      opacity: .82;
      transition: opacity .2s ease, border-color .2s ease;
    }
    .graphify-live-status.active {
      opacity: 1;
      border-color: #2f80ff;
      box-shadow: 0 0 15px rgba(47, 128, 255, .32);
    }
    .graphify-live-marker {
      position: absolute;
      width: 1px;
      height: 1px;
      transform: translate(-50%, -50%);
      transition: left .22s ease-out, top .22s ease-out;
    }
    .graphify-live-marker .triangle {
      position: absolute;
      width: 20px;
      height: 20px;
      left: -10px;
      top: -10px;
      background: var(--agent-color, #2f80ff);
      clip-path: polygon(0 0, 100% 50%, 0 100%);
      filter: drop-shadow(0 0 6px var(--agent-color, #2f80ff));
      animation: graphify-live-triangle-pulse 780ms ease-in-out infinite;
    }
    .graphify-live-marker .ring {
      position: absolute;
      width: 32px;
      height: 32px;
      left: -16px;
      top: -16px;
      border: 1px solid var(--agent-color, #2f80ff);
      border-radius: 50%;
      opacity: .45;
      animation: graphify-live-ring 1.35s ease-out infinite;
    }
    .graphify-live-marker .label {
      position: absolute;
      left: 15px;
      top: -12px;
      min-width: max-content;
      padding: 3px 7px;
      border-left: 2px solid var(--agent-color, #2f80ff);
      border-radius: 3px;
      background: rgba(15, 15, 26, .82);
      color: #dbeaff;
      font-size: 11px;
      line-height: 1.25;
      box-shadow: 0 0 12px rgba(0, 0, 0, .28);
    }
    .graphify-live-marker .detail {
      display: block;
      margin-top: 2px;
      color: #8bbcff;
      font-size: 10px;
      opacity: .86;
    }
    @keyframes graphify-live-triangle-pulse {
      0%, 100% { transform: rotate(0deg) scale(.82); opacity: .74; }
      50% { transform: rotate(18deg) scale(1.16); opacity: 1; }
    }
    @keyframes graphify-live-ring {
      0% { transform: scale(.55); opacity: .58; }
      100% { transform: scale(1.65); opacity: 0; }
    }
  `;
  document.head.appendChild(style);

  const markerLayer = overlay.querySelector(".graphify-live-markers");
  const status = overlay.querySelector(".graphify-live-status");
  const markers = new Map();
  let activity = { agents: [] };
  let frame = 0;

  function markerPosition(agent, index, timestamp) {
    const network = window.__graphifyNetwork;
    if (!network || !agent.node_id) return null;
    try {
      const positions = network.getPositions([agent.node_id]);
      const position = positions[agent.node_id];
      if (!position) return null;
      const dom = network.canvasToDOM(position);
      const angle = timestamp / 950 + index * 2.15;
      const orbit = 16 + 4 * Math.sin(timestamp / 430 + index);
      return {
        x: dom.x + Math.cos(angle) * orbit,
        y: dom.y + Math.sin(angle) * orbit
      };
    } catch (_) {
      return null;
    }
  }

  function ensureMarker(agent) {
    const key = String(agent.id || "agent");
    let marker = markers.get(key);
    if (marker) return marker;
    marker = document.createElement("div");
    marker.className = "graphify-live-marker";
    marker.dataset.agentId = key;
    marker.innerHTML = '<span class="ring"></span><span class="triangle"></span><span class="label"></span>';
    markerLayer.appendChild(marker);
    markers.set(key, marker);
    return marker;
  }

  function render(timestamp) {
    const agents = Array.isArray(activity.agents) ? activity.agents : [];
    const liveIds = new Set(agents.map(agent => String(agent.id || "agent")));
    for (const [key, marker] of markers) {
      if (!liveIds.has(key)) {
        marker.remove();
        markers.delete(key);
      }
    }

    agents.forEach((agent, index) => {
      const marker = ensureMarker(agent);
      const color = agent.color || "#2f80ff";
      marker.style.setProperty("--agent-color", color);
      const label = marker.querySelector(".label");
      const detail = agent.detail || agent.source_file || agent.url || agent.state || "working";
      label.textContent = String(agent.label || agent.id || "agent");
      const detailElement = document.createElement("span");
      detailElement.className = "detail";
      detailElement.textContent = String(detail);
      label.appendChild(detailElement);
      const position = markerPosition(agent, index, timestamp);
      if (position) {
        marker.style.left = `${position.x}px`;
        marker.style.top = `${position.y}px`;
        marker.style.display = "block";
      } else {
        // Keep web/tool activity visible even when it has no graph node.
        marker.style.left = `${24 + index * 28}px`;
        marker.style.top = `${62 + index * 34}px`;
        marker.style.display = "block";
      }
    });

    status.textContent = agents.length
      ? `LIVE · ${agents.length} agent${agents.length === 1 ? "" : "s"} active`
      : "LIVE · waiting for agents";
    status.classList.toggle("active", agents.length > 0);
    frame = requestAnimationFrame(render);
  }

  async function poll() {
    try {
      const response = await fetch(`/graphify-out/.agent-activity.json?t=${Date.now()}`, {
        cache: "no-store",
        headers: { "Accept": "application/json" }
      });
      if (response.ok) activity = await response.json();
    } catch (_) {
      // A closed server or a temporarily unavailable feed should not disturb
      // the underlying graph.  The next poll will retry quietly.
    }
    window.setTimeout(poll, 450);
  }

  poll();
  frame = requestAnimationFrame(render);
  window.addEventListener("beforeunload", () => cancelAnimationFrame(frame));
})();
