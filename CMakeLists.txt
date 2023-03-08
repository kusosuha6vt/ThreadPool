cmake_minimum_required(VERSION 3.24)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 14)

add_executable(ThreadPool main.cpp src/thread_pool.cpp include/thread_pool.h)
