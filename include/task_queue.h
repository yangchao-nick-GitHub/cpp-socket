#include <iostream>
#include <vector>
#include <queue>
#include <deque>
#include <mutex>
#include <string>
#include <functional>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <unordered_map>
#include <atomic>

template<typename T>
class ComQueue {
public:
    ComQueue() = default;
    ComQueue(const ComQueue&) = delete;
    ComQueue(ComQueue&&) = default;

    void push(const T& task)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tq_.emplace_back(task);
    }

    bool tryPop(T& task)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!tq_.empty()) {
            task = std::move(tq_.front());
            tq_.pop_front();
            return true;
        }
        return false;
    }

    bool isEmpty()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return tq_.empty();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return tq_.size();
    }

private:
    std::mutex mtx_;
    std::deque<T> tq_;
};

class WorkQueue {
public:
    explicit WorkQueue(uint32_t min_thread_num = 4, uint32_t max_thread_num = 10)
    {
        min_thread_num_ = std::max(1U, min_thread_num);
        max_thread_num_ = std::max(min_thread_num_, max_thread_num);
        init();
    }

    ~WorkQueue()
    {
        is_running = false;
        thread_cond_.notify_all();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void submit(std::function<void()> task)
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        task_quque_.push(task);
        adjustThreadNum();
        thread_cond_.notify_one();
    }

    void workMonitor()
    {
        while (is_running) {
            if (task_quque_.isEmpty() && thread_num_.load() > min_thread_num_) {
                break;
            }

            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(thread_mutex_);
                thread_cond_.wait_for(lock, std::chrono::seconds(1), [this] {
                    return !is_running || !task_quque_.isEmpty();
                });

                if (!task_quque_.tryPop(task)) {
                    continue;
                }
            }

            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Thread pool error: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Thread pool error: unknown exception" << std::endl;
            }
        }
        thread_num_.fetch_sub(1);
    }

    void init()
    {
        for (uint32_t i = 0; i < min_thread_num_; i++) {
            addWorker();
        }
    }

    void addWorker()
    {
        threads_.emplace_back(&WorkQueue::workMonitor, this);
        thread_num_.fetch_add(1);
        history_max_thread_num_ = std::max(thread_num_.load(), history_max_thread_num_.load());
    }

public:
    std::atomic<uint32_t> thread_num_ {0};
    std::atomic<uint32_t> history_max_thread_num_ {0};
private:
    
    uint32_t min_thread_num_ {1};
    uint32_t max_thread_num_ {10};
    std::vector<std::thread> threads_;
    ComQueue<std::function<void()>> task_quque_;
    std::mutex thread_mutex_;
    std::condition_variable thread_cond_;
    bool is_running {true};

    void adjustThreadNum()
    {
        if (task_quque_.size() > thread_num_.load() * 2 && thread_num_.load() < max_thread_num_) {
            addWorker();
        }
    }
};
