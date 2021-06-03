cmake_minimum_required(VERSION 3.1.0)
project(GOMIPROXY VERSION 0.1.0)

add_compile_options(-pthread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# add_compile_options(-lrt)
# add_compile_options(-std=gnu17)

include(CTest)
enable_testing()

add_executable(GOMIPROXY proxy.c)
target_link_libraries(GOMIPROXY PRIVATE Threads::Threads)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
message(WARNING "COMPILER=${CMAKE_C_COMPILER}")