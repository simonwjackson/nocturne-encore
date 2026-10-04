cmake_minimum_required(VERSION 3.25)
project(scene_expansion LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../common/mod_cmake/rexmod.cmake)

rexmod_add_plugin(scene_expansion
    mod_main.cpp
    expand.cpp
)
