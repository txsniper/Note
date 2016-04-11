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
}
}




#endif /* COMMON_LOCK_H_ */
