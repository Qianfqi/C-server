#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <chrono>
#include <queue>
class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });
                        if (stop && tasks.empty()) {
                            return;
                        }
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// 示例任务函数
int square(int num) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return num * num;
}

int main() {
    ThreadPool pool(4);  // 创建线程池，使用4个线程

    std::vector<std::future<int>> results;
    for (int i = 1; i <= 10; ++i) {
        results.emplace_back(pool.enqueue(square, i));  // 将任务添加到线程池中执行
    }
    
    // 获取任务结果并输出
    for (auto& result : results) {
        std::cout << "Result: " << result.get() << std::endl;
    }

    return 0;
}