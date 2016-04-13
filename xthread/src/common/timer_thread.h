#ifndef XTHREAD_TIMER_THREAD_H
#define XTHREAD_TIMER_THREAD_H

#include <vector>
#include <pthread.h>
#include <atomic>
#include <memory>
#include "../common/util.h"
#include "../base/lock.h"
#include "../base/noncopyable.h"
namespace xthread {
struct TimerThreadOptions {
	size_t num_buckets;
	void (*begin_fn)(void *);
	void (*end_fn) (void *);
	void *args;
	TimerThreadOptions();
};

class TimerThread : base::NonCopyable {
public:
	struct Task;
	class Bucket;
	typedef uint64_t TaskId;
	const static TaskId INVALID_TASK_ID = 0;
	TimerThread();
	~TimerThread();

	int start(const TimerThreadOptions* options);
	void stop_and_join();
    std::weak_ptr<Task>  schedule(void (*fn)(void *), void* arg, const timespec& abstime);
	int unschedule(std::weak_ptr<Task> wpTask);

private:
	void run();
	static void * run_timer_thread(void* arg);
	bool _started;
    std::atomic<bool> _stop;

	TimerThreadOptions _options;
	Bucket* _buckets;

    base::MutexLock mutexLock_;
	int64_t _nearest_run_time;

    // _nsignals: 用来判断futex需要等待的数据是否发生变化
	int _nsignals;
	pthread_t _thread;

};

TimerThread* get_or_create_global_timer_thread();
TimerThread* get_global_timer_thread();

}
#endif
