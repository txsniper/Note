#ifndef XTHREAD_COMMON_OBJ_POOL_OBJECT_POOL_H
#define XTHREAD_COMMON_OBJ_POOL_OBJECT_POOL_H
#include <cstddef>
namespace xthread
{
    namespace base
    {
        template <typename T> struct ObjectPoolConfig {
            static const size_t OBJECT_POOL_BLOCK_MAX_SIZE      = 128 * 1024;
            static const size_t OBJECT_POOL_BLOCK_MAX_ITEM      = 512;
            static const size_t OBJECT_POOL_FREE_CHUNK_MAX_ITEM = 0;

            static size_t getObjectPoolBlockItemNum() {

                // step1 : 根据Block内存空间的大小计算最多可以分配多少个Object
                size_t n1 = OBJECT_POOL_BLOCK_MAX_SIZE / sizeof(T);

                // step2 : 处理特殊情况
                size_t n2 = (n1 < 1 ? 1 : n1);

                // step3: 不能超过每个Block最大的对象数
                return (n2 > OBJECT_POOL_BLOCK_MAX_ITEM) ?
                    OBJECT_POOL_BLOCK_MAX_ITEM : n2;
            }

            // 每个Block分配的ITEM数量
            static const size_t BLOCK_ITEM_NUM = getObjectPoolBlockItemNum();

            static size_t getObjectPoolFreeChunkItemNum() {
                return (OBJECT_POOL_FREE_CHUNK_MAX_ITEM > 0) ?
                    OBJECT_POOL_FREE_CHUNK_MAX_ITEM : BLOCK_ITEM_NUM;
            }

            // Free Chunk最大的ITEM数量
            static const size_t FREE_CHUNK_ITEM_NUM = getObjectPoolFreeChunkItemNum();
        };

    }
}
#endif
