cmake_minimum_required(VERSION 3.0)
project(lazer C)

if (WIN32)
    list(APPEND defines _WIN32_LEAN_AND_MEAN _WIN32_NT=0x0600)
    list(APPEND libraries kernel32)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND defines _GNU_SOURCE _POSIX_C_SOURCE=200112)
    list(APPEND libraries pthread)
endif()

set(src_dir src)
file(GLOB_RECURSE sources ${src_dir}/*.c)
file(GLOB_RECURSE headers ${src_dir}/*.h)

add_executable(lazer ${sources} ${headers})
target_link_libraries(lazer ${libraries})
target_compile_definitions(lazer PRIVATE ${defines})
target_include_directories(lazer PRIVATE ${src_dir})
