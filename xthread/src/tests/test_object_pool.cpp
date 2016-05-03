#include <gtest/gtest.h>
#include <string>
#include <cstdio>
#include <vector>
#include <cstdio>
#include <pthread.h>
#include "../common/obj_pool/object_pool.h"
#include "../base/time.h"
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class TestObject {
    public:
        TestObject() {
            printf("create Object %p\n", this);
            mark = 0xdeadbeef;
        }
        ~TestObject() {
            printf("destroy Object %p\n", this);
            if (mark != 0xdeadbeef) {
                abort();
            }
        }

    private:
        char data[100];
        size_t  mark;
};

namespace xthread
{
namespace base
{
template <> class ObjectPoolConfig<TestObject> {
    public:
        static const size_t OBJECT_POOL_FREE_CHUNK_ITEM_NUM = 2;
        static const size_t OBJECT_POOL_GROUP_NUM = 2;
        static const size_t OBJECT_POOL_BLOCK_ITEM_NUM = 3;
        static const size_t OBJECT_POOL_FREE_LIST_INIT_SIZE = 2;
        static const size_t OBJECT_POOL_GROUP_BLOCK_NUM = 2;
};
}
}
class ObjectPoolTest : public ::testing::Test {
    protected:
        ObjectPoolTest() {

        }
        virtual ~ObjectPoolTest() {

        }
        virtual void SetUp() {
            srand(static_cast<unsigned int>(time(NULL)));
        }
        virtual void TearDown() {

        }
};

TEST_F(ObjectPoolTest, create_data) {
    TestObject* a = xthread::base::get_object<TestObject>();
    TestObject* b = xthread::base::get_object<TestObject>();
    std::string poolConfigStr = xthread::base::ObjectPool<TestObject>::config2String();
    std::cout<<poolConfigStr<<std::endl;
    std::cout<<"a "<<a<<"  b: "<<b<<std::endl;
    xthread::base::return_object<TestObject>(a);
    xthread::base::return_object<TestObject>(b);
    std::string info_str = xthread::base::get_local_pool_info<TestObject>();
    std::cout<< info_str<<std::endl;
}

TEST_F(ObjectPoolTest, create_array_data) {
    const size_t len = 13;
    TestObject* arr[len] = {0};
    size_t count = 0;
    for(size_t i = 0; i < len; ++i) {
        arr[i] = xthread::base::get_object<TestObject>();
        count++;
    }
    for(size_t i = 0; i < count; ++i) {
        xthread::base::return_object<TestObject>(arr[i]);
    }
    const size_t len1 = 30;
    TestObject* brr[len1] = {0};
    for(size_t i = 0; i < len1; ++i) {
        brr[i] = xthread::base::get_object<TestObject>();
        count++;
    }
    for(size_t i = 0; i < len1; ++i) {
        xthread::base::return_object<TestObject>(brr[i]);
    }

    const size_t len2 = 120;
    TestObject* crr[len2] = {0};
    for(size_t i = 0; i < len2; ++i) {
        crr[i] = xthread::base::get_object<TestObject>();
        count++;
    }
    TestObject* a = xthread::base::get_object<TestObject>();
    TestObject* b = xthread::base::get_object<TestObject>();
    (void)a;
    (void)b;
    (void)arr;
    (void)crr;
    std::string info_str = xthread::base::get_local_pool_info<TestObject>();
    std::string pool_str = xthread::base::get_pool_info<TestObject>();
    std::cout<< info_str<<std::endl<<pool_str<<std::endl;
    xthread::base::clear_objects<TestObject>();
}

struct NoDefCtor {
    explicit NoDefCtor(int param)
        : value(param) {

    }
    explicit NoDefCtor(int param, int param1)
        : value(param + param1) {

    }
    int value;
};

TEST_F(ObjectPoolTest, test_no_def_ctor) {
    using namespace xthread::base;
    NoDefCtor* a =  get_object<NoDefCtor>(5);
    EXPECT_EQ(5, a->value);
    NoDefCtor* b =  get_object<NoDefCtor>(5,10);
    EXPECT_EQ(15, b->value);
}


class TestObjectDyMark {
    public:
        TestObjectDyMark(size_t temp_mark) {
            printf("create Object %p\n", this);
            mark = temp_mark;
        }
        ~TestObjectDyMark() {
            printf("destroy Object %p\n", this);
        }
    private:
        char data[100];
        size_t  mark;
};

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

void* thread_func(void *arg) {
    uintptr_t n = reinterpret_cast<uintptr_t>(arg);
    //size_t n = static_cast<size_t>(arg_ptr);
    std::vector<TestObjectDyMark*> arr;
    arr.reserve(n);
    for(size_t i = 0; i < n; ++i) {
        arr.push_back(xthread::base::get_object<TestObjectDyMark>(0));
        uint32_t sleep_ms = rand() % 20;
        thread_sleep(sleep_ms);
    }

    for(size_t i = 0; i < n; ++i) {
        TestObjectDyMark* ptr = arr.back();
        xthread::base::return_object<TestObjectDyMark>(ptr);
        uint32_t sleep_ms = rand() % 15;
        thread_sleep(sleep_ms);
        arr.pop_back();
    }
    printf("THREAD %zd exit\n", n);
    return NULL;
}
TEST_F(ObjectPoolTest, test_multi_thread) {
    const size_t thread_count = 2;
    pthread_t tid[thread_count];
    for(size_t i = 0; i < thread_count; ++i) {
        uintptr_t param = (i + 3) * 5 + 7;
        EXPECT_EQ(0, pthread_create(&tid[i], NULL, thread_func, reinterpret_cast<void*>(param)));
    }
    uint32_t sleep_ms = rand() % 31;
    thread_sleep(sleep_ms);

    size_t n = 100;
    std::vector<TestObjectDyMark*> arr;
    arr.reserve(n);
    for(size_t i = 0; i < n; ++i) {
        arr.push_back(xthread::base::get_object<TestObjectDyMark>(0));
        sleep_ms = rand() % 20;
        thread_sleep(sleep_ms);
    }

    for(size_t i = 0; i < n; ++i) {
        TestObjectDyMark* ptr = arr.back();
        xthread::base::return_object<TestObjectDyMark>(ptr);
        sleep_ms = rand() % 15;
        thread_sleep(sleep_ms);
        arr.pop_back();
    }

    for(size_t i = 0; i < thread_count; ++i) {
        pthread_join(tid[i], NULL);
    }
    std::string info_str = xthread::base::get_local_pool_info<TestObjectDyMark>();
    std::string pool_str = xthread::base::get_pool_info<TestObjectDyMark>();
    std::cout<< info_str<<std::endl<<pool_str<<std::endl;
    xthread::base::clear_objects<TestObjectDyMark>();
}
