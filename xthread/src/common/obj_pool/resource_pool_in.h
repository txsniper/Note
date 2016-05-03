#ifndef XTHREAD_COMMON_RESOURCE_POOL_IN_H
#define XTHREAD_COMMON_RESOURCE_POOL_IN_H
#include <cstddef>
#include <atomic>
#include <vector>
#include <string>
#include <stdio.h>
#include "../macros.h"
#include "../../base/thread_exit_helper.h"
#include "../../base/lock.h"
#include "../../base/lock_guard.h"
namespace xthread
{
    namespace base
    {
        template <typename T>
            struct ResourceId {
                uint64_t value;
                operator uint64_t() const {
                    return value;
                }
            };

        template <typename T>
            class ResourcePoolConfig
            {
                public:
                static const size_t RESOURCE_POOL_FREE_CHUNK_ITEM_NUM  = 512;
                static const size_t RESOURCE_POOL_BLOCK_MAX_SIZE       = 128 * 1024;
                static const size_t RESOURCE_POOL_BLOCK_MAX_ITEM_NUM   = 512;

                static const size_t temp1 = RESOURCE_POOL_BLOCK_MAX_SIZE / sizeof(T);
                static const size_t temp2 = temp1 > 1 ? temp1 : 1;
                static const size_t RESOURCE_POOL_BLOCK_ITEM_NUM = temp2 > RESOURCE_POOL_BLOCK_MAX_ITEM_NUM ? RESOURCE_POOL_BLOCK_MAX_ITEM_NUM : temp2;

                static const size_t RESOURCE_POOL_GROUP_BLOCK_NUM = 512;

                static const size_t RESOURCE_POOL_FREE_LIST_INIT_SIZE = 128;

                static const size_t RESOURCE_POOL_GROUP_NUM = 65536;

            };

        template <typename T>
            class ResourcePool {
                public:
                    static const size_t MAX_GROUP_NUM       = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_NUM;
                    static const size_t GROUP_BLOCK_NUM     = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_BLOCK_NUM;
                    static const size_t BLOCK_ITEM_NUM      = ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM;
                    static const size_t FREE_LIST_INIT_SIZE  = ResourcePoolConfig<T>::RESOURCE_POOL_FREE_LIST_INIT_SIZE;
                    static const size_t FREE_CHUNK_ITEM_NUM = ResourcePoolConfig<T>::RESOURCE_POOL_FREE_CHUNK_ITEM_NUM;

                    struct FreeChunkItems
                    {
                        size_t nitems;
                        ResourceId<T>     items[FREE_CHUNK_ITEM_NUM];
                        FreeChunkItems()
                            : nitems(0)
                        {
                            memset(items, 0, sizeof(ResourceId<T>) * FREE_CHUNK_ITEM_NUM);
                        }
                    };

                    struct ResourceBlock
                    {
                        size_t  nitems;
                        char    items[sizeof(T) * BLOCK_ITEM_NUM];
                        ResourceBlock()
                            : nitems(0) {
                            memset(items, 0, sizeof(T) * BLOCK_ITEM_NUM);
                        }
                    };

                    struct ResourceBlockGroup
                    {
                        std::atomic<size_t> nblock;
                        std::atomic<ResourceBlock*> blocks[GROUP_BLOCK_NUM];
                        ResourceBlockGroup()
                            : nblock(0)
                        {
                            memset(blocks, 0, sizeof(ResourceBlock*) * GROUP_BLOCK_NUM);
                        }
                    };

                    typedef std::vector<FreeChunkItems*> FreeChunkList;

                public:
                    ResourceBlock* getBlock(size_t* index);
                    bool addGroup(size_t curr_ngroup);
                    bool push_free_chunk(const FreeChunkItems& curr_free);
                    bool pop_free_chunk(FreeChunkItems& ret_free);
                    static std::string config2String(){
                        const size_t max_size = 256;
                        char str[max_size] = {0};
                        ::snprintf(str, max_size, "MAX_GROUP_NUM[%zd]; GROUP_BLOCK_NUM[%zd]; BLOCK_ITEM_NUM[%zd]; FREE_CHUNK_ITEM_NUM[%zd]", MAX_GROUP_NUM, GROUP_BLOCK_NUM, BLOCK_ITEM_NUM, FREE_CHUNK_ITEM_NUM);
                        return str;
                    }

                    class LocalPool
                    {
                        public:
                            LocalPool(ResourcePool* pool)
                                : pool_(pool), local_block_(NULL), local_block_index_(0){
                                }

                            ~LocalPool() {
                                if (local_free_.nitems > 0) {
                                    pool_->push_free_chunk(local_free_);
                                }
                                pool_->clearLocalPoolFromDctr();
                            }

                            void clear_objects() {
                                if (local_free_.nitems > 0) {
                                    pool_->push_free_chunk(local_free_);
                                }
                            }

                            #include "macro_defines.h"
                            bool return_resource(ResourceId<T> id) {
                                // printf("return resource id[%ld]\n", id.value);
                                if (local_free_.nitems < FREE_CHUNK_ITEM_NUM) {
                                    local_free_.items[local_free_.nitems++] = id;
                                    return true;
                                }
                                else {
                                    bool ret = pool_->push_free_chunk(local_free_);
                                    if (ret) {
                                        local_free_.nitems = 0;
                                        local_free_.items[local_free_.nitems++] = id;
                                        return true;
                                    }
                                }
                                return false;
                            }

                            T* get_resource(ResourceId<T>* id) {
                                GET_RESOURCE();
                            }

                            template <typename A1>
                            T* get_resource(ResourceId<T>* id, const A1& a1) {
                                GET_RESOURCE((a1));
                            }

                            template <typename A1, typename A2>
                            T* get_resource(ResourceId<T>* id, const A1& a1, const A2& a2) {
                                GET_RESOURCE((a1,a2));
                            }

                            inline std::string getLocalPoolInfo()const {
                                const size_t max_size = 256;
                                char str[max_size] = {0};
                                ::snprintf(str, max_size, "pool[%p], local_pool[%p], cur_block[%p], cur_block_index[%zd], FreeChunk.nfree[%zd]", pool_, this, local_block_, local_block_index_, local_free_.nitems);
                                return str;
                           }

                        private:
                            ResourcePool*   pool_;
                            FreeChunkItems  local_free_;
                            ResourceBlock*  local_block_;
                            size_t          local_block_index_;
                    };
                    friend class LocalPool;
                public:

                    static inline T* get_addr_by_id_unsafe(ResourceId<T> id) {
                        // step1 : 获取block的下标
                        size_t block_index = id.value / BLOCK_ITEM_NUM;

                        // step2 : 获取group的下标
                        size_t group_index  = block_index / GROUP_BLOCK_NUM;
                        ResourceBlockGroup* group = groups_[group_index].load(std::memory_order_consume);
                        ResourceBlock* block = group->blocks[block_index & (GROUP_BLOCK_NUM - 1)].load(std::memory_order_consume);
                        T* addr = static_cast<T*>(block->items) + id.value - block_index * BLOCK_ITEM_NUM;
                        return addr;
                    }

                    static inline T* get_addr_by_id_safe(ResourceId<T> id) {
                        size_t block_index  = id.value / BLOCK_ITEM_NUM;
                        size_t group_index = block_index / GROUP_BLOCK_NUM;
                        // printf("id[%ld], block_index [%zd], group_inde[%zd]\n", id.value, block_index, group_index);
                        if (likely(group_index < MAX_GROUP_NUM)) {
                            ResourceBlockGroup* group = groups_[group_index].load(std::memory_order_consume);
                            if (likely(group != NULL)) {
                                ResourceBlock* block = group->blocks[block_index & (GROUP_BLOCK_NUM - 1)].load(std::memory_order_consume);
                                if (likely(block != NULL)) {
                                    size_t itemOffset = id.value - block_index * BLOCK_ITEM_NUM;
                                    if (likely(itemOffset < block->nitems)) {
                                        T* addr = reinterpret_cast<T*>(block->items) + itemOffset;
                                        return addr;
                                    }
                                }
                            }
                        }
                        return NULL;
                    }

                    static ResourcePool* getInstance();

                    LocalPool* get_or_new_local_pool();

                    T* get_resource(ResourceId<T>* id) {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            T* ret = lp->get_resource(id);
                            return ret;
                        }
                        return NULL;
                    }

                    template <typename PARAM_A>
                    T* get_resource(ResourceId<T>* id, const PARAM_A& a) {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            T* ret = lp->get_resource(id, a);
                            return ret;
                        }
                        return NULL;
                    }

                    template <typename PARAM_A, typename PARAM_B>
                    T* get_resource(ResourceId<T>* id, const PARAM_A& a, const PARAM_B& b) {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            T* ret = lp->get_resource(id, a, b);
                            return ret;
                        }
                        return NULL;
                    }

                    bool return_resource(ResourceId<T> id) {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            return lp->return_resource(id);
                        }
                        return false;
                    }

                    // 释放空间，结束时调用
                    void clear_objects() {
                        if (local_pool_) {
                            local_pool_->clear_objects();
                            local_pool_ = NULL;
                        }
                        FreeChunkItems notUse;
                        while( pop_free_chunk(notUse));

                        if (nlocal_.fetch_sub(1, std::memory_order_relaxed) == 1) {
                            size_t ngroup = ngroup_.load(std::memory_order_relaxed);
                            while (ngroup) {
                                ResourceBlockGroup* curr_group = groups_[--ngroup].load(std::memory_order_relaxed);
                                if (curr_group) {
                                    size_t block_num = curr_group->nblock.load(std::memory_order_release);
                                    while (block_num) {
                                        ResourceBlock* block = curr_group->blocks[--block_num].load(std::memory_order_relaxed);
                                        if (!block) {
                                            continue;
                                        }
                                        size_t item_num = block->nitems;
                                        while (item_num > 0) {
                                            T* arr_ptr = reinterpret_cast<T*>(block->items);
                                            // NOTE : 这里不能使用 T obj = arr_ptr[--item_num], 这将导致将数组中的对象赋值给obj临时变量
                                            arr_ptr[--item_num].~T();

                                        }
                                        delete block;
                                    }
                                }
                                delete curr_group;
                            }
                        }
                    }

                    std::string get_local_pool_info() {
                        LocalPool* lp = get_or_new_local_pool();
                        return lp->getLocalPoolInfo();
                    }

                    std::string get_pool_info() {
                        const int max_size = 255;
                        char str[max_size] = {0};
                        snprintf(str, max_size, "pool_[%p], free_list_[%zd], nlocal_[%zd], local_pool_[%p], ngroup_[%zd]", instance_.load(std::memory_order_relaxed), free_list_.size(), nlocal_.load(std::memory_order_relaxed), local_pool_, ngroup_.load(std::memory_order_relaxed));
                        return str;
                    }
                private:
                    ResourcePool() {
                        free_list_.reserve(FREE_LIST_INIT_SIZE);
                    };

                    static void clearLocalPoolFromDctr() {
                        local_pool_ = NULL;
                        nlocal_.fetch_sub(1, std::memory_order_relaxed);
                    }

                    // 删除的操作要等到线程结束执行时
                    static void deleteLocalPool(void* arg) {
                        LocalPool* lp = reinterpret_cast<LocalPool*>(arg);
                        delete lp;
                    }

                private:
                    static std::atomic<ResourcePool*> instance_;
                    static MutexLock instance_lock_;
                    FreeChunkList   free_list_;
                    MutexLock       free_list_lock_;

                    static std::atomic<size_t> nlocal_;
                    static thread_local LocalPool* local_pool_;

                    static std::atomic<size_t> ngroup_;
                    static std::atomic<ResourceBlockGroup*> groups_[MAX_GROUP_NUM];
                    static MutexLock groups_lock_;
            };

        template <typename T>
            std::atomic<ResourcePool<T>*> ResourcePool<T>::instance_(NULL);

        template <typename T>
            MutexLock ResourcePool<T>::instance_lock_;

        template <typename T>
            std::atomic<size_t> ResourcePool<T>::nlocal_(0);

        template <typename T>
            thread_local typename ResourcePool<T>::LocalPool* ResourcePool<T>::local_pool_ = NULL;

        template <typename T>
            std::atomic<size_t> ResourcePool<T>::ngroup_(0);

        template <typename T>
            std::atomic<typename ResourcePool<T>::ResourceBlockGroup*> ResourcePool<T>::groups_[ResourcePool<T>::MAX_GROUP_NUM] = {};
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
            typename ResourcePool<T>::ResourceBlock* ResourcePool<T>::getBlock(size_t* index) {
                // printf(" GET NEW BLOCK\n");
                ResourceBlock* newBlock = new (std::nothrow) ResourceBlock;
                if (!newBlock) {
                    return NULL;
                }

                size_t ngroup = 0;
                do {
                    // step1 : 获取group的数量
                    ngroup = ngroup_.load(std::memory_order_acquire);
                    //printf("ngroup[%zd]\n", ngroup);
                    if ((ngroup >= 1) && (ngroup <= MAX_GROUP_NUM)) {
                        ResourceBlockGroup* group = groups_[ngroup - 1].load(std::memory_order_acquire);
                        if (group) {
                            // step2 : 获取block的下标，然后将block数量加一
                            size_t block_index = group->nblock.fetch_add(1, std::memory_order_relaxed);
                            // printf("curr_block [%zd]\n", block_index);
                            if (block_index < GROUP_BLOCK_NUM) {
                                group->blocks[block_index].store(newBlock, std::memory_order_release);
                                // 返回newBlock的下标
                                *index = ((ngroup - 1) * GROUP_BLOCK_NUM) + block_index;
                                // printf("block_index [%zd]\n", *index);
                                return newBlock;
                            }
                            group->nblock.fetch_sub(1, std::memory_order_relaxed);
                        }
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
                    ResourceBlockGroup* newGroup = new (std::nothrow) ResourceBlockGroup;
                    if (!newGroup) {
                        return false;
                    }
                    groups_[ngroup].store(newGroup, std::memory_order_release);
                    ngroup_.store(ngroup + 1, std::memory_order_acquire);
                    // printf("new group [%zd] ----------------\n", ngroup + 1);
                    return true;
                }
                // printf("no new group [%zd] [%zd]\n", ngroup, MAX_GROUP_NUM);
                return false;
            }

        template <typename T>
            typename ResourcePool<T>::LocalPool* ResourcePool<T>::get_or_new_local_pool() {
                ResourcePool<T>* pool = ResourcePool<T>::getInstance();
                ResourcePool<T>::LocalPool* lp = local_pool_;
                if (likely(lp)) {
                    return lp;
                }
                lp = new (std::nothrow) ResourcePool<T>::LocalPool(pool);
                if (!lp) {
                    return NULL;
                }
                local_pool_ = lp;
                registerThreadExitFunc(ResourcePool<T>::deleteLocalPool, reinterpret_cast<void*>(lp));
                nlocal_.fetch_add(1, std::memory_order_relaxed);
                return lp;
            }

        template <typename T>
            bool ResourcePool<T>::push_free_chunk(const typename ResourcePool<T>::FreeChunkItems& curr_free) {
                FreeChunkItems* newFreeChunkItems = new (std::nothrow) FreeChunkItems(curr_free);
                if (!newFreeChunkItems) {
                    return false;
                }
                MutexGuard<MutexLock> guard(free_list_lock_);
                free_list_.push_back(newFreeChunkItems);
                return true;
            }

        template <typename T>
            bool ResourcePool<T>::pop_free_chunk(ResourcePool<T>::FreeChunkItems& ret_free) {
                if (free_list_.empty()) {
                    return false;
                }
                MutexGuard<MutexLock> guard(free_list_lock_);
                if (free_list_.empty()) {
                    return false;
                }
                FreeChunkItems *p = free_list_.back();
                free_list_.pop_back();
                ret_free = *p;
                delete p;
                return true;
            }
    }
}
#endif
