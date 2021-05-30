cmake_minimum_required(VERSION 3.0.0)
project(GOMIPROXY VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(GOMIPROXY proxy.c)
add_compile_options(-std=c17)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
