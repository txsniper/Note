#ifndef XTHREAD_COMMON_OBJ_POOL_OBJECT_POOL_H
#define XTHREAD_COMMON_OBJ_POOL_OBJECT_POOL_H
#include <cstddef>
namespace xthread
{
    namespace base
    {
        template <typename T> struct ObjectPoolConfig {
            private:
            static const size_t OBJECT_POOL_BLOCK_MAX_SIZE      = 128 * 1024;
            static const size_t OBJECT_POOL_BLOCK_MAX_ITEM      = 512;
            static const size_t OBJECT_POOL_FREE_CHUNK_MAX_ITEM = 512;

            // step1 : 根据Block内存空间的大小计算最多可以分配多少个Object
            static const size_t n1 = OBJECT_POOL_BLOCK_MAX_SIZE / sizeof(T);
            // step2 : 处理特殊情况
            static const size_t n2 = (n1 < 1 ? 1 : n1);
            public:

            // group的最大量
            static const size_t OBJECT_POOL_GROUP_NUM = 65536;
            // 每一个group中Block的最大量
            static const size_t OBJECT_POOL_GROUP_BLOCK_NUM = 512;

            // 每个Block分配的ITEM数量 (根据计算得出)
            static const size_t OBJECT_POOL_BLOCK_ITEM_NUM =((n2 > OBJECT_POOL_BLOCK_MAX_ITEM) ? OBJECT_POOL_BLOCK_MAX_ITEM : n2);

            // Free Chunk最大的ITEM数量
            //static size_t FREE_CHUNK_ITEM_NUM;
            static const size_t OBJECT_POOL_FREE_CHUNK_ITEM_NUM = ((OBJECT_POOL_FREE_CHUNK_MAX_ITEM > 0) ? OBJECT_POOL_FREE_CHUNK_MAX_ITEM : 0);

            static const size_t OBJECT_POOL_FREE_LIST_INIT_SIZE = 1024;
        };
    }
}
#endif
