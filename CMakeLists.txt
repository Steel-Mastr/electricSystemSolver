cmake_minimum_required(VERSION 4.0)
project(electric_system_solver)

set(CMAKE_CXX_STANDARD 20)

add_executable(electric_system_solver main.cpp
        include/json.hpp
)
