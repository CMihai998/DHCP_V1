cmake_minimum_required(VERSION 3.17)
project(DHCP_V1 C)

set(CMAKE_C_STANDARD 11)

add_executable(DHCP_V1 main.c common.h set.c)