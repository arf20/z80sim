cmake_minimum_required(VERSION 3.18)

project(z80sim)

set(CMAKE_CXX_FLAGS "-fms-extensions")

add_executable(z80sim main.cpp ihex_copy.c ihex_parse.c ihex_record.c)

set_property(TARGET z80sim PROPERTY CXX_STANDARD 17)

