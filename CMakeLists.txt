cmake_minimum_required(VERSION 3.10)

project(EchoServer VERSION 1.0
                   DESCRIPTION "A echo server."
                   LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(server server.cpp)
add_executable(client client.cpp)
