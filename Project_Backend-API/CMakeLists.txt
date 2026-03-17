cmake_minimum_required(VERSION 3.16)
project(Project_Backend_API LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_executable(Project_Backend-API
    Source.cpp
    HTTPBackend.cpp
    Logger.cpp
    basic.cpp
    sha256.cpp
)

target_include_directories(Project_Backend-API PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
)

target_compile_definitions(Project_Backend-API PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(Project_Backend-API PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${JSONCPP_LIBRARIES}
)
