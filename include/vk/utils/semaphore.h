/**
Semaphore Utility

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_UTILS_SEMAPHORE_H
#define INCLUDE_VK_UTILS_SEMAPHORE_H

#include <mutex>
#include <future>

class Semaphore {

    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    size_t m_count;

public:
    explicit Semaphore(const size_t count) : m_count(count) {}

    [[nodiscard]] size_t getCount() const {
        return m_count;
    };

    void lock() {
        std::unique_lock lck(m_mutex);
        m_conditionVariable.wait(lck, [this] {
            return m_count != 0;
        });
        --m_count;
    }

    void unlock() {  // call after critical section
        std::unique_lock lck(m_mutex);
        ++m_count;
        m_conditionVariable.notify_one();
    }
};

template<typename T>
bool isReady(const std::future<T> &f) {
    if (f.valid()) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    return false;
}

#endif // INCLUDE_VK_UTILS_SEMAPHORE_H
