#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../lib/mutex.h"
#include "../lib/condition_variable.h"

#define NUM_THREADS 6

mutex m;
CV cv;

int ready = 0;
int awakened = 0;

/* ---------- Worker ---------- */

void* worker(void*) {
    m.lock();
    while (!ready) {
        cv.wait(m);
    }
    awakened++;
    m.unlock();
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    printf("Starting threads...\n");

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, NULL);

    sleep(1); // ensure threads are waiting

    printf("Testing notify_one...\n");

    m.lock();
    ready = 1;
    cv.notify_one();
    m.unlock();

    sleep(1);

    if (awakened != 1) {
        printf("notify_one failed. awakened=%d\n", awakened);
        return 1;
    }

    printf("notify_one passed\n");

    printf("Testing notify_all...\n");

    m.lock();
    cv.notify_all();
    m.unlock();

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    if (awakened != NUM_THREADS) {
        printf("notify_all failed. awakened=%d\n", awakened);
        return 1;
    }

    printf("notify_all passed\n");
    printf("All condition variable tests passed\n");

    return 0;
}