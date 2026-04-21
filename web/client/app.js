const $ = (id) => document.getElementById(id);

let graph = null;
let appState = null;
let selectedRoute = null;
let lastRequestId = null;

function cityName(cityId) {
  const c = graph?.cities?.find((x) => x.id === cityId);
  return c ? c.name : cityId;
}

function routeText(route) {
  if (!route?.path?.length) return "—";
  return route.path.map(cityName).join(" → ");
}

async function apiGet(url) {
  const r = await fetch(url);
  const j = await r.json();
  if (!r.ok) throw new Error(j?.error || "Request failed");
  return j;
}

async function apiPost(url, body) {
  const r = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  const j = await r.json();
  if (!r.ok) throw new Error(j?.error || "Request failed");
  return j;
}

function setOptions(selectEl, cities) {
  selectEl.innerHTML = "";
  for (const c of cities) {
    const opt = document.createElement("option");
    opt.value = c.id;
    opt.textContent = `${c.name}${c.kind === "major" ? " (major)" : ""}`;
    selectEl.appendChild(opt);
  }
}

function svgEl(name) {
  return document.createElementNS("http://www.w3.org/2000/svg", name);
}

function drawMap() {
  const svg = $("mapSvg");
  svg.innerHTML = "";
  if (!graph) return;

  const byId = new Map(graph.cities.map((c) => [c.id, c]));

  // roads
  for (const [a, b] of graph.roads) {
    const ca = byId.get(a);
    const cb = byId.get(b);
    if (!ca || !cb) continue;
    const line = svgEl("line");
    line.setAttribute("x1", ca.x);
    line.setAttribute("y1", ca.y);
    line.setAttribute("x2", cb.x);
    line.setAttribute("y2", cb.y);
    line.setAttribute("class", "road");
    line.dataset.a = a;
    line.dataset.b = b;
    svg.appendChild(line);
  }

  // route highlight overlay
  if (selectedRoute?.path?.length >= 2) {
    for (let i = 0; i < selectedRoute.path.length - 1; i++) {
      const a = selectedRoute.path[i];
      const b = selectedRoute.path[i + 1];
      const ca = byId.get(a);
      const cb = byId.get(b);
      if (!ca || !cb) continue;
      const line = svgEl("line");
      line.setAttribute("x1", ca.x);
      line.setAttribute("y1", ca.y);
      line.setAttribute("x2", cb.x);
      line.setAttribute("y2", cb.y);
      line.setAttribute("class", "road road--highlight");
      svg.appendChild(line);
    }
  }

  // cities
  for (const c of graph.cities) {
    const g = svgEl("g");
    const circle = svgEl("circle");
    circle.setAttribute("cx", c.x);
    circle.setAttribute("cy", c.y);
    circle.setAttribute("r", c.kind === "major" ? 7 : 5);
    circle.setAttribute("class", c.kind === "major" ? "city-major" : "city-minor");
    g.appendChild(circle);

    const label = svgEl("text");
    label.setAttribute("x", c.x + 10);
    label.setAttribute("y", c.y + 4);
    label.setAttribute("class", "city-label");
    label.textContent = c.name;
    g.appendChild(label);
    svg.appendChild(g);
  }

  if (selectedRoute?.path?.length) {
    $("mapStatus").textContent = `Selected route: ${routeText(selectedRoute)} (distance: ${selectedRoute.distance})`;
  } else {
    $("mapStatus").textContent = "Select From/To and submit a request to see the route.";
  }
}

function renderDriverSelect() {
  const sel = $("driverSelect");
  sel.innerHTML = "";
  for (const d of appState?.drivers ?? []) {
    const opt = document.createElement("option");
    opt.value = d.id;
    opt.textContent = `${d.name} · ${cityName(d.cityId)} · ${d.status}`;
    sel.appendChild(opt);
  }
}

function renderIncomingShares(userName) {
  const container = $("incomingShares");
  container.innerHTML = "";
  if (!userName) return;

  const rides = (appState?.rides ?? []).filter((r) => r.primaryUser === userName && r.status === "active");
  if (!rides.length) {
    container.innerHTML = `<div class="muted">No active ride for ${userName}.</div>`;
    return;
  }

  for (const ride of rides) {
    const pending = (ride.shareProposals ?? []).filter((p) => p.status === "pending");
    if (!pending.length) {
      const el = document.createElement("div");
      el.className = "item";
      el.innerHTML = `
        <div class="item__title">Ride ${ride.id}</div>
        <div class="item__sub">No pending share proposals.</div>
      `;
      container.appendChild(el);
      continue;
    }

    for (const p of pending) {
      const el = document.createElement("div");
      el.className = "item";
      el.innerHTML = `
        <div class="item__title">${p.fromUser} wants to share</div>
        <div class="item__sub">Pickup: ${cityName(p.pickupCityId)} · Drop: ${cityName(p.dropCityId)}</div>
        <div class="item__sub">Ride: ${ride.id} · Driver: ${ride.driverName}</div>
        <div class="item__actions">
          <button class="btn btn--ok" data-act="accept" data-ride="${ride.id}" data-proposal="${p.id}">Accept</button>
          <button class="btn btn--danger" data-act="reject" data-ride="${ride.id}" data-proposal="${p.id}">Reject</button>
        </div>
      `;
      el.addEventListener("click", async (e) => {
        const btn = e.target.closest("button");
        if (!btn) return;
        const rideId = btn.dataset.ride;
        const proposalId = btn.dataset.proposal;
        const accept = btn.dataset.act === "accept";
        try {
          await apiPost("/api/share/decide", { rideId, proposalId, accept });
          await refreshAll();
        } catch (err) {
          alert(err.message);
        }
      });
      container.appendChild(el);
    }
  }
}

function renderDriverView() {
  const driverId = $("driverSelect").value;
  const driver = (appState?.drivers ?? []).find((d) => d.id === driverId);
  $("driverStatus").textContent = driver
    ? `${driver.name} is ${driver.status} at ${cityName(driver.cityId)}`
    : "Select a driver.";

  const reqContainer = $("driverRequests");
  reqContainer.innerHTML = "";

  const relevant = (appState?.requests ?? []).filter((r) => r.status === "pending_driver" && (!r.assignedDriverId || r.assignedDriverId === driverId));
  if (!relevant.length) {
    reqContainer.innerHTML = `<div class="muted">No pending requests assigned to this driver.</div>`;
  } else {
    for (const r of relevant) {
      const el = document.createElement("div");
      el.className = "item";
      el.innerHTML = `
        <div class="item__title">${r.userName}: ${cityName(r.fromCityId)} → ${cityName(r.toCityId)}</div>
        <div class="item__sub">Route: ${routeText(r.route)} (distance: ${r.route.distance})</div>
        <div class="item__sub">Solo fare: ₹${r.soloFare}${r.wantsSharing ? " · sharing enabled" : ""}</div>
        <div class="item__actions">
          <button class="btn btn--ok" data-act="accept" data-req="${r.id}">Accept</button>
          <button class="btn btn--danger" data-act="decline" data-req="${r.id}">Decline</button>
          <button class="btn" data-act="show" data-req="${r.id}">Show path</button>
        </div>
      `;
      el.addEventListener("click", async (e) => {
        const btn = e.target.closest("button");
        if (!btn) return;
        const reqId = btn.dataset.req;
        if (btn.dataset.act === "show") {
          const req = (appState?.requests ?? []).find((x) => x.id === reqId);
          selectedRoute = req?.route ?? null;
          drawMap();
          return;
        }
        const accept = btn.dataset.act === "accept";
        try {
          await apiPost("/api/driver/respond", { driverId, requestId: reqId, accept });
          await refreshAll();
        } catch (err) {
          alert(err.message);
        }
      });
      reqContainer.appendChild(el);
    }
  }

  const nContainer = $("driverNotifications");
  nContainer.innerHTML = "";
  const notes = (appState?.notifications ?? []).filter((n) => n.driverId === driverId).slice(-10).reverse();
  if (!notes.length) {
    nContainer.innerHTML = `<div class="muted">No driver notifications yet.</div>`;
  } else {
    for (const n of notes) {
      const el = document.createElement("div");
      el.className = "item";
      el.innerHTML = `
        <div class="item__title">${n.type}</div>
        <div class="item__sub">${n.message}</div>
        <div class="item__sub">${n.at}</div>
      `;
      nContainer.appendChild(el);
    }
  }
}

function renderShareCandidates() {
  // no-op placeholder (populated by findShareBtn)
}

function renderTopbar() {
  $("serverTime").textContent = appState?.serverTime ? `Server: ${appState.serverTime}` : "";
}

async function refreshAll() {
  graph = await apiGet("/api/graph");
  appState = await apiGet("/api/state");

  setOptions($("fromCity"), graph.cities);
  setOptions($("toCity"), graph.cities);
  renderDriverSelect();
  renderTopbar();
  drawMap();
  renderDriverView();
  renderIncomingShares($("userName").value.trim());
}

function shareCandidateCard(candidate, requestId) {
  const el = document.createElement("div");
  el.className = "item";
  el.innerHTML = `
    <div class="item__title">Ride ${candidate.rideId} · Driver: ${candidate.driverName}</div>
    <div class="item__sub">Route: ${routeText(candidate.route)} (distance: ${candidate.route.distance})</div>
    <div class="item__sub">Pickup / Drop for sharing</div>
    <div class="grid">
      <label class="field">
        <span>Pickup city</span>
        <select class="pickup"></select>
      </label>
      <label class="field">
        <span>Drop city</span>
        <select class="drop"></select>
      </label>
    </div>
    <div class="item__actions">
      <button class="btn btn--primary" data-act="propose">Request share</button>
      <button class="btn" data-act="show">Show path</button>
    </div>
  `;
  setOptions(el.querySelector("select.pickup"), graph.cities);
  setOptions(el.querySelector("select.drop"), graph.cities);
  el.querySelector("select.pickup").value = $("fromCity").value;
  el.querySelector("select.drop").value = $("toCity").value;

  el.addEventListener("click", async (e) => {
    const btn = e.target.closest("button");
    if (!btn) return;
    if (btn.dataset.act === "show") {
      selectedRoute = candidate.route;
      drawMap();
      return;
    }
    const pickupCityId = el.querySelector("select.pickup").value;
    const dropCityId = el.querySelector("select.drop").value;
    try {
      await apiPost("/api/share/propose", {
        requestId,
        rideId: candidate.rideId,
        pickupCityId,
        dropCityId
      });
      await refreshAll();
      alert("Share request sent to the rider in that cab. They must approve.");
    } catch (err) {
      alert(err.message);
    }
  });

  return el;
}

function wireEvents() {
  $("refreshBtn").addEventListener("click", refreshAll);
  $("driverSelect").addEventListener("change", renderDriverView);

  $("submitRide").addEventListener("click", async () => {
    const userName = $("userName").value.trim();
    const fromCityId = $("fromCity").value;
    const toCityId = $("toCity").value;
    const wantsSharing = $("wantsSharing").checked;
    if (!userName) {
      alert("Enter a username.");
      return;
    }
    try {
      const res = await apiPost("/api/request", { userName, fromCityId, toCityId, wantsSharing });
      const req = res.request;
      lastRequestId = req.id;
      selectedRoute = req.route;
      $("userRequestResult").innerHTML = `
        <div class="pill">Request: <b>${req.id}</b></div>
        <div class="pill">Assigned driver: <b>${req.assignedDriverId ?? "any"}</b></div>
        <div class="pill">Solo fare: <b>₹${req.soloFare}</b></div>
        <div class="muted" style="margin-top:8px">Route: ${routeText(req.route)} (distance: ${req.route.distance})</div>
      `;
      await refreshAll();
    } catch (err) {
      $("userRequestResult").textContent = err.message;
    }
  });

  $("findShareBtn").addEventListener("click", async () => {
    const container = $("shareCandidates");
    container.innerHTML = "";
    if (!lastRequestId) {
      container.innerHTML = `<div class="muted">Submit a request with “Enable sharing” first.</div>`;
      return;
    }
    try {
      const res = await apiGet(`/api/share/candidates?requestId=${encodeURIComponent(lastRequestId)}`);
      if (!res.candidates.length) {
        container.innerHTML = `<div class="muted">No active rides found going the same way right now.</div>`;
        return;
      }
      for (const c of res.candidates) {
        container.appendChild(shareCandidateCard(c, lastRequestId));
      }
    } catch (err) {
      container.innerHTML = `<div class="muted">${err.message}</div>`;
    }
  });

  $("userName").addEventListener("input", () => renderIncomingShares($("userName").value.trim()));
}

async function main() {
  wireEvents();
  await refreshAll();
}

main().catch((e) => {
  // eslint-disable-next-line no-console
  console.error(e);
  alert(e.message);
});

