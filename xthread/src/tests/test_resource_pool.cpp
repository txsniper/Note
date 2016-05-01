#include <gtest/gtest.h>
#include <string>
#include <cstdio>
#include <vector>
#include <cstdio>
#include "../common/obj_pool/resource_pool.h"
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class TestObject {
    public:
        TestObject() {
            printf("create Object %p\n", this);
        }
        ~TestObject() {
            printf("destroy Object %p\n", this);
        }

    private:
        char data[100];
};

namespace xthread
{
namespace base
{
template <> class ResourcePoolConfig<TestObject> {
    public:
        static const size_t RESOURCE_POOL_FREE_CHUNK_ITEM_NUM = 2;
        static const size_t RESOURCE_POOL_GROUP_NUM = 2;
        static const size_t RESOURCE_POOL_BLOCK_ITEM_NUM = 3;
        static const size_t RESOURCE_POOL_FREE_LIST_INIT_NUM = 2;
        static const size_t RESOURCE_POOL_GROUP_BLOCK_NUM = 2;
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

/*
TEST_F(ObjectPoolTest, create_data) {
    TestObject* a = xthread::base::get_object<TestObject>();
    TestObject* b = xthread::base::get_object<TestObject>();
    std::string poolConfigStr = xthread::base::ResourcePool<TestObject>::config2String();
    std::cout<<poolConfigStr<<std::endl;
    std::cout<<"a "<<a<<"  b: "<<b<<std::endl;
    xthread::base::return_object<TestObject>(a);
    xthread::base::return_object<TestObject>(b);
    std::string info_str = xthread::base::get_local_pool_info<TestObject>();
    std::cout<< info_str<<std::endl;
}
*/

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
}
