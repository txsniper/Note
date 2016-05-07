#include <algorithm>
#include <gtest/gtest.h>
#include <pthread.h>
#include "../base/lock.h"
#include "../base/lock_guard.h"
#include "../common/work_stealing_queue.h"
#include "../common/macros.h"
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class test_wsq_suite : public ::testing::Test {
    protected:
        test_wsq_suite() {

        }
        virtual ~test_wsq_suite() {

        }
        virtual void SetUp() {

        }
        virtual void TearDown() {

        }
};

typedef size_t value_type;
value_type seed = 0;
std::atomic<size_t> npushed(0);
const size_t N = 1000000;
xthread::base::MutexLock lock;
void* steal_thread(void* arg) {
    std::vector<value_type> *stolen = new std::vector<value_type>;
    stolen->reserve(N);
    xthread::WorkStealingQueue<value_type> *q = static_cast<xthread::WorkStealingQueue<value_type>* >(arg);
    value_type val;
    while (npushed.load(std::memory_order_relaxed) != N) {
        if (q->steal(&val)) {
            stolen->push_back(val);
        }
        else {
            asm volatile("pause\n": : :"memory");
        }
    }
    return stolen;
}

void* push_thread(void* arg) {
    seed = 0;
    xthread::WorkStealingQueue<value_type> *q =
        static_cast<xthread::WorkStealingQueue<value_type>*>(arg);
    for (size_t i = 0; i < N; ++i, ++seed, ++npushed) {
        xthread::base::MutexGuard<xthread::base::MutexLock> guard(lock);
        q->push(seed);
    }
    return NULL;
}

void* pop_thread(void* arg) {
    std::vector<value_type> *popped = new std::vector<value_type>;
    popped->reserve(N);
    xthread::WorkStealingQueue<value_type> *q =
        static_cast<xthread::WorkStealingQueue<value_type>*>(arg);
    while (npushed.load(std::memory_order_relaxed) != N) {
        value_type val;
        bool res;
        {
            xthread::base::MutexGuard<xthread::base::MutexLock> guard(lock);
            res = q->pop(&val);
        }
        if (res) {
            popped->push_back(val);
        }
    }
    return popped;
}

TEST_F(test_wsq_suite, sanity) {
    xthread::WorkStealingQueue<value_type> q;
    ASSERT_EQ(true, q.init(N));
    pthread_t rth[8];
    pthread_t wth, pop_th;
    for (size_t i = 0; i < ARRAY_SIZE(rth); ++i) {
        ASSERT_EQ(0, pthread_create(&rth[i], NULL, steal_thread, &q));
    }
    ASSERT_EQ(0, pthread_create(&wth, NULL, push_thread, &q));
    ASSERT_EQ(0, pthread_create(&pop_th, NULL, pop_thread, &q));


    std::vector<value_type> stolen;
    stolen.reserve(N);
    size_t nstolen = 0, npopped = 0;
    for (size_t i = 0; i < ARRAY_SIZE(rth); ++i) {
        std::vector<value_type>* res = NULL;
        pthread_join(rth[i], reinterpret_cast<void**>(&res));
        for (size_t j = 0; j < res->size(); ++j, ++nstolen) {
            stolen.push_back((*res)[j]);
        }
    }
    pthread_join(wth, NULL);
    std::vector<value_type>* res = NULL;
    pthread_join(pop_th, reinterpret_cast<void**>(&res));
    for (size_t j = 0; j < res->size(); ++j, ++npopped) {
        stolen.push_back((*res)[j]);
    }

    value_type val;
    while (q.pop(&val)) {
        stolen.push_back(val);
    }

    std::sort(stolen.begin(), stolen.end());
    stolen.resize(std::unique(stolen.begin(), stolen.end()) - stolen.begin());

    ASSERT_EQ(N, stolen.size());
    ASSERT_EQ(0UL, stolen[0]);
    ASSERT_EQ(N-1, stolen[N-1]);
    std::cout << "stolen=" << nstolen << " popped=" << npopped << " left=" << (N - nstolen - npopped)  << std::endl;
}
