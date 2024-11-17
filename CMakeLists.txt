cmake_minimum_required(VERSION 3.20)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Installation directory" FORCE)
message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

project(sntp)

add_subdirectory(src/sntp_client)
add_subdirectory(examples/sntp_clent_example)