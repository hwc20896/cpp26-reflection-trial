cmake_minimum_required(VERSION 3.25)
project(cpp26_reflection_trial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
        src/
)

target_compile_options(
    ${PROJECT_NAME} PRIVATE
        -freflection
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
        stdc++exp
)