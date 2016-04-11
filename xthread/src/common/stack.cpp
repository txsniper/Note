#include <unistd.h>
#include <sys/mman.h>
#include <algorithm>
#include <stdlib.h>
#include "stack.h"

namespace xthread
{
    extern const int PAGESIZE    = getpagesize();
    extern const int PAGESIZE_M1 = PAGESIZE - 1;

    const int MIN_STACKSIZE      = PAGESIZE * 2;
    const int MIN_GUARDSIZE      = PAGESIZE;

    int SmallStackClass::stack_size_flag = StackConfig::STACK_SIZE_SMALL;
    int NormalStackClass::stack_size_flag = StackConfig::STACK_SIZE_NORMAL;
    int LargeStackClass::stack_size_flag = StackConfig::STACK_SIZE_LARGE;
    /*
     * alloc stack , return the higher address
     */
    void* alloc_stack(int* inout_stacksize, int* inout_guardsize){
        int stacksize = (std::max(*inout_stacksize, MIN_STACKSIZE) + PAGESIZE_M1) & ~PAGESIZE_M1;
        int guardsize = (std::max(*inout_guardsize, MIN_GUARDSIZE) + PAGESIZE_M1) & ~PAGESIZE_M1;

        const int memsize = stacksize + guardsize;
        void* mem = mmap(NULL, memsize, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
        if (mem == MAP_FAILED) {
            return NULL;
        }
        char* aligned_mem = reinterpret_cast<char*>((reinterpret_cast<std::uintptr_t>(mem) + PAGESIZE_M1) & ~PAGESIZE_M1);
        const long offset = aligned_mem - static_cast<char*>(mem);
        if (guardsize <= offset ||
            mprotect(aligned_mem, guardsize - offset, PROT_NONE) != 0) {
            munmap(mem, memsize);
            return NULL;
        }
        *inout_stacksize = stacksize;
        *inout_guardsize = guardsize;
        return static_cast<char*>(mem) + memsize;
    }

    void dealloc_stack(void* mem, int stacksize, int guardsize) {
        int memsize = stacksize + guardsize;
        if(static_cast<char*>(mem) > (static_cast<char*>(NULL) + memsize)) {
            munmap(static_cast<char*>(mem) - memsize, memsize);
        }
    }
}
