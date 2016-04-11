#include <list>
#include <memory>
#include <limits>
#include <atomic>
#include <algorithm>
#include <new>
#include "timer_thread.h"
#include "macros.h"
#include "../base/lock.h"
#include "../base/lock_guard.h"
#include "../base/time.h"
#include "../base/futex.h"
#include "log.h"

namespace xthread
{
    TimerThreadOptions::TimerThreadOptions()
        : num_buckets(12),
        begin_fn(NULL),
        end_fn(NULL),
        args(NULL){

        }

    struct TimerThreadTaskStat {
        static const uint32_t TASK_STATUS_INIT     = 0;
        static const uint32_t TASK_STATUS_RUNNING  = 1;
        static const uint32_t TASK_STATUS_FINISHED = 2;
    };

    struct TimerThread::Task
    {
        int64_t run_time;
        void (*fn)(void*);
        void *arg;

        TaskId task_id;
        std::atomic<uint32_t> task_status;

        bool run_and_del();
        bool try_del();
    };


    class TimerThread::Bucket {
        public:
            typedef std::list<std::shared_ptr<TimerThread::Task> >* TaskListPtr;
            Bucket()
                : _nearest_run_time(std::numeric_limits<int64_t>::max())
            {
                tasks_ = new (std::nothrow) std::list<std::shared_ptr<TimerThread::Task> >();
            }

            ~Bucket() {
                delete tasks_;
            }

            std::weak_ptr<TimerThread::Task> schedule(void (*fn)(void*), void* arg, const timespec& abstime, bool* earlier);
            TaskListPtr consume_tasks();

        private:
            base::MutexLock lock_;
            int64_t _nearest_run_time;
            TaskListPtr tasks_;
    };


    TimerThread::Bucket::TaskListPtr TimerThread::Bucket::consume_tasks() {
        TaskListPtr ret = NULL;
        base::MutexGuard<base::MutexLock>  guard(lock_);
        ret = tasks_;
        tasks_ = new (std::nothrow) std::list<std::shared_ptr<TimerThread::Task> >();
        _nearest_run_time = std::numeric_limits<int64_t>::max();
        return ret;
    }

    std::weak_ptr<TimerThread::Task>  TimerThread::Bucket::schedule(void (*fn)(void*),
            void* arg,
            const timespec& abstime,
            bool *earlier) {
        std::shared_ptr<Task> spTask = std::make_shared<TimerThread::Task>();
        if (unlikely(spTask == NULL)) {
            *earlier = false;
            return std::weak_ptr<TimerThread::Task>();
        }
        Task* pTask = spTask.get();
        pTask->fn = fn;
        pTask->arg = arg;
        pTask->run_time = xthread::base::timespec_to_microseconds(abstime);
        pTask->task_status.store(TimerThreadTaskStat::TASK_STATUS_INIT, std::memory_order_relaxed);
        //TaskId id = make_task_id(task, 0);
        *earlier = false;
        {
            base::MutexGuard<base::MutexLock> guard(lock_);
            if (pTask->run_time < _nearest_run_time) {
                _nearest_run_time = pTask->run_time;
                *earlier = true;
            }
            tasks_->push_back(spTask);
        }
        std::weak_ptr<Task> wp(spTask);
        return wp;
    }

    /*
    inline TimerThread::TaskId make_task_id(TimerThread::Task* task_addr,
            int initial_stat)
    {
        return TimerThread::TaskId((uint64_t)( (uint64_t)task_addr | initial_stat));
    }

    inline TimerThread::Task* get_task_addr(TimerThread::TaskId task_id) {
        return (TimerThread::Task*) (task_id & 0xFFFFFFFFC);
    }

    inline uint32_t get_task_status(TimerThread::TaskId task_id) {
        return (uint32_t)(task_id & 0x00000003);
    }

    */

    inline bool task_greater(const std::shared_ptr<TimerThread::Task>& a, const std::shared_ptr<TimerThread::Task>& b) {
        return a->run_time > b->run_time;
    }

    void* TimerThread::run_timer_thread(void* arg) {
        TimerThread* timer_thread = static_cast<TimerThread*>(arg);
        timer_thread->run();
        return NULL;
    }

    TimerThread::TimerThread()
        : _started(false),
        _stop(false),
        _buckets(NULL),
        _nearest_run_time(std::numeric_limits<int64_t>::max()),
        _nsignals(0),
        _thread(0) {
    }

    TimerThread::~TimerThread() {
        delete [] _buckets;
        _buckets = NULL;
    }

    int TimerThread::start(const TimerThreadOptions* options) {
        if (_started) {
            return 0;
        }
        if (options) {
            _options = *options;
        }
        if (_options.num_buckets == 0 || _options.num_buckets > 1024) {
            return -1;
        }
        _buckets = new (std::nothrow) Bucket[_options.num_buckets];
        if (unlikely(_buckets == NULL)) {
            return -1;
        }
        int ret = pthread_create(&_thread, NULL, TimerThread::run_timer_thread, this);
        if (ret) {
            return -1;
        }
        _started = true;
        return 0;
    }


    inline uint64_t fmix64(uint64_t k) {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdLLU;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53LLU;
        k ^= k >> 33;
        return k;
    }

    std::weak_ptr<TimerThread::Task> TimerThread::schedule(
            void (*fn)(void*), void* arg, const timespec& abstime) {
        if (_stop.load(std::memory_order_relaxed) || !_started) {
            return std::weak_ptr<TimerThread::Task>();
        }
        size_t bucket_index = fmix64(pthread_self()) % _options.num_buckets;
        bool earlier = false;
        std::weak_ptr<Task> wpTask = _buckets[bucket_index].schedule(fn, arg, abstime, &earlier);
        if (earlier) {
            bool earlier_global = false;
            int64_t task_run_time = xthread::base::timespec_to_microseconds(abstime);
            {
                base::MutexGuard<base::MutexLock> guard(mutexLock_);
                if (task_run_time < _nearest_run_time) {
                    _nearest_run_time = task_run_time;
                    ++_nsignals;
                    earlier_global = true;
                }
            }
            if (earlier_global) {
                //唤醒TimerThread
                log_debug("wake TimerThread for early \n");
                base::futex_wake_private(&_nsignals, 1);
            }
        }
        return wpTask;
    }

    int TimerThread::unschedule(std::weak_ptr<TimerThread::Task> wpTask) {
        std::shared_ptr<TimerThread::Task> spTask(wpTask.lock());
        if (spTask) {
            uint32_t expected_stat = TimerThreadTaskStat::TASK_STATUS_INIT;
            TimerThread::Task* pTask = spTask.get();
            if (pTask->task_status.compare_exchange_strong(expected_stat,
                        TimerThreadTaskStat::TASK_STATUS_FINISHED,
                        std::memory_order_acquire)) {
                return 0;
            }
            return (expected_stat == TimerThreadTaskStat::TASK_STATUS_RUNNING) ? 1 : -1;
        }
        return -1;
    }

    bool TimerThread::Task::run_and_del() {
        //uint32_t initial_stat = get_task_status(task_id);
        uint32_t initial_stat =TimerThreadTaskStat::TASK_STATUS_INIT;
        uint32_t expected_stat = initial_stat;
        if (task_status.compare_exchange_strong(expected_stat,
                    TimerThreadTaskStat::TASK_STATUS_RUNNING, std::memory_order_acquire)) {
            fn(arg);
            task_status.store(TimerThreadTaskStat::TASK_STATUS_FINISHED, std::memory_order_release);
            return true;
        }
        else if (expected_stat == TimerThreadTaskStat::TASK_STATUS_FINISHED) {
            //
            return false;
        }
        else {
            return false;
        }
    }

    bool TimerThread::Task::try_del() {
        if (task_status.load(std::memory_order_relaxed) == TimerThreadTaskStat::TASK_STATUS_FINISHED) {
            return true;
        }
        return false;
    }

    void TimerThread::run() {
        if (_options.begin_fn) {
            _options.begin_fn(_options.args);
        }

        std::vector<std::shared_ptr<Task> > tasks;
        tasks.reserve(4096);
        while (!_stop.load(std::memory_order_relaxed)) {
            {
                base::MutexGuard<base::MutexLock> guard(mutexLock_);
                _nearest_run_time = std::numeric_limits<int64_t>::max();
            }

            // step1 : 获取所有的Task
            for(size_t i = 0; i < _options.num_buckets; ++i) {
                Bucket& bucket = _buckets[i];
                Bucket::TaskListPtr bucket_tasks = bucket.consume_tasks();
                while (!bucket_tasks->empty()) {
                    std::shared_ptr<TimerThread::Task> spTask = bucket_tasks->front();
                    bucket_tasks->pop_front();
                    if (!spTask->try_del()) {
                        tasks.push_back(spTask);
                        std::push_heap(tasks.begin(), tasks.end(), task_greater);
                    }
                }
                delete bucket_tasks;
            }


            // step2 : 检查最早执行的Task,bRePoll用于判断当前的最早执行Task是否发生变化
            bool bRePoll = false;
            while (!tasks.empty()) {
                std::shared_ptr<Task> task = tasks[0];
                if (task->try_del()) {
                    std::pop_heap(tasks.begin(), tasks.end());
                    tasks.pop_back();
                    continue;
                }
                if (task->run_time > base::gettimeofday_us()) {
                    //printf("[%ld],[%ld]\n",task->run_time, base::gettimeofday_us());
                    break;
                }
                {
                    base::MutexGuard<base::MutexLock> guard(mutexLock_);
                    if (_nearest_run_time < task->run_time) {
                        bRePoll = true;
                        break;
                    }
                }
                std::pop_heap(tasks.begin(), tasks.end(), task_greater);
                tasks.pop_back();
                if (task->run_and_del()) {
                    // 执行Task并将状态置为FINISHED成功
                }
            }
            if (bRePoll) {
                continue;
            }

            // step3 : 更新全局的最早执行时间, 利用futex的特性来防止
            // 插入的最新的最早执行的Task不生效, futex如果发现等待的数据
            // 与期望的不一致，则放弃等待
            int64_t next_run_time = std::numeric_limits<int64_t>::max();
            if (!tasks.empty()) {
                next_run_time = tasks[0]->run_time;
            }

            int expected_signal = 0;
            {
                base::MutexGuard<base::MutexLock> guard(mutexLock_);
                if (_nearest_run_time < next_run_time) {
                    continue;
                }
                else {
                    _nearest_run_time = next_run_time;
                    expected_signal = _nsignals;
                }
            }

            timespec next_timeout = {0, 0};
            timespec* pTimeOut = NULL;
            int64_t now = base::gettimeofday_us();
            if (next_run_time != std::numeric_limits<int64_t>::max()) {
                int64_t diff = next_run_time - now;
                next_timeout = base::us2timespec(diff);
                log_debug("NEXT_TIMEOUT [%lld] [%lld]\n", diff, next_timeout.tv_sec);
                pTimeOut = &next_timeout;
            }
            log_debug("wait start [%d][%lld] [%lld] [%x] [%x]\n", _nsignals, next_run_time, now, &_nsignals, pTimeOut);
            long int ret = base::futex_wait_private(&_nsignals, expected_signal, pTimeOut);
            if (ret == -1) {
                log_debug("errno [%lld]", errno);
            }
            log_debug("wait returned [%d][%lld] [%lld]\n", expected_signal, next_run_time, ret);
        }
    }

    void TimerThread::stop_and_join() {
        _stop.store(true, std::memory_order_relaxed);
        if (_started) {
            {
                base::MutexGuard<base::MutexLock>  guard(mutexLock_);
                _nearest_run_time = 0;
                ++_nsignals;
            }

            // 如果不是TimerThread自己调用这个函数，则唤醒TimerThread
            if (pthread_self() != _thread) {
                log_debug("stop TimerThread, wake \n");
                base::futex_wake_private(&_nsignals, 1);
                pthread_join(_thread, NULL);
            }
        }
    }

    static pthread_once_t g_timer_thread_once = PTHREAD_ONCE_INIT;
    static TimerThread* g_timer_thread = NULL;

    static void init_global_timer_thread() {
        g_timer_thread = new (std::nothrow) TimerThread();
        TimerThreadOptions option;
        g_timer_thread->start(&option);
    }

    TimerThread* get_or_create_global_timer_thread() {
        pthread_once(&g_timer_thread_once, init_global_timer_thread);
        return g_timer_thread;
    }

}
