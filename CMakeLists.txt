cmake_minimum_required(VERSION 3.0)
project(lazer C)

if (CMAKE_BUILD_TYPE MATCHES "Release")
    list(APPEND cflags)
else()
    list(APPEND cflags -Wall)
endif()

if (WIN32)
    list(APPEND defines _WIN32_LEAN_AND_MEAN _WIN32_NT=0x0600)
    list(APPEND libraries kernel32)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND defines _GNU_SOURCE _POSIX_C_SOURCE=200112)
    list(APPEND libraries pthread)
endif()

list(APPEND cflags -mcx16)
list(APPEND libraries atomic)

file(GLOB_RECURSE sources src/*.c)
file(GLOB_RECURSE headers src/*.h)

add_executable(lazer ${sources} ${headers})
target_link_libraries(lazer ${libraries})
target_include_directories(lazer PRIVATE src)
target_compile_options(lazer PRIVATE ${cflags})
target_compile_definitions(lazer PRIVATE ${defines})
