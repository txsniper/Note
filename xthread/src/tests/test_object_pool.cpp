#include <gtest/gtest.h>
#include <string>
#include <cstdio>
#include <vector>
#include "../common/obj_pool/object_pool.h"
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
    xthread::base::return_object<TestObject>(a);
    xthread::base::return_object<TestObject>(b);
}
