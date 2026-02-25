#pragma once
#include <pthread.h>
#include "mutex.h"

class CV {
private:
    pthread_cond_t cond;

public:
    CV() {
        pthread_cond_init(&cond, nullptr);
    }

    ~CV() {
        pthread_cond_destroy(&cond);
    }

    // Wait requires the mutex to be locked
    void wait(mutex& mutex) {
        pthread_cond_wait(&cond, mutex.native_handle());
    }

    void notify_one() {
        pthread_cond_signal(&cond);
    }

    void notify_all() {
        pthread_cond_broadcast(&cond);
    }

    // Prevent copying
    CV(const CV&) = delete;
    CV& operator=(const CV&) = delete;
};