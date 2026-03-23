#include <iostream>
#include <queue>
#include <thread>
#include <algorithm>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <syncstream>
#include <future>
#include <memory>


class ThreadPool {
    std::queue<std::function<void()>> tasks_;
    std::vector<std::jthread> threads_;
    std::condition_variable cv_;
    std::mutex m_;
    bool flag_stop_ = false;

public:

    ThreadPool(size_t size) {
        threads_.reserve(size);

        for(size_t i = 0; i < size; ++i) {
            threads_.emplace_back(&ThreadPool::worker, this);
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lg(m_);
            flag_stop_ = true;
        }
        cv_.notify_all();
    }

    void worker() {
        while(true) {
       std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_);
                cv_.wait(lock, [this] {return !tasks_.empty() || flag_stop_;});

                if(tasks_.empty() && flag_stop_) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

    template<typename T, typename ... Args>
   auto submit(T&& f, Args&& ... args) {

        using R = std::invoke_result_t<T,Args...>;

        auto task = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<T>(f), std::forward<Args>(args)...));
        std::future<R> future = task->get_future();

        {
            std::lock_guard<std::mutex> lg(m_);

            tasks_.emplace(
                [task]() {
                    (*task)();
                }
            );
        }

        cv_.notify_one();

        return future;
    }



};


int sum(int a, int b) {
    return a + b;
}



int main()
{
    ThreadPool pool(4);

    for(int i = 0; i < 10; ++i) {
        pool.submit([i]{
            std::osyncstream(std::cout) << "task " << i << "\n";
        });
    } 

    auto f = pool.submit([]{
        return 42;
    });

    auto f2 = pool.submit(sum, 1, 3);


    std::cout << f.get() << "\n";
    std::cout << f2.get() << "\n";

    return 0;
}