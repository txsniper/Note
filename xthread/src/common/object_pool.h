#ifndef XTHREAD_COMMON_OBJECT_POOL_H
#define XTHREAD_COMMON_OBJECT_POOL_H
#include <cstddef>
#include <atomic>
namespace xthread
{

namespace base
{
    // 每个Block分配的最大内存单位数
    template <typename T> struct ObjectPoolBlockMaxSize {
        static const size_t value = 128 * 1024;
    };

    // 每个Block包含的最大Object数
    template <typename T> struct ObjectPoolBlockMaxItem {
        static const size_t value = 512;
    };

    // 空闲的Object在放到全局列表之前会保存到线程本地的Chunk内
    template <typename T> struct ObjectPoolFreeChunkMaxItem {
        static const size_t value = 0;
    };

    template <typename T> struct ObjectPoolFreeChunkMaxItemDynamic {
        static size_t value() { return 0; }
    };


    // ObjectPool对新创建的对象调用此函数, 如果返回false则立刻销毁对象, 并且
    // get_object()应该返回NULL . 这在内部调用构造函数失败时很有用(ENOMEM)
    template <typename T> struct ObjectPoolValidator {
        static bool validate(const T*) { return true; }
    };
}
}

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

    // 每一个BlockGroup中含有的Block数量
    static const size_t OP_GROUP_NBLOCK = (1UL << OP_GROUP_NBLOCK_NBIT);
    static const size_t OP_INITIAL_FREE_LIST_SIZE = 1024;

    template <typename T>
    class ObjectPoolBlockItemNum {
        private :
            static const size_t N1 = ObjectPoolBlockMaxSize<T>::value / sizeof(T);
            static const size_t N2 = (N1 < 1 ? 1 : N1);
        public:
            static const size_t value = (N2 > ObjectPoolBlockMaxItem<T>::value ?
                                         ObjectPoolBlockMaxItem<T>::value : N2);
    };

    template <typename T>
    size_t getObjectPoolBlockItemNum() {

        // step1 : 根据Block内存空间的大小计算最多可以分配多少个Object
        size_t n1 = ObjectPoolBlockMaxSize<T>::value / sizeof(T);

        // step2 : 处理特殊情况
        size_t n2 = (n1 < 1 ? 1 : n1);

        // step3: 不能超过每个Block最大的对象数
        return (n2 > ObjectPoolBlockMaxItem<T>::value ?
                ObjectPoolBlockMaxItem<T>::value : n2);
    }

    template <typename T>
    class ObjectPool {
    public:
        // 每个Block分配的ITEM数量
        static const size_t BLOCK_NITEM = getObjectPoolBlockItemNum<T>();

        // Free Chunk最大的ITEM数量
        static const size_t FREE_CHUNK_NITEM =
            ObjectPoolFreeChunkMaxItem<T>::value > 0 ?
            ObjectPoolFreeChunkMaxItem<T>::value : BLOCK_NITEM;

        // 空闲的对象在进入全局空闲列表之前先放在这里
        typedef ObjectPoolFreeChunk<T, FREE_CHUNK_NITEM> FreeChunk;

        // 每一个Block包含一个对象Item列表
        struct Block {
            size_t nitem;
            char items[sizeof(T) * BLOCK_NITEM];
            Block() : nitem(0) {}
        };

        // 每一个BlockGroup包含一个Block指针数组
        struct BlockGroup {
            std::atomic<size_t> nblock;
            std::atomic<Block*> nblocks[OP_GROUP_NBLOCK];
            BlockGroup() : nblock(0) {
                memset(nblocks, 0, sizeof(std::atomic<Block*>) * OP_GROUP_NBLOCK);
            }
        };

        class LocalPool {
            public:
                explicit LocalPool(ObjectPool* pool)
                    : pool_(pool)
                    , cur_block_(NULL)
                    , cur_block_index_(0) {
                    cur_free_.nfree(0);
                }

                ~LocalPool() {
                    if (cur_free_.nfree) {
                        pool_->push_free_chunk(cur_free_);
                    }
                }
            private:
                ObjectPool* pool_;
                Block* cur_block_;
                size_t cur_block_index_;
                FreeChunk cur_free_;
        };

    private:
        ObjectPool() {

        }

    private:
        static std::atomic<ObjectPool*> singleton_;
    };

}
}
#endif
