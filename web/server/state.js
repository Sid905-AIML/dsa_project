import { punjabGraph } from "./punjabGraph.js";

function nowIso() {
  return new Date().toISOString();
}

function edgeKey(a, b) {
  return a < b ? `${a}::${b}` : `${b}::${a}`;
}

function buildAdjacency(graph) {
  const byId = new Map(graph.cities.map((c) => [c.id, c]));
  const adj = new Map(graph.cities.map((c) => [c.id, []]));
  const weights = new Map();

  for (const [a, b] of graph.roads) {
    const ca = byId.get(a);
    const cb = byId.get(b);
    if (!ca || !cb) continue;
    const dx = ca.x - cb.x;
    const dy = ca.y - cb.y;
    const w = Math.max(1, Math.round(Math.sqrt(dx * dx + dy * dy) / 20));
    adj.get(a).push({ to: b, w });
    adj.get(b).push({ to: a, w });
    weights.set(edgeKey(a, b), w);
  }

  return { byId, adj, weights };
}

const { byId: cityById, adj } = buildAdjacency(punjabGraph);

export function shortestPath(fromId, toId) {
  if (!cityById.has(fromId) || !cityById.has(toId)) {
    return { path: [], distance: Infinity };
  }
  if (fromId === toId) return { path: [fromId], distance: 0 };

  const dist = new Map();
  const prev = new Map();
  const visited = new Set();
  for (const id of cityById.keys()) dist.set(id, Infinity);
  dist.set(fromId, 0);

  while (visited.size < cityById.size) {
    let u = null;
    let best = Infinity;
    for (const [id, d] of dist.entries()) {
      if (visited.has(id)) continue;
      if (d < best) {
        best = d;
        u = id;
      }
    }
    if (u === null) break;
    if (u === toId) break;
    visited.add(u);

    for (const { to, w } of adj.get(u) ?? []) {
      if (visited.has(to)) continue;
      const alt = dist.get(u) + w;
      if (alt < dist.get(to)) {
        dist.set(to, alt);
        prev.set(to, u);
      }
    }
  }

  const d = dist.get(toId);
  if (!Number.isFinite(d) || d === Infinity) return { path: [], distance: Infinity };

  const path = [];
  let cur = toId;
  while (cur) {
    path.push(cur);
    cur = prev.get(cur);
    if (cur === fromId) {
      path.push(fromId);
      break;
    }
  }
  path.reverse();
  return { path, distance: d };
}

export function computeSoloFare(distance) {
  const base = 40;
  const perUnit = 15;
  return Math.round(base + perUnit * distance);
}

export function computeSharedPricing(soloFare, riders) {
  // Requirement: each rider pays less than solo, but total > solo.
  // We do this by: total = solo * 1.30, split equally.
  const total = Math.round(soloFare * 1.3);
  const each = Math.round(total / riders);
  return { each, total };
}

let nextId = 1;
function id(prefix) {
  nextId += 1;
  return `${prefix}_${nextId}`;
}

export const state = {
  graph: punjabGraph,
  drivers: [
    { id: "d1", name: "Ravi", cityId: "amritsar", status: "idle" },
    { id: "d2", name: "Priya", cityId: "ludhiana", status: "idle" },
    { id: "d3", name: "Arjun", cityId: "bathinda", status: "idle" },
    { id: "d4", name: "Simran", cityId: "jalandhar", status: "idle" }
  ],
  requests: [],
  rides: [],
  notifications: []
};

export function snapshot() {
  return {
    serverTime: nowIso(),
    drivers: state.drivers,
    requests: state.requests,
    rides: state.rides,
    notifications: state.notifications.slice(-50)
  };
}

function nearestIdleDriver(fromCityId) {
  const idle = state.drivers.filter((d) => d.status === "idle");
  if (idle.length === 0) return null;
  let best = null;
  let bestDist = Infinity;
  for (const d of idle) {
    const sp = shortestPath(d.cityId, fromCityId);
    if (sp.distance < bestDist) {
      bestDist = sp.distance;
      best = d;
    }
  }
  return best;
}

export function createRideRequest({ userName, fromCityId, toCityId, wantsSharing }) {
  const route = shortestPath(fromCityId, toCityId);
  if (!route.path.length) {
    return { ok: false, error: "No route found between selected cities." };
  }

  const request = {
    id: id("req"),
    userName,
    fromCityId,
    toCityId,
    wantsSharing: !!wantsSharing,
    status: "pending_driver", // pending_driver | accepted | declined | cancelled
    createdAt: nowIso(),
    route,
    soloFare: computeSoloFare(route.distance),
    assignedDriverId: null,
    rideId: null
  };

  const driver = nearestIdleDriver(fromCityId);
  if (driver) {
    request.assignedDriverId = driver.id;
  }

  state.requests.push(request);
  state.notifications.push({
    id: id("n"),
    type: "request_created",
    at: nowIso(),
    message: `New request by ${userName}: ${fromCityId} → ${toCityId}`,
    requestId: request.id,
    driverId: request.assignedDriverId
  });

  return { ok: true, request };
}

export function driverRespond({ driverId, requestId, accept }) {
  const req = state.requests.find((r) => r.id === requestId);
  const driver = state.drivers.find((d) => d.id === driverId);
  if (!req) return { ok: false, error: "Request not found." };
  if (!driver) return { ok: false, error: "Driver not found." };
  if (req.status !== "pending_driver") return { ok: false, error: "Request is not pending." };
  if (req.assignedDriverId && req.assignedDriverId !== driverId) {
    return { ok: false, error: "This request is assigned to a different driver." };
  }
  if (driver.status !== "idle") return { ok: false, error: "Driver is not idle." };

  if (!accept) {
    req.status = "declined";
    state.notifications.push({
      id: id("n"),
      type: "request_declined",
      at: nowIso(),
      message: `Driver ${driver.name} declined request ${req.id}.`,
      requestId: req.id,
      driverId
    });
    return { ok: true, request: req };
  }

  const ride = {
    id: id("ride"),
    driverId,
    driverName: driver.name,
    primaryUser: req.userName,
    passengers: [
      {
        userName: req.userName,
        pickupCityId: req.fromCityId,
        dropCityId: req.toCityId,
        pricing: { mode: "solo", soloFare: req.soloFare, pay: req.soloFare }
      }
    ],
    fromCityId: req.fromCityId,
    toCityId: req.toCityId,
    route: req.route,
    status: "active", // active | completed
    shareProposals: [] // proposals from other users to join
  };

  req.status = "accepted";
  req.rideId = ride.id;
  req.assignedDriverId = driverId;
  driver.status = "on_ride";
  driver.cityId = req.toCityId;

  state.rides.push(ride);
  state.notifications.push({
    id: id("n"),
    type: "ride_started",
    at: nowIso(),
    message: `Driver ${driver.name} accepted ${req.userName}'s ride.`,
    requestId: req.id,
    rideId: ride.id,
    driverId
  });
  return { ok: true, request: req, ride };
}

function sameDirectionRouteScore(aRoute, bRoute) {
  // Simple heuristic: share candidates if they share at least 2 consecutive nodes.
  const a = aRoute.path;
  const b = bRoute.path;
  let best = 0;
  for (let i = 0; i < a.length - 1; i++) {
    for (let j = 0; j < b.length - 1; j++) {
      if (a[i] === b[j] && a[i + 1] === b[j + 1]) {
        best = Math.max(best, 2);
      }
    }
  }
  return best;
}

export function listShareCandidates(requestId) {
  const req = state.requests.find((r) => r.id === requestId);
  if (!req) return { ok: false, error: "Request not found." };
  if (!req.wantsSharing) return { ok: false, error: "Request not marked for sharing." };
  if (req.status !== "pending_driver") return { ok: false, error: "Sharing is only available before driver acceptance." };

  const candidates = state.rides
    .filter((ride) => ride.status === "active")
    .map((ride) => {
      const score = sameDirectionRouteScore(req.route, ride.route);
      return { rideId: ride.id, driverName: ride.driverName, driverId: ride.driverId, score, route: ride.route };
    })
    .filter((c) => c.score >= 2);

  return { ok: true, candidates };
}

export function proposeShare({ requestId, rideId, pickupCityId, dropCityId }) {
  const req = state.requests.find((r) => r.id === requestId);
  const ride = state.rides.find((r) => r.id === rideId);
  if (!req) return { ok: false, error: "Request not found." };
  if (!ride) return { ok: false, error: "Ride not found." };
  if (req.status !== "pending_driver") return { ok: false, error: "Request is not pending." };
  if (!req.wantsSharing) return { ok: false, error: "Request not marked for sharing." };

  const proposal = {
    id: id("share"),
    fromUser: req.userName,
    requestId: req.id,
    pickupCityId,
    dropCityId,
    status: "pending", // pending | accepted | rejected
    createdAt: nowIso()
  };
  ride.shareProposals.push(proposal);

  state.notifications.push({
    id: id("n"),
    type: "share_proposed",
    at: nowIso(),
    message: `${req.userName} requested to share ride ${ride.id} (driver ${ride.driverName}).`,
    rideId: ride.id,
    requestId: req.id,
    toUser: ride.primaryUser
  });

  return { ok: true, proposal };
}

export function decideShare({ rideId, proposalId, accept }) {
  const ride = state.rides.find((r) => r.id === rideId);
  if (!ride) return { ok: false, error: "Ride not found." };
  const proposal = ride.shareProposals.find((p) => p.id === proposalId);
  if (!proposal) return { ok: false, error: "Proposal not found." };
  if (proposal.status !== "pending") return { ok: false, error: "Proposal already decided." };

  if (!accept) {
    proposal.status = "rejected";
    state.notifications.push({
      id: id("n"),
      type: "share_rejected",
      at: nowIso(),
      message: `${ride.primaryUser} rejected share request from ${proposal.fromUser}.`,
      rideId,
      requestId: proposal.requestId
    });
    return { ok: true, ride };
  }

  // Accept: convert ride to shared pricing for 2 riders.
  proposal.status = "accepted";
  const primary = ride.passengers[0];
  const soloFare = primary.pricing.soloFare ?? computeSoloFare(ride.route.distance);
  const pricing = computeSharedPricing(soloFare, 2);

  primary.pricing = { mode: "shared", soloFare, pay: pricing.each };
  ride.passengers.push({
    userName: proposal.fromUser,
    pickupCityId: proposal.pickupCityId,
    dropCityId: proposal.dropCityId,
    pricing: { mode: "shared", soloFare, pay: pricing.each }
  });

  const req = state.requests.find((r) => r.id === proposal.requestId);
  if (req) {
    req.status = "accepted";
    req.assignedDriverId = ride.driverId;
    req.rideId = ride.id;
  }

  state.notifications.push({
    id: id("n"),
    type: "share_accepted",
    at: nowIso(),
    message: `${ride.primaryUser} accepted sharing with ${proposal.fromUser}. Driver notified for pickup/drop.`,
    rideId,
    requestId: proposal.requestId,
    driverId: ride.driverId
  });

  return { ok: true, ride, pricing };
}

