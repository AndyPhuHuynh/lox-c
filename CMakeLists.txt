cmake_minimum_required(VERSION 3.5.0)

project(clox VERSION 0.0.1)

set(CLOX_HEADERS
        include/chunk.h
        include/debug.h
        include/memory.h
        include/value.h)

set(CLOX_HEADERS
        src/main.c
        src/chunk.c
        src/debug.c
        src/memory.c
        src/value.c)

add_executable(clox ${CLOX_SOURCES} ${CLOX_HEADERS})

target_include_directories(clox PRIVATE
    ${CMAKE_SOURCE_DIR}/include)