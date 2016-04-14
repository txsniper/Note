#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <algorithm>
#include "thread_exit_helper.h"

namespace xthread
{
namespace base
{
static pthread_key_t    helper_exit_key;
static pthread_once_t   helper_exit_key_init = PTHREAD_ONCE_INIT;

// 线程退出时调用
static void threadExitKeyDel(void* arg) {
    delete static_cast<ThreadExitHelper*>(arg);
}

// 进程退出时调用
static void threadExitKeyDelGlobal() {
    ThreadExitHelper* tep = static_cast<ThreadExitHelper* >(pthread_getspecific(helper_exit_key));
    if (tep != NULL) {
        pthread_setspecific(helper_exit_key, NULL);
        delete tep;
    }
}

// 创建key和注册销毁函数, 多个线程只执行一次, 销毁函数在线程退出时销毁
static void threadExitKeyCreate() {
    pthread_key_create(&helper_exit_key, threadExitKeyDel);

    // 为保险期间,设置在进程退出时也执行一遍
    atexit(threadExitKeyDelGlobal);
}

ThreadExitHelper::~ThreadExitHelper() {
    while (!funcs_.empty()) {
        ExitFuncAndParam efap = funcs_.back();
        funcs_.pop_back();
        efap.first(efap.second);
    }
}

ThreadExitHelper* ThreadExitHelper::get_or_create_new_helper() {
    pthread_once(&helper_exit_key_init, threadExitKeyCreate);
    ThreadExitHelper* tep = static_cast<ThreadExitHelper*>(pthread_getspecific(helper_exit_key));
    if (tep) {
        return tep;
    }
    tep = new (std::nothrow) ThreadExitHelper();
    if (!tep) {
        return NULL;
    }
    pthread_setspecific(helper_exit_key, tep);
    return tep;
}

int ThreadExitHelper::add(Fn fn, void* param) {
    try{
        funcs_.push_back(std::make_pair(fn, param));
    }catch(...) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

void ThreadExitHelper::remove(Fn fn, void* param) {
    std::vector<ExitFuncAndParam>::iterator it = std::find(funcs_.begin(), funcs_.end(), std::make_pair(fn, param));
    if (it != funcs_.end()) {
        std::vector<ExitFuncAndParam>::iterator ite = it + 1;
        for(;ite != funcs_.end() && (ite->first == fn) && (ite->second == param); ite++) {

        }
        funcs_.erase(it, ite);
    }
}

int registerThreadExitFunc(ThreadExitHelper::Fn fn, void* param) {
    ThreadExitHelper* helper = ThreadExitHelper::get_or_create_new_helper();
    return helper->add(fn, param);
}

void unregisterThreadExitFunc(ThreadExitHelper::Fn fn, void* param) {
    ThreadExitHelper* helper = ThreadExitHelper::get_or_create_new_helper();
    return helper->remove(fn, param);
}

}
}
