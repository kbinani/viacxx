cmake_minimum_required(VERSION 3.16)
project(viacxx)

if ("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  add_subdirectory(deps/json EXCLUDE_FROM_ALL)

  add_executable(create-backward-converter tool/create-backward-converter.cpp)
  target_link_libraries(create-backward-converter nlohmann_json)

  set(via_asset_files
    #begin via_asset_files
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.10to1.11.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.11to1.12.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.12to1.13.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.13.2to1.14.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.13to1.13.2.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.14to1.15.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.15to1.16.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.16.2to1.17.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.16to1.16.2.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.17to1.18.json
    deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data/mapping-1.9.4to1.10.json
    #end via_asset_files
  )
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/include/via/backward.hpp ${CMAKE_CURRENT_SOURCE_DIR}/src/backward.cxx
    COMMAND create-backward-converter ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps/ViaBackwards/common/src/main/resources/assets/viabackwards/data
    DEPENDS create-backward-converter ${via_asset_files})

  add_custom_target(prepare-backward-converter
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/via/backward.hpp ${CMAKE_CURRENT_SOURCE_DIR}/src/backward.cxx
    SOURCES ${via_asset_files})
endif()

add_library(viacxx STATIC src/backwards.cxx include/via/backwards.hpp)
target_include_directories(viacxx PUBLIC include)
