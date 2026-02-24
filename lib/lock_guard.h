#include "mutex.h"
#include <cassert>

class LockGuard {
    private:
        bool locked;
        mutex* m;
    public:
        LockGuard(mutex* m) : locked(false), m(m) {
            m->lock();
            locked = true;
        }

        ~LockGuard() {
            if (locked) {
                m->unlock();
                locked = false;
            }
        }

        // prevent copying
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
};