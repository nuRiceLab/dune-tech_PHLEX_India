cmake_minimum_required(VERSION 3.31)
project(inference LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(phlex REQUIRED)
find_package(Arrow REQUIRED)
find_package(phlex_arrow_common REQUIRED)
find_package(edep_arrow REQUIRED)

# Phlex module libinference.so: rasterizes the edep segments (see rasterize.cpp)
add_library(rasterize MODULE rasterize.cpp)
target_link_libraries(rasterize PRIVATE phlex::module phlex_arrow_common::phlex_arrow_common
                                        edep_arrow::edep_arrow Arrow::arrow_shared)
