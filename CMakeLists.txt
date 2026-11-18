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
add_executable(flova-linux-transport-test test/host/test_flova_linux_transport.cpp)
target_include_directories(flova-linux-transport-test PRIVATE
  packages/flova-arduino/include)
target_link_libraries(flova-linux-transport-test PRIVATE flova-linux)
set_property(TARGET flova-linux-transport-test APPEND_STRING PROPERTY LINK_FLAGS
  " -Wl,--wrap=getaddrinfo,--wrap=connect,--wrap=SSL_connect,--wrap=SSL_shutdown,--wrap=SSL_get_error,--wrap=SSL_read")
add_executable(flova-core-test test/host/test_flova_core.cpp)
target_link_libraries(flova-core-test PRIVATE flova-core)
add_executable(flova-configuration-installer-test test/host/test_flova_configuration_installer.cpp)
target_link_libraries(flova-configuration-installer-test PRIVATE flova-core)
add_executable(flova-link-cbor-test test/host/test_flova_link_cbor.cpp)
target_link_libraries(flova-link-cbor-test PRIVATE flova-core)
add_executable(flova-link-fuzz-test test/host/test_flova_link_fuzz.cpp)
target_link_libraries(flova-link-fuzz-test PRIVATE flova-core)
add_executable(flova-device-mqtt-test test/host/test_flova_device_mqtt.cpp)
target_include_directories(flova-device-mqtt-test PRIVATE
  test/host/arduino-shim
  packages/flova-arduino/include)
target_link_libraries(flova-device-mqtt-test PRIVATE flova-core flova-coremqtt)
add_executable(flova-custom-board examples/custom-board-basic/src/main.cpp)
target_link_libraries(flova-custom-board PRIVATE flova-core)
add_executable(flova-configuration-test test/host/test_flova_configuration.cpp)
target_include_directories(flova-configuration-test PRIVATE
  packages/flova-arduino/include
  packages/flova-esp32/include)
target_link_libraries(flova-configuration-test PRIVATE flova-core)
add_executable(flova-runtime-services-test test/host/test_flova_runtime_services.cpp)
target_include_directories(flova-runtime-services-test PRIVATE packages/flova-arduino/include)
target_link_libraries(flova-runtime-services-test PRIVATE flova-core)
add_executable(flova-lifecycle-test test/host/test_flova_lifecycle.cpp)
target_include_directories(flova-lifecycle-test PRIVATE
  test/host/arduino-shim packages/flova-arduino/include)
target_link_libraries(flova-lifecycle-test PRIVATE flova-core)
add_executable(flova-wifi-recovery-test test/host/test_flova_wifi_recovery.cpp)
target_include_directories(flova-wifi-recovery-test PRIVATE packages/flova-arduino/include)
target_link_libraries(flova-wifi-recovery-test PRIVATE flova-core)
enable_testing()
add_test(NAME flova-linux-rx-test COMMAND flova-linux-transport-test receive)
add_test(NAME flova-linux-resolver-test COMMAND flova-linux-transport-test resolver)
set_tests_properties(flova-linux-rx-test flova-linux-resolver-test PROPERTIES TIMEOUT 10)
add_test(NAME flova-wifi-recovery-test COMMAND flova-wifi-recovery-test)
add_executable(flova-mqtt-test test/host/test_flova_mqtt.cpp)
target_include_directories(flova-mqtt-test PRIVATE
  test/host/arduino-shim packages/flova-arduino/include)
target_link_libraries(flova-mqtt-test PRIVATE flova-core flova-coremqtt)
add_test(NAME flova-mqtt-test COMMAND flova-mqtt-test)
add_executable(flova-esp8266-ota-writer-test test/host/test_flova_esp8266_ota_writer.cpp)
target_include_directories(flova-esp8266-ota-writer-test PRIVATE packages/flova-esp8266/include)
add_test(NAME flova-esp8266-ota-writer-test COMMAND flova-esp8266-ota-writer-test)
foreach(mode IN ITEMS production development)
  set(target flova-provisioning-security-${mode}-test)
  add_executable(${target} test/host/test_flova_provisioning_security.cpp)
  target_include_directories(${target} PRIVATE packages/flova-arduino/include)
  if(mode STREQUAL "development")
    target_compile_definitions(${target} PRIVATE FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING=1)
  endif()
  add_test(NAME ${target} COMMAND ${target})
endforeach()
add_executable(flova-hardware-test test/host/test_flova_hardware.cpp)
target_include_directories(flova-hardware-test PRIVATE
  test/host/arduino-shim packages/flova-arduino/include
  packages/flova-esp32/include packages/flova-esp8266/include)
target_link_libraries(flova-hardware-test PRIVATE flova-core)
add_test(NAME flova-hardware-test COMMAND flova-hardware-test)
add_test(NAME flova-core-test COMMAND flova-core-test)
add_test(NAME flova-configuration-installer-test COMMAND flova-configuration-installer-test)
add_test(NAME flova-link-cbor-test COMMAND flova-link-cbor-test)
add_test(NAME flova-link-fuzz-test COMMAND flova-link-fuzz-test)
add_test(NAME flova-device-mqtt-test COMMAND flova-device-mqtt-test)
add_test(NAME flova-configuration-test COMMAND flova-configuration-test)
add_test(NAME flova-runtime-services-test COMMAND flova-runtime-services-test)
add_test(NAME flova-lifecycle-test COMMAND flova-lifecycle-test)

add_executable(flova-linux-test test/host/test_flova_linux.cpp)
target_link_libraries(flova-linux-test PRIVATE flova-linux "-Wl,--wrap=fsync" "-Wl,--wrap=rename")
add_test(NAME flova-linux-test COMMAND flova-linux-test)

add_executable(flova-linux-example examples/linux-device/main.cpp)
target_link_libraries(flova-linux-example PRIVATE flova-linux)

add_executable(flova-tls-memory-test test/host/test_flova_tls_memory.cpp)
target_include_directories(flova-tls-memory-test PRIVATE packages/flova-esp8266/include)
add_test(NAME flova-tls-memory-test COMMAND flova-tls-memory-test)

add_executable(flova-compact-runtime-test test/host/test_flova_compact_runtime.cpp)
target_link_libraries(flova-compact-runtime-test PRIVATE flova-core)
target_compile_definitions(flova-compact-runtime-test PRIVATE FLOVA_STREAM_DESCRIPTORS_IN_STORAGE=1)
add_test(NAME flova-compact-runtime-test COMMAND flova-compact-runtime-test)

function(add_flova_logging_test target)
  add_executable(${target} test/host/test_flova_logging.cpp)
  target_include_directories(${target} PRIVATE
    packages/flova-embedded-sdk/include packages/flova-arduino/include)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_flova_logging_test(flova-logging-default-test)
add_flova_logging_test(flova-logging-all-test)
target_compile_definitions(flova-logging-all-test PRIVATE FLOVA_LOG_LEVEL=FLOVA_LOG_LEVEL_TRACE)
add_flova_logging_test(flova-logging-disabled-test)
target_compile_definitions(flova-logging-disabled-test PRIVATE FLOVA_LOGGING_ENABLED=0)

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/../tmp")
set_tests_properties(flova-linux-test PROPERTIES ENVIRONMENT "TMPDIR=${CMAKE_SOURCE_DIR}/../tmp")
