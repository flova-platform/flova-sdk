#pragma once

#include <stdint.h>

namespace flova {

struct StorageInfo {
  uint32_t usableBytes;
  uint32_t availableBytes;
  uint32_t maxRecordBytes;
  uint32_t eraseBlockBytes;
  uint16_t writeGranularity;
  bool persistent;
  bool wearSensitive;
  StorageInfo()
      : usableBytes(0), availableBytes(0), maxRecordBytes(0), eraseBlockBytes(0),
        writeGranularity(1), persistent(false), wearSensitive(false) {}
};

// Source compatibility for applications built against SDK 0.3.x.
typedef StorageInfo StorageCapabilities;

enum class HistoryOverflow : uint8_t { DropOldest, DropNewest };

struct HistoryRetentionPolicy {
  uint32_t maximumBytes;
  uint32_t maximumRecords;
  uint32_t maximumAgeSeconds;
  uint32_t minimumIntervalMs;
  HistoryOverflow overflow;
  HistoryRetentionPolicy()
      : maximumBytes(0), maximumRecords(0), maximumAgeSeconds(0), minimumIntervalMs(0),
        overflow(HistoryOverflow::DropOldest) {}
};

}  // namespace flova
