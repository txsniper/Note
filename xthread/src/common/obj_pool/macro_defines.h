#ifndef XTHREAD_COMMON_OBJECT_POOL_MACROS_DEFINES_H
#define XTHREAD_COMMON_OBJECT_POOL_MACROS_DEFINES_H
#define GET_OBJECT(CTOR_ARGS) \
    if (local_free_.nitems > 0) {   \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    else if (pool_->pop_free_chunk(local_free_)) {        \
        T* ptr = local_free_.items[--local_free_.nitems]; \
        return ptr;                                       \
    }                                                     \
    if (local_block_ && local_block_->nitems < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) + local_block_->nitems) T CTOR_ARGS;   \
        local_block_->nitems++;                            \
        return ptr;                                       \
    }                                                     \
    local_block_ = pool_->getBlock(&local_block_index_); \
    if (local_block_ && local_block_->nitems < ResourcePoolConfig<T>::RESOURCE_POOL_BLOCK_ITEM_NUM) { \
        T* ptr = new (reinterpret_cast<T*>(local_block_->items) +  local_block_->nitems) T CTOR_ARGS;   \
        local_block_->nitems++;                            \
        return ptr;                                       \
    }                                                     \
    return NULL;

#endif
