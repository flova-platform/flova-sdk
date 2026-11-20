#pragma once

#include <stdint.h>

#ifndef FLOVA_OTA_TOTAL_TIMEOUT_MS
#define FLOVA_OTA_TOTAL_TIMEOUT_MS 300000UL
#endif

static_assert(FLOVA_OTA_TOTAL_TIMEOUT_MS > 0 && FLOVA_OTA_TOTAL_TIMEOUT_MS <= 0x7fffffffUL,
              "OTA total timeout must fit a wrap-safe tick interval");

inline bool flovaOtaTimedOut(uint32_t now, uint32_t startedAt,
                            uint32_t lastProgressAt, uint32_t inactivityMs) {
  return static_cast<uint32_t>(now - startedAt) >= FLOVA_OTA_TOTAL_TIMEOUT_MS ||
         static_cast<uint32_t>(now - lastProgressAt) >= inactivityMs;
}
