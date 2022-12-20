#pragma once

#include <stdlib.h>
#include <queue>
#include <mutex>
#include <optional>


namespace utils {

    template<typename T>
    class SafeQ {

        std::queue<T> queue_;

        mutable std::mutex mutex_;
    
    
    public:
        SafeQ() = default;
        SafeQ(const SafeQ<T> &) = delete ;
        SafeQ& operator=(const SafeQ<T> &) = delete ;
    
        SafeQ(SafeQ<T>&& other) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_ = std::move(other.queue_);
        }
        
        virtual ~SafeQ() { }
        
        unsigned long size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

        bool empty() const {
            return queue_.empty();
        }

        
        std::optional<T> unsafePop() {
            if (queue_.empty()) {
            return {};
            }
            T tmp = queue_.front();
            queue_.pop();
            return tmp;
        }

        
        std::optional<T> pop() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) {
            return {};
            }
            T tmp = queue_.front();
            queue_.pop();
            return tmp;
        }
        
        void push(const T &item) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(item);
        }

        void unsafePush(const T &item) {
            queue_.push(item);
        }
    };   

}
