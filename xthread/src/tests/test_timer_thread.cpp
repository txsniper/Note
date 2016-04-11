#include <gtest/gtest.h>
#include <vector>
#include <pthread.h>
#include "../common/timer_thread.h"
#include "../base/time.h"
#include "../base/futex.h"
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace xthread {
    long timespec_diff_us(const timespec& ts1, const timespec& ts2) {
        return (ts1.tv_sec - ts2.tv_sec) * 100000L +
            (ts1.tv_nsec - ts2.tv_nsec) / 1000;
    }

    void thread_sleep(uint32_t ms) {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
        pthread_mutex_lock(&mutex);
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + ms / 1000;
        timeout.tv_nsec = now.tv_usec + (ms % 1000) * 1000000L;
        pthread_cond_timedwait(&cond, &mutex, &timeout);
    }

    class TimeKeeper {
        public:
            TimeKeeper(timespec run_time)
                : expected_run_time_(run_time), name_(NULL), sleep_ms_(0)
            {

            }

            TimeKeeper(timespec run_time, const char* name)
                : expected_run_time_(run_time), name_(name), sleep_ms_(0)
            {

            }

            TimeKeeper(timespec run_time, const char* name, int sleep_ms)
                : expected_run_time_(run_time), name_(name), sleep_ms_(sleep_ms)
            {

            }

            void schedule(TimerThread* timer_thread) {
                task_ = timer_thread->schedule(
                        TimeKeeper::routine, this, expected_run_time_
                        );

            }

            void unschedule(TimerThread* timer_thread) {
                int ret = timer_thread->unschedule(task_);
                printf("unschedule [%s] [%d]\n", name_, ret);
            }

            void run() {
                printf("RUN [%s]\n", name_);
                timespec  current_time;
                clock_gettime(CLOCK_REALTIME, &current_time);
                run_times_.push_back(current_time);
                int saved_sleep_ms = sleep_ms_;
                if (saved_sleep_ms > 0) {
                    timespec timeout = base::milliseconds2timespec(saved_sleep_ms);
                    printf("WAIT\n");
                    base::futex_wait_private(&sleep_ms_, saved_sleep_ms, &timeout);
                }
            }

            void wakeup() {
                if (sleep_ms_ != 0) {
                    sleep_ms_ = 0;
                    base::futex_wake_private(&sleep_ms_, 1);
                }
            }

            void expect_first_run() {
                expect_first_run(expected_run_time_);
            }

            void expect_first_run(timespec expected_run_time) {
                ASSERT_TRUE(!run_times_.empty());
                long diff = timespec_diff_us(run_times_[0], expected_run_time);
                EXPECT_GE(diff, 0);
                EXPECT_LE(diff, kEpsilonUs);
            }

            void expect_not_run() {
                EXPECT_TRUE(run_times_.empty());
            }

            static void routine(void *arg) {
                TimeKeeper* keeper = static_cast<TimeKeeper*>(arg);
                keeper->run();
            }

        public:
            timespec expected_run_time_;
            std::weak_ptr<TimerThread::Task> task_;
        private:
            const char* name_;
            int sleep_ms_;
            std::vector<timespec> run_times_;
            static const long kEpsilonUs;
    };
    const long TimeKeeper::kEpsilonUs = 50000;

    class TimerThreadTest: public ::testing::Test{

    };

    TEST_F(TimerThreadTest, RunTasks)
    {
        TimerThread timer_thread;
        ASSERT_EQ(0, timer_thread.start(NULL));

        timespec _4s_later = base::seconds_from_now(6);
        TimeKeeper keeper1(_4s_later, "keeper1");
        keeper1.schedule(&timer_thread);

        TimeKeeper keeper2(_4s_later, "keeper2");
        keeper2.schedule(&timer_thread);

        timespec _2s_later = base::seconds_from_now(2);
        TimeKeeper keeper3(_2s_later, "keeper3");
        keeper3.schedule(&timer_thread);
        thread_sleep(4000);
        keeper1.unschedule(&timer_thread);

        thread_sleep(9000);
        timer_thread.stop_and_join();
    }
}
