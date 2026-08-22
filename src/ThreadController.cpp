/*
    ThreadController.cpp - Implementation of the ThreadController class

    See ThreadController.h for full documentation and change history.
*/

#include "Thread.h"
#include "ThreadController.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ThreadController::ThreadController(unsigned long _interval) : Thread() {
    cached_size = 0;
    clear();
    setInterval(_interval);

#ifdef USE_THREAD_NAMES
    snprintf(ThreadName, sizeof(ThreadName), "ThreadController %d", ThreadID);
#endif
}

// ---------------------------------------------------------------------------
// Scheduler
// ---------------------------------------------------------------------------

void ThreadController::run() {
    // Execute the controller's own callback if one was registered.
    if (_onRun != NULL)
        _onRun();

    // Snapshot the clock once and share it across all shouldRun() checks
    // so every thread in this pass sees the same reference time.
    unsigned long time = millis();

    int checks = 0;
    for (int i = 0; i < MAX_THREADS && checks < cached_size; i++) {
        if (thread[i] != NULL) {
            checks++;
            if (thread[i]->shouldRun(time))
                thread[i]->run();
        }
    }

    // Mark the controller itself as having run so its own interval is tracked.
    runned();
}

// ---------------------------------------------------------------------------
// Thread management
// ---------------------------------------------------------------------------

bool ThreadController::add(Thread *_thread) {
    // Reject if this thread is already registered.
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] != NULL && thread[i]->ThreadID == _thread->ThreadID)
            return true;
    }

    // Find the first empty slot and insert.
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] == NULL) {
            thread[i] = _thread;
            cached_size++;
            return true;
        }
    }

    // Pool is full.
    return false;
}

void ThreadController::remove(int id) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] != NULL && thread[i]->ThreadID == id) {
            thread[i] = NULL;
            cached_size--;
            return;
        }
    }
}

void ThreadController::remove(Thread *_thread) {
    remove(_thread->ThreadID);
}

void ThreadController::clear() {
    for (int i = 0; i < MAX_THREADS; i++)
        thread[i] = NULL;
    cached_size = 0;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int ThreadController::size(bool cached) {
    if (cached)
        return cached_size;

    // Recount from the array and resync the cache.
    int count = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] != NULL)
            count++;
    }
    cached_size = count;
    return cached_size;
}

Thread *ThreadController::get(int index) {
    int pos = -1;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] != NULL) {
            pos++;
            if (pos == index)
                return thread[i];
        }
    }
    return NULL;
}

Thread *ThreadController::get(const char *name) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread[i] != NULL && strcmp(name, thread[i]->getName()) == 0)
            return thread[i];
    }
    return NULL;
}
