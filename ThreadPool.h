#include <vector>             // 引入标准向量容器，用于存储工作线程
#include <queue>              // 引入标准队列容器，用于存储待执行的任务
#include <thread>             // 引入线程库，用于创建和管理线程
#include <mutex>              // 引入互斥锁，用于确保线程安全
#include <condition_variable> // 引入条件变量，用于线程等待和通知
#include <functional>         // 引入函数对象相关库，用于包装和执行任务
#include <future>             // 引入future库，用于管理异步任务的结果
using namespace std;
class ThreadPool
{
private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex queue_mutex;
    condition_variable condition;
    bool stop;

public:
    ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; i++)
        {
            workers.emplace_back(
                [this]
                {
                    while (true)
                    {
                        function<void()> task;
                        {
                            unique_lock<mutex> lock(this->queue_mutex);
                            // 使用条件变量等待任务或停止信号
                            this->condition.wait(lock, [this]
                                                 { return this->stop || !this->tasks.empty(); });
                            if (this->stop && this->tasks.empty())
                                return;
                            task = move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                });
        }
    }

    // 将函数调用包装为任务并添加到任务队列，返回与任务关联的 future
    /*
    这段代码是C++中用于创建一个异步任务处理函数模板的部分，该函数模板接收一个可调用对象F和一组参数Args...，
    并返回一个与异步任务结果关联的std::future对象。以下是详细解释：

    std::future<typename std::result_of<F(Args...)>::type>

    std::future: C++标准库中的组件，用于表示异步计算的结果。
    当你启动一个异步操作时，可以获取一个std::future对象，通过它可以在未来某个时间点查询异步操作是否完成，并获取其返回值。

    typename std::result_of<F(Args...)>::type:
    这部分是类型推导表达式，使用了C++11引入的std::result_of模板。
    std::result_of可以用来确定给定可调用对象F在传入参数列表Args...时的调用结果类型。
    在这里，它的作用是推断出函数f(args...)调用后的返回类型。

    整体来看，这个返回类型的声明意味着enqueue函数将返回一个std::future对象，
    而这个future所指向的结果数据类型正是由f(args...)调用后得到的类型。

    using return_type = typename std::result_of<F(Args...)>::type;

    using关键字在这里定义了一个别名return_type，它是对上述类型推导结果的引用。

    在后续的代码中，return_type将被用来表示异步任务的返回类型，简化代码并提高可读性。
    */
    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> future<typename result_of<F(Args...)>::type>
    {
        using return_type = typename result_of<F(Args...)>::type;
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...));
        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(queue_mutex);
            if (stop)
                throw runtime_error("enqueue on a stopped ThreadPool");
            tasks.emplace([task]()
                          { (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    ~ThreadPool()
    {
        {
            unique_lock<mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto &worker : workers)
        {
            worker.join();
        }
    }
};