cmake_minimum_required(VERSION 3.10)
project(flova_sdk_core LANGUAGES C CXX)
include(GNUInstallDirs)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(third_party/coremqtt)

add_library(flova-link-cbor STATIC
  third_party/zcbor/flova/zcbor_common.c
  third_party/zcbor/flova/zcbor_decode.c
  third_party/zcbor/flova/zcbor_encode.c
  third_party/zcbor/flova/zcbor_print.c
  protocol/generated/firmware/src/flova_link_decode.c
  protocol/generated/firmware/src/flova_link_encode.c)
target_include_directories(flova-link-cbor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/zcbor/include>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/protocol/generated/firmware/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(flova-core INTERFACE)
set_target_properties(flova-core PROPERTIES EXPORT_NAME Core)
target_include_directories(flova-core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/packages/flova-embedded-sdk/include>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/protocol/generated/firmware/include>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/zcbor/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(flova-core INTERFACE flova-link-cbor)
target_compile_options(flova-core INTERFACE
  -Wall -Wextra -fno-exceptions -fno-rtti)
target_compile_definitions(flova-core INTERFACE
  FLOVA_DATASTREAM_CAPACITY=64 FLOVA_PARAMETER_CAPACITY=8 FLOVA_TEXT_CAPACITY=128 FLOVA_HISTORY_CAPACITY=64
  FLOVA_COMMAND_DEDUP_CAPACITY=16 FLOVA_HARDWARE_INPUT_CAPACITY=16
  FLOVA_HARDWARE_OUTPUT_CAPACITY=16
)

install(TARGETS flova-link-cbor flova-core flova-coremqtt EXPORT FlovaCoreTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY packages/flova-embedded-sdk/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY protocol/generated/firmware/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY third_party/zcbor/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT FlovaCoreTargets
  FILE FlovaCoreTargets.cmake
  NAMESPACE Flova::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlovaCore)

add_subdirectory(packages/flova-linux)

add_executable(flova-custom-board examples/custom-board-basic/src/main.cpp)
target_link_libraries(flova-custom-board PRIVATE flova-core)

add_executable(flova-linux-example examples/linux-device/main.cpp)
target_link_libraries(flova-linux-example PRIVATE flova-linux)

enable_testing()
add_subdirectory(test/host)
