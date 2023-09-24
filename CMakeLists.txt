cmake_minimum_required(VERSION 3.27.4)
project(restaraunt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(cpprestsdk CONFIG REQUIRED)
find_package(SOCI CONFIG REQUIRED)

add_executable(restaraunt main.cpp)

target_link_libraries(restaraunt PRIVATE SOCI::soci_core SOCI::soci_empty SOCI::soci_sqlite3 cpprestsdk::cpprest cpprestsdk::cpprestsdk_zlib_internal cpprestsdk::cpprestsdk_brotli_internal)
