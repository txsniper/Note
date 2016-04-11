#ifndef XTHREAD_COMMON_STACK_H
#define XTHREAD_COMMON_STACK_H
#include <boost/context/all.hpp>
#include <new>

#include "macros.h"

namespace xthread
{
    // BOOST_VERSION > 1.56
    typedef boost::context::fcontext_t Context;
    inline void jump(Context c1, Context c2) {
        boost::context::jump_fcontext(&c1, c2, 0);
    }

    struct StackType {
        static const int STACK_TYPE_MAIN    = 0;
        static const int STACK_TYPE_PTHREAD = 1;
        static const int STACK_TYPE_SMALL   = 2;
        static const int STACK_TYPE_NORMAL  = 3;
        static const int STACK_TYPE_LARGE   = 4;
    };

    struct StackConfig {
        static const int GUARD_PAGE_SIZE   = 4096;
        static const int STACK_SIZE_SMALL  = 32768;
        static const int STACK_SIZE_NORMAL = 1048576;
        static const int STACK_SIZE_LARGE  = 8388608;
    };

    struct StackContainer {
        Context context;
        int stacksize;
        int guardsize;
        void *stack;
        int stacktype;
    };

    struct MainStackClass {};

    struct SmallStackClass {
        static int stack_size_flag;
        const static int stacktype = StackType::STACK_TYPE_SMALL;
    };

    struct NormalStackClass {
        static int stack_size_flag;
        const static int stacktype = StackType::STACK_TYPE_NORMAL;
    };

    struct LargeStackClass {
        static int stack_size_flag;
        const static int stacktype = StackType::STACK_TYPE_LARGE;
    };

    void* alloc_stack(int* stacksize, int* guardsize);
    void dealloc_stack(void* mem, int stacksize, int guardsize);

    template <typename StackClass>
        class StackContainerFactory {
            public:
                static StackContainer* get_stack(void (*entry)(intptr_t)) {
                    StackContainer* newSC = new (std::nothrow) StackContainer;
                    if (unlikely(newSC == NULL)) {
                        return NULL;
                    }
                    newSC->stacksize = StackClass::stack_size_flag;
                    newSC->guardsize = StackConfig::GUARD_PAGE_SIZE;
                    newSC->stack = alloc_stack(&(newSC->stacksize), &(newSC->guardsize));
                    if (unlikely(newSC->stack == NULL)) {
                        newSC->context = NULL;
                        return newSC;
                    }
                    newSC->context = boost::context::make_fcontext(newSC->stack, newSC->stacksize, entry);
                    newSC->stacktype = StackClass::stacktype;
                    return newSC;
                }

                static void return_stack(StackContainer* sc) {
                    if (sc->stack) {
                        dealloc_stack(sc->stack, sc->stacksize, sc->guardsize);
                        sc->stack = NULL;
                    }
                    delete sc;
                }
        };

    template<>
        class StackContainerFactory<MainStackClass> {
            public:
                static StackContainer* get_stack(void (*)(intptr_t)) {
                    StackContainer *sc = new (std::nothrow) StackContainer;
                    if (unlikely(sc == NULL)) {
                        return NULL;
                    }
                    sc->stacksize = 0;
                    sc->guardsize = 0;
                    sc->stacktype = StackType::STACK_TYPE_MAIN;
                    return sc;
                }

                static void return_stack(StackContainer *sc) {
                    delete sc;
                }
        };

    inline StackContainer* get_stack(const int type, void(*entry)(intptr_t)) {
        switch(type) {
            case StackType::STACK_TYPE_PTHREAD:
                return NULL;
            case StackType::STACK_TYPE_SMALL:
                return StackContainerFactory<SmallStackClass>::get_stack(entry);
            case StackType::STACK_TYPE_NORMAL:
                return StackContainerFactory<NormalStackClass>::get_stack(entry);
            case StackType::STACK_TYPE_LARGE:
                return StackContainerFactory<LargeStackClass>::get_stack(entry);
            case StackType::STACK_TYPE_MAIN:
                return StackContainerFactory<MainStackClass>::get_stack(entry);
        }
        return NULL;
    }

    inline void return_stack(StackContainer* sc) {
        if (NULL == sc) {
            return;
        }
        switch (sc->stacktype) {
            case StackType::STACK_TYPE_PTHREAD:
                assert(false);
                return;
            case StackType::STACK_TYPE_SMALL:
                return StackContainerFactory<SmallStackClass>::return_stack(sc);
            case StackType::STACK_TYPE_NORMAL:
                return StackContainerFactory<NormalStackClass>::return_stack(sc);
            case StackType::STACK_TYPE_LARGE:
                return StackContainerFactory<LargeStackClass>::return_stack(sc);
            case StackType::STACK_TYPE_MAIN:
                return StackContainerFactory<MainStackClass>::return_stack(sc);
        }
    }


}  // namespace

#endif
