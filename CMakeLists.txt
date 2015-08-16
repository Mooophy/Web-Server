cmake_minimum_required(VERSION 3.2)
project(Web_Server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES main.cpp)
add_executable(Web_Server ${SOURCE_FILES})