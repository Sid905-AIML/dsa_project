/*
 * ============================================================
 *  Queue-Based Cab Allocation & Ride Sharing System
 *  DSA Project  |  C++
 * ============================================================
 *  Core Data Structures:
 *    - Request Queue  : FIFO queue of incoming ride requests
 *    - Waiting Queue  : Unmatched / shareable requests
 *    - Driver Pool    : Fixed array of registered drivers
 *    - Active Rides   : Tracks driver -> rider mapping
 *
 *  Design:
 *    - Manhattan distance for fast proximity checks
 *    - FIFO ensures fair request handling
 *    - Queue scan for ride-sharing before assigning driver
 *    - No enum / auto / static_cast -- plain readable types
 * ============================================================
 */

#include <iostream>
#include <string>
#include <queue>
#include <cmath>     // abs()
#include <climits>   // INT_MAX

using namespace std;

// ─────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────
const int MAX_DRIVERS     = 20;  // max drivers in pool
const int SHARE_THRESHOLD = 3;   // max distance to allow ride-sharing
const int DRIVER_IDLE     = 0;   // driver is available
const int DRIVER_ON_RIDE  = 1;   // driver is currently on a ride

// ─────────────────────────────────────────────
//  Data Structures
// ─────────────────────────────────────────────

struct Location {
    int x;
    int y;
};

struct RideRequest {
    int      request_id;
    string   user_name;
    Location source;
    Location destination;
    bool     wants_sharing;      // true = open to sharing
    bool     is_shared;          // true = merged into a shared ride
    string   shared_with;        // co-passenger name (if shared)
    int      assigned_driver_id; // -1 = not yet assigned
};

struct Driver {
    int      driver_id;
    string   name;
    Location location;
    int      state;          // DRIVER_IDLE or DRIVER_ON_RIDE
    bool     is_registered;  // false = empty slot
};

// Tracks one live ride: which driver is carrying which rider
struct ActiveRide {
    int    driver_id;
    int    request_id;
    string rider_name;
    string co_rider;     // empty if solo ride
    bool   in_use;       // false = empty slot
};

// ─────────────────────────────────────────────
//  Global State
// ─────────────────────────────────────────────

queue<RideRequest> request_queue;  // incoming FIFO requests
queue<RideRequest> waiting_queue;  // waiting for a free driver

Driver     driver_pool[MAX_DRIVERS];
int        driver_count = 0;

ActiveRide active_rides[MAX_DRIVERS];  // one slot per possible driver
int        next_request_id      = 1;
int        total_rides_completed = 0;
int        total_shared_rides    = 0;

// ─────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────

int manhattan_distance(Location a, Location b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

void print_separator(char ch = '-', int width = 55) {
    for (int i = 0; i < width; i++) cout << ch;
    cout << "\n";
}

// Returns index in driver_pool for a given driver_id, or -1 if not found
int find_driver_index_by_id(int driver_id) {
    for (int i = 0; i < driver_count; i++) {
        if (driver_pool[i].is_registered && driver_pool[i].driver_id == driver_id)
            return i;
    }
    return -1;
}

// Returns index of nearest IDLE driver to 'src', or -1 if none
int find_nearest_driver(Location src) {
    int best_index    = -1;
    int best_distance = INT_MAX;
    for (int i = 0; i < driver_count; i++) {
        if (!driver_pool[i].is_registered) continue;
        if (driver_pool[i].state != DRIVER_IDLE) continue;
        int dist = manhattan_distance(driver_pool[i].location, src);
        if (dist < best_distance) {
            best_distance = dist;
            best_index    = i;
        }
    }
    return best_index;
}

// ─────────────────────────────────────────────
//  Active Ride Tracker
// ─────────────────────────────────────────────

void record_active_ride(int driver_id, RideRequest req) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (!active_rides[i].in_use) {
            active_rides[i].in_use      = true;
            active_rides[i].driver_id   = driver_id;
            active_rides[i].request_id  = req.request_id;
            active_rides[i].rider_name  = req.user_name;
            active_rides[i].co_rider    = req.shared_with;
            return;
        }
    }
}

void clear_active_ride(int driver_id) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (active_rides[i].in_use && active_rides[i].driver_id == driver_id) {
            active_rides[i].in_use = false;
            return;
        }
    }
}

// Returns the rider name for a driver currently ON_RIDE, or empty string
string get_rider_for_driver(int driver_id) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (active_rides[i].in_use && active_rides[i].driver_id == driver_id)
            return active_rides[i].rider_name;
    }
    return "";
}

// ─────────────────────────────────────────────
//  Driver Management
// ─────────────────────────────────────────────

bool register_driver(int id, string name, int x, int y) {
    if (driver_count >= MAX_DRIVERS) {
        cout << "[ERROR] Driver pool is full.\n";
        return false;
    }
    // Prevent duplicate driver IDs
    if (find_driver_index_by_id(id) != -1) {
        cout << "[ERROR] Driver ID " << id << " already exists.\n";
        return false;
    }
    Driver d;
    d.driver_id      = id;
    d.name           = name;
    d.location.x    = x;
    d.location.y    = y;
    d.state          = DRIVER_IDLE;
    d.is_registered  = true;
    driver_pool[driver_count++] = d;
    cout << "[DRIVER REGISTERED] " << name << " at (" << x << "," << y
         << ") | ID: " << id << "\n";
    return true;
}

// ─────────────────────────────────────────────
//  Ride Completion
// ─────────────────────────────────────────────

// Forward-declare for auto-dispatch after completion
void try_dispatch_waiting();

void complete_ride(int driver_id) {
    print_separator('=');
    cout << "[RIDE COMPLETE]\n";

    int idx = find_driver_index_by_id(driver_id);
    if (idx == -1) {
        cout << "[ERROR] Driver ID " << driver_id << " not found.\n";
        print_separator('=');
        return;
    }
    // Guard: driver must actually be ON_RIDE
    if (driver_pool[idx].state != DRIVER_ON_RIDE) {
        cout << "[ERROR] Driver " << driver_pool[idx].name
             << " is already IDLE — no active ride to complete.\n";
        print_separator('=');
        return;
    }

    clear_active_ride(driver_id);
    driver_pool[idx].state = DRIVER_IDLE;
    // Location stays at destination (already set when ride was assigned)
    cout << "  Driver " << driver_pool[idx].name
         << " is now IDLE at ("
         << driver_pool[idx].location.x << ","
         << driver_pool[idx].location.y << ")\n";
    total_rides_completed++;

    // Auto-dispatch waiting riders with the newly freed driver
    try_dispatch_waiting();
    print_separator('=');
}

// ─────────────────────────────────────────────
//  Ride Sharing Logic
// ─────────────────────────────────────────────
/*
 *  Scans waiting_queue for a compatible co-passenger.
 *  Both must want sharing AND source+dest proximity <= SHARE_THRESHOLD.
 *  Returns true + fills matched_req if a match is found.
 */
bool try_ride_share(RideRequest &new_req, RideRequest &matched_req) {
    if (!new_req.wants_sharing) return false;

    queue<RideRequest> temp;
    bool found = false;

    while (!waiting_queue.empty()) {
        RideRequest candidate = waiting_queue.front();
        waiting_queue.pop();

        if (!found && candidate.wants_sharing) {
            int src_dist = manhattan_distance(new_req.source,      candidate.source);
            int dst_dist = manhattan_distance(new_req.destination,  candidate.destination);
            if ((src_dist + dst_dist) <= SHARE_THRESHOLD) {
                matched_req = candidate;
                found = true;
                continue;  // don't push back — it's being matched
            }
        }
        temp.push(candidate);
    }
    waiting_queue = temp;
    return found;
}

// ─────────────────────────────────────────────
//  Core Assignment Logic (shared by instant + batch dispatch)
// ─────────────────────────────────────────────

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
    driver_pool[driver_idx].state = DRIVER_ON_RIDE;
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

    // Advance driver's position to the dropoff point
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
        if (idx == -1) {
            still_waiting.push(w);
        } else {
            assign_driver_to_request(w, false);
        }
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
         << " | (" << src_x << "," << src_y << ") -> (" << dst_x << "," << dst_y << ")"
         << " | Share: " << (wants_sharing ? "YES" : "NO") << "\n";

    if (show_booking_header) {
        print_separator('=');
        cout << "  BOOKING STATUS FOR REQUEST #" << req.request_id << "\n";
        print_separator('=');
    }

    // Try ride-share match before assigning driver
    RideRequest matched;
    bool share_found = try_ride_share(req, matched);
    if (share_found) {
        req.is_shared       = true;
        req.shared_with     = matched.user_name;
        matched.is_shared   = true;
        matched.shared_with = req.user_name;
        cout << "  Ride-share match found with " << matched.user_name
             << " (Request #" << matched.request_id << ")\n";
        // Matched rider piggybacks on this assignment
    }

    assign_driver_to_request(req, show_booking_header);
}

// ─────────────────────────────────────────────
//  Batch Process (option 2) — processes any
//  requests left in request_queue
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
//  Status Display
// ─────────────────────────────────────────────

void display_driver_status() {
    print_separator('=');
    cout << "  DRIVER STATUS BOARD\n";
    print_separator('=');
    cout << "  ID  | Name            | Location | State    | Carrying\n";
    print_separator();
    for (int i = 0; i < driver_count; i++) {
        if (!driver_pool[i].is_registered) continue;

        string state_str = (driver_pool[i].state == DRIVER_IDLE) ? "IDLE    " : "ON RIDE ";
        string rider     = (driver_pool[i].state == DRIVER_ON_RIDE)
                               ? get_rider_for_driver(driver_pool[i].driver_id)
                               : "-";

        cout << "  " << driver_pool[i].driver_id << "   | " << driver_pool[i].name;
        int pad = 15 - (int)driver_pool[i].name.size();
        for (int p = 0; p < pad; p++) cout << " ";
        cout << " | (" << driver_pool[i].location.x << ","
                       << driver_pool[i].location.y << ")    | "
             << state_str << " | " << rider << "\n";
    }
    print_separator();
}

void display_queue_status() {
    print_separator('=');
    cout << "  QUEUE STATUS\n";
    print_separator('=');

    // Count idle/busy drivers
    int idle_count = 0;
    for (int i = 0; i < driver_count; i++) {
        if (driver_pool[i].is_registered && driver_pool[i].state == DRIVER_IDLE)
            idle_count++;
    }

    cout << "  Drivers          : " << driver_count << " total, "
         << idle_count << " idle, "
         << (driver_count - idle_count) << " on ride\n";
    cout << "  Pending Queue    : " << request_queue.size() << " requests\n";
    cout << "  Waiting Queue    : " << waiting_queue.size() << " riders\n";
    cout << "  Rides Completed  : " << total_rides_completed << "\n";
    cout << "  Shared Rides     : " << total_shared_rides << "\n";

    // List waiting riders by name
    if (!waiting_queue.empty()) {
        print_separator();
        cout << "  Waiting riders:\n";
        queue<RideRequest> temp = waiting_queue;
        while (!temp.empty()) {
            RideRequest w = temp.front(); temp.pop();
            cout << "    #" << w.request_id << " " << w.user_name
                 << " | (" << w.source.x << "," << w.source.y
                 << ") -> (" << w.destination.x << "," << w.destination.y << ")"
                 << (w.wants_sharing ? " [share ok]" : "") << "\n";
        }
    }
    print_separator('=');
}

void display_active_rides() {
    print_separator('=');
    cout << "  ACTIVE RIDES\n";
    print_separator('=');

    bool any = false;
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (!active_rides[i].in_use) continue;
        any = true;
        // Find driver name
        int idx = find_driver_index_by_id(active_rides[i].driver_id);
        string dname = (idx != -1) ? driver_pool[idx].name : "?";

        cout << "  Driver: " << dname
             << " (ID: " << active_rides[i].driver_id << ")"
             << " | Rider: " << active_rides[i].rider_name
             << " | Req #" << active_rides[i].request_id;
        if (!active_rides[i].co_rider.empty())
            cout << " [shared with " << active_rides[i].co_rider << "]";
        cout << "\n";
        cout << "    >> To complete: option 3 -> Driver ID: "
             << active_rides[i].driver_id << "\n";
    }
    if (!any) cout << "  No active rides right now.\n";
    print_separator('=');
}

// ─────────────────────────────────────────────
//  Cancel a Waiting Request
// ─────────────────────────────────────────────

void cancel_waiting_request(int request_id) {
    queue<RideRequest> temp;
    bool found = false;
    while (!waiting_queue.empty()) {
        RideRequest w = waiting_queue.front(); waiting_queue.pop();
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

// ─────────────────────────────────────────────
//  Interactive Menu
// ─────────────────────────────────────────────

void show_menu() {
    print_separator('=');
    cout << "  CAB ALLOCATION SYSTEM\n";
    print_separator('=');
    cout << "  1. Submit a Ride Request\n";
    cout << "  2. Process Pending Queue (batch)\n";
    cout << "  3. Mark a Ride as Complete\n";
    cout << "  4. View Driver Status\n";
    cout << "  5. View Queue & Stats\n";
    cout << "  6. Add a New Driver\n";
    cout << "  7. View Active Rides\n";
    cout << "  8. Cancel a Waiting Request\n";
    cout << "  0. Exit\n";
    print_separator();
    cout << "  Enter choice: ";
}

void menu_submit_request() {
    string name;
    int sx, sy, dx, dy;
    char share_ch;
    cout << "\n  User Name         : "; cin >> name;
    cout << "  Source X Y        : "; cin >> sx >> sy;
    cout << "  Destination X Y   : "; cin >> dx >> dy;
    cout << "  Want ride sharing? (y/n): "; cin >> share_ch;
    bool share = (share_ch == 'y' || share_ch == 'Y');
    submit_and_dispatch(name, sx, sy, dx, dy, share, true);
}

void menu_complete_ride() {
    int driver_id;
    cout << "\n  Driver ID (shown when ride was assigned): ";
    cin >> driver_id;
    complete_ride(driver_id);
}

void menu_add_driver() {
    int id, x, y;
    string name;
    cout << "\n  Driver ID    : "; cin >> id;
    cout << "  Driver Name  : "; cin >> name;
    cout << "  Location X Y : "; cin >> x >> y;
    register_driver(id, name, x, y);
}

void menu_cancel_request() {
    int req_id;
    cout << "\n  Request ID to cancel: "; cin >> req_id;
    cancel_waiting_request(req_id);
}

// ─────────────────────────────────────────────
//  Demo Seed Data
// ─────────────────────────────────────────────

void load_demo_data() {
    cout << "\n[DEMO] Setting up 3 drivers and 5 ride requests...\n\n";

    register_driver(101, "Ravi",  2, 3);
    register_driver(102, "Priya", 8, 1);
    register_driver(103, "Arjun", 5, 7);
    cout << "\n";

    // Each request is submitted and immediately dispatched
    submit_and_dispatch("Alice",   1, 2,  6, 8,  true,  false);
    submit_and_dispatch("Bob",     2, 3,  6, 9,  true,  false);
    submit_and_dispatch("Charlie", 9, 0,  3, 3,  false, false);
    submit_and_dispatch("Diana",   5, 6,  1, 1,  true,  false);
    submit_and_dispatch("Eve",    10,10,  0, 0,  false, false);
    cout << "\n";
}

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────

int main() {
    // Zero out active rides array
    for (int i = 0; i < MAX_DRIVERS; i++)
        active_rides[i].in_use = false;

    cout << "\n";
    print_separator('=');
    cout << "   QUEUE-BASED CAB ALLOCATION & RIDE SHARING SYSTEM\n";
    print_separator('=');

    load_demo_data();
    display_driver_status();
    display_queue_status();

    int choice = -1;
    while (choice != 0) {
        show_menu();
        cin >> choice;
        cout << "\n";

        if      (choice == 1) menu_submit_request();
        else if (choice == 2) { process_pending_queue(); display_driver_status(); }
        else if (choice == 3) menu_complete_ride();
        else if (choice == 4) display_driver_status();
        else if (choice == 5) display_queue_status();
        else if (choice == 6) menu_add_driver();
        else if (choice == 7) display_active_rides();
        else if (choice == 8) menu_cancel_request();
        else if (choice == 0) cout << "  Exiting. Goodbye!\n";
        else                  cout << "  [ERROR] Invalid option.\n";

        cout << "\n";
    }
    return 0;
}
