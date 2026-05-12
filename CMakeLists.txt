cmake_minimum_required(VERSION 4.1)
project(Compiler_Register_Allocation_Algorithms)

set(CMAKE_CXX_STANDARD 20)

add_executable(Compiler_Register_Allocation_Algorithms
        main.cpp
        LiveRange.cpp
        Web.cpp
        InterferenceGraph.cpp
        Parser.cpp
)