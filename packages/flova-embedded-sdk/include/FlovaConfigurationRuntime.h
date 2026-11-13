#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "FlovaBuildConfig.h"
#include "FlovaDatastreamId.h"

// This is the shared, serialized-format-free configuration contract.  Link
// adapters fill exactly one Unit at a time; SDKs and board glue consume it
// without seeing CBOR or a generic object tree.
namespace flova {
namespace config {

static const size_t kConfigurationTextBytes = FLOVA_TEXT_CAPACITY;

enum class ValueKind : uint8_t { Boolean = 0, Int64 = 1, Float32 = 2, Float64 = 3, Text = 4 };
enum class UnitKind : uint8_t { Datastream = 0, System = 1, Safety = 3, Parameter = 5 };
enum class MappingKind : uint8_t { DigitalInput = 0, DigitalOutput = 1, AnalogInput = 2, PwmOutput = 3 };
enum class SafetyPolicy : uint8_t { None = 0, Minimum = 1, Maximum = 2, Range = 3, CommandExpiry = 4 };

struct Value {
  ValueKind kind;
  union {
    bool boolean;
    int64_t integer;
    float float32;
    double float64;
    char text[kConfigurationTextBytes];
  } data;

};

struct HardwareMapping {
  MappingKind kind;
  uint16_t pin;
  char pinReference[65];
  bool hasActiveHigh;
  bool activeHigh;
  bool hasPull;
  uint8_t pull;
  bool hasDebounceMs;
  uint32_t debounceMs;
  bool hasSampleMs;
  uint32_t sampleMs;
  bool hasMinimumOutputMs;
  uint32_t minimumOutputMs;

};

struct Datastream {
  DatastreamId id;
  uint8_t valueType;
  char uuid[37];
  char key[FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1];
  bool hasMinimum;
  bool hasMaximum;
  bool hasDefault;
  bool hasMapping;
  Value minimum;
  Value maximum;
  Value defaultValue;
  HardwareMapping mapping;
};

struct System {
  bool hasHeartbeatMs;
  uint32_t heartbeatMs;
  bool hasStatusLedPin;
  uint8_t statusLedPin;
  char statusLedPinReference[65];
  bool hasStatusLedActiveLow;
  bool statusLedActiveLow;
  bool hasBatchFlushMs;
  uint32_t batchFlushMs;
  bool hasFactoryResetPin;
  uint8_t factoryResetPin;
  char factoryResetPinReference[65];
  bool hasFactoryResetActiveLow;
  bool factoryResetActiveLow;
  bool hasFactoryResetProfile;
  uint8_t factoryResetProfile;
  bool hasFactoryResetTapCount;
  uint8_t factoryResetTapCount;
  bool hasFactoryResetHoldMs;
  uint32_t factoryResetHoldMs;
  bool hasFactoryResetWindowMs;
  uint32_t factoryResetWindowMs;
  bool hasFactoryResetDebounceMs;
  uint32_t factoryResetDebounceMs;
  bool hasFactoryResetReleaseConfirm;
  bool factoryResetReleaseConfirm;
};

struct Safety {
  DatastreamId datastreamId;
  SafetyPolicy policy;
  bool hasMinimum;
  bool hasMaximum;
  bool hasTimeoutMs;
  Value minimum;
  Value maximum;
  uint32_t timeoutMs;
};

struct Parameter {
  char key[FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1];
  uint8_t valueType;
  bool hasValue;
  bool hasDefault;
  Value value;
  Value defaultValue;
};

struct Unit {
  UnitKind kind;
  union {
    Datastream datastream;
    System system;
    Safety safety;
    Parameter parameter;
  } data;
};

inline bool valueTypeMatches(const Value& value, uint8_t type) {
  if ((value.kind == ValueKind::Float32 && !isfinite(value.data.float32)) ||
      (value.kind == ValueKind::Float64 && !isfinite(value.data.float64)) ||
      (value.kind == ValueKind::Text &&
       strnlen(value.data.text, sizeof(value.data.text)) == sizeof(value.data.text))) return false;
  return static_cast<uint8_t>(value.kind) == type ||
         (type == 2 && value.kind == ValueKind::Float32) ||
         (type == 3 && value.kind == ValueKind::Float64);
}

}  // namespace config
}  // namespace flova
