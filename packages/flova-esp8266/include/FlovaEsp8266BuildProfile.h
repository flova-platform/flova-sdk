#pragma once

#include <FlovaEsp8266FlashLog.h>

#ifndef FLOVA_STREAM_DESCRIPTORS_IN_STORAGE
#define FLOVA_STREAM_DESCRIPTORS_IN_STORAGE 1
#endif
#define FLOVA_BOARD_NAME "ESP8266"

#if defined(MMU_IRAM_HEAP)
#error "Flova ESP8266 requires the stock MMU; IRAM heap allocation is unsupported"
#endif
// Official Flova Link/OTA endpoints bound server TLS records to 2048 bytes.
// Arbitrary endpoints must explicitly select the full 16384-byte RX profile.
#ifndef FLOVA_ESP8266_BOUNDED_TLS_RECORDS
#define FLOVA_ESP8266_BOUNDED_TLS_RECORDS 1
#endif

// Board defaults keep the portable runtime explicitly bounded for ESP8266
// heap constraints. An application may define any value before including a
// Flova header.
#ifndef FLOVA_DATASTREAM_CAPACITY
#define FLOVA_DATASTREAM_CAPACITY 4
#endif
#ifndef FLOVA_PARAMETER_CAPACITY
#define FLOVA_PARAMETER_CAPACITY 4
#endif
#ifndef FLOVA_MAX_ACTIVE_DATASTREAMS_LIMIT
#define FLOVA_MAX_ACTIVE_DATASTREAMS_LIMIT 4
#endif
#ifndef FLOVA_TEXT_CAPACITY
#define FLOVA_TEXT_CAPACITY 48
#endif
#ifndef FLOVA_HISTORY_CAPACITY
#define FLOVA_HISTORY_CAPACITY 4
#endif
#ifndef FLOVA_COMMAND_DEDUP_CAPACITY
#define FLOVA_COMMAND_DEDUP_CAPACITY 4
#endif
#ifndef FLOVA_HARDWARE_INPUT_CAPACITY
#define FLOVA_HARDWARE_INPUT_CAPACITY 2
#endif
#ifndef FLOVA_HARDWARE_OUTPUT_CAPACITY
#define FLOVA_HARDWARE_OUTPUT_CAPACITY 2
#endif
