#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../lib/mutex.h"

#define NUM_THREADS 8
#define INCREMENTS 1000000

mutex m;
long counter = 0;
int inside = 0;

/* ---------- Test 1: Mutual Exclusion Counter ---------- */

void* counter_worker(void*) {
    for (int i = 0; i < INCREMENTS; i++) {
        m.lock();
        counter++;
        m.unlock();
    }
    return NULL;
}

/* ---------- Test 2: Detect Simultaneous Entry ---------- */

void* exclusivity_worker(void*) {
    for (int i = 0; i < 200000; i++) {
        m.lock();

        inside++;
        if (inside > 1) {
            printf("FAILED: multiple threads inside critical section\n");
            exit(1);
        }

        inside--;
        m.unlock();
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    printf("Running m Counter Test...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, counter_worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    long expected = (long)NUM_THREADS * INCREMENTS;
    if (counter != expected) {
        printf("Counter mismatch. Expected %ld, got %ld\n", expected, counter);
        return 1;
    }

    printf("Counter test passed\n");

    printf("Running Exclusivity Test...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, exclusivity_worker, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Exclusivity test passed\n");
    printf("All mutex tests passed\n");
    return 0;
}