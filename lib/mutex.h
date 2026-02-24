// my_mutex.h
#pragma once
#include <pthread.h>

class mutex {
private:
    pthread_mutex_t mtx;

public:
    mutex() {
        pthread_mutex_init(&mtx, nullptr);
    }

    ~mutex() {
        pthread_mutex_destroy(&mtx);
    }

    void lock() {
        pthread_mutex_lock(&mtx);
    }

    void unlock() {
        pthread_mutex_unlock(&mtx);
    }

    bool try_lock() {
        return pthread_mutex_trylock(&mtx) == 0;
    }

    pthread_mutex_t* native_handle() {
        return &mtx;
    }

    // prevent copying
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;
};