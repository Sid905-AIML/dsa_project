/*
 * main.cpp
 * --------
 * Global variable definitions, queue/stats display,
 * interactive menu, demo seed data, and main().
 */

#include "types.h"

// ─────────────────────────────────────────────
//  Global Variable Definitions
//  (declared extern in types.h, defined here)
// ─────────────────────────────────────────────

queue<RideRequest> request_queue;
queue<RideRequest> waiting_queue;
Driver             driver_pool[MAX_DRIVERS];
int                driver_count        = 0;
ActiveRide         active_rides[MAX_DRIVERS];
int                next_request_id      = 1;
int                total_rides_completed = 0;
int                total_shared_rides    = 0;

// ─────────────────────────────────────────────
//  Queue & Stats Display
// ─────────────────────────────────────────────

void display_queue_status() {
    print_separator('=');
    cout << "  QUEUE STATUS\n";
    print_separator('=');

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
