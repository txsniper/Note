set(HEADERS
    futex.h
    lock.h
    lock_guard.h
    noncopyable.h
    time.h
    thread_exit_helper.h
)

set(base_SRCS
    thread_exit_helper.cpp
    )
add_library(xthread_base ${base_SRCS})
install(FILES ${HEADERS} DESTINATION include/xthread/base)
install(TARGETS xthread_base DESTINATION lib)
