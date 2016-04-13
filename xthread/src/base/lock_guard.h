#ifndef XTHREAD_COMMON_LOCK_GUARD_H
#define XTHREAD_COMMON_LOCK_GUARD_H

namespace xthread
{

namespace base {

template <typename T>
class MutexGuard {
public:
	MutexGuard(T& mutex)
		: _mutex(mutex) {
		_mutex.lock();
	}
	~MutexGuard() {
		_mutex.unlock();
	}
private:
	T& _mutex;
};
}
}
#endif
