cmake_minimum_required(VERSION 3.20)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Installation directory" FORCE)
message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

project(sntp_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SNTP_CLIENT_ALL_SRC
    sntp_types.h
    sntp_client.hpp
    sntp_client.cpp
    system_clock.hpp
)

if(APPLE)
    file(GLOB APPLE_SOURCES "apple/*.cpp" "apple/*.hpp" "apple/*.h")
    source_group("apple" ${APPLE_SOURCES})
    list(APPEND SNTP_CLIENT_ALL_SRC ${APPLE_SOURCES})
endif()

if(ANDROID)
    file(GLOB ANDROID_SOURCES "android/*.cpp" "android/*.hpp" "android/*.h")
    source_group("android" ${ANDROID_SOURCES})
    list(APPEND SNTP_CLIENT_ALL_SRC ${ANDROID_SOURCES})
endif()

if(WIN32)
    file(GLOB WINDOWS_SOURCES "windows/*.cpp" "windows/*.hpp" "windows/*.h")
    source_group("windows" ${WINDOWS_SOURCES})
    list(APPEND SNTP_CLIENT_ALL_SRC ${WINDOWS_SOURCES})
endif()

if(UNIX AND NOT APPLE)
    file(GLOB LINUX_SOURCES "linux/*.cpp" "linux/*.hpp" "linux/*.h")
    source_group("linux" ${LINUX_SOURCES})
    list(APPEND SNTP_CLIENT_ALL_SRC ${LINUX_SOURCES})
endif()

add_library(${PROJECT_NAME} ${SNTP_CLIENT_ALL_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "sntp_client.hpp"
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)
