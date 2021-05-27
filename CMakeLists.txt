cmake_minimum_required(VERSION 3.0.0)
project(UDPHomeWork VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(UDPHomeWork soc.c)
add_compile_options(-std=c11)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
