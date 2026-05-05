cmake_minimum_required(VERSION 3.16)
project(arm_project_simple LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(ARMP_IS_ARM FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|arm64|armv7|armv8|arm)")
    set(ARMP_IS_ARM TRUE)
    message(STATUS "[arm_project_simple] Architecture: ARM (${CMAKE_SYSTEM_PROCESSOR})")
else()
    message(STATUS "[arm_project_simple] Architecture: ${CMAKE_SYSTEM_PROCESSOR} (NEON functions will fallback to scalar)")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    add_compile_options($<$<CONFIG:Release>:-O3>)
endif()

if(ARMP_IS_ARM AND NOT MSVC)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
        add_compile_options(-mfpu=neon -mfloat-abi=hard)
    endif()
endif()

add_subdirectory(third_party)

add_executable(arm_project_simple
    src/main.cpp
    src/gui.cpp
    src/cli.cpp
    src/algo.cpp
    src/algo_scalar.cpp
)

target_include_directories(arm_project_simple PRIVATE src)

if(NOT MSVC)
    set_source_files_properties(
        src/algo_scalar.cpp
        PROPERTIES COMPILE_OPTIONS "-fno-tree-vectorize"
    )
endif()

if(ARMP_GUI_AVAILABLE)
    target_link_libraries(arm_project_simple PRIVATE armp_imgui)
    target_compile_definitions(arm_project_simple PRIVATE HAVE_GUI=1)
endif()

find_package(Threads REQUIRED)
target_link_libraries(arm_project_simple PRIVATE Threads::Threads)

set_target_properties(arm_project_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
