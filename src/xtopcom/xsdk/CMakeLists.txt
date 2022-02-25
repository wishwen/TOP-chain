# option
option(BUILD_SHARED_LIBS "build shared library(.so/.a)" OFF)

# version required
cmake_minimum_required(VERSION 3.8)
project(cppsdk)

if(BUILD_SHARED_LIBS)
    set(shared_str "shared")
else()
    set(shared_str "static")
endif()

message("build info: ${PROJECT_NAME}|${CMAKE_SYSTEM_NAME}|${CMAKE_BUILD_TYPE}|${shared_str}")

# path definitions
set(PROJ_DIR ${PROJECT_SOURCE_DIR})
set(SRC_DIR ${PROJ_DIR})

# include definitions
include_directories(
    ${SRC_DIR}
)

if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    add_definitions(-DLOG_CONSOLE)
    if (NOT MSVC)
        add_compile_options("-g")
    endif()
endif()

if(NOT MSVC)
    add_compile_options(-Wall)
    # add_compile_options(-fpermissive)
    add_compile_options(-fPIC)
endif()

aux_source_directory(${SRC_DIR} cppsdk_src)

# link definitions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    link_directories(${XDEPENDS}/libs/linux)
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Wno-write-strings)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    link_directories(${XDEPENDS}/libs/mac)
    add_compile_options(-Wno-writable-strings)
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options(-Wno-write-strings)
    add_compile_options(-Wno-unused-variable)
    add_compile_options(-Wno-unused-function)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    link_directories(${XDEPENDS}/libs/win)
    add_definitions(-D_WIN32_WINNT=0x0501)
    add_definitions(-DNOMINMAX)  # enable std::min/std::max
    add_definitions(-D_CRT_SECURE_NO_WARNINGS) # disable strncpy -> strncpy_s warnings
    add_compile_options(/Zc:__cplusplus) # using correct __cplusplus value
    add_compile_options(/utf-8) # input/output as utf-8
endif()

set(LINK_ARGS cppsdk xdata xbase)
if(NOT MSVC)
    set(LINK_ARGS ${LINK_ARGS})
endif()

# message("cppsdk_src: ${cppsdk_src}")
add_library(cppsdk ${cppsdk_src})

add_library(cppsdk_shared SHARED ${cppsdk_src})
target_link_libraries(cppsdk_shared ${LINK_ARGS})
