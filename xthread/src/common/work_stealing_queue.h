#ifndef XTHREAD_COMMON_WORK_STEALING_QUEUE_H
#define XTHREAD_COMMON_WORK_STEALING_QUEUE_H
#include <cstddef>
#include <atomic>
#include <new>
#include "../base/noncopyable.h"
namespace xthread
{
    template <typename T>
    class WorkStealingQueue : base::NonCopyable{
    public:
        WorkStealingQueue()
            : bottom_(1),
              top_(1),
              capacity_(0),
              buffer_(NULL)
        {

        }

        ~WorkStealingQueue() {
            delete[] buffer_;
            buffer_ = NULL;
        }

        bool init(size_t capacity) {
            if (capacity_ != 0) {
                return false;
            }
            if (capacity == 0) {
                return false;
            }

            buffer_ = new (std::nothrow) T[capacity];
            if (buffer_ == NULL) {
                return false;
            }
            capacity_ = capacity;
            return true;
        }

        /*
         *  同一时刻只能有一个线程push
         */
        bool push(const T& param) {
            const size_t b = bottom_.load(std::memory_order_relaxed);
            const size_t t = top_.load(std::memory_order_acquire);
            if (b - t >= capacity_) {
                return false;
            }
            buffer_[b%capacity_] = param;
            bottom_.store(b+1, std::memory_order_release);
            return true;
        }

        bool pop(T* val) {
            const size_t b = bottom_.load(std::memory_order_relaxed) - 1;
            bottom_.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            size_t t = top_.load(std::memory_order_relaxed);
            bool popped = false;
            if (t <= b) {
                *val = buffer_[b%capacity_];
                if (t != b) {
                    return true;
                }
                popped = top_.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed);
            }
            bottom_.store(b + 1, std::memory_order_relaxed);
            return popped;
        }

        bool steal(T* val) {
            size_t t = top_.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            const size_t b = bottom_.load(std::memory_order_acquire);
            if (t >= b) {
                return false;
            }
            *val = buffer_[t % capacity_];
            return top_.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed);
        }

        size_t volatile_size() const {
            const size_t b = bottom_.load(std::memory_order_relaxed);
            const size_t t = top_.load(std::memory_order_relaxed);
            return (b < t ? 0 : (b - t));
        }
    private:
        std::atomic<size_t> bottom_;
        std::atomic<size_t> top_;
        size_t capacity_;
        T* buffer_;
    };
}

#endif
