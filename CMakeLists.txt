cmake_minimum_required(VERSION 3.22)
project(MetalFarmMania LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(deps)

set(TARGET MetalFarmMania)

file(GLOB_RECURSE TARGET_SOURCES CONFIGURE_DEPENDS
	src/*.cpp
)

add_library(${TARGET} SHARED
	${TARGET_SOURCES}
)

target_include_directories(${TARGET} PRIVATE include)
target_link_libraries(${TARGET} PUBLIC MetalFarmManiaDeps)

set_target_properties(${TARGET} PROPERTIES
	OUTPUT_NAME "main"
)
