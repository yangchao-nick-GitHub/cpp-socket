cmake_minimum_required(VERSION 3.10)

project(SocketFramework LANGUAGES CXX)

option(BUILD_DEBUG "Build in debug mode" ON)

if(BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "Building in DEBUG mode")
else()
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Building in RELEASE mode")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(COMMON_SOURCES src/socket.cpp)  
set(CLIENT_SOURCES example/client.cpp)  
set(SERVER_SOURCES example/server.cpp)  

add_executable(server ${SERVER_SOURCES} ${COMMON_SOURCES})
target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE pthread)


add_executable(client ${CLIENT_SOURCES} ${COMMON_SOURCES})
target_include_directories(client PRIVATE include)
target_link_libraries(client PRIVATE pthread)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(server PRIVATE -g)
    target_compile_options(client PRIVATE -g)
endif()
