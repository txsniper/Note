#ifndef XTHREAD_COMMON_OBJECT_POOL_INL_H
#define XTHREAD_COMMON_OBJECT_POOL_INL_H
#include <atomic>
#include <cstddef>

#ifdef XTHREAD_OBJECT_POOL_NEED_FREE_ITEM_NUM
#define XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_ADD1 \
    (_global_nfree.fetch_add(1, std::memory_order_releaxed))
#define XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_SUB1 \
    (_global_nfree.fetch_sub(1, std::memory_order_releaxed))
#else
#define XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_ADD1
#define XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_SUB1
#endif

namespace xthread
{
namespace base
{
    template <typename T, size_t NITEM>
    struct ObjectPoolFreeChunk {
        size_t nfree;
        T* ptrs[NITEM];
    };

    struct ObjectPoolInfo {
        size_t local_pool_num;
        size_t block_group_num;
        size_t block_num;
        size_t item_num;
        size_t block_item_num;
        size_t free_chunk_item_num;
        size_t total_size;
#ifdef XTHREAD_OBJECT_POOL_NEED_FREE_ITEM_NUM
        size_t free_item_num;
#endif
    };

    static const size_t OP_MAX_BLOCK_NGROUP = 65535;
    static const size_t OP_GROUP_NBLOCK_NBIT = 16;
    static const size_t OP_GROUP_NBLOCK = (1UL << OP_GROUP_NBLOCK_NBIT);
    static const size_t OP_INITIAL_FREE_LIST_SIZE = 1024;

    template <typename T>
    class ObjectPoolBlockItemNum {
        static const size_t N1 = ObjectPoolBlockMaxSize<T>::value /
    };
    template <typename T>
    class ObjectPool {
    public:

    };
}
}
#endif
