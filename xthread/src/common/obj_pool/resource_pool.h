#ifndef XTHREAD_COMMON_RESOURCE_POOL_H
#define XTHREAD_COMMON_RESOURCE_POOL_H

#include "resource_pool_in.h"
namespace xthread
{
namespace base
{
template <typename T>
T* get_object() {
    return ResourcePool<T>::getInstance()->get_object();
}

template <typename T>
bool return_object(T* ptr) {
    return ResourcePool<T>::getInstance()->return_object(ptr);
}

template <typename T>
std::string get_local_pool_info() {
    return ResourcePool<T>::getInstance()->get_local_pool_info();
}

template <typename T>
std::string get_pool_info() {
    return ResourcePool<T>::getInstance()->get_pool_info();
}

}
}
#endif
