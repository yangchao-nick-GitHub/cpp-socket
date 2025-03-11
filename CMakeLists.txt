cmake_minimum_required(VERSION 3.10)

project(SocketFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(COMMON_SOURCES src/socket.cpp)  
set(CLIENT_SOURCES example/client.cpp)  
set(SERVER_SOURCES example/server.cpp)  

add_executable(server ${SERVER_SOURCES} ${COMMON_SOURCES})
target_include_directories(server PRIVATE include)

add_executable(client ${CLIENT_SOURCES} ${COMMON_SOURCES})
target_include_directories(client PRIVATE include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(server PRIVATE -g)
    target_compile_options(client PRIVATE -g)
endif()
