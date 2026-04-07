/*
 * driver.cpp
 * ----------
 * Driver registration, active ride tracking,
 * ride completion, and driver status display.
 */

#include "types.h"

// ─────────────────────────────────────────────
//  Driver Registration
// ─────────────────────────────────────────────

bool register_driver(int id, string name, int x, int y) {
    if (driver_count >= MAX_DRIVERS) {
        cout << "[ERROR] Driver pool is full.\n";
        return false;
    }
    if (find_driver_index_by_id(id) != -1) {
        cout << "[ERROR] Driver ID " << id << " already exists.\n";
        return false;
    }
    Driver d;
    d.driver_id     = id;
    d.name          = name;
    d.location.x   = x;
    d.location.y   = y;
    d.state         = DRIVER_IDLE;
    d.is_registered = true;
    driver_pool[driver_count++] = d;
    cout << "[DRIVER REGISTERED] " << name
         << " at (" << x << "," << y << ") | ID: " << id << "\n";
    return true;
}

// ─────────────────────────────────────────────
//  Driver Lookup
// ─────────────────────────────────────────────

// Returns pool index for driver_id, or -1 if not found
int find_driver_index_by_id(int driver_id) {
    for (int i = 0; i < driver_count; i++) {
        if (driver_pool[i].is_registered && driver_pool[i].driver_id == driver_id)
            return i;
    }
    return -1;
}

// Returns pool index of nearest IDLE driver to src, or -1 if none
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

void mark_driver_on_ride(int pool_index) {
    driver_pool[pool_index].state = DRIVER_ON_RIDE;
}

// ─────────────────────────────────────────────
//  Active Ride Tracker
// ─────────────────────────────────────────────

void record_active_ride(int driver_id, RideRequest req) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (!active_rides[i].in_use) {
            active_rides[i].in_use     = true;
            active_rides[i].driver_id  = driver_id;
            active_rides[i].request_id = req.request_id;
            active_rides[i].rider_name = req.user_name;
            active_rides[i].co_rider   = req.shared_with;
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

// Returns name of rider in driver's cab, or empty string if idle
string get_rider_for_driver(int driver_id) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (active_rides[i].in_use && active_rides[i].driver_id == driver_id)
            return active_rides[i].rider_name;
    }
    return "";
}

// ─────────────────────────────────────────────
//  Ride Completion
// ─────────────────────────────────────────────

void complete_ride(int driver_id) {
    print_separator('=');
    cout << "[RIDE COMPLETE]\n";

    int idx = find_driver_index_by_id(driver_id);
    if (idx == -1) {
        cout << "[ERROR] Driver ID " << driver_id << " not found.\n";
        print_separator('=');
        return;
    }
    if (driver_pool[idx].state != DRIVER_ON_RIDE) {
        cout << "[ERROR] Driver " << driver_pool[idx].name
             << " is already IDLE — no active ride to complete.\n";
        print_separator('=');
        return;
    }

    clear_active_ride(driver_id);
    driver_pool[idx].state = DRIVER_IDLE;
    // Location is already set to the drop-off point from when ride was assigned
    cout << "  Driver " << driver_pool[idx].name
         << " is now IDLE at ("
         << driver_pool[idx].location.x << ","
         << driver_pool[idx].location.y << ")\n";
    total_rides_completed++;

    // Auto-assign waiting riders with the newly freed driver
    try_dispatch_waiting();
    print_separator('=');
}

// ─────────────────────────────────────────────
//  Driver Display
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

void display_active_rides() {
    print_separator('=');
    cout << "  ACTIVE RIDES\n";
    print_separator('=');
    bool any = false;
    for (int i = 0; i < MAX_DRIVERS; i++) {
        if (!active_rides[i].in_use) continue;
        any = true;
        int idx = find_driver_index_by_id(active_rides[i].driver_id);
        string dname = (idx != -1) ? driver_pool[idx].name : "?";
        cout << "  Driver : " << dname
             << " (ID: " << active_rides[i].driver_id << ")"
             << " | Rider: " << active_rides[i].rider_name
             << " | Req #"  << active_rides[i].request_id;
        if (!active_rides[i].co_rider.empty())
            cout << " [shared with " << active_rides[i].co_rider << "]";
        cout << "\n";
        cout << "    >> To complete: option 3 -> Driver ID: "
             << active_rides[i].driver_id << "\n";
    }
    if (!any) cout << "  No active rides right now.\n";
    print_separator('=');
}
