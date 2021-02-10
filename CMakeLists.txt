cmake_minimum_required(VERSION 3.13)
project(imgproc)

if (NOT DEFINED GVS_DEV_PATH)
    set(GVS_DEV_PATH /home/gvs/work/libgvs)
endif()

include(${GVS_DEV_PATH}/compiler.cmake)

set(LIB11_INCLUDE_DIRS ${GVS_DEV_PATH}/include)
set(LIB11 ${GVS_DEV_PATH}/build/libgvs.a)

include_directories(src ${LIB11_INCLUDE_DIRS})

set(SOURCES
        src/hash_impl.hpp
        src/bmp_averager.hh
        src/grid.hh
        src/bmp.hh
        src/point.hh src/point.cpp
        src/px.hh src/px.cpp
        src/utils.hh src/utils.cpp
        src/globals.hh src/globals.cpp
        src/worker_thread.hh src/worker_thread.cpp
        src/procargs.hh src/procargs.cpp
        src/user_interactive.hh src/user_interactive.cpp
        src/tags.hh src/tags.cpp
        )

add_executable(imgproc ${SOURCES} src/main.cpp)
target_link_libraries(imgproc ${LIB11} jpeg png crypto Threads::Threads)

add_executable(tests ${SOURCES}
        tests/test_main.cpp
        tests/test_hash.cpp
)

target_link_libraries(tests ${LIB11} jpeg png crypto Catch2::Catch2 Threads::Threads)
