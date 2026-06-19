cmake_minimum_required(VERSION 3.5.0)

project(clox VERSION 0.0.1)

set(CLOX_HEADERS
        include/chunk.h
        include/compiler.h
        include/debug.h
        include/memory.h
        include/object.h
        include/scanner.h
        include/value.h
        include/vm.h)

set(CLOX_SOURCES
        src/main.c
        src/chunk.c
        src/compiler.c
        src/debug.c
        src/memory.c
        src/object.c
        src/scanner.c
        src/value.c
        src/vm.c)

add_executable(clox ${CLOX_SOURCES} ${CLOX_HEADERS})

target_include_directories(clox PRIVATE
    ${CMAKE_SOURCE_DIR}/include)

IF(MSVC)
    target_compile_options(clox PRIVATE /W4 /WX)
ELSE()
    target_compile_options(clox PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion)
ENDIF()