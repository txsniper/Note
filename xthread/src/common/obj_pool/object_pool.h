#ifndef XTHREAD_COMMON_OBJECT_POOL_H
#define XTHREAD_COMMON_OBJECT_POOL_H
#include "object_pool_in.h"
namespace xthread
{
namespace base
{
template <typename T>
inline T* get_object() {
    return ObjectPool<T>::getInstance()->get_object();
}

template <typename T, typename A1>
inline T* get_object(const A1& a1) {
    return ObjectPool<T>::getInstance()->get_object(a1);
}

template <typename T, typename A1, typename A2>
inline T* get_object(const A1& a1, const A2& a2) {
    return ObjectPool<T>::getInstance()->get_object(a1, a2);
}

template <typename T>
inline int return_object(T* obj) {
    return ObjectPool<T>::getInstance()->return_object(obj);
}

template <typename T>
inline void clear_objects() {
    return ObjectPool<T>::getInstance()->clear_objects();
}
}
}
#endif
