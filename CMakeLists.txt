# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include(pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(Otakutachi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED BUILD_TIME_PROVIDED_VERSION)
    set(BUILD_TIME_PROVIDED_VERSION 255.255.255)
    if (NOT DEFINED BUILD_VERSION_TYPE)
      set(BUILD_VERSION_TYPE U)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")
    endif()
else ()
  if (NOT DEFINED BUILD_VERSION_TYPE)
    set(BUILD_VERSION_TYPE R)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    endif()
endif()
message("BUILD_TIME_PROVIDED_VERSION=${BUILD_TIME_PROVIDED_VERSION} BUILD_VERSION_TYPE=${BUILD_VERSION_TYPE}")

project(${PROJECT_NAME} VERSION ${BUILD_TIME_PROVIDED_VERSION})

configure_file(include/BuildVersion.h.in include/BuildVersion.h)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)

# Tell CMake where to find the executable source file
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} ${PICO_TINYUSB_PATH}/src)

add_subdirectory(libs/git-commit-tracking)

add_executable(${PROJECT_NAME} 
    src/main.c
    src/cartbus.c
    src/msc_disk.cpp
    src/usb_descriptors.c
    src/lfs_hal.c
    src/EmuFATFS.cpp
    src/lfs.c
    src/lfs_util.c
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/git-commit-tracking
    ${PICO_TINYUSB_PATH}/src
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    hardware_flash
    hardware_pio
    hardware_dma
    tinyusb_device
    tinyusb_board
    cmake_git_commit_tracking
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/cartbus.pio)

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
