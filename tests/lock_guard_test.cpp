#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../lib/mutex.h"
#include "../lib/lock_guard.h"

#define NUM_THREADS 8
#define INCREMENTS 800000

mutex m;
long counter = 0;
int inside = 0;

/* -------------------------------------------------- */
/* Test 1: Basic RAII Mutual Exclusion               */
/* -------------------------------------------------- */

void* counter_worker(void*) {
    for (int i = 0; i < INCREMENTS; i++) {
        LockGuard guard(&m);
        counter++;
    }
    return NULL;
}

/* -------------------------------------------------- */
/* Test 2: Ensure Destructor Releases Lock           */
/* -------------------------------------------------- */

void scope_release_test() {
    {
        LockGuard guard(&m);
        counter++;
    }
    // guard destroyed here
}

/* -------------------------------------------------- */
/* Test 3: Detect Simultaneous Entry (RAII version)  */
/* -------------------------------------------------- */

void* exclusivity_worker(void*) {
    for (int i = 0; i < 200000; i++) {
        LockGuard guard(&m);

        inside++;
        if (inside > 1) {
            printf("FAILED: multiple threads inside critical section\n");
            exit(1);
        }

        inside--;
    }
    return NULL;
}

/* -------------------------------------------------- */
/* Main                                              */
/* -------------------------------------------------- */

int main() {
    pthread_t threads[NUM_THREADS];

    printf("Running LockGuard Mutual Exclusion Test...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, counter_worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    long expected = (long)NUM_THREADS * INCREMENTS;
    if (counter != expected) {
        printf("Counter mismatch. Expected %ld, got %ld\n", expected, counter);
        return 1;
    }

    printf("Mutual exclusion test passed\n");

    printf("Running Scope Release Test...\n");

    scope_release_test();

    if (!m.try_lock()) {
        printf("Lock not released after scope exit\n");
        return 1;
    }
    m.unlock();

    printf("Scope release test passed\n");

    printf("Running Exclusivity Stress Test...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, exclusivity_worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Exclusivity stress test passed\n");

    printf("All LockGuard tests passed\n");
    return 0;
}