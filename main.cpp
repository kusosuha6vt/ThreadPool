#include <chrono>
#include <iostream>
#include "thread_pool.h"

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    thread_pool::ThreadPool threadPool(8);
    std::function<void()> const func1 = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cout << "func1 from other thread!\n";
    };
    std::function<void()> const func2 = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "func2 from other thread!\n";
    };
    threadPool.add_task(func1);
    threadPool.add_task(func2);
    threadPool.wait_all();
    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = finish - start;
    std::cerr << "Program ended in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(duration)
                     .count()
              << "ms\n";
    return 0;
}
