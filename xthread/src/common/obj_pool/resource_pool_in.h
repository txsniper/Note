#ifndef XTHREAD_COMMON_RESOURCE_POOL_IN_H
#define XTHREAD_COMMON_RESOURCE_POOL_IN_H
#include <cstddef>
#include <atomic>
#include <vector>
#include "../macros.h"
#include "../../base/thread_exit_helper.h"
#include "../../base/lock.h"
#include "../../base/lock_guard.h"
#include "macro_defines.h"
namespace xthread
{
    namespace base
    {

        template <typename T>
            class ResourcePoolConfig
            {
                static const size_t RESOURCE_POOL_FREE_CHUNK_ITEM_NUM  = 512;
                static const size_t RESOURCE_POOL_BLOCK_MAX_SIZE       = 128 * 1024;
                static const size_t RESOURCE_POOL_BLOCK_MAX_ITEM_NUM   = 512;

                static const size_t temp1 = RESOURCE_POOL_BLOCK_MAX_SIZE / sizeof(T) < 1;
                static const size_t temp2 = temp1 > 1 ? temp1 : 1;
                static const size_t RESOURCE_POOL_BLOCK_ITEM_NUM = temp2 > RESOURCE_POOL_BLOCK_MAX_ITEM_NUM ? RESOURCE_POOL_BLOCK_MAX_ITEM_NUM : temp2;

                static const size_t RESOURCE_POOL_GROUP_BLOCK_NUM = 512;

                static const size_t RESOURCE_POOL_FREE_LIST_INIT_NUM = 128;

                static const size_t RESOURCE_POOL_GROUP_NUM = 65536;
            };

        template <typename T>
            struct FreeChunkItems
            {
                size_t nitems;
                T*     items[ResourcePoolConfig<T>::RESOURCE_POOL_FREE_CHUNK_ITEM_NUM];
            };

        template <typename T>
            struct ResourceBlock
            {
                size_t  nitem;
                char    items[sizeof(T) * ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM];
            };

        template <typename T>
            struct ResourceBlockGroup
            {
                std::atomic<size_t> nblock;
                std::atomic<ResourceBlock<T>*> blocks[ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_BLOCK_NUM];
            };

        template <typename T>
            class ResourcePool {
                public:
                    typedef std::vector<FreeChunkItems<T>* > FreeChunkList;
                    static const size_t MAX_GROUP_NUM = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_NUM;
                    static const size_t FREE_LIST_INIT_NUM = ResourcePoolConfig<T>::RESOURCE_POOL_FREE_LIST_INIT_NUM;

                    class LocalPool
                    {
                        public:
                            LocalPool(ResourcePool* pool)
                                : pool_(pool), local_block_(NULL), local_block_index_(0){
                            }

                            ~LocalPool() {
                                pool_->push_free_chunk(local_free_);
                            }

                        public:

                            T* get_object() {

                            }

                            template <typename A1>
                            T* get_object(const A1& a1) {
                                GET_OBJECT((a1));
                            }

                        private:
                            ResourcePool* pool_;
                            FreeChunkItems<T> local_free_;
                            ResourceBlock<T>* local_block_;
                            size_t local_block_index_;
                    };
                    friend class LocalPool;
                public:
                    static ResourcePool* getInstance();

                    LocalPool* get_or_new_local_pool();
                    ResourceBlock<T>* getBlock(size_t* index);
                    bool addGroup(size_t curr_ngroup);

                    bool push_free_chunk(const FreeChunkItems<T>& curr_free);
                    bool pop_free_chunk(FreeChunkItems<T>& ret_free);
                private:
                    ResourcePool() {
                        free_list_.reserve(FREE_LIST_INIT_NUM);
                    };

                    static void deleteLocalPool(void* arg) {
                        delete reinterpret_cast<LocalPool*>(arg);
                    }

                private:
                    static std::atomic<ResourcePool*> instance_;
                    static MutexLock instance_lock_;
                    FreeChunkList   free_list_;
                    MutexLock       free_list_lock_;

                    static thread_local LocalPool* local_pool_;

                    static std::atomic<size_t> ngroup_;
                    static std::atomic<ResourceBlockGroup<T>*> groups_[MAX_GROUP_NUM];
                    static MutexLock groups_lock_;
            };

        template <typename T>
            std::atomic<ResourcePool<T>*> ResourcePool<T>::instance_(NULL);

        template <typename T>
            MutexLock ResourcePool<T>::instance_lock_;

        template <typename T>
            thread_local typename ResourcePool<T>::LocalPool* ResourcePool<T>::local_pool_ = NULL;

        template <typename T>
            std::atomic<size_t> ResourcePool<T>::ngroup_(0);

        template <typename T>
            std::atomic<ResourceBlockGroup<T>*> ResourcePool<T>::groups_[ResourcePool<T>::MAX_GROUP_NUM] = {};
        template <typename T>
            MutexLock ResourcePool<T>::groups_lock_;

        template <typename T>
            ResourcePool<T>* ResourcePool<T>::getInstance() {
                ResourcePool<T>* ptr = instance_.load(std::memory_order_consume);
                if (likely(ptr)) {
                    return ptr;
                }
                MutexGuard<MutexLock> guard(instance_lock_);
                ptr = new (std::nothrow) ResourcePool<T>();
                if (likely(ptr)) {
                    instance_.store(ptr, std::memory_order_release);
                }
                return ptr;
            }

        template <typename T>
            ResourceBlock<T>* ResourcePool<T>::getBlock(size_t* index) {
                ResourceBlock<T>* newBlock = new (std::nothrow) ResourceBlock<T>;
                if (!newBlock) {
                    return NULL;
                }

                const size_t GROUP_NBLOCK = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_BLOCK_NUM;
                size_t ngroup = 0;
                do {
                    ngroup = ngroup_.load(std::memory_order_acquire);
                    if ((ngroup >= 1) && (ngroup < MAX_GROUP_NUM)) {
                        ResourceBlockGroup<T>* group = groups_[ngroup - 1].load(std::memory_order_acquire);
                        if (group) {
                            size_t block_index = group->nblock.fetch_add(1, std::memory_order_relaxed);
                            if (block_index < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) {
                                group->blocks[block_index].store(newBlock, std::memory_order_release);
                                *index = ((ngroup - 1) * GROUP_NBLOCK) + block_index;
                                return newBlock;
                            }
                        }
                    }
                    else {
                        break;
                    }
                } while (addGroup(ngroup));
                delete newBlock;
                return NULL;
            }

        template <typename T>
            bool ResourcePool<T>::addGroup(size_t curr_ngroup) {
                if (curr_ngroup != ngroup_.load(std::memory_order_acquire)) {
                    return true;
                }
                size_t MAX_GROUP_NUM = ResourcePool<T>::MAX_GROUP_NUM;
                MutexGuard<MutexLock> guard(groups_lock_);
                size_t ngroup = ngroup_.load(std::memory_order_acquire);
                if (curr_ngroup != ngroup) {
                    return true;
                }
                if (ngroup < MAX_GROUP_NUM) {
                    ResourceBlockGroup<T>* newGroup = new (std::nothrow) ResourceBlockGroup<T>;
                    if (!newGroup) {
                        return false;
                    }
                    groups_[ngroup].store(newGroup, std::memory_order_release);
                    ngroup_.store(ngroup + 1, std::memory_order_acquire);
                    return true;
                }
                return false;
            }

        template <typename T>
            typename ResourcePool<T>::LocalPool* ResourcePool<T>::get_or_new_local_pool() {
                ResourcePool<T>::LocalPool* lp = local_pool_;
                if (likely(lp)) {
                    return lp;
                }
                lp = new (std::nothrow) ResourcePool<T>::LocalPool;
                if (!lp) {
                    return NULL;
                }
                local_pool_ = lp;
                registerThreadExitFunc(ResourcePool<T>::deleteLocalPool, reinterpret_cast<void*>(lp));
                return lp;
            }

        template <typename T>
            bool ResourcePool<T>::push_free_chunk(const FreeChunkItems<T>& curr_free) {
                FreeChunkItems<T>* newFreeChunkItems = new (std::nothrow) FreeChunkItems<T>(curr_free);
                if (!newFreeChunkItems) {
                    return false;
                }
                MutexGuard<MutexLock> guard(free_list_lock_);
                free_list_.push_back(newFreeChunkItems);
                return true;
            }

        template <typename T>
            bool ResourcePool<T>::pop_free_chunk(FreeChunkItems<T>& ret_free) {
                if (free_list_.empty()) {
                    return false;
                }
                MutexGuard<MutexLock> guard(free_list_lock_);
                if (free_list_.empty()) {
                    return false;
                }
                FreeChunkItems<T> *p = free_list_.back();
                free_list_.pop_back();
                ret_free = *p;
                delete p;
                return true;
            }
    }
}
#endif
