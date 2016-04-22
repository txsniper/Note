#ifndef XTHREAD_COMMON_OBJECT_POOL_MACROS_DEFINES_H
#define XTHREAD_COMMON_OBJECT_POOL_MACROS_DEFINES_H
#define GET_OBJECT_NEW(CTOR_ARGS) \
    if (local_free_.nitems > 0) {   \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    else if (pool_->pop_free_chunk(local_free_)) {        \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    if (local_block_ && local_block_->nitem < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) + local_block_->nitem) T ARGS;   \
        local_block_->nitem++;                            \
        return ptr;                                       \
    }                                                     \
    local_block_ = pool_->getBlock(&local_block_index_); \
    if (local_block_ && local_block_->nitem < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) +  local_block_->nitem) T ARGS;   \
        local_block_->nitem++;                            \
        return ptr;                                       \
    }                                                     \
    return NULL;


#define GET_OBJECT(CTOR_ARGS) \
    /*step1 : get free item from local */                 \
    if(local_free_.nitems > 0) {                          \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    /* step2 : get free item from global */               \
    else if (pool_->pop_free_chunk(local_free_)) {        \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    /* step3 : get from local block */                    \
    if (local_block_ && local_block_->nitem < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) + local_block_->nitem) T ARGS;   \
        local_block_->nitem++;                            \
        return ptr;                                       \
    }                                                     \
    /* step4: get new block from global then return */    \
    local_block_ = ResourcePool::add_block(&local_block_index_); \
    if (local_block_ && local_block_->nitem < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) +  local_block_->nitem) T ARGS;   \
        local_block_->nitem++;                            \
        return ptr;                                       \
    }                                                     \
    return NULL;
#endif
