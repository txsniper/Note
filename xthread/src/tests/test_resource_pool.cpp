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


class ObjectPoolTest : public ::testing::Test {

};

TEST_F(ObjectPoolTest, create_data) {
    TestObject* a = xthread::base::get_object<TestObject>();
    TestObject* b = xthread::base::get_object<TestObject>();
    std::string poolConfigStr = xthread::base::ResourcePool<TestObject>::config2String();
    std::cout<<poolConfigStr<<std::endl;
    xthread::base::return_object<TestObject>(a);
    xthread::base::return_object<TestObject>(b);
    std::string info_str = xthread::base::get_local_pool_info<TestObject>();
    std::cout<< info_str<<std::endl;
}

TEST_F(ObjectPoolTest, create_array_data) {
    const size_t len = 1026;
    TestObject* arr[len] = {0};
    for(size_t i = 0; i < len; ++i) {
        arr[i] = xthread::base::get_object<TestObject>();
    }
    (void)arr;
    std::string info_str = xthread::base::get_local_pool_info<TestObject>();
    std::cout<< info_str<<std::endl;
}
