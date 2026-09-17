cmake_minimum_required(VERSION 3.10)
project(patient_dashboard_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(patient_server
    src/main.cpp
    src/Cipher.cpp
    src/Storage.cpp
)

target_include_directories(patient_server PRIVATE include)
target_link_libraries(patient_server PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
