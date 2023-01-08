cmake_minimum_required(VERSION 3.24)
project(chafon_rfid C)

set(CMAKE_C_STANDARD 11)

#======================================
set(CMAKE_FIND_DEBUG_MODE TRUE)
set(CMAKE_VERBOSE_MAKEFILE ON)
#======================================

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message("debug mode")
endif()

add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG>")

add_executable(chafon_rfid example.c rfid.c rfid.h)
