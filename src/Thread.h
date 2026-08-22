/*
    Thread.h - A schedulable callback object for Arduino cooperative multitasking

    A Thread holds a callback function and fires it on a configurable interval.
    It tracks whether it "should" run based on the current millisecond tick,
    and measures how long each run takes.

    Original library:
        Created by Ivan Seidel Gomes, March 2013
        https://github.com/ivanseidel/ArduinoThread
        Released into the public domain.

    Modifications by Gordon Anderson / GAA Custom Electronics, LLC:
        - Added setName() / getName() for human-readable thread identification
        - Added getID() to retrieve the unique thread ID
        - Added getInterval() to read the current interval setting
        - Added setNextRunTime() to manually schedule the next execution
        - Added run-time measurement (runTimeMs()) so callers can profile tasks
        - Fixed _cached_next_run calculation so interval is measured from
          dispatch time, not from completion time (runned() called before callback)
        - Added millis() rollover guard in shouldRun() for robust long-uptime
          operation
        - Switched all time values to unsigned long to match millis() return type
          and ensure correct unsigned wraparound arithmetic near the 49-day rollover
*/

#ifndef Thread_h
#define Thread_h

#include "gthread_compat.h"
#include <inttypes.h>

/*
    Uncomment to enable auto-generated ThreadName strings (uses more RAM).
    Useful when logging thread activity or displaying a thread list in a UI.

    Heap-free: each Thread carries a small fixed buffer rather than a
    dynamically allocated string.
*/
// #define USE_THREAD_NAMES 1

#ifdef USE_THREAD_NAMES
  #ifndef THREAD_NAME_LEN
    #define THREAD_NAME_LEN 24
  #endif
#endif

class Thread {
protected:
    // Desired interval between runs (milliseconds)
    unsigned long interval;

    // Timestamp of the last completed run (milliseconds)
    unsigned long last_run;

    // Pre-computed timestamp for the next scheduled run.
    // Cached so shouldRun() is a simple comparison with no arithmetic.
    unsigned long _cached_next_run;

    // Wall-clock time at the start of the current run() call
    unsigned long startTime;

    // Duration of the most recent run() call (milliseconds)
    unsigned long runTime;

    // Human-readable name for this thread (optional).
    //
    // NOTE: this is a borrowed pointer, not a copy. The caller owns the
    // storage and must keep it alive for the life of the Thread. String
    // literals (the normal case) satisfy this automatically. Never defaults
    // to NULL — it points at "" until setName() is called — so getName() is
    // always safe to hand to strcmp().
    const char *Name;

    /*
        Call this BEFORE executing the callback (as done in run() below).
        Records last_run and pre-computes _cached_next_run.

        IMPORTANT: If you extend Thread and override run(), you MUST call
        runned() inside your override, otherwise the thread will execute on
        every scheduler tick and never yield.
    */
    void runned(unsigned long time = 0);

    // Callback invoked by run() when no subclass overrides run()
    void (*_onRun)(void);

public:
    // Set false to temporarily disable this thread without removing it
    // from the controller.
    bool enabled;

    // Unique ID, initialized from the object's memory address at construction.
    int ThreadID;

#ifdef USE_THREAD_NAMES
    // Auto-generated name ("Thread <id>"), enabled by USE_THREAD_NAMES.
    // Fixed buffer - no heap allocation.
    char ThreadName[THREAD_NAME_LEN];
#endif

    // Construct with an optional callback and interval (ms).
    Thread(void (*callback)(void) = NULL, unsigned long _interval = 0);

    // --- Configuration ---

    // Set a human-readable name for this thread.
    //
    // The pointer is stored, not copied - the caller owns the storage and
    // must keep it valid for the life of the Thread. Passing NULL resets the
    // name to "" rather than storing NULL.
    void setName(const char *name);

    // Return the thread's name. Never NULL; returns "" if never set.
    const char *getName(void) const;

    // Set the interval between runs (milliseconds).
    virtual void setInterval(unsigned long _interval);

    // Directly set the absolute next run timestamp (millis()).
    // Use this to schedule a one-shot delay or align execution to an event.
    virtual void setNextRunTime(unsigned long _nextTime);

    // Register a callback to execute on each run.
    void onRun(void (*callback)(void));

    // --- Queries ---

    // Return the unique thread ID.
    int getID(void) const;

    // Return the currently configured interval (ms).
    unsigned long getInterval(void) const;

    // Return the wall-clock duration of the most recent run() call (ms).
    unsigned long runTimeMs(void) const;

    // Return the timestamp (millis()) at which this thread last ran.
    unsigned long getLastRunTime(void) const;

    // Return the absolute timestamp (millis()) of the next scheduled run.
    // Note this is the *cached* value; it is not recomputed until the thread
    // next runs or setInterval()/setNextRunTime() is called.
    unsigned long getNextRunTime(void) const;

    // Enabled state.  `enabled` is a public member and may still be written
    // directly; these are provided so callers that only hold a Thread* have a
    // symmetric, greppable API.
    bool isEnabled(void) const;
    void setEnabled(bool state);

    // Discriminates a plain Thread from a ThreadController without RTTI.
    // ThreadController overrides this to return true.  Needed because a
    // controller nested inside another controller is stored as a Thread* and
    // is otherwise indistinguishable from a leaf task.
    virtual bool isController(void) const;

#ifdef THREAD_STATS
    // --- Run-time statistics (opt in with -D THREAD_STATS) ---
    //
    // Roughly 20 bytes per Thread.  Updated in run(); a subclass that
    // overrides run() and does not call Thread::run() will not accumulate
    // statistics.

    unsigned long runCount;   // Number of completed run() calls
    unsigned long totalRun;   // Sum of all run durations (ms)
    unsigned long minRun;     // Shortest run (ms); valid only if runCount > 0
    unsigned long maxRun;     // Longest run (ms)
    unsigned long overruns;   // Runs where runTime exceeded the interval

    // Zero all counters.  Called by the constructor.
    void resetStats(void);

    // Mean run duration (ms), or 0 when runCount == 0.  Integer division.
    unsigned long avgRunMs(void) const;
#endif

    // Return true if enough time has elapsed (and the thread is enabled).
    // Pass an explicit millis() snapshot to avoid redundant calls when
    // checking many threads at once; omit (or pass 0) to read millis() here.
    virtual bool shouldRun(unsigned long time = 0);

    // Execute the callback and update timing bookkeeping.
    virtual void run();
};

#endif // Thread_h
