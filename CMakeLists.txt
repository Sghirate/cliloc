cmake_minimum_required(VERSION 3.8)
project(cliloc C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(cliloc src/main.c)
