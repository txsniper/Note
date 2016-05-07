/*
 * lock.h
 *
 *  Created on: 2016Äê3ÔÂ23ÈÕ
 *      Author: Administrator
 */

#ifndef BASE_LOCK_H_
#define BASE_LOCK_H_
#include <pthread.h>
#include "noncopyable.h"
#include <stdio.h>
namespace xthread
{

namespace base
{
class MutexLock : NonCopyable {
public:
	MutexLock() {
        //printf("mutex init [%p]\n", &mutex_);
		pthread_mutex_init(&mutex_ , NULL);
	}
	~MutexLock() {

		pthread_mutex_destroy(&mutex_);
	}

	void lock() {
        //printf("mutex lock [%p]\n", &mutex_);
		pthread_mutex_lock(&mutex_);
	}
	void unlock() {
        //printf("mutex unlock [%p]\n", &mutex_);
		pthread_mutex_unlock(&mutex_);
	}
private:
	pthread_mutex_t mutex_;
};

class SpinLock : NonCopyable {
public:
    SpinLock() {
        pthread_spin_init(&lock_, 0);
    }
    ~SpinLock() {
        pthread_spin_destroy(&lock_);
    }

    void lock() {
        pthread_spin_lock(&lock_);
    }

    void unlock() {
        pthread_spin_unlock(&lock_);
    }
private:
    pthread_spinlock_t lock_;
};
}
}




#endif /* COMMON_LOCK_H_ */
