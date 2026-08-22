/*
    ThreadController.h - Manages a fixed-size pool of Thread objects

    ThreadController extends Thread, so a controller can itself be nested
    inside another controller, enabling hierarchical scheduling.

    Original library:
        Created by Ivan Seidel Gomes, March 2013
        https://github.com/ivanseidel/ArduinoThread
        Released into the public domain.

    Modifications by Gordon Anderson / GAA Custom Electronics, LLC:
        - Added get(const char *name) to look up a thread by name
        - Corrected loop-exit condition in run() (< vs <=)
        - Switched interval parameter to unsigned long to match Thread base class
*/

#ifndef ThreadController_h
#define ThreadController_h

#include "Thread.h"
#include <inttypes.h>

// Maximum number of Thread slots managed by one controller.
// Increase if your application requires more concurrent tasks.
#define MAX_THREADS 15

class ThreadController : public Thread {
protected:
    // Fixed-size pointer array — NULL entries are empty slots.
    Thread *thread[MAX_THREADS];

    // Cached count of non-NULL slots; kept in sync by add/remove/clear.
    int cached_size;

public:
    // Construct with an optional run interval (ms).
    // Passing 0 (default) makes the controller run on every scheduler tick.
    ThreadController(unsigned long _interval = 0);

    // Run all threads whose shouldRun() returns true, then mark this
    // controller as having run.
    void run() override;

    // Identifies this object as a controller so callers walking a thread
    // list can recurse into it.  See Thread::isController().
    bool isController(void) const override { return true; }

    // --- Thread management ---

    // Add a thread to the first available slot.
    // Returns true on success, false if the pool is full or the thread
    // is already registered.
    bool add(Thread *_thread);

    // Remove a thread by ID or pointer.
    void remove(int _id);
    void remove(Thread *_thread);

    // Remove all threads and reset the pool to empty.
    void clear();

    // --- Queries ---

    // Return the number of registered threads.
    // Pass cached=false to recount from the array (slower but exact).
    int size(bool cached = true);

    // Return the Nth registered thread (by insertion order), or NULL.
    Thread *get(int index);

    // Return the first thread whose getName() matches, or NULL.
    Thread *get(const char *name);
};

#endif // ThreadController_h
