#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "FlovaBuildConfig.h"
#include "FlovaConfigurationRuntime.h"
#include "FlovaDatastreamId.h"
#include "FlovaStorageTypes.h"
#include "FlovaScheduling.h"

// Canonical standalone embedded SDK runtime. This header intentionally owns only
// bounded domain state and service seams; board code supplies Link, Storage,
// Clock, and Logger implementations. Keep Arduino, ESP, GPIO, networking,
// filesystem, exceptions, RTTI, and unbounded containers out of this include
// closure so the same core can be host-tested and reused by other boards.
namespace flova {

static const size_t kMaxDatastreams = FLOVA_DATASTREAM_CAPACITY;
static const size_t kMaxText = FLOVA_TEXT_CAPACITY;

enum class ValueType : uint8_t { Boolean, Int64, Float, Double, Text };
enum class Mode : uint8_t { State, Sample, Command, Event };
enum class OfflinePolicy : uint8_t { KeepLatest, StoreHistory, Drop, Reject };
enum class PersistencePolicy : uint8_t {
  None, Persistent, Immediate = Persistent, OnChange, Deferred
};
enum class Origin : uint8_t { Unknown, LocalLogic, SensorRead, PhysicalInput, UserCommand, CloudAutomation, DeviceRestore, Internal };
enum class Quality : uint8_t { Good, Stale, Invalid, HardwareError };
enum class WriteStatus : uint8_t { Accepted, Rejected, NoChange, Failed };
enum class MessageKind : uint8_t {
  StateUpdate,
  WriteRequest,
  Acknowledgement,
  Error,
  FlowControl,
  Rejection,
  Heartbeat,
  Configuration,
  TimeRequest,
  TimeResponse
};

struct WriteResult {
  WriteStatus status;
  const char* reason;
  const char* message;
  static WriteResult accept() { return {WriteStatus::Accepted, "", ""}; }
  static WriteResult reject(const char* reason, const char* message = "") { return {WriteStatus::Rejected, reason, message}; }
  static WriteResult noChange() { return {WriteStatus::NoChange, "", ""}; }
  static WriteResult failure(const char* reason, const char* message = "") { return {WriteStatus::Failed, reason, message}; }
  bool accepted() const { return status == WriteStatus::Accepted || status == WriteStatus::NoChange; }
};

inline WriteResult accept() { return WriteResult::accept(); }
inline WriteResult unchanged() { return WriteResult::noChange(); }
inline WriteResult reject(const char* reason, const char* message = "") {
  return WriteResult::reject(reason, message);
}

class Text {
 public:
  Text(const char* value = "") : valid_(copy(value)) {}
  const char* c_str() const { return value_; }
  bool empty() const { return value_[0] == 0; }
  bool valid() const { return valid_; }

 private:
  friend struct Value;
  bool copy(const char* value) {
    if (!value) value = "";
    const size_t length = strnlen(value, kMaxText);
    if (length >= kMaxText) {
      value_[0] = 0;
      return false;
    }
    memcpy(value_, value, length + 1);
    return true;
  }
  char value_[kMaxText];
  bool valid_;
};

struct RgbColor { uint8_t r, g, b; };

inline int rgbHexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

inline bool parseRgbHex(const Text& text, RgbColor& color) {
  if (!text.valid()) return false;
  const char* hex = text.c_str();
  if (strnlen(hex, 8) != 7 || hex[0] != '#') return false;
  uint8_t channels[3];
  for (size_t index = 0; index < 3; ++index) {
    const int high = rgbHexDigit(hex[index * 2 + 1]);
    const int low = rgbHexDigit(hex[index * 2 + 2]);
    if (high < 0 || low < 0) return false;
    channels[index] = static_cast<uint8_t>((high << 4) | low);
  }
  color = {channels[0], channels[1], channels[2]};
  return true;
}

inline Text formatRgbHex(const RgbColor& color) {
  char hex[8];
  snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);
  return Text(hex);
}

struct Value {
  ValueType type;
  union {
    union { bool boolean; int64_t integer; float floating; double number; } scalar;
    char text[kMaxText];
  };
  Value() : type(ValueType::Text), text{} {}
  static Value from(bool v) { Value out; out.type = ValueType::Boolean; out.scalar.boolean = v; return out; }
  static Value from(int64_t v) { Value out; out.type = ValueType::Int64; out.scalar.integer = v; return out; }
  static Value from(float v) { Value out; out.type = ValueType::Float; out.scalar.floating = v; return out; }
  static Value from(double v) { Value out; out.type = ValueType::Double; out.scalar.number = v; return out; }
  static Value from(const char* v) { Value out; out.type = ValueType::Text; copy(out.text, v); return out; }
  static Value from(const Text& v) { return from(v.c_str()); }
  template <size_t N>
  static void copy(char (&target)[N], const char* source) {
    if (!source) source = "";
    const size_t length = strnlen(source, N - 1);
    memcpy(target, source, length);
    target[length] = 0;
  }
};

inline bool validValue(const Value& value) {
  switch (value.type) {
    case ValueType::Boolean: case ValueType::Int64: return true;
    case ValueType::Float: return isfinite(value.scalar.floating);
    case ValueType::Double: return isfinite(value.scalar.number);
    case ValueType::Text: return strnlen(value.text, kMaxText) < kMaxText;
  }
  return false;
}

inline bool operator==(const Value& a, const Value& b) {
  if (a.type != b.type) return false;
  if (a.type == ValueType::Boolean) return a.scalar.boolean == b.scalar.boolean;
  if (a.type == ValueType::Int64) return a.scalar.integer == b.scalar.integer;
  if (a.type == ValueType::Float) return a.scalar.floating == b.scalar.floating;
  if (a.type == ValueType::Double) return a.scalar.number == b.scalar.number;
  return strcmp(a.text, b.text) == 0;
}

struct Message {
  MessageKind kind;
  DatastreamId datastreamId;
  Value value;
  char commandId[kMaxText];
  char correlationId[kMaxText];
  char reason[kMaxText];
  uint32_t revision;
  uint64_t messageId;
  uint64_t timestamp;
  uint64_t monotonic;
  uint64_t expiresAtUtcMs;
  uint32_t retryAfterMs;
  Origin origin;
  uint8_t resultStatus;
  Message()
      : kind(MessageKind::StateUpdate),
        datastreamId(FLOVA_INVALID_DATASTREAM_ID),
        revision(0),
        messageId(0),
        timestamp(0),
        monotonic(0),
        expiresAtUtcMs(0),
        retryAfterMs(0),
        origin(Origin::Unknown),
        resultStatus(0) {
    commandId[0] = correlationId[0] = reason[0] = 0;
  }
};

typedef bool (*DatastreamKeyReader)(void*, size_t, char*, size_t);
typedef bool (*StateUpdateReader)(void*, size_t, DatastreamId&, uint32_t&, Value&);

typedef void (*MessageReceiver)(void* context, const Message& message);

class Link {
 public:
  virtual ~Link() {}
  // Transport callbacks must queue bounded work and return. Device::run()
  // owns the point at which queued messages may invoke application hardware.
  virtual bool begin() = 0;
  virtual bool connected() const = 0;
  virtual bool applicationPaused() const { return false; }
  virtual bool send(const Message& message) = 0;
  // Optional bounded aggregation. Return the accepted prefix; every reading in
  // that prefix shares messageId and is retired by the same delivery ACK.
  virtual bool batchesState() const { return false; }
  virtual size_t sendStateBatch(StateUpdateReader, void*, size_t, uint64_t) { return 0; }
  virtual void poll() = 0;
  virtual void setReceiver(MessageReceiver receiver, void* context) = 0;
  virtual uint32_t messageNonce() const = 0;
  virtual bool bindDatastreams(const char* const*, size_t, DatastreamId*) { return false; }
  virtual bool bindDatastreamKeys(DatastreamKeyReader, void*, size_t, DatastreamId*) { return false; }
  // Binding may complete after authentication. Adapters must keep the IDs
  // bounded and expose the completed result through readDatastreamBinding().
  virtual bool bindingReady() const { return true; }
  virtual bool readDatastreamBinding(DatastreamId*, size_t) const { return false; }
  virtual void schedulingDeclarations(SchedulingReader, void*, size_t) {}
};

class Storage {
 public:
  virtual ~Storage() {}
  // The runtime owns service startup. Volatile or application-initialized
  // storage can keep the default; board storage overrides it.
  virtual bool begin() { return true; }
  virtual bool read(const char* key, void* output, size_t size) = 0;
  virtual bool write(const char* key, const void* value, size_t size) = 0;
  virtual bool remove(const char* key) = 0;
  // Removes only records owned by the Flova runtime.
  virtual bool clear() { return false; }
  virtual StorageCapabilities capabilities() const { return StorageCapabilities(); }
  // Backend facts for explicit developer diagnostics, never feature opt-in.
  virtual StorageInfo info() const { return capabilities(); }
  // Explicit diagnostics may refresh expensive filesystem accounting. Normal
  // mutations must not traverse the filesystem just to update a usage counter.
  virtual void refreshCapacity() {}
};

class Clock {
 public:
  virtual ~Clock() {}
  virtual uint64_t milliseconds() const = 0;
  virtual bool utcValid() const { return false; }
  virtual uint64_t utcMilliseconds() const { return 0; }
  virtual void setUtc(uint64_t, uint64_t) {}
};
class Logger { public: virtual ~Logger() {} virtual void log(const char* message) = 0; };

struct Diagnostics {
  uint32_t droppedHistory, expiredHistory, rejectedDeliveries, queueOverflow,
      storageFailures, duplicateCommands, clockSyncFailures;
  Diagnostics()
      : droppedHistory(0), expiredHistory(0), rejectedDeliveries(0),
        queueOverflow(0), storageFailures(0), duplicateCommands(0),
        clockSyncFailures(0) {}
};

template <typename T> struct Snapshot {
  T value;
  bool hasValue;
  uint64_t updatedAt;
  Origin origin;
  Quality quality;
  bool dirty;
  uint32_t revision;
};

class Device;
template <typename T> class Datastream;
template <typename T> class Setting;
template <typename T> struct Codec;

class Device {
 public:
  typedef WriteResult (*ValueWriteHandler)(void* context, const Value& value);
  typedef WriteResult (*FactoryResetHandler)(void* context,
                                             const char* commandId);
  Device(Link& link, Storage& storage, Clock& clock, Logger& logger)
      : link_(link), storage_(storage), clock_(clock), logger_(logger), count_(0), historyHead_(0), historyCount_(0), lastTimeRequest_(0), timeRequestStarted_(0), timeSequence_(0), nextMessageId_(static_cast<uint64_t>(link.messageNonce()) << 32), retryNotBefore_(0), historyLastSentAt_(0), factoryResetHandler_(nullptr), factoryResetContext_(nullptr), started_(false), bindingPending_(false), bindingFailed_(false) {
    pendingTimeId_[0] = 0;
  }

  // Bind names once, restore bounded state, and start the board-supplied link.
  // A remote binding may complete after begin(); persisted hardware values are
  // applied from run() after the binding is complete.
  bool begin() {
    if (started_) return false;
    if (!link_.messageNonce()) return false;
    for (size_t i = 0; i < count_; ++i) {
      if (states_[i].scheduling != Scheduling::Disabled &&
          (states_[i].mode == Mode::Sample || states_[i].mode == Mode::Event))
        return false;
    }
    link_.schedulingDeclarations(readScheduling, this, count_);
    link_.setReceiver(receive, this);
    DatastreamId ids[kMaxDatastreams] = {};
#if FLOVA_STREAM_DESCRIPTORS_IN_STORAGE
    if (count_ && !link_.bindDatastreamKeys(readKey, this, count_, ids)) return false;
#else
    const char* keys[kMaxDatastreams] = {};
    for (size_t i = 0; i < count_; ++i) keys[i] = descriptors_[i].key;
    if (count_ && !link_.bindDatastreams(keys, count_, ids)) return false;
#endif
    if (count_ && link_.bindingReady()) {
      if (!applyBinding(ids)) return false;
    } else if (count_) {
      bindingPending_ = true;
    }
    // Stored configuration may already have supplied stable runtime IDs.
    // Restore those values before cloud binding so local hardware can recover
    // after a power cycle while the network is unavailable.
    restore();
    restoreHistory();
    restoreResults();
    if (!link_.begin()) return false;
    started_ = true;
    return true;
  }

  // Call from the board loop. Hardware handlers and queued remote commands
  // are applied only through this lifecycle boundary.
  void run() {
    link_.poll();
    if (link_.applicationPaused()) return;
    if (bindingPending_) {
      if (link_.bindingReady()) {
        DatastreamId ids[kMaxDatastreams] = {};
        if (!link_.readDatastreamBinding(ids, count_) || !applyBinding(ids)) {
          bindingFailed_ = true;
          bindingPending_ = false;
          return;
        }
        bindingPending_ = false;
        // Direct portable integrations may not have IDs until binding.
        restore();
        restoreHistory();
        restoreResults();
      }
    }
    applyRestoredHardware();
    flushDuePersistence();
    if (!started_ || bindingPending_ || bindingFailed_ || !link_.connected()) return;
    flushResults();
    if (retryNotBefore_ && clock_.milliseconds() < retryNotBefore_) return;
    syncTime();
    // Start after the last selected stream so a busy producer cannot starve peers.
    if (link_.batchesState()) {
      publishBatch();
    } else {
      const size_t start = publishCursor_;
      for (size_t n = 0; n < count_; ++n) {
        const size_t slot = (start + n) % count_;
        if (states_[slot].dirty) publish(states_[slot]);
      }
    }
    flushHistory();
  }

  const Diagnostics& diagnostics() const { return diagnostics_; }
  StorageInfo storageInfo(bool refresh = false) {
    if (refresh) storage_.refreshCapacity();
    return storage_.info();
  }
  StorageInfo storageCapabilities(bool refresh = false) { return storageInfo(refresh); }
  bool flushPersistence() {
    bool ok = true;
    for (size_t i = 0; i < count_; ++i)
      if (states_[i].persistenceDirty) ok = savePersistence(states_[i]) && ok;
    return ok;
  }
  size_t datastreamCount() const { return count_; }
  // Universal firmware may explicitly delegate cloud scheduling for mapped
  // outputs. Custom declarations still opt in per stream before begin().
  bool allowCloudSchedulingForOutputs(bool enabled) {
    if (started_) return false;
    cloudSchedulingForOutputs_ = enabled;
    return true;
  }
  Scheduling scheduling(DatastreamId id) const {
    for (size_t i = 0; i < count_; ++i)
      if (states_[i].id == id) return states_[i].scheduling;
    return Scheduling::Disabled;
  }
  bool ready() const { return started_ && !bindingPending_ && !bindingFailed_ && link_.connected(); }
  uint64_t originateMessageId() { return ++nextMessageId_; }

  void setFactoryResetHandler(FactoryResetHandler handler, void* context) {
    factoryResetHandler_ = handler;
    factoryResetContext_ = context;
  }

  bool commandResultPending(const char* commandId) const {
    if (!commandId || !commandId[0]) return false;
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
      if (!pendingResults_[i].active) continue;
      char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(i));
      if (!storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_))) return true;
      if (strcmp(resultWorkspace_.message.commandId, commandId) == 0) return true;
    }
    return false;
  }

  template <typename T> Datastream<T> datastream(const char* key);
  template <typename T> Datastream<T> stream(const char* key) { return datastream<T>(key); }
  template <typename T> Setting<T> setting(const char* key, const T& defaultValue);

  bool setWriteHandler(DatastreamId id, ValueWriteHandler handler,
                       void* context) {
    State* current = stateForId(id);
    if (!current || !handler) return false;
    current->writeHandler.valueResult = handler;
    current->writeKind = WriteHandlerKind::ValueResult;
    current->writeContext = context;
    return true;
  }

  bool clearWriteHandler(DatastreamId id, ValueWriteHandler handler,
                         void* context) {
    State* current = stateForId(id);
    if (!current || current->writeKind != WriteHandlerKind::ValueResult ||
        current->writeHandler.valueResult != handler ||
        current->writeContext != context)
      return false;
    current->writeHandler.valueResult = nullptr;
    current->writeContext = nullptr;
    current->writeKind = WriteHandlerKind::None;
    return true;
  }

  WriteResult write(DatastreamId id, const Value& value,
                    Origin origin = Origin::LocalLogic) {
    State* current = stateForId(id);
    return current ? apply(*current, value, origin)
                   : WriteResult::failure("unknown_datastream");
  }

  WriteResult report(DatastreamId id, const Value& value,
                     Origin origin = Origin::SensorRead) {
    State* current = stateForId(id);
    return current ? report(*current, value, origin)
                   : WriteResult::failure("unknown_datastream");
  }

  // Link adapters call this with one schema-decoded bounded unit. The public
  // datastream API remains typed and never exposes the wire representation.
  bool applyConfigurationUnit(const config::Unit& unit) {
    if (unit.kind == config::UnitKind::System && unit.data.system.hasBatchFlushMs) {
      if (unit.data.system.batchFlushMs > 0x7fffffffUL) return false;
      batchFlushMs_ = unit.data.system.batchFlushMs;
      batchWaiting_ = false;
    }
    if (unit.kind == config::UnitKind::Datastream) {
      ValueType type;
      if (unit.data.datastream.valueType == 0) type = ValueType::Boolean;
      else if (unit.data.datastream.valueType == 1) type = ValueType::Int64;
      else if (unit.data.datastream.valueType == 2) type = ValueType::Float;
      else if (unit.data.datastream.valueType == 3) type = ValueType::Double;
      else if (unit.data.datastream.valueType == 4) type = ValueType::Text;
      else return false;
      if (!flovaValidDatastreamId(unit.data.datastream.id)) return false;
      State* current = nullptr;
      for (size_t i = 0; i < count_; ++i) if (states_[i].id == unit.data.datastream.id) { current = &states_[i]; break; }
      State* named = find(unit.data.datastream.key, type);
      if (current && named && current != named) return false;
      if (!current) current = named ? named : state(unit.data.datastream.key, type);
      if (!current) return false;
      current->id = unit.data.datastream.id;
      if (cloudSchedulingForOutputs_ && unit.data.datastream.hasMapping &&
          (unit.data.datastream.mapping.kind == config::MappingKind::DigitalOutput ||
           unit.data.datastream.mapping.kind == config::MappingKind::PwmOutput))
        current->scheduling = Scheduling::CloudOnly;
      return true;
    }
    if (unit.kind == config::UnitKind::Safety) {
      State* current = 0;
      for (size_t i = 0; i < count_; ++i) if (states_[i].id == unit.data.safety.datastreamId) { current = &states_[i]; break; }
      if (!current) return false;
      Descriptor descriptor;
      const size_t slot = static_cast<size_t>(current - states_);
      if (!readDescriptor(slot, descriptor)) return false;
      descriptor.hasSafetyMinimum = unit.data.safety.hasMinimum && descriptor.safetyMinimum.assign(unit.data.safety.minimum);
      descriptor.hasSafetyMaximum = unit.data.safety.hasMaximum && descriptor.safetyMaximum.assign(unit.data.safety.maximum);
      const uint8_t policy = static_cast<uint8_t>(unit.data.safety.policy);
      if (policy > 4 ||
          (unit.data.safety.hasMinimum && !descriptor.hasSafetyMinimum) ||
          (unit.data.safety.hasMaximum && !descriptor.hasSafetyMaximum) ||
          !writeDescriptor(slot, descriptor)) return false;
      current->safetyPolicy = policy;
      return true;
    }
    if (unit.kind == config::UnitKind::Parameter) {
      ParameterState* parameter = parameterFor(unit.data.parameter.key);
      ValueType declaredType;
      if (!parameter || !configurationValueType(unit.data.parameter.valueType, declaredType) ||
          declaredType != parameter->valueType) return false;
      if ((unit.data.parameter.hasValue && !config::valueTypeMatches(unit.data.parameter.value, unit.data.parameter.valueType)) ||
          (unit.data.parameter.hasDefault && !config::valueTypeMatches(unit.data.parameter.defaultValue, unit.data.parameter.valueType))) return false;
      if (unit.data.parameter.hasDefault && !storeParameterValue(parameter->value, unit.data.parameter.defaultValue)) return false;
      if (unit.data.parameter.hasValue && !storeParameterValue(parameter->value, unit.data.parameter.value)) return false;
      parameter->hasValue = unit.data.parameter.hasDefault || unit.data.parameter.hasValue || parameter->hasValue;
      return true;
    }
    return true;
  }

  bool validateConfigurationUnit(const config::Unit& unit) {
    if (unit.kind == config::UnitKind::System && unit.data.system.hasBatchFlushMs &&
        unit.data.system.batchFlushMs > 0x7fffffffUL) return false;
    if (unit.kind == config::UnitKind::Datastream) {
      ValueType type;
      if (!configurationValueType(unit.data.datastream.valueType, type) ||
          !flovaValidDatastreamId(unit.data.datastream.id) ||
          !unit.data.datastream.key[0] ||
          strnlen(unit.data.datastream.key,
                  sizeof(unit.data.datastream.key)) >=
              sizeof(unit.data.datastream.key))
        return false;
      if ((unit.data.datastream.hasMinimum &&
           !config::valueTypeMatches(unit.data.datastream.minimum,
                                    unit.data.datastream.valueType)) ||
          (unit.data.datastream.hasMaximum &&
           !config::valueTypeMatches(unit.data.datastream.maximum,
                                    unit.data.datastream.valueType)) ||
          (unit.data.datastream.hasDefault &&
           !config::valueTypeMatches(unit.data.datastream.defaultValue,
                                    unit.data.datastream.valueType)))
        return false;
      Descriptor descriptor;
      for (size_t i = 0; i < count_; ++i) {
        if (!readDescriptor(i, descriptor)) return false;
        if (strcmp(descriptor.key, unit.data.datastream.key) == 0)
          return states_[i].valueType == type;
      }
      return true;
    }
    if (unit.kind == config::UnitKind::Safety) {
      const uint8_t policy = static_cast<uint8_t>(unit.data.safety.policy);
      NumericBound ignored;
      return flovaValidDatastreamId(unit.data.safety.datastreamId) &&
             policy <= 4 &&
             (!unit.data.safety.hasMinimum ||
              ignored.assign(unit.data.safety.minimum)) &&
             (!unit.data.safety.hasMaximum ||
              ignored.assign(unit.data.safety.maximum));
    }
    if (unit.kind == config::UnitKind::Parameter) {
      ValueType type;
      const size_t keyLength = strnlen(unit.data.parameter.key, sizeof(unit.data.parameter.key));
      ParameterState* parameter = parameterFor(unit.data.parameter.key);
      return unit.data.parameter.key[0] && keyLength <= FLOVA_MAX_DATASTREAM_KEY_LENGTH &&
             parameter && configurationValueType(unit.data.parameter.valueType, type) &&
             parameter->valueType == type &&
             (!unit.data.parameter.hasValue || config::valueTypeMatches(unit.data.parameter.value, unit.data.parameter.valueType)) &&
             (!unit.data.parameter.hasDefault || config::valueTypeMatches(unit.data.parameter.defaultValue, unit.data.parameter.valueType));
    }
    return unit.kind == config::UnitKind::System;
  }

 private:
  template <typename T> friend class Datastream;
  template <typename T> friend class Setting;

  enum class WriteHandlerKind : uint8_t {
    None,
    ValueResult,
    BoolResult, Int64Result, FloatResult, DoubleResult, TextResult,
    BoolResultContext, Int64ResultContext, FloatResultContext,
    DoubleResultContext, TextResultContext,
    BoolVoid, Int64Void, FloatVoid, DoubleVoid, TextVoid,
    BoolVoidContext, Int64VoidContext, FloatVoidContext,
    DoubleVoidContext, TextVoidContext
  };

  union WriteHandler {
    ValueWriteHandler valueResult;
    WriteResult (*boolResult)(bool);
    WriteResult (*int64Result)(int64_t);
    WriteResult (*floatResult)(float);
    WriteResult (*doubleResult)(double);
    WriteResult (*textResult)(Text);
    WriteResult (*boolResultContext)(void*, bool);
    WriteResult (*int64ResultContext)(void*, int64_t);
    WriteResult (*floatResultContext)(void*, float);
    WriteResult (*doubleResultContext)(void*, double);
    WriteResult (*textResultContext)(void*, Text);
    void (*boolVoid)(bool);
    void (*int64Void)(int64_t);
    void (*floatVoid)(float);
    void (*doubleVoid)(double);
    void (*textVoid)(Text);
    void (*boolVoidContext)(void*, bool);
    void (*int64VoidContext)(void*, int64_t);
    void (*floatVoidContext)(void*, float);
    void (*doubleVoidContext)(void*, double);
    void (*textVoidContext)(void*, Text);
    WriteHandler() : valueResult(0) {}
  };

  // Safety bounds are numeric; reserving text storage here costs twice the
  // text capacity for every stream, including boolean and text streams.
  struct NumericBound {
    ValueType type;
    union { int64_t integer; double number; } value;
    NumericBound() : type(ValueType::Int64), value{} {}
    bool assign(const config::Value& input) {
      if (input.kind == config::ValueKind::Int64) {
        type = ValueType::Int64; value.integer = input.data.integer; return true;
      }
      type = ValueType::Double;
      if (input.kind == config::ValueKind::Float32) value.number = input.data.float32;
      else if (input.kind == config::ValueKind::Float64) value.number = input.data.float64;
      else return false;
      return isfinite(value.number);
    }
    int compare(const Value& input) const {
      if (type == ValueType::Int64 && input.type == ValueType::Int64)
        return (input.scalar.integer > value.integer) - (input.scalar.integer < value.integer);
      if (input.type == ValueType::Int64)
        return compareIntegerToNumber(input.scalar.integer, value.number);
      const double actual = input.type == ValueType::Float ? input.scalar.floating : input.scalar.number;
      if (type == ValueType::Int64)
        return -compareIntegerToNumber(value.integer, actual);
      return (actual > value.number) - (actual < value.number);
    }
    static int compareIntegerToNumber(int64_t integer, double number) {
      // Check the exclusive upper limit before conversion: INT64_MAX rounds
      // up to 2^63 as a double, and converting that double to int64_t is invalid.
      if (number >= 9223372036854775808.0) return -1;
      if (number < -9223372036854775808.0) return 1;
      const int64_t truncated = static_cast<int64_t>(number);
      if (integer != truncated) return (integer > truncated) - (integer < truncated);
      const double whole = static_cast<double>(truncated);
      return (whole > number) - (whole < number);
    }
  };

  struct Descriptor {
    NumericBound safetyMinimum;
    NumericBound safetyMaximum;
    HistoryRetentionPolicy history;
    char key[FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1] = {};
    bool hasSafetyMinimum = false;
    bool hasSafetyMaximum = false;
  };

  struct ParameterState {
    char key[FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1] = {};
    Value value;
    ValueType valueType = ValueType::Text;
    bool hasValue = false;
  };

  static bool storeParameterValue(Value& target, const config::Value& source) {
    target.type = static_cast<ValueType>(source.kind);
    if (target.type == ValueType::Text) {
      Value::copy(target.text, source.data.text);
    } else if (target.type == ValueType::Boolean) target.scalar.boolean = source.data.boolean;
    else if (target.type == ValueType::Int64) target.scalar.integer = source.data.integer;
    else if (target.type == ValueType::Float) target.scalar.floating = source.data.float32;
    else if (target.type == ValueType::Double) target.scalar.number = source.data.float64;
    else return false;
    return true;
  }

  ParameterState* parameterFor(const char* key) {
    if (!key || !*key || strnlen(key, FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1) > FLOVA_MAX_DATASTREAM_KEY_LENGTH) return nullptr;
    for (size_t i = 0; i < parameterCount_; ++i)
      if (!strcmp(parameters_[i].key, key)) return &parameters_[i];
    return nullptr;
  }

  template <typename T> ParameterState* registerParameter(const char* key, const T& defaultValue) {
    if (started_ || !key || !*key || strnlen(key, FLOVA_MAX_DATASTREAM_KEY_LENGTH + 1) > FLOVA_MAX_DATASTREAM_KEY_LENGTH) return nullptr;
    if (ParameterState* existing = parameterFor(key)) return existing->valueType == Codec<T>::encode(defaultValue).type ? existing : nullptr;
    if (parameterCount_ >= FLOVA_PARAMETER_CAPACITY) return nullptr;
    Value encoded = Codec<T>::encode(defaultValue);
    if (!Codec<T>::valid(defaultValue) || !validValue(encoded)) return nullptr;
    ParameterState& parameter = parameters_[parameterCount_++];
    memcpy(parameter.key, key, strlen(key) + 1);
    parameter.valueType = encoded.type;
    parameter.value = encoded;
    parameter.hasValue = true;
    return &parameter;
  }

  bool readDescriptor(size_t slot, Descriptor& out) {
    if (slot >= count_) return false;
#if FLOVA_STREAM_DESCRIPTORS_IN_STORAGE
    if (descriptorCacheSlot_ != slot) {
      char key[24]; snprintf(key, sizeof(key), "stream:%u", static_cast<unsigned>(slot));
      Descriptor next;
      if (!storage_.read(key, &next, sizeof(next)) ||
          !next.key[0] || strnlen(next.key, sizeof(next.key)) >= sizeof(next.key)) {
        ++diagnostics_.storageFailures;
        return false;
      }
      descriptorCache_ = next;
      descriptorCacheSlot_ = slot;
    }
    out = descriptorCache_;
#else
    out = descriptors_[slot];
#endif
    return true;
  }

  bool writeDescriptor(size_t slot, const Descriptor& value) {
#if FLOVA_STREAM_DESCRIPTORS_IN_STORAGE
    char key[24]; snprintf(key, sizeof(key), "stream:%u", static_cast<unsigned>(slot));
    Descriptor verified;
    // Registration is explicit setup work. No filesystem work runs from a
    // global constructor; board sketches register streams from setup().
    if (!storage_.begin()) return false;
    const bool identical = storage_.read(key, &verified, sizeof(verified)) &&
                           memcmp(&verified, &value, sizeof(value)) == 0;
    if (!identical && (!storage_.write(key, &value, sizeof(value)) ||
        !storage_.read(key, &verified, sizeof(verified)) ||
        memcmp(&verified, &value, sizeof(value)) != 0)) {
      descriptorCacheSlot_ = kMaxDatastreams;
      ++diagnostics_.storageFailures;
      return false;
    }
    descriptorCache_ = value;
    descriptorCacheSlot_ = slot;
#else
    descriptors_[slot] = value;
#endif
    return true;
  }

  static bool readKey(void* context, size_t slot, char* output, size_t capacity) {
    Descriptor descriptor;
    Device& self = *static_cast<Device*>(context);
    if (!self.readDescriptor(slot, descriptor)) return false;
    const size_t length = strlen(descriptor.key);
    if (!output || capacity <= length) return false;
    memcpy(output, descriptor.key, length + 1);
    return true;
  }

  struct State {
    // Store the payload beside the other tags, avoiding Value's standalone
    // alignment padding in every resident registry slot.
    union Payload {
      decltype(Value::scalar) scalar;
      char text[kMaxText];
      Payload() : text{} {}
    } payload;
    uint64_t updatedAt = 0;
    uint64_t lastHistoryAt = 0;
    uint32_t revision = 0;
    uint32_t lastCloudRevision = 0;
    uint32_t persistenceIntervalMs = 1000;
    uint32_t persistenceDueAt = 0;
    WriteHandler writeHandler;
    void* writeContext = nullptr;
    DatastreamId id = FLOVA_INVALID_DATASTREAM_ID;
    ValueType valueType : 3;
    Mode mode : 2;
    OfflinePolicy offline : 2;
    PersistencePolicy persistence : 2;
    Origin origin : 3;
    Quality quality : 2;
    bool hasValue : 1;
    bool dirty : 1;
    bool restorePending : 1;
    bool persistenceDirty : 1;
    bool persistenceStored : 1;
    WriteHandlerKind writeKind : 5;
    uint8_t safetyPolicy : 3;
    Scheduling scheduling = Scheduling::Disabled;
    State() : valueType(ValueType::Text), mode(Mode::State),
        offline(OfflinePolicy::KeepLatest), persistence(PersistencePolicy::None),
        origin(Origin::Unknown), quality(Quality::Stale), hasValue(false),
        dirty(false), restorePending(false), persistenceDirty(false),
        persistenceStored(false), writeKind(WriteHandlerKind::None),
        safetyPolicy(0) {}
  };
  static bool readScheduling(void* context, size_t index, uint16_t& id,
                             Scheduling& mode) {
    Device& device = *static_cast<Device*>(context);
    if (index >= device.count_ || !flovaValidDatastreamId(device.states_[index].id)) return false;
    id = device.states_[index].id;
    mode = device.states_[index].scheduling;
    return true;
  }
  static Value readValue(const State& state) {
    Value result;
    result.type = state.valueType;
    if (result.type == ValueType::Text) memcpy(result.text, state.payload.text, kMaxText);
    else result.scalar = state.payload.scalar;
    return result;
  }
  static void storeValue(State& state, const Value& value) {
    state.valueType = value.type;
    if (value.type == ValueType::Text) memcpy(state.payload.text, value.text, kMaxText);
    else state.payload.scalar = value.scalar;
  }

  struct PendingResult { uint32_t magic; bool active; uint64_t sentAt; Message message; PendingResult() : magic(0x43524554UL), active(false), sentAt(0) {} };
  struct ResultIndex {
    uint64_t messageId = 0;
    uint64_t sentAt = 0;
    uint64_t sequence = 0;
    uint32_t fingerprint = 0;
    bool active = false;
  };
  struct Persisted { uint32_t magic; DatastreamId datastreamId; Value value; uint32_t revision; };
  struct HistoryRecord { DatastreamId datastreamId; Value value; uint64_t messageId; uint64_t timestamp; uint64_t monotonic; uint64_t expiresAt; Origin origin; uint32_t revision; };
  struct HistoryMeta { uint32_t magic; uint16_t head; uint16_t count; };

  State* state(const char* key, ValueType type) {
    if (!key || !*key || strlen(key) > FLOVA_MAX_DATASTREAM_KEY_LENGTH) return nullptr;
    Descriptor descriptor;
    for (size_t i = 0; i < count_; ++i) {
      if (!readDescriptor(i, descriptor)) return nullptr;
      if (strcmp(descriptor.key, key) == 0)
        return states_[i].valueType == type ? &states_[i] : nullptr;
    }
    if (count_ == kMaxDatastreams) return nullptr;
    Descriptor initial{};
    memcpy(initial.key, key, strlen(key) + 1);
    if (!writeDescriptor(count_, initial)) return nullptr;
    State& out = states_[count_++];
    out.valueType = type;
    return &out;
  }
  State* find(const char* key, ValueType type) {
    if (!key) return nullptr;
    Descriptor descriptor;
    for (size_t i = 0; i < count_; ++i) {
      if (!readDescriptor(i, descriptor)) return nullptr;
      if (strcmp(descriptor.key, key) == 0)
        return states_[i].valueType == type ? &states_[i] : nullptr;
    }
    return nullptr;
  }
  bool retention(State& state, const HistoryRetentionPolicy& value) {
    Descriptor descriptor;
    const size_t slot = static_cast<size_t>(&state - states_);
    if (!readDescriptor(slot, descriptor)) return false;
    descriptor.history = value;
    return writeDescriptor(slot, descriptor);
  }
  State* stateForId(DatastreamId id) {
    for (size_t i = 0; i < count_; ++i)
      if (states_[i].id == id) return &states_[i];
    return 0;
  }

  static void receive(void* context, const Message& message) { static_cast<Device*>(context)->receive(message); }
  bool applyBinding(const DatastreamId* ids) {
    if (!ids) return false;
    for (size_t i = 0; i < count_; ++i) {
      if (!flovaValidDatastreamId(ids[i])) return false;
      for (size_t prior = 0; prior < i; ++prior) if (ids[prior] == ids[i]) return false;
    }
    for (size_t i = 0; i < count_; ++i) {
      states_[i].id = ids[i];
      if (states_[i].hasValue && states_[i].offline == OfflinePolicy::KeepLatest)
        states_[i].dirty = true;
    }
    return true;
  }
  void receive(const Message& message) {
    if (message.kind == MessageKind::TimeResponse) return acceptTime(message);
    if (message.kind == MessageKind::Acknowledgement) {
      acknowledgeDelivery(message.messageId);
      return;
    }
    if (message.kind == MessageKind::FlowControl) {
      retryNotBefore_ = clock_.milliseconds() + message.retryAfterMs;
      return;
    }
    if (message.kind == MessageKind::Rejection) {
      rejectDelivery(message.messageId);
      return;
    }
    if (message.kind != MessageKind::WriteRequest) return;
    const int prior = resultForCommand(message.commandId);
    if (prior >= 0) {
      ++diagnostics_.duplicateCommands;
      // Replay the original outcome, never convert a rejection to success.
      link_.send(resultWorkspace_.message);
      return;
    }
    if (prior == -2 || !reserveCommand(message)) {
      replyStorageUnavailable(message);
      return;
    }
    if (message.datastreamId == FLOVA_FACTORY_RESET_DATASTREAM_ID) {
      if (message.expiresAtUtcMs &&
          (!clock_.utcValid() || clock_.utcMilliseconds() >= message.expiresAtUtcMs))
        return acknowledge(message, WriteResult::reject(clock_.utcValid() ? "command_expired" : "utc_time_required"), nullptr);
      const WriteResult result =
          message.value.type == ValueType::Boolean && message.value.scalar.boolean &&
                  factoryResetHandler_
              ? factoryResetHandler_(factoryResetContext_, message.commandId)
              : WriteResult::reject("factory_reset_invalid");
      acknowledge(message, result, nullptr);
      return;
    }
    State* current = 0;
    for (size_t i = 0; i < count_; ++i) if (states_[i].id == message.datastreamId) { current = &states_[i]; break; }
    if (!current) return acknowledge(message, WriteResult::reject("unknown_datastream"), 0);
    if (message.expiresAtUtcMs && (!clock_.utcValid() || clock_.utcMilliseconds() >= message.expiresAtUtcMs)) return acknowledge(message, WriteResult::reject(clock_.utcValid() ? "command_expired" : "utc_time_required"), current);
    bool stale = message.revision && message.revision <= current->lastCloudRevision;
    WriteResult result = stale
        ? current->hasValue ? WriteResult::noChange() : WriteResult::reject("state_unavailable")
        : apply(*current, message.value, message.origin);
    if (result.accepted() && message.revision > current->lastCloudRevision) current->lastCloudRevision = message.revision;
    acknowledge(message, result, current);
  }

  static uint32_t commandFingerprint(const char* commandId) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; commandId && i < kMaxText && commandId[i]; ++i)
      hash = (hash ^ static_cast<uint8_t>(commandId[i])) * 16777619UL;
    return hash;
  }

  int resultForCommand(const char* commandId) {
    if (!commandId || !commandId[0] || strnlen(commandId, kMaxText) == kMaxText) return -1;
    const uint32_t fingerprint = commandFingerprint(commandId);
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
      if (!pendingResults_[i].messageId || pendingResults_[i].fingerprint != fingerprint) continue;
      char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(i));
      if (!storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
          !validResult(resultWorkspace_)) {
        ++diagnostics_.storageFailures;
        return -2;
      }
      if (!strcmp(resultWorkspace_.message.commandId, commandId)) return static_cast<int>(i);
    }
    return -1;
  }

  static bool validResult(const PendingResult& result) {
    return result.magic == 0x43524554UL && result.message.messageId &&
           strnlen(result.message.commandId, kMaxText) < kMaxText &&
           strnlen(result.message.correlationId, kMaxText) < kMaxText &&
           strnlen(result.message.reason, kMaxText) < kMaxText &&
           validValue(result.message.value) &&
           (result.message.kind == MessageKind::Acknowledgement || result.message.kind == MessageKind::Error);
  }

  bool storeResult(size_t slot, const Message& message, uint64_t sequence) {
    resultWorkspace_.magic = 0x43524554UL;
    resultWorkspace_.active = true;
    // The old persisted sentAt field is now the durable ledger sequence.
    // Retry timing remains volatile in ResultIndex.
    resultWorkspace_.sentAt = sequence;
    // Preserve the complete object representation for exact read-back checking,
    // including padding in this existing ABI-local persisted record format.
    memcpy(&resultWorkspace_.message, &message, sizeof(message));
    char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(slot));
    if (!storage_.write(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
        !storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
        resultWorkspace_.magic != 0x43524554UL || !resultWorkspace_.active ||
        resultWorkspace_.sentAt != sequence ||
        memcmp(&resultWorkspace_.message, &message, sizeof(message)) != 0) {
      ++diagnostics_.storageFailures;
      return false;
    }
    ResultIndex& index = pendingResults_[slot];
    index.messageId = message.messageId;
    index.sequence = sequence;
    index.fingerprint = commandFingerprint(message.commandId);
    index.sentAt = 0;
    index.active = true;
    return true;
  }

  // Keep full Message temporaries out of receive's frame. Otherwise GCC keeps
  // that frame live while acknowledge() builds a second reply, exhausting the
  // ESP8266 continuation stack during a durable command completion.
#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  void replyStorageUnavailable(const Message& request) {
    Message reply;
    fillReply(reply, request, WriteResult::failure("command_storage_unavailable"), nullptr);
    link_.send(reply);
  }

#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  bool reserveCommand(const Message& request) {
    if (!request.commandId[0] || strnlen(request.commandId, kMaxText) == kMaxText) return false;
    size_t slot = FLOVA_COMMAND_DEDUP_CAPACITY;
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
      if (pendingResults_[i].active) continue;
      if (!pendingResults_[i].messageId) { slot = i; break; }
      if (slot == FLOVA_COMMAND_DEDUP_CAPACITY || pendingResults_[i].sequence < pendingResults_[slot].sequence) slot = i;
    }
    if (slot == FLOVA_COMMAND_DEDUP_CAPACITY) { ++diagnostics_.queueOverflow; return false; }
    // Persist an indeterminate outcome BEFORE touching hardware. A reset in
    // the hardware/result gap must never silently execute the operation again.
    Message intent;
    fillReply(intent, request, WriteResult::failure("command_outcome_unknown"), nullptr);
    return storeResult(slot, intent, ++resultSequence_);
  }

  WriteResult apply(State& state, const Value& value, Origin origin) {
    if (link_.applicationPaused()) return WriteResult::failure("maintenance");
    if (state.valueType != value.type) return WriteResult::reject("type_mismatch");
    if (!validValue(value)) return WriteResult::reject("invalid_value");
    if ((state.mode == Mode::Sample || state.mode == Mode::Event)) return WriteResult::reject("not_writable");
    if (!safe(state, value)) return WriteResult::reject(state.safetyPolicy == 1 || state.safetyPolicy == 3 ? "safety_minimum" : "safety_maximum");
    if (!link_.connected() && state.offline == OfflinePolicy::Reject)
      return WriteResult::reject("offline_delivery_required");
    if (state.hasValue && readValue(state) == value && !state.restorePending)
      return WriteResult::noChange();
    if (state.hasValue && readValue(state) == value && state.restorePending) {
      WriteResult result = invoke(state, value);
      if (result.accepted()) {
        state.restorePending = false;
        state.quality = Quality::Good;
      }
      return result;
    }
    WriteResult result = invoke(state, value);
    // A rejected hardware write is authoritative: do not update cache,
    // persistence, revision, or outbound state until the handler accepts it.
    if (result.accepted()) update(state, value, origin);
    return result;
  }

  WriteResult report(State& state, const Value& value, Origin origin) {
    if (link_.applicationPaused()) return WriteResult::failure("maintenance");
    if (state.valueType != value.type) return WriteResult::reject("type_mismatch");
    if (!validValue(value)) return WriteResult::reject("invalid_value");
    if (!link_.connected() && state.offline == OfflinePolicy::Reject)
      return WriteResult::reject("offline_delivery_required");
    update(state, value, origin);
    return WriteResult::accept();
  }

  WriteResult invoke(State& s, const Value& v) {
    switch (s.writeKind) {
      case WriteHandlerKind::ValueResult: return s.writeHandler.valueResult(s.writeContext, v);
      case WriteHandlerKind::BoolResult: return s.writeHandler.boolResult(v.scalar.boolean);
      case WriteHandlerKind::Int64Result: return s.writeHandler.int64Result(v.scalar.integer);
      case WriteHandlerKind::FloatResult: return s.writeHandler.floatResult(v.scalar.floating);
      case WriteHandlerKind::DoubleResult: return s.writeHandler.doubleResult(v.scalar.number);
      case WriteHandlerKind::TextResult: return s.writeHandler.textResult(Text(v.text));
      case WriteHandlerKind::BoolResultContext: return s.writeHandler.boolResultContext(s.writeContext, v.scalar.boolean);
      case WriteHandlerKind::Int64ResultContext: return s.writeHandler.int64ResultContext(s.writeContext, v.scalar.integer);
      case WriteHandlerKind::FloatResultContext: return s.writeHandler.floatResultContext(s.writeContext, v.scalar.floating);
      case WriteHandlerKind::DoubleResultContext: return s.writeHandler.doubleResultContext(s.writeContext, v.scalar.number);
      case WriteHandlerKind::TextResultContext: return s.writeHandler.textResultContext(s.writeContext, Text(v.text));
      case WriteHandlerKind::BoolVoid: s.writeHandler.boolVoid(v.scalar.boolean); break;
      case WriteHandlerKind::Int64Void: s.writeHandler.int64Void(v.scalar.integer); break;
      case WriteHandlerKind::FloatVoid: s.writeHandler.floatVoid(v.scalar.floating); break;
      case WriteHandlerKind::DoubleVoid: s.writeHandler.doubleVoid(v.scalar.number); break;
      case WriteHandlerKind::TextVoid: s.writeHandler.textVoid(Text(v.text)); break;
      case WriteHandlerKind::BoolVoidContext: s.writeHandler.boolVoidContext(s.writeContext, v.scalar.boolean); break;
      case WriteHandlerKind::Int64VoidContext: s.writeHandler.int64VoidContext(s.writeContext, v.scalar.integer); break;
      case WriteHandlerKind::FloatVoidContext: s.writeHandler.floatVoidContext(s.writeContext, v.scalar.floating); break;
      case WriteHandlerKind::DoubleVoidContext: s.writeHandler.doubleVoidContext(s.writeContext, v.scalar.number); break;
      case WriteHandlerKind::TextVoidContext: s.writeHandler.textVoidContext(s.writeContext, Text(v.text)); break;
      default: return WriteResult::failure("write_handler_missing");
    }
    return WriteResult::accept();
  }

  static bool toValue(const config::Value& source, Value& target) {
    if (source.kind == config::ValueKind::Boolean) { target = Value::from(source.data.boolean); return true; }
    if (source.kind == config::ValueKind::Int64) { target = Value::from(source.data.integer); return true; }
    if (source.kind == config::ValueKind::Float32) { target = Value::from(source.data.float32); return true; }
    if (source.kind == config::ValueKind::Float64) { target = Value::from(source.data.float64); return true; }
    if (source.kind == config::ValueKind::Text) { target = Value::from(source.data.text); return true; }
    return false;
  }

  static bool configurationValueType(uint8_t input, ValueType& output) {
    if (input == 0) output = ValueType::Boolean;
    else if (input == 1) output = ValueType::Int64;
    else if (input == 2) output = ValueType::Float;
    else if (input == 3) output = ValueType::Double;
    else if (input == 4) output = ValueType::Text;
    else return false;
    return true;
  }

  bool safe(const State& state, const Value& value) {
    if (!state.safetyPolicy || state.safetyPolicy == 4 || value.type == ValueType::Text) return true;
    if (value.type == ValueType::Boolean) return false;
    Descriptor descriptor;
    if (!readDescriptor(static_cast<size_t>(&state - states_), descriptor)) return false;
    if ((state.safetyPolicy == 1 || state.safetyPolicy == 3) && descriptor.hasSafetyMinimum) {
      if (descriptor.safetyMinimum.compare(value) < 0) return false;
    }
    if ((state.safetyPolicy == 2 || state.safetyPolicy == 3) && descriptor.hasSafetyMaximum) {
      if (descriptor.safetyMaximum.compare(value) > 0) return false;
    }
    return true;
  }

  static double numeric(const Value& value) {
    if (value.type == ValueType::Int64) return static_cast<double>(value.scalar.integer);
    return value.type == ValueType::Float ? value.scalar.floating : value.scalar.number;
  }

  void setWrite(State* s, WriteResult (*h)(bool)) { if (s) { s->writeHandler.boolResult = h; s->writeKind = WriteHandlerKind::BoolResult; } }
  void setWrite(State* s, WriteResult (*h)(int64_t)) { if (s) { s->writeHandler.int64Result = h; s->writeKind = WriteHandlerKind::Int64Result; } }
  void setWrite(State* s, WriteResult (*h)(float)) { if (s) { s->writeHandler.floatResult = h; s->writeKind = WriteHandlerKind::FloatResult; } }
  void setWrite(State* s, WriteResult (*h)(double)) { if (s) { s->writeHandler.doubleResult = h; s->writeKind = WriteHandlerKind::DoubleResult; } }
  void setWrite(State* s, WriteResult (*h)(Text)) { if (s) { s->writeHandler.textResult = h; s->writeKind = WriteHandlerKind::TextResult; } }
  void setWrite(State* s, WriteResult (*h)(void*, bool), void* c) { if (s) { s->writeHandler.boolResultContext = h; s->writeKind = WriteHandlerKind::BoolResultContext; s->writeContext = c; } }
  void setWrite(State* s, WriteResult (*h)(void*, int64_t), void* c) { if (s) { s->writeHandler.int64ResultContext = h; s->writeKind = WriteHandlerKind::Int64ResultContext; s->writeContext = c; } }
  void setWrite(State* s, WriteResult (*h)(void*, float), void* c) { if (s) { s->writeHandler.floatResultContext = h; s->writeKind = WriteHandlerKind::FloatResultContext; s->writeContext = c; } }
  void setWrite(State* s, WriteResult (*h)(void*, double), void* c) { if (s) { s->writeHandler.doubleResultContext = h; s->writeKind = WriteHandlerKind::DoubleResultContext; s->writeContext = c; } }
  void setWrite(State* s, WriteResult (*h)(void*, Text), void* c) { if (s) { s->writeHandler.textResultContext = h; s->writeKind = WriteHandlerKind::TextResultContext; s->writeContext = c; } }
  void setWrite(State* s, void (*h)(bool)) { if (s) { s->writeHandler.boolVoid = h; s->writeKind = WriteHandlerKind::BoolVoid; } }
  void setWrite(State* s, void (*h)(int64_t)) { if (s) { s->writeHandler.int64Void = h; s->writeKind = WriteHandlerKind::Int64Void; } }
  void setWrite(State* s, void (*h)(float)) { if (s) { s->writeHandler.floatVoid = h; s->writeKind = WriteHandlerKind::FloatVoid; } }
  void setWrite(State* s, void (*h)(double)) { if (s) { s->writeHandler.doubleVoid = h; s->writeKind = WriteHandlerKind::DoubleVoid; } }
  void setWrite(State* s, void (*h)(Text)) { if (s) { s->writeHandler.textVoid = h; s->writeKind = WriteHandlerKind::TextVoid; } }
  void setWrite(State* s, void (*h)(void*, bool), void* c) { if (s) { s->writeHandler.boolVoidContext = h; s->writeKind = WriteHandlerKind::BoolVoidContext; s->writeContext = c; } }
  void setWrite(State* s, void (*h)(void*, int64_t), void* c) { if (s) { s->writeHandler.int64VoidContext = h; s->writeKind = WriteHandlerKind::Int64VoidContext; s->writeContext = c; } }
  void setWrite(State* s, void (*h)(void*, float), void* c) { if (s) { s->writeHandler.floatVoidContext = h; s->writeKind = WriteHandlerKind::FloatVoidContext; s->writeContext = c; } }
  void setWrite(State* s, void (*h)(void*, double), void* c) { if (s) { s->writeHandler.doubleVoidContext = h; s->writeKind = WriteHandlerKind::DoubleVoidContext; s->writeContext = c; } }
  void setWrite(State* s, void (*h)(void*, Text), void* c) { if (s) { s->writeHandler.textVoidContext = h; s->writeKind = WriteHandlerKind::TextVoidContext; s->writeContext = c; } }

  void update(State& state, const Value& value, Origin origin) {
    const bool changed = !state.hasValue || !(readValue(state) == value);
    storeValue(state, value); state.hasValue = true; state.origin = origin; state.quality = Quality::Good; state.restorePending = false; state.revision++; state.updatedAt = clock_.milliseconds();
    const bool bound = flovaValidDatastreamId(state.id);
    state.dirty = bound && (link_.connected() || state.offline == OfflinePolicy::KeepLatest);
    if (bound && !link_.connected() && state.offline == OfflinePolicy::StoreHistory) {
      Descriptor descriptor;
      if (readDescriptor(static_cast<size_t>(&state - states_), descriptor) &&
          (!descriptor.history.minimumIntervalMs || !state.lastHistoryAt ||
           clock_.milliseconds() - state.lastHistoryAt >= descriptor.history.minimumIntervalMs)) {
        if (queueHistory(state, descriptor.history))
          state.lastHistoryAt = clock_.milliseconds();
      }
    }
    if (state.persistence != PersistencePolicy::None &&
        (state.persistence == PersistencePolicy::Persistent || changed ||
         !state.persistenceStored)) {
      if (state.persistence == PersistencePolicy::Deferred) {
        // Anchor the deadline to the first unsaved update, not the latest
        // sample: a continuously changing sensor must eventually be saved.
        if (!state.persistenceDirty)
          state.persistenceDueAt = static_cast<uint32_t>(clock_.milliseconds()) +
                                   state.persistenceIntervalMs;
        state.persistenceDirty = true;
      } else {
        state.persistenceDirty = true;
        savePersistence(state);
      }
    }
    if (bound && link_.connected()) {
      if (!link_.batchesState()) publish(state);
      else if (!batchWaiting_) {
        batchDueAt_ = clock_.milliseconds() + batchFlushMs_;
        batchWaiting_ = true;
      }
    }
  }

  struct Delivery {
    uint64_t messageId = 0;
    uint64_t sentAt = 0;
    uint32_t revision = 0;
    uint16_t slot = 0;
  };
  struct BatchContext { Device* device; uint16_t slots[4]; };
  static bool readBatchState(void* context, size_t index, DatastreamId& id,
                             uint32_t& revision, Value& value) {
    if (index >= 4) return false;
    BatchContext& batch = *static_cast<BatchContext*>(context);
    State& state = batch.device->states_[batch.slots[index]];
    id = state.id; revision = state.revision; value = readValue(state);
    return true;
  }

  void publishBatch() {
    const uint64_t now = clock_.milliseconds();
    if (batchWaiting_ && now < batchDueAt_) return;
    size_t free = 0;
    for (auto& delivery : deliveries_) {
      if (delivery.messageId && now - delivery.sentAt >= 5000) delivery = Delivery{};
      if (!delivery.messageId) ++free;
    }
    if (!free) return;
    BatchContext batch = {this, {}};
    size_t count = 0;
    for (size_t n = 0; n < count_ && count < free; ++n) {
      const size_t slot = (publishCursor_ + n) % count_;
      if (!states_[slot].dirty || !flovaValidDatastreamId(states_[slot].id)) continue;
      bool pending = false;
      for (const auto& delivery : deliveries_)
        if (delivery.messageId && delivery.slot == slot) pending = true;
      if (!pending) batch.slots[count++] = static_cast<uint16_t>(slot);
    }
    if (!count) { batchWaiting_ = false; return; }
    if (!batchWaiting_ && batchFlushMs_) {
      batchDueAt_ = now + batchFlushMs_;
      batchWaiting_ = true;
      return;
    }
    const uint64_t messageId = originateMessageId();
    const size_t accepted = link_.sendStateBatch(readBatchState, &batch, count, messageId);
    if (!accepted || accepted > count) return;
    size_t next = 0;
    for (auto& delivery : deliveries_) {
      if (delivery.messageId || next == accepted) continue;
      delivery.messageId = messageId;
      delivery.slot = batch.slots[next++];
      delivery.revision = states_[delivery.slot].revision;
      delivery.sentAt = now;
      publishCursor_ = (delivery.slot + 1) % count_;
    }
    batchWaiting_ = false;
  }
  // Coalesced state needs a bounded number of outstanding transmissions, not
  // one timer/message record per declared datastream. A retry of a changed
  // value receives a fresh ID; an ACK only retires the revision actually sent.
  void publish(State& state) {
    if (retryNotBefore_ && clock_.milliseconds() < retryNotBefore_) return;
    if (!flovaValidDatastreamId(state.id)) return;
    const size_t slot = static_cast<size_t>(&state - states_);
    Delivery* delivery = nullptr;
    for (auto& entry : deliveries_) {
      if (entry.messageId && entry.slot == slot) { delivery = &entry; break; }
    }
    const uint64_t now = clock_.milliseconds();
    if (delivery && delivery->sentAt && now - delivery->sentAt < 5000) return;
    if (!delivery) {
      for (auto& entry : deliveries_) if (!entry.messageId) { delivery = &entry; break; }
    }
    if (!delivery) return;
    if (!delivery->messageId || delivery->revision != state.revision) {
      delivery->messageId = originateMessageId();
      delivery->revision = state.revision;
      delivery->slot = static_cast<uint16_t>(slot);
    }
    Message message; message.kind = MessageKind::StateUpdate;
    message.messageId = delivery->messageId; message.datastreamId = state.id;
    message.value = readValue(state); message.revision = delivery->revision;
    message.origin = state.origin;
    message.timestamp = clock_.utcValid() ? clock_.utcMilliseconds() : 0;
    message.monotonic = now;
    if (link_.send(message)) {
      delivery->sentAt = now ? now : 1;
      publishCursor_ = (slot + 1) % count_;
    } else {
      *delivery = Delivery{};
    }
  }

  bool finishDelivery(uint64_t messageId, bool rejected) {
    bool found = false;
    for (auto& delivery : deliveries_) {
      if (!delivery.messageId || delivery.messageId != messageId) continue;
      State& state = states_[delivery.slot];
      if (state.revision == delivery.revision) state.dirty = false;
      delivery = Delivery{};
      if (rejected) ++diagnostics_.rejectedDeliveries;
      found = true;
    }
    return found;
  }

  void acknowledge(const Message& request, const WriteResult& result, const State* state) {
    Message reply;
    fillReply(reply, request, result, state);
    queueResult(reply);
  }

  void fillReply(Message& reply, const Message& request, const WriteResult& result, const State* state) {
    reply.kind = result.accepted() ? MessageKind::Acknowledgement : MessageKind::Error; reply.messageId = originateMessageId(); reply.datastreamId = request.datastreamId; Value::copy(reply.commandId, request.commandId); Value::copy(reply.correlationId, request.correlationId); Value::copy(reply.reason, result.reason); reply.revision = request.revision; reply.resultStatus = result.status == WriteStatus::NoChange ? 2 : result.accepted() ? 0 : 3; if (state && state->hasValue) reply.value = readValue(*state);
  }

  void queueResult(const Message& message) {
    const int slot = resultForCommand(message.commandId);
    if (slot >= 0 && storeResult(static_cast<size_t>(slot), message, pendingResults_[slot].sequence))
      sendResult(static_cast<size_t>(slot));
    // If completion cannot be committed, retain the intent and fail closed.
  }

  void sendResult(size_t slot) {
    ResultIndex& result = pendingResults_[slot];
    const uint64_t now = clock_.milliseconds();
    if (result.sentAt && now - result.sentAt < 5000) return;
    result.sentAt = now ? now : 1;
    char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(slot));
    if (!storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
        resultWorkspace_.magic != 0x43524554UL ||
        resultWorkspace_.message.messageId != result.messageId) {
      ++diagnostics_.storageFailures;
      return;
    }
    link_.send(resultWorkspace_.message);
  }

  void flushResults() {
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i)
      if (pendingResults_[i].active) sendResult(i);
  }

  bool persist(const State& state) {
    if (!flovaValidDatastreamId(state.id)) return false;
    Persisted record; record.magic = 0x464C4F57UL; record.datastreamId = state.id; record.value = readValue(state); record.revision = state.revision;
    char key[24]; snprintf(key, sizeof(key), "dsid:%u", static_cast<unsigned>(state.id)); return storage_.write(key, &record, sizeof(record));
  }

  bool savePersistence(State& state) {
    if (!state.persistenceDirty) return true;
    if (!persist(state)) {
      state.quality = Quality::HardwareError;
      ++diagnostics_.storageFailures;
      state.persistenceDueAt = static_cast<uint32_t>(clock_.milliseconds()) + 1000;
      return false;
    }
    state.persistenceDirty = false;
    state.persistenceStored = true;
    state.quality = Quality::Good;
    return true;
  }

  void flushDuePersistence() {
    const uint32_t now = static_cast<uint32_t>(clock_.milliseconds());
    // One due record per loop bounds maintenance work on both board families.
    for (size_t n = 0; n < count_; ++n) {
      const size_t slot = (persistenceCursor_ + n) % count_;
      State& state = states_[slot];
      if (state.persistenceDirty && flovaValidDatastreamId(state.id) &&
          static_cast<int32_t>(now - state.persistenceDueAt) >= 0) {
        savePersistence(state);
        persistenceCursor_ = (slot + 1) % count_;
        return;
      }
    }
  }

  void restore() {
    for (size_t i = 0; i < count_; ++i) if (states_[i].persistence != PersistencePolicy::None) {
      if (!flovaValidDatastreamId(states_[i].id)) continue;
      char key[24]; snprintf(key, sizeof(key), "dsid:%u", static_cast<unsigned>(states_[i].id)); Persisted restored;
      if (!states_[i].persistenceDirty && storage_.read(key, &restored, sizeof(restored)) && restored.magic == 0x464C4F57UL && restored.datastreamId == states_[i].id && restored.value.type == states_[i].valueType && validValue(restored.value)) { storeValue(states_[i], restored.value); states_[i].revision = restored.revision; states_[i].hasValue = true; states_[i].origin = Origin::DeviceRestore; states_[i].quality = Quality::Good; states_[i].persistenceStored = true; states_[i].restorePending = states_[i].writeKind != WriteHandlerKind::None; }
    }
  }

  void applyRestoredHardware() {
    if (clock_.milliseconds() < restoreRetryAt_) return;
    bool failed = false;
    for (size_t i = 0; i < count_; ++i) {
      State& state = states_[i];
      if (!state.restorePending) continue;
      const Value value = readValue(state);
      WriteResult result = validValue(value) && safe(state, value)
          ? invoke(state, value) : WriteResult::reject("unsafe_restore");
      if (result.accepted()) {
        state.restorePending = false;
        state.quality = Quality::Good;
        state.dirty = flovaValidDatastreamId(state.id) &&
                      (link_.connected() ||
                       state.offline == OfflinePolicy::KeepLatest);
      } else {
        failed = true;
        state.quality = Quality::HardwareError;
        state.dirty = false;
      }
    }
    restoreRetryAt_ = failed ? clock_.milliseconds() + 1000 : 0;
  }

  bool queueHistory(const State& state, const HistoryRetentionPolicy& history) {
    expireHistory();
    const uint32_t recordBytes = sizeof(HistoryRecord);
    const uint32_t recordLimit = history.maximumRecords ? history.maximumRecords : FLOVA_HISTORY_CAPACITY;
    const size_t byteLimit = history.maximumBytes ? history.maximumBytes : FLOVA_HISTORY_CAPACITY * sizeof(HistoryRecord);
    if (history.overflow == HistoryOverflow::DropNewest &&
        (historyCount_ >= recordLimit || (historyCount_ + 1) * recordBytes > byteLimit)) {
      diagnostics_.queueOverflow++; diagnostics_.droppedHistory++; return false;
    }
    while (historyCount_ && (historyCount_ >= recordLimit || (historyCount_ + 1) * recordBytes > byteLimit))
      if (!dropOldest(false, true)) return false;
    if (historyCount_ >= FLOVA_HISTORY_CAPACITY || recordBytes > byteLimit) {
      diagnostics_.queueOverflow++; diagnostics_.droppedHistory++; return false;
    }
    size_t slot = (historyHead_ + historyCount_) % FLOVA_HISTORY_CAPACITY; HistoryRecord& record = historyWorkspace_;
    record.datastreamId = state.id; record.value = readValue(state); record.messageId = originateMessageId(); record.timestamp = clock_.utcValid() ? clock_.utcMilliseconds() : 0; record.monotonic = clock_.milliseconds(); record.expiresAt = record.timestamp && history.maximumAgeSeconds ? record.timestamp + static_cast<uint64_t>(history.maximumAgeSeconds) * 1000 : 0; record.origin = state.origin; record.revision = state.revision; historyCount_++;
    char key[24]; snprintf(key, sizeof(key), "history:%u", (unsigned)slot);
    if (!storage_.write(key, &record, sizeof(record))) {
      historyCount_--; diagnostics_.storageFailures++; return false;
    }
    if (!persistHistoryMeta()) {
      historyCount_--;
      return false;
    }
    if (historyCount_ == 1) cacheHistoryHead(record);
    return true;
  }
  void flushHistory() {
    expireHistory();
    if (!historyCount_) return;
    if (!ensureHistoryHead() || (clock_.utcValid() && historyHeadExpiresAt_ &&
        historyHeadExpiresAt_ <= clock_.utcMilliseconds())) return;
    const uint64_t now = clock_.milliseconds();
    if (historyLastSentAt_ && now - historyLastSentAt_ < 5000) return;
    if (!readHistory(historyHead_)) return;
    HistoryRecord& record = historyWorkspace_; Message message; message.kind = MessageKind::StateUpdate; message.messageId = record.messageId; message.datastreamId = record.datastreamId; message.value = record.value; message.timestamp = record.timestamp; message.monotonic = record.monotonic; message.origin = record.origin; message.revision = record.revision; if (link_.send(message)) historyLastSentAt_ = now ? now : 1;
  }
  bool dropOldest(bool expired, bool dropped) {
    const size_t oldHead = historyHead_;
    historyHead_ = (historyHead_ + 1) % FLOVA_HISTORY_CAPACITY;
    --historyCount_;
    // Commit the live range before deleting its old head. A reset must never
    // leave durable metadata pointing at a record we have already removed.
    if (!persistHistoryMeta()) {
      historyHead_ = oldHead;
      ++historyCount_;
      return false;
    }
    historyHeadCached_ = false;
    historyReadRetryAt_ = 0;
    historyLastSentAt_ = 0;
    char key[24]; snprintf(key, sizeof(key), "history:%u", (unsigned)oldHead);
    if (!storage_.remove(key)) ++diagnostics_.storageFailures;
    if (expired) ++diagnostics_.expiredHistory;
    else if (dropped) ++diagnostics_.droppedHistory;
    return true;
  }
  void cacheHistoryHead(const HistoryRecord& record) {
    historyHeadMessageId_ = record.messageId;
    historyHeadExpiresAt_ = record.expiresAt;
    historyHeadCached_ = true;
  }
  bool ensureHistoryHead() {
    return historyHeadCached_ || readHistory(historyHead_);
  }
  bool readHistory(size_t slot) {
    const uint64_t now = clock_.milliseconds();
    if (historyReadRetryAt_ && now < historyReadRetryAt_) return false;
    char key[24]; snprintf(key, sizeof(key), "history:%u", static_cast<unsigned>(slot));
    if (storage_.read(key, &historyWorkspace_, sizeof(historyWorkspace_))) {
      historyReadRetryAt_ = 0;
      if (slot == historyHead_) cacheHistoryHead(historyWorkspace_);
      return true;
    }
    ++diagnostics_.storageFailures;
    historyReadRetryAt_ = now + 1000;
    return false;
  }
  void expireHistory() {
    if (!clock_.utcValid()) return;
    const uint64_t now = clock_.utcMilliseconds();
    // Reclaim at most one expired record per call; a long outage must not
    // turn the first connected loop into a queue-sized flash-write burst.
    if (historyCount_ && ensureHistoryHead() &&
           historyHeadExpiresAt_ && historyHeadExpiresAt_ <= now)
      dropOldest(true, false);
  }
  bool persistHistoryMeta() { HistoryMeta meta = {0x48495355UL, (uint16_t)historyHead_, (uint16_t)historyCount_}; if (storage_.write("history.meta", &meta, sizeof(meta))) return true; ++diagnostics_.storageFailures; return false; }
  void restoreHistory() {
    historyCount_ = 0;
    historyLastSentAt_ = 0;
    historyHeadCached_ = false;
    historyReadRetryAt_ = 0;
    HistoryMeta meta; if (!storage_.read("history.meta", &meta, sizeof(meta)) || meta.magic != 0x48495355UL || meta.head >= FLOVA_HISTORY_CAPACITY || meta.count > FLOVA_HISTORY_CAPACITY) return;
    historyHead_ = meta.head; historyCount_ = 0;
    for (size_t i = 0; i < meta.count; ++i) { size_t slot = (meta.head + i) % FLOVA_HISTORY_CAPACITY; char key[24]; snprintf(key, sizeof(key), "history:%u", (unsigned)slot); if (!storage_.read(key, &historyWorkspace_, sizeof(HistoryRecord))) { diagnostics_.storageFailures++; break; } historyCount_++; }
  }
  void restoreResults() {
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
      char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(i));
      PendingResult& restored = resultWorkspace_;
      if (storage_.read(key, &restored, sizeof(restored)) &&
          validResult(restored)) {
        pendingResults_[i].active = restored.active;
        pendingResults_[i].messageId = restored.message.messageId;
        pendingResults_[i].sentAt = 0;
        pendingResults_[i].fingerprint = commandFingerprint(restored.message.commandId);
        pendingResults_[i].sequence = restored.sentAt;
        if (restored.sentAt > resultSequence_) resultSequence_ = restored.sentAt;
        State* state = stateForId(restored.message.datastreamId);
        if (state && restored.message.kind == MessageKind::Acknowledgement &&
            restored.message.revision > state->lastCloudRevision)
          state->lastCloudRevision = restored.message.revision;
      }
    }
  }
  void syncTime() {
    uint64_t now = clock_.milliseconds();
    if (timeRequestStarted_ && now - timeRequestStarted_ > 30000) { timeRequestStarted_ = 0; diagnostics_.clockSyncFailures++; }
    if (timeRequestStarted_ || (clock_.utcValid() && now - lastTimeRequest_ < 21600000ULL)) return;
    Message request; request.kind = MessageKind::TimeRequest; request.messageId = originateMessageId(); request.monotonic = now; snprintf(request.commandId, sizeof(request.commandId), "time-%lu", (unsigned long)++timeSequence_); if (link_.send(request)) { timeRequestStarted_ = now ? now : 1; lastTimeRequest_ = now; Value::copy(pendingTimeId_, request.commandId); }
  }
  void acceptTime(const Message& response) {
    if (!timeRequestStarted_ || !response.timestamp || strcmp(response.commandId, pendingTimeId_) != 0) return;
    uint64_t received = clock_.milliseconds(); uint64_t uncertainty = (received - timeRequestStarted_) / 2; clock_.setUtc(response.timestamp + uncertainty, uncertainty); timeRequestStarted_ = 0;
  }

  void acknowledgeDelivery(uint64_t messageId) {
    if (!messageId) return;
    for (size_t i = 0; i < FLOVA_COMMAND_DEDUP_CAPACITY; ++i) {
      if (!pendingResults_[i].active || pendingResults_[i].messageId != messageId)
        continue;
      char key[16]; snprintf(key, sizeof(key), "cmd:%u", static_cast<unsigned>(i));
      if (!storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
          !validResult(resultWorkspace_)) { ++diagnostics_.storageFailures; return; }
      resultWorkspace_.active = false;
      if (!storage_.write(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
          !storage_.read(key, &resultWorkspace_, sizeof(resultWorkspace_)) ||
          !validResult(resultWorkspace_) || resultWorkspace_.active) {
        ++diagnostics_.storageFailures;
        return;
      }
      pendingResults_[i].active = false;
      return;
    }
    if (finishDelivery(messageId, false)) return;
    if (historyCount_ && ensureHistoryHead() && historyHeadMessageId_ == messageId) {
      dropOldest(false, false);
      historyLastSentAt_ = 0;
    }
  }

  void rejectDelivery(uint64_t messageId) {
    if (!messageId) return;
    if (finishDelivery(messageId, true)) return;
    if (historyCount_ && ensureHistoryHead() && historyHeadMessageId_ == messageId) {
      dropOldest(false, true);
      diagnostics_.rejectedDeliveries++;
      historyLastSentAt_ = 0;
    }
  }

  Link& link_; Storage& storage_; Clock& clock_; Logger& logger_; State states_[kMaxDatastreams]; size_t count_;
  size_t persistenceCursor_ = 0;
  uint64_t restoreRetryAt_ = 0;
  uint64_t historyHeadMessageId_ = 0;
  uint64_t historyHeadExpiresAt_ = 0;
  uint64_t historyReadRetryAt_ = 0;
  bool historyHeadCached_ = false;
  ParameterState parameters_[FLOVA_PARAMETER_CAPACITY]; size_t parameterCount_ = 0;
#if FLOVA_STREAM_DESCRIPTORS_IN_STORAGE
  Descriptor descriptorCache_{};
  size_t descriptorCacheSlot_ = kMaxDatastreams;
#else
  Descriptor descriptors_[kMaxDatastreams]{};
#endif
  Delivery deliveries_[4];
  size_t publishCursor_ = 0;
  uint64_t batchDueAt_ = 0;
  uint32_t batchFlushMs_ = 50;
  bool batchWaiting_ = false;
  uint64_t resultSequence_ = 0;
  ResultIndex pendingResults_[FLOVA_COMMAND_DEDUP_CAPACITY];
  mutable PendingResult resultWorkspace_;
  HistoryRecord historyWorkspace_; size_t historyHead_, historyCount_;
  uint64_t lastTimeRequest_, timeRequestStarted_; uint32_t timeSequence_; uint64_t nextMessageId_; uint64_t retryNotBefore_; uint64_t historyLastSentAt_; char pendingTimeId_[kMaxText]; Diagnostics diagnostics_;
  FactoryResetHandler factoryResetHandler_; void* factoryResetContext_;
  bool started_, bindingPending_, bindingFailed_;
  bool cloudSchedulingForOutputs_ = false;
};

template <> struct Codec<bool> { static bool valid(bool) { return true; } static Value encode(bool v) { return Value::from(v); } static bool decode(const Value& v) { return v.scalar.boolean; } };
template <> struct Codec<int64_t> { static bool valid(int64_t) { return true; } static Value encode(int64_t v) { return Value::from(v); } static int64_t decode(const Value& v) { return v.scalar.integer; } };
template <> struct Codec<float> { static bool valid(float) { return true; } static Value encode(float v) { return Value::from(v); } static float decode(const Value& v) { return v.scalar.floating; } };
template <> struct Codec<double> { static bool valid(double) { return true; } static Value encode(double v) { return Value::from(v); } static double decode(const Value& v) { return v.scalar.number; } };
template <> struct Codec<Text> { static bool valid(const Text& v) { return v.valid(); } static Value encode(const Text& v) { return Value::from(v); } static Text decode(const Value& v) { return Text(v.text); } };

template <typename T> class Datastream {
 public:
  Datastream() : device_(nullptr), slot_(kMaxDatastreams) {}
  Datastream(Device& device, Device::State* state) : device_(&device), slot_(state ? static_cast<uint16_t>(state - device.states_) : kMaxDatastreams) {}
  bool valid() const { return state() != nullptr; }
  bool bound() const { return state() && flovaValidDatastreamId(state()->id); }
  bool hasValue() const { return state() && state()->hasValue; }
  T value() const { return hasValue() ? Codec<T>::decode(device_->readValue(*state())) : T(); }
  Snapshot<T> snapshot() const { Snapshot<T> out = {value(), hasValue(), 0, Origin::Unknown, Quality::Stale, false, 0}; if (state()) { out.updatedAt = state()->updatedAt; out.origin = state()->origin; out.quality = state()->quality; out.dirty = state()->dirty; out.revision = state()->revision; } return out; }
  WriteResult write(const T& value) { return !Codec<T>::valid(value) ? WriteResult::reject("text_too_long") : state() ? device_->apply(*state(), Codec<T>::encode(value), Origin::LocalLogic) : WriteResult::failure("registration_full"); }
  WriteResult report(const T& value, Origin origin = Origin::SensorRead) { return !Codec<T>::valid(value) ? WriteResult::reject("text_too_long") : state() ? device_->report(*state(), Codec<T>::encode(value), origin) : WriteResult::failure("registration_full"); }
  Datastream& mode(Mode value) { if (state()) state()->mode = value; return *this; }
  Datastream& scheduling(Scheduling value) {
    if (!state() || device_->started_ || static_cast<uint8_t>(value) > 1)
      slot_ = kMaxDatastreams;
    else state()->scheduling = value;
    return *this;
  }
  Datastream& offline(OfflinePolicy value) { if (state()) state()->offline = value; return *this; }
  Datastream& retention(const HistoryRetentionPolicy& value) { if (state() && !device_->retention(*state(), value)) slot_ = kMaxDatastreams; return *this; }
  Datastream& persist(PersistencePolicy value, uint32_t intervalMs = 1000) {
    if (state() && static_cast<uint8_t>(value) <= static_cast<uint8_t>(PersistencePolicy::Deferred) &&
        (value != PersistencePolicy::Deferred || (intervalMs && intervalMs <= 0x7fffffffUL))) {
      if (state()->persistence != value || state()->persistenceIntervalMs != intervalMs) {
        state()->persistence = value;
        state()->persistenceIntervalMs = intervalMs;
        state()->persistenceStored = false;
        state()->persistenceDirty = value != PersistencePolicy::None && state()->hasValue;
        state()->persistenceDueAt = static_cast<uint32_t>(device_->clock_.milliseconds()) +
            (value == PersistencePolicy::Deferred ? intervalMs : 0);
      }
    } else slot_ = kMaxDatastreams;
    return *this;
  }
  bool flush() { return state() && device_->savePersistence(*state()); }
  Datastream& onWrite(WriteResult (*handler)(T)) { if (device_) device_->setWrite(state(), handler); return *this; }
  Datastream& onWrite(WriteResult (*handler)(void*, T), void* context) { if (device_) device_->setWrite(state(), handler, context); return *this; }
  Datastream& onWrite(void (*handler)(T)) { if (device_) device_->setWrite(state(), handler); return *this; }
  Datastream& onWrite(void (*handler)(void*, T), void* context) { if (device_) device_->setWrite(state(), handler, context); return *this; }
 private:
  Device::State* state() const {
    return device_ && slot_ < device_->count_ ? &device_->states_[slot_] : nullptr;
  }
  Device* device_;
  uint16_t slot_;

};

template <typename T> class Setting {
 public:
  Setting(Device& device, Device::ParameterState* parameter) : device_(device), parameter_(parameter) {}
  bool valid() const { return parameter_ != nullptr; }
  bool hasValue() const { return parameter_ && parameter_->hasValue; }
  T value() const { return hasValue() ? Codec<T>::decode(parameter_->value) : T(); }
 private:
  Device& device_;
  Device::ParameterState* parameter_;
};

template <> inline Datastream<bool> Device::datastream<bool>(const char* key) { return Datastream<bool>(*this, state(key, ValueType::Boolean)); }
template <> inline Datastream<int64_t> Device::datastream<int64_t>(const char* key) { return Datastream<int64_t>(*this, state(key, ValueType::Int64)); }
template <> inline Datastream<float> Device::datastream<float>(const char* key) { return Datastream<float>(*this, state(key, ValueType::Float)); }
template <> inline Datastream<double> Device::datastream<double>(const char* key) { return Datastream<double>(*this, state(key, ValueType::Double)); }
template <> inline Datastream<Text> Device::datastream<Text>(const char* key) { return Datastream<Text>(*this, state(key, ValueType::Text)); }

template <typename T> inline Setting<T> Device::setting(const char* key, const T& defaultValue) {
  return Setting<T>(*this, registerParameter(key, defaultValue));
}

}  // namespace flova
