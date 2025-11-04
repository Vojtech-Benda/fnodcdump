cmake_minimum_required(VERSION 3.30)
project(fnodcdump LANGUAGES CXX)

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

add_executable(fnodcdump)

target_sources(fnodcdump PRIVATE src/main.cpp src/dumpTags.cpp)
target_include_directories(fnodcdump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

target_link_libraries(fnodcdump PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK)
target_link_libraries(fnodcdump PRIVATE $<$<AND:$<BOOL:${MINGW}>,$<CONFIG:Release>>:-static>)

set_target_properties(fnodcdump PROPERTIES DEBUG_POSTFIX d)
target_compile_features(fnodcdump PRIVATE cxx_std_20)