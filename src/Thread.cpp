/*
    Thread.cpp - Implementation of the Thread class

    See Thread.h for full documentation and change history.
*/

#include "Thread.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Thread::Thread(void (*callback)(void), unsigned long _interval) {
    enabled            = true;
    _onRun             = NULL;
    _cached_next_run   = 0;
    last_run           = 0;
    startTime          = 0;
    runTime            = 0;

    // Use the object's memory address as a simple unique ID.
    ThreadID = (int)this;

#ifdef USE_THREAD_NAMES
    ThreadName = "Thread ";
    ThreadName += ThreadID;
#endif

    if (callback != NULL)
        onRun(callback);

    setInterval(_interval);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void Thread::setName(const char *name) {
    Name = name;
}

const char *Thread::getName(void) const {
    return Name.c_str();
}

void Thread::onRun(void (*callback)(void)) {
    _onRun = callback;
}

void Thread::setInterval(unsigned long _interval) {
    interval = _interval;
    // Recompute next run based on the last known run time.
    _cached_next_run = last_run + interval;
}

void Thread::setNextRunTime(unsigned long _nextTime) {
    _cached_next_run = _nextTime;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int Thread::getID(void) const {
    return ThreadID;
}

unsigned long Thread::getInterval(void) const {
    return interval;
}

unsigned long Thread::runTimeMs(void) const {
    return runTime;
}

bool Thread::shouldRun(unsigned long time) {
    // If no snapshot was provided, read the clock now.
    if (time == 0) time = millis();

    // Rollover guard: if the clock has wrapped behind last_run, force an
    // immediate run rather than waiting ~49 days for the counter to catch up.
    if (time < last_run) _cached_next_run = time;

    // Run if the scheduled time has arrived and the thread is enabled.
    return (time >= _cached_next_run) && enabled;
}

// ---------------------------------------------------------------------------
// Internal bookkeeping
// ---------------------------------------------------------------------------

void Thread::runned(unsigned long time) {
    if (time == 0) time = millis();

    last_run = time;

    // Schedule the next run relative to dispatch time (not completion time),
    // so accumulated callback latency does not drift the overall period.
    _cached_next_run = last_run + interval;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

void Thread::run() {
    // Record dispatch time and pre-compute next run BEFORE calling the
    // callback.  This keeps the period consistent regardless of how long
    // the callback takes.
    runned();

    // Time the callback for profiling via runTimeMs().
    startTime = millis();
    if (_onRun != NULL)
        _onRun();
    runTime = millis() - startTime;
}
