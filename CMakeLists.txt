cmake_minimum_required(VERSION 3.5.0)

project(clox VERSION 0.0.1)

file(GLOB_RECURSE INCLUDES
    ${CMAKE_SOURCE_DIR}/include/*.h)

file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_executable(clox ${SOURCES} ${INCLUDES})

target_include_directories(clox PRIVATE
    ${CMAKE_SOURCE_DIR}/include)