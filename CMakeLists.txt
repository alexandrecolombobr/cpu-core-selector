cmake_minimum_required(VERSION 4.1)
project (cpu_core_selector
        VERSION 0.1
        DESCRIPTION "A small project to auto detect P-Cores and E-Cores to pin threads"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(OpenMP REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(UDEV REQUIRED libudev)

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(${PROJECT_NAME} cpu_core_selector.cpp)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fopenmp -O3 -flto")

# Create a simple configuration header
configure_file(config.h.in config.h)

# Include the configuration header in the build
target_include_directories(${PROJECT_NAME}
        PUBLIC "${PROJECT_BINARY_DIR}"
        PRIVATE ${UDEV_INCLUDE_DIRS})

target_link_libraries(cpu_core_selector ${OPENSSL_LIBRARIES} OpenMP::OpenMP_CXX ${UDEV_LIBRARIES})