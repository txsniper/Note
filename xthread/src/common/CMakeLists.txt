set(HEADERS
    macros.h
    stack.h
    timer_thread.h
    util.h
    log.h
)
install(FILES ${HEADERS} DESTINATION include/xthread/common)

set(common_SRCS
    stack.cpp
    log.cpp
    timer_thread.cpp
)
add_library(xthread_common ${common_SRCS})
install(TARGETS xthread_common DESTINATION lib)

