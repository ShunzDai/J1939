cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

add_compile_options(-Wall -Wextra -Werror -Wfatal-errors -Wno-unused-parameter -Wno-missing-field-initializers)

project(j1939)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/config
)

add_definitions(-DJ1939_MOCK)

add_subdirectory(src)
add_subdirectory(test)
