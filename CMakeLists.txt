cmake_minimum_required(VERSION 3.30)
project(fnodcdump CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -static-libgcc -static-libstdc++")

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

add_executable(fnodcdump main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK)