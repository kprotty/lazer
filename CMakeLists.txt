cmake_minimum_required(VERSION 3.0)
project(lazer C)

file(GLOB_RECURSE sources src/*.c src/*.h)

add_executable(lazer ${sources})
target_include_directories(lazer PRIVATE src/ include/)