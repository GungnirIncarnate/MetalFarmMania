include(FetchContent)

FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
)

FetchContent_MakeAvailable(json)

add_library(MetalFarmManiaDeps INTERFACE)

target_link_libraries(MetalFarmManiaDeps INTERFACE nlohmann_json)
target_include_directories(MetalFarmManiaDeps INTERFACE ${nlohmann_json_SOURCE_DIR}/include)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/RE-UE4SS/CMakeLists.txt")
	add_subdirectory(RE-UE4SS)
	target_link_libraries(MetalFarmManiaDeps INTERFACE UE4SS)
else()
	message(STATUS "RE-UE4SS not present yet; MetalFarmManiaDeps is configured as a placeholder target.")
endif()
