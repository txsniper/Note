#ifndef XTHREAD_COMMON_OBJECT_POOL_H
#define XTHREAD_COMMON_OBJECT_POOL_H
#include <cstddef>
#include <atomic>
#include <vector>
#include "../../base/lock.h"
#include "../../base/lock_guard.h"
#include "../../base/thread_exit_helper.h"
#include "object_pool_config.h"
#include "../macros.h"

namespace xthread
{

namespace base
{

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
    class ObjectPool {
    public:
        static const size_t BLOCK_NITEM         = ObjectPoolConfig<T>::BLOCK_ITEM_NUM;
        static const size_t FREE_CHUNK_NITEM    = ObjectPoolConfig<T>::FREE_CHUNK_ITEM_NUM;
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
            std::atomic<Block*> blocks[OP_GROUP_NBLOCK];
            BlockGroup() : nblock(0) {
                memset(blocks, 0, sizeof(std::atomic<Block*>) * OP_GROUP_NBLOCK);
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
                    pool_->clear_local_pool();
                }

                void clear_local_pool() {
                    local_pool_ = NULL;
                    if (nlocal_.fetch_sub(1, std::memory_order_relaxed)) {
                        return;
                    }
                }

                static void deleteLocalPool(void* lp) {
                    delete static_cast<LocalPool*>(lp);
                }


#define GET_OBJECT(CTOR_ARGS)                       \
    /* step1 : 从局部FreeChunks分配 */              \
    if (cur_free_.nfree) {                          \
        XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_SUB1      \
        return cur_free_.ptrs[--cur_free_.nfree];   \
    }                                               \
    /* step2 : 从全局FreeChunk分配  */              \
    if (pool_->pop_free_chunk(cur_free_)) {         \
        XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_SUB1      \
        return cur_free_.ptrs[--cur_free_.nfree];   \
    }                                               \
    /* step3 : 从本地block分配      */              \
    if (cur_block_ && cur_block_->nitem < BLOCK_NITEM) {                        \
        T* obj = new ((T*)cur_block_->items + cur_block_->nitem) T CTOR_ARGS;   \
        if (!ObjectPoolValidator<T>::validate(obj)) {                           \
            obj->~T();                                                          \
            return NULL;                                                        \
        }                                                                       \
        cur_block_->nitem++;                                                    \
        return obj;                                                             \
    }                                                                           \
    /* step4: 获取新的Block */                                                  \
    cur_block_ = ObjectPool::add_block(&cur_block_index_);                      \
    if (cur_block_ && cur_block_->nitem < BLOCK_NITEM) {                        \
        T* obj = new ((T*)cur_block_->items + cur_block_->nitem) T CTOR_ARGS;   \
        if (!ObjectPoolValidator<T>::validate(obj)) {                           \
            obj->~T();                                                          \
            return NULL;                                                        \
        }                                                                       \
        cur_block_->nitem++;                                                    \
        return obj;                                                             \
    }                                                                           \
    return NULL;

                inline T* get() {
                    GET_OBJECT();
                }

                template <typename A1>
                inline T* get(const A1& a1) {
                    GET_OBJECT((a1));
                }

                template <typename A1, typename A2>
                inline T* get(const A1& a1, const A2& a2) {
                    GET_OBJECT((a1, a2));
                }

                inline int return_object(T* obj) {
                    if (cur_free_.nfree < ObjectPool::FREE_CHUNK_NITEM) {
                        cur_free_.ptr[cur_free_.nfree++] = obj;
                        XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_ADD1;
                        return 0;
                    }
                    if (pool_->push_free_chunks(cur_free_)) {
                        cur_free_.ptr[0] = obj;
                        cur_free_.nfree = 1;
                        XTHREAD_OBJECT_POOL_FREE_ITEM_NUM_ADD1;
                        return 0;
                    }
                    return -1;
                }
            private:
                // 全局pool
                ObjectPool* pool_;
                // 当前的Block对象,包含了对象列表的存储空间
                Block* cur_block_;
                size_t cur_block_index_;
                // 包含指向Item的指针,初始时为空,在有内存块归还时先归还给FreeChunk
                FreeChunk cur_free_;
        };

        friend class LocalPool;

    public:
        static ObjectPool* getInstance();
        inline T* get_object() {
            LocalPool* lp = get_or_new_local_pool();
            if (likely(lp != NULL)) {
                return lp->get();
            }
            return NULL;
        }

        template <typename A1>
        inline T* get_object(const A1& a1) {
            LocalPool* lp = get_or_new_local_pool();
            if (likely(lp != NULL)) {
                return lp->get(a1);
            }
            return NULL;
        }

        template <typename A1, typename A2>
        inline T* get_object(const A1& a1, const A2& a2) {
            LocalPool* lp = get_or_new_local_pool();
            if (likely(lp != NULL)) {
                return lp->get(a1, a2);
            }
            return NULL;
        }

        inline int return_object(T* obj) {
            LocalPool* lp = get_or_new_local_pool();
            if (likely(lp != NULL)) {
                return lp->return_object(obj);
            }
            return -1;
        }

        void clear_objects() {
            LocalPool* lp = local_pool_;
            if (likely(lp)) {
                local_pool_ = NULL;
                registerThreadExitFunc(LocalPool::deleteLocalPool, static_cast<void*>(lp));
                delete lp;
            }
        }

    private:
        ObjectPool() {

        }

        bool pop_free_chunk(FreeChunk& c);
        bool push_free_chunk(const FreeChunk& c);

        LocalPool* get_or_new_local_pool();
        static Block*   add_block(size_t* index);
        static bool     add_block_group(size_t old_ngroup);

    private:
        // 单例
        static std::atomic<ObjectPool*> singleton_;
        static MutexLock                singleton_lock_;

        static MutexLock    local_pool_mutex;
        static thread_local LocalPool*  local_pool_;

        static std::atomic<size_t> nlocal_;
        static std::atomic<size_t> ngroup_;

        // block_groups_ : atomic指针数组
        static std::atomic<BlockGroup*> block_groups_[OP_MAX_BLOCK_NGROUP];
        static MutexLock    block_group_mutex_;

        MutexLock               free_chunks_lock_;
        std::vector<FreeChunk*> free_chunks_;

    };

    template <typename T>
    std::atomic<ObjectPool<T>*> ObjectPool<T>::singleton_(NULL);

    template <typename T>
    MutexLock ObjectPool<T>::singleton_lock_;

    template <typename T>
    std::atomic<typename ObjectPool<T>::BlockGroup*>
    ObjectPool<T>::block_groups_[OP_MAX_BLOCK_NGROUP] = {};

    template <typename T>
    ObjectPool<T>* ObjectPool<T>::getInstance() {
        ObjectPool<T>* instance = singleton_.load(std::memory_order_consume);
        if (instance){
            return instance;
        }
        MutexGuard<MutexLock> g(singleton_lock_);
        if (!instance) {
            instance = new ObjectPool<T>();
            singleton_.store(instance, std::memory_order_release);
        }
        return instance;
    }

    template <typename T>
    bool ObjectPool<T>::pop_free_chunk(FreeChunk& c) {
        // 如果为空,则直接返回(可能漏掉当前别的线程正在插入的情况)
        // 如果大量的线程请求这里,这里直接不加锁判断可以降低锁争用
        if (free_chunks_.empty()) {
            return false;
        }
        MutexGuard<MutexLock> g(free_chunks_lock_);
        if (free_chunks_.empty()) {
            return false;
        }
        FreeChunk* p = free_chunks_.back();
        free_chunks_.pop_back();
        c = *p;
        delete p;
        return true;
    }

    template <typename T>
    bool ObjectPool<T>::push_free_chunk(const FreeChunk& c) {
        FreeChunk* c2 = new (std::nothrow) FreeChunk(c);
        if (c2 == NULL) {
            return false;
        }
        MutexGuard<MutexLock> g(free_chunks_lock_);
        free_chunks_.push_back(c2);
        return true;
    }

    template <typename T>
    typename ObjectPool<T>::LocalPool* ObjectPool<T>::get_or_new_local_pool() {
        ObjectPool<T>::LocalPool* lp = local_pool_;
        if (likely(lp != NULL)) {
            return lp;
        }
        lp = new(std::nothrow) LocalPool(this);
        if (lp == NULL) {
            return NULL;
        }
        MutexGuard<MutexLock> guard(local_pool_mutex);
        local_pool_ = lp;
        registerThreadExitFunc(LocalPool::deleteLocalPool, static_cast<LocalPool*>(lp));
        nlocal_.fetch_add(1, std::memory_order_relaxed);
        return lp;
    }

    template <typename T>
    typename ObjectPool<T>::Block* ObjectPool<T>::add_block(size_t* index) {
        // step1 新建Block
        Block* new_block = new (std::nothrow) Block;
        if (new_block == NULL) {
            return NULL;
        }
        size_t ngroup;
        do {
            ngroup = ngroup_.load(std::memory_order_acquire);
            if (ngroup >= 1) {
                BlockGroup* g = block_groups_[ngroup - 1].load(std::memory_order_consume);
                size_t block_index = g->nblock.fetch_add(1, std::memory_order_relaxed);
                if (block_index < OP_GROUP_NBLOCK) {
                    g->blocks[block_index].store(new_block, std::memory_order_release);
                    // index是计算所有Group在内的Index
                    *index = (ngroup - 1) * OP_GROUP_NBLOCK + block_index;
                    return new_block;
                }
                g->nblock.fetch_sub(1, std::memory_order_relaxed);
            }
        }while (add_block_group(ngroup));
        delete new_block;
        return NULL;
    }

    template <typename T>
    bool ObjectPool<T>::add_block_group(size_t old_ngroup) {
        BlockGroup* bg = NULL;
        // 防止多个线程同时进来生成多个bg
        MutexGuard<MutexLock> guard(block_group_mutex_);
        size_t ngroup = ngroup_.load(std::memory_order_acquire);
        if (ngroup != old_ngroup) {
            return true;
        }
        if (ngroup < OP_MAX_BLOCK_NGROUP) {
            bg = new (std::nothrow) BlockGroup;
            if (NULL != bg) {
                block_groups_[ngroup].store(bg, std::memory_order_release);
                ngroup_.store(ngroup + 1, std::memory_order_release);
            }
        }
        return bg != NULL;
    }
}
}
#endif
