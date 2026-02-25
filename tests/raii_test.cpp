#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../lib/mutex.h"
#include "../lib/lock_guard.h"

#define NUM_THREADS 8
#define INCREMENTS 500000

mutex m;
long counter = 0;

/* ---------- RAII Worker ---------- */

void* worker(void*) {
    for (int i = 0; i < INCREMENTS; i++) {
        LockGuard guard(&m);
        counter++;
    }
    return NULL;
}

/* ---------- Scope Exit Test ---------- */

void scope_test() {
    LockGuard guard(&m);
    counter++;
    // guard releases automatically on scope exit
}

int main() {
    pthread_t threads[NUM_THREADS];

    printf("Running RAII Thread Safety Test...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    long expected = (long)NUM_THREADS * INCREMENTS;
    if (counter != expected) {
        printf("RAII counter mismatch\n");
        return 1;
    }

    printf("RAII multithread test passed\n");

    printf("Running Scope Release Test...\n");
    scope_test();

    if (!m.try_lock()) {
        printf("Mutex not released after scope exit\n");
        return 1;
    }
    m.unlock();

    printf("Scope release test passed\n");
    printf("All RAII tests passed\n");
    return 0;
}