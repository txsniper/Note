#ifndef XTHREAD_COMMON_RESOURCE_POOL_H
#define XTHREAD_COMMON_RESOURCE_POOL_H

#include "resource_pool_in.h"
namespace xthread
{
namespace base
{
template <typename T>
T* get_resource(ResourceId<T>* id) {
    return ResourcePool<T>::getInstance()->get_resource(id);
}

template <typename T>
bool return_resource(ResourceId<T> id) {
    return ResourcePool<T>::getInstance()->return_resource(id);
}

template <typename T, typename PARAM_A>
T* get_resource(ResourceId<T>* id, const PARAM_A& a) {
    return ResourcePool<T>::getInstance()->get_resource(id, a);
}

template <typename T, typename PARAM_A, typename PARAM_B>
T* get_resource(ResourceId<T>* id, const PARAM_A& a, const PARAM_B& b) {
    return ResourcePool<T>::getInstance()->get_resource(id, a, b);
}

template <typename T>
std::string get_local_pool_info() {
    return ResourcePool<T>::getInstance()->get_local_pool_info();
}

template <typename T>
std::string get_pool_info() {
    return ResourcePool<T>::getInstance()->get_pool_info();
}

template <typename T>
void clear_objects() {
    return ResourcePool<T>::getInstance()->clear_objects();
}
}
}
#endif
