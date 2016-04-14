#ifndef XTHREAD_BASE_THREAD_EXIT_HELPER
#define XTHREAD_BASE_THREAD_EXIT_HELPER
#include <vector>
#include <utility>
#include <pthread.h>
#include "noncopyable.h"
namespace xthread
{
namespace base
{

class ThreadExitHelper : base::NonCopyable
{
    public:
        typedef void (*Fn)(void*);
        typedef std::pair<Fn, void*> ExitFuncAndParam;

        static ThreadExitHelper* get_or_create_new_helper();
        ~ThreadExitHelper();
        int add(Fn fn, void* param);
        void remove(Fn fn, void* param);
    private:
        ThreadExitHelper() {}
    private:
        std::vector<ExitFuncAndParam> funcs_;
};

int registerThreadExitFunc(ThreadExitHelper::Fn fn, void* param);
void unregisterThreadExitFunc(ThreadExitHelper::Fn fn, void* param);

}
}
#endif
