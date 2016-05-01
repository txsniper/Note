#ifndef XTHREAD_COMMON_RESOURCE_POOL_IN_H
#define XTHREAD_COMMON_RESOURCE_POOL_IN_H
#include <cstddef>
#include <atomic>
#include <vector>
#include <string>
#include "../macros.h"
#include "../../base/thread_exit_helper.h"
#include "../../base/lock.h"
#include "../../base/lock_guard.h"
namespace xthread
{
    namespace base
    {

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

                static const size_t RESOURCE_POOL_FREE_LIST_INIT_NUM = 128;

                static const size_t RESOURCE_POOL_GROUP_NUM = 65536;

            };

        template <typename T>
            class ResourcePool {
                public:
                    static const size_t FREE_CHUNK_ITEM_NUM = ResourcePoolConfig<T>::RESOURCE_POOL_FREE_CHUNK_ITEM_NUM;
                    static const size_t MAX_GROUP_NUM       = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_NUM;
                    static const size_t BLOCK_ITEM_NUM      = ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM;
                    static const size_t FREE_LIST_INIT_NUM  = ResourcePoolConfig<T>::RESOURCE_POOL_FREE_LIST_INIT_NUM;
                    static const size_t GROUP_BLOCK_NUM     = ResourcePoolConfig<T>::RESOURCE_POOL_GROUP_BLOCK_NUM;
                    struct FreeChunkItems
                    {
                        size_t nitems;
                        T*     items[FREE_CHUNK_ITEM_NUM];
                        FreeChunkItems()
                            : nitems(0)
                        {

                        }
                    };

                    struct ResourceBlock
                    {
                        size_t  nitems;
                        char    items[sizeof(T) * BLOCK_ITEM_NUM];
                        ResourceBlock()
                            : nitems(0) {
                        }
                    };

                    struct ResourceBlockGroup
                    {
                        std::atomic<size_t> nblock;
                        std::atomic<ResourceBlock*> blocks[GROUP_BLOCK_NUM];
                        ResourceBlockGroup()
                            : nblock(0)
                        {

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
                        ::snprintf(str, max_size, "FREE_CHUNK_ITEM_NUM[%zd]; BLOCK_ITEM_NUM[%zd]; GROUP_BLOCK_NUM[%zd], FREE_LIST_CHUNK_INIT_NUM[%zd]", FREE_CHUNK_ITEM_NUM, BLOCK_ITEM_NUM, GROUP_BLOCK_NUM, FREE_LIST_INIT_NUM);
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
                            }

                            #include "macro_defines.h"
                            bool return_object(T* ptr) {
                                if (local_free_.nitems < FREE_CHUNK_ITEM_NUM) {
                                    local_free_.items[local_free_.nitems++] = ptr;
                                    return true;
                                }
                                else {
                                    bool ret = pool_->push_free_chunk(local_free_);
                                    if (ret) {
                                        local_free_.nitems = 0;
                                        local_free_.items[local_free_.nitems++] = ptr;
                                        return true;
                                    }
                                }
                                return false;
                            }

                            T* get_object() {
                                GET_OBJECT();
                            }

                            template <typename A1>
                            T* get_object(const A1& a1) {
                                GET_OBJECT((a1));
                            }

                            template <typename A1, typename A2>
                            T* get_object(const A1& a1, const A2& a2) {
                                GET_OBJECT((a1,a2));
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
                    static ResourcePool* getInstance();

                    LocalPool* get_or_new_local_pool();

                    T* get_object() {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            T* ret = lp->get_object();
                            return ret;
                        }
                        return NULL;
                    }

                    bool return_object(T* ptr) {
                        LocalPool* lp = get_or_new_local_pool();
                        if (likely(lp)) {
                            return lp->return_object(ptr);
                        }
                        return false;
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
                        free_list_.reserve(FREE_LIST_INIT_NUM);
                    };

                    static void deleteLocalPool(void* arg) {
                        LocalPool* lp = reinterpret_cast<LocalPool*>(arg);
                        delete lp;
                        local_pool_ = NULL;
                        nlocal_.fetch_sub(1, std::memory_order_relaxed);
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
                ResourceBlock* newBlock = new (std::nothrow) ResourceBlock;
                if (!newBlock) {
                    return NULL;
                }

                size_t ngroup = 0;
                do {
                    ngroup = ngroup_.load(std::memory_order_acquire);
                    if ((ngroup >= 1) && (ngroup < MAX_GROUP_NUM)) {
                        ResourceBlockGroup* group = groups_[ngroup - 1].load(std::memory_order_acquire);
                        if (group) {
                            size_t block_index = group->nblock.fetch_add(1, std::memory_order_relaxed);
                            if (block_index < GROUP_BLOCK_NUM) {
                                group->blocks[block_index].store(newBlock, std::memory_order_release);
                                *index = ((ngroup - 1) * GROUP_BLOCK_NUM) + block_index;
                                return newBlock;
                            }
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
                    return true;
                }
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
