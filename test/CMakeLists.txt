project(Test CXX)

cmake_minimum_required(VERSION 3.2)

find_package(Threads REQUIRED)

add_executable(taskloop taskloop.cpp)
target_link_libraries(taskloop Threads::Threads)

target_compile_options(taskloop PRIVATE -Wall -Werror)
