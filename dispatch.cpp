/*
 * dispatch.cpp
 * ------------
 * Ride sharing scan, driver assignment logic,
 * queue dispatch, request submission, and cancellation.
 */

#include "types.h"

// ─────────────────────────────────────────────
//  Ride Sharing
// ─────────────────────────────────────────────
/*
 *  Scans the waiting_queue for a compatible co-passenger.
 *  Both must want sharing AND combined src+dst distance <= SHARE_THRESHOLD.
 *  Returns true and fills matched_req if a match is found.
 *  The matched request is removed from waiting_queue.
 */
bool try_ride_share(RideRequest &new_req, RideRequest &matched_req) {
    if (!new_req.wants_sharing) return false;

    queue<RideRequest> temp;
    bool found = false;

    while (!waiting_queue.empty()) {
        RideRequest candidate = waiting_queue.front();
        waiting_queue.pop();

        if (!found && candidate.wants_sharing) {
            int src_dist = manhattan_distance(new_req.source,     candidate.source);
            int dst_dist = manhattan_distance(new_req.destination, candidate.destination);
            if ((src_dist + dst_dist) <= SHARE_THRESHOLD) {
                matched_req = candidate;
                found = true;
                continue;  // don't re-queue — it's being matched
            }
        }
        temp.push(candidate);
    }
    waiting_queue = temp;
    return found;
}

// ─────────────────────────────────────────────
//  Core Assignment
// ─────────────────────────────────────────────
/*
 *  Finds the nearest idle driver for req.source.
 *  If found: marks driver ON_RIDE, records the active ride,
 *            moves driver position to destination, prints result.
 *  If not found: pushes req to waiting_queue.
 *
 *  verbose = true  -> full booking confirmation block (for user submissions)
 *  verbose = false -> compact one-line log (for batch/auto dispatch)
 */
void assign_driver_to_request(RideRequest req, bool verbose) {
    int driver_idx = find_nearest_driver(req.source);

    if (driver_idx == -1) {
        if (verbose) {
            cout << "\n  *** NO DRIVER AVAILABLE RIGHT NOW ***\n";
            cout << "  Your request has been queued. A driver will\n";
            cout << "  be assigned as soon as one becomes free.\n";
            print_separator('=');
        }
        waiting_queue.push(req);
        return;
    }

    req.assigned_driver_id = driver_pool[driver_idx].driver_id;
    mark_driver_on_ride(driver_idx);
    record_active_ride(driver_pool[driver_idx].driver_id, req);

    int dist = manhattan_distance(driver_pool[driver_idx].location, req.source);

    if (req.is_shared) total_shared_rides++;

    if (verbose) {
        cout << "\n  DRIVER ASSIGNED!\n";
        cout << "  ─────────────────────────────────────────\n";
        cout << "  Driver Name   : " << driver_pool[driver_idx].name << "\n";
        cout << "  Driver ID     : " << driver_pool[driver_idx].driver_id << "\n";
        cout << "  Coming from   : (" << driver_pool[driver_idx].location.x
             << "," << driver_pool[driver_idx].location.y << ")\n";
        cout << "  Your pickup   : (" << req.source.x << "," << req.source.y << ")\n";
        cout << "  Distance away : " << dist << " blocks\n";
        if (req.is_shared)
            cout << "  Co-passenger  : " << req.shared_with << "\n";
        cout << "  >> To mark complete: option 3 -> Driver ID: "
             << driver_pool[driver_idx].driver_id << "\n";
        cout << "  ─────────────────────────────────────────\n";
        print_separator('=');
    } else {
        cout << "  [ASSIGNED] Request #" << req.request_id
             << " (" << req.user_name << ")"
             << " -> Driver " << driver_pool[driver_idx].name
             << " (ID: " << driver_pool[driver_idx].driver_id << ")"
             << " | " << dist << " blocks away\n";
        cout << "  >> To complete: option 3 -> Driver ID: "
             << driver_pool[driver_idx].driver_id << "\n";
    }

    // Advance driver's position to the drop-off point
    driver_pool[driver_idx].location = req.destination;
}

// ─────────────────────────────────────────────
//  Auto-dispatch Waiting Queue
// ─────────────────────────────────────────────

void try_dispatch_waiting() {
    if (waiting_queue.empty()) return;

    cout << "\n  Checking waiting queue (" << waiting_queue.size() << " riders)...\n";
    queue<RideRequest> still_waiting;

    while (!waiting_queue.empty()) {
        RideRequest w = waiting_queue.front();
        waiting_queue.pop();
        int idx = find_nearest_driver(w.source);
        if (idx == -1)
            still_waiting.push(w);
        else
            assign_driver_to_request(w, false);
    }
    waiting_queue = still_waiting;

    if (waiting_queue.empty())
        cout << "  All waiting riders have been assigned.\n";
    else
        cout << "  " << waiting_queue.size() << " rider(s) still waiting.\n";
}

// ─────────────────────────────────────────────
//  Request Submission
// ─────────────────────────────────────────────

void submit_and_dispatch(string user_name,
                         int src_x, int src_y,
                         int dst_x, int dst_y,
                         bool wants_sharing,
                         bool show_booking_header) {
    // Validate: source must differ from destination
    if (src_x == dst_x && src_y == dst_y) {
        cout << "[ERROR] Source and destination cannot be the same location.\n";
        return;
    }

    RideRequest req;
    req.request_id         = next_request_id++;
    req.user_name          = user_name;
    req.source.x           = src_x;
    req.source.y           = src_y;
    req.destination.x      = dst_x;
    req.destination.y      = dst_y;
    req.wants_sharing      = wants_sharing;
    req.is_shared          = false;
    req.shared_with        = "";
    req.assigned_driver_id = -1;

    cout << "[REQUEST] #" << req.request_id << " | " << user_name
         << " | (" << src_x << "," << src_y
         << ") -> (" << dst_x << "," << dst_y << ")"
         << " | Share: " << (wants_sharing ? "YES" : "NO") << "\n";

    if (show_booking_header) {
        print_separator('=');
        cout << "  BOOKING STATUS FOR REQUEST #" << req.request_id << "\n";
        print_separator('=');
    }

    // Try ride-share match before assigning a driver
    RideRequest matched;
    bool share_found = try_ride_share(req, matched);
    if (share_found) {
        req.is_shared       = true;
        req.shared_with     = matched.user_name;
        matched.is_shared   = true;
        matched.shared_with = req.user_name;
        cout << "  Ride-share match found with " << matched.user_name
             << " (Request #" << matched.request_id << ")\n";
    }

    assign_driver_to_request(req, show_booking_header);
}

// ─────────────────────────────────────────────
//  Batch Process (requests left in request_queue)
// ─────────────────────────────────────────────

void process_pending_queue() {
    if (request_queue.empty()) {
        cout << "  No pending requests in queue.\n";
        return;
    }
    print_separator('=');
    cout << "  BATCH PROCESSING (" << request_queue.size() << " pending)\n";
    print_separator('=');
    while (!request_queue.empty()) {
        RideRequest req = request_queue.front();
        request_queue.pop();
        print_separator();
        cout << "[PROCESSING] Request #" << req.request_id
             << " for " << req.user_name << "\n";
        RideRequest matched;
        bool share_found = try_ride_share(req, matched);
        if (share_found) {
            req.is_shared       = true;
            req.shared_with     = matched.user_name;
            matched.is_shared   = true;
            matched.shared_with = req.user_name;
            cout << "  Share match with " << matched.user_name << "\n";
        }
        assign_driver_to_request(req, false);
    }
}

// ─────────────────────────────────────────────
//  Cancel Waiting Request
// ─────────────────────────────────────────────

void cancel_waiting_request(int request_id) {
    queue<RideRequest> temp;
    bool found = false;
    while (!waiting_queue.empty()) {
        RideRequest w = waiting_queue.front();
        waiting_queue.pop();
        if (!found && w.request_id == request_id) {
            found = true;
            cout << "  [CANCELLED] Request #" << request_id
                 << " (" << w.user_name << ") removed from waiting queue.\n";
        } else {
            temp.push(w);
        }
    }
    waiting_queue = temp;
    if (!found)
        cout << "  [ERROR] Request #" << request_id
             << " not found in waiting queue.\n";
}
