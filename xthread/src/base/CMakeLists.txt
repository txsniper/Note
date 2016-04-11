set(HEADERS
    futex.h
    lock.h
    lock_guard.h
    noncopyable.h
    time.h
)
install(FILES ${HEADERS} DESTINATION include/xthread/base)
