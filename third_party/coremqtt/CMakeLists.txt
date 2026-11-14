add_library(flova-coremqtt STATIC
  source/core_mqtt.c
  source/core_mqtt_state.c
  source/core_mqtt_serializer.c
  source/core_mqtt_prop_serializer.c
  source/core_mqtt_prop_deserializer.c
  source/core_mqtt_serializer_private.c)
target_include_directories(flova-coremqtt PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/interface>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/config>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
