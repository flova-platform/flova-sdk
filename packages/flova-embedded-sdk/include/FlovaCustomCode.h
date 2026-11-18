#pragma once

// Small opt-in helpers for custom application code. They keep the typed
// Datastream API and its text-based Console keys visible while reducing the
// amount of C++ syntax beginners need to learn.
#include "FlovaDevice.h"

#include <stdint.h>
#include <type_traits>

namespace flova {
namespace detail {
// Built-in facades use the same runtime for handle resolution and key methods.
// Downstream clients using the helper's key-based backend retain that path.
template <typename Client>
struct UseResolvedDatastream : std::false_type {};
}  // namespace detail
}  // namespace flova

struct FlovaDatastreamCommand {
  const char* key;
  const char* commandId;
  uint32_t desiredVersion;
  flova::Value value;
};

typedef flova::WriteResult (*FlovaDatastreamCommandHandler)(
    void*, const FlovaDatastreamCommand&);

enum class FlovaDatastreamHandlerKind : uint8_t {
  None,
  Result,
  ResultWithContext,
  Void,
  VoidWithContext,
};

template <typename T>
struct FlovaDatastreamType;

template <> struct FlovaDatastreamType<bool> {
  static flova::ValueType value() { return flova::ValueType::Boolean; }
};
template <> struct FlovaDatastreamType<int64_t> {
  static flova::ValueType value() { return flova::ValueType::Int64; }
};
template <> struct FlovaDatastreamType<float> {
  static flova::ValueType value() { return flova::ValueType::Float; }
};
template <> struct FlovaDatastreamType<double> {
  static flova::ValueType value() { return flova::ValueType::Double; }
};
template <> struct FlovaDatastreamType<flova::Text> {
  static flova::ValueType value() { return flova::ValueType::Text; }
};

template <typename T>
struct FlovaDatastreamBinding {
  typedef flova::WriteResult (*ResultHandler)(T);
  typedef flova::WriteResult (*ResultContextHandler)(void*, T);
  typedef void (*VoidHandler)(T);
  typedef void (*VoidContextHandler)(void*, T);

  FlovaDatastreamHandlerKind kind = FlovaDatastreamHandlerKind::None;
  union {
    ResultHandler result;
    ResultContextHandler resultContext;
    VoidHandler voidHandler;
    VoidContextHandler voidContext;
  } handler = {};
  void* context = nullptr;

  // Copy the callback and its application context into the runtime. The
  // runtime never borrows this binding or a temporary stream helper.
  bool attach(flova::Datastream<T> stream) const {
    switch (kind) {
      case FlovaDatastreamHandlerKind::Result:
        return stream.tryOnWrite(handler.result);
      case FlovaDatastreamHandlerKind::ResultWithContext:
        return stream.tryOnWrite(handler.resultContext, context);
      case FlovaDatastreamHandlerKind::Void:
        return stream.tryOnWrite(handler.voidHandler);
      case FlovaDatastreamHandlerKind::VoidWithContext:
        return stream.tryOnWrite(handler.voidContext, context);
      default: return false;
    }
  }

  static flova::WriteResult dispatch(void* context,
                                     const FlovaDatastreamCommand& command) {
    if (!context) return flova::WriteResult::failure("write_handler_missing");
    FlovaDatastreamBinding& binding = *static_cast<FlovaDatastreamBinding*>(context);
    if (command.value.type != FlovaDatastreamType<T>::value())
      return flova::WriteResult::reject("type_mismatch");

    const T value = flova::Codec<T>::decode(command.value);
    switch (binding.kind) {
      case FlovaDatastreamHandlerKind::Result:
        return binding.handler.result ? binding.handler.result(value)
                                      : flova::WriteResult::failure("write_handler_missing");
      case FlovaDatastreamHandlerKind::ResultWithContext:
        return binding.handler.resultContext
                   ? binding.handler.resultContext(binding.context, value)
                   : flova::WriteResult::failure("write_handler_missing");
      case FlovaDatastreamHandlerKind::Void:
        if (binding.handler.voidHandler) binding.handler.voidHandler(value);
        return binding.handler.voidHandler ? flova::WriteResult::accept()
                                           : flova::WriteResult::failure("write_handler_missing");
      case FlovaDatastreamHandlerKind::VoidWithContext:
        if (binding.handler.voidContext) binding.handler.voidContext(binding.context, value);
        return binding.handler.voidContext ? flova::WriteResult::accept()
                                           : flova::WriteResult::failure("write_handler_missing");
      default:
        return flova::WriteResult::failure("write_handler_missing");
    }
  }
};

template <typename T, typename Client>
class FlovaDatastream {
 public:
  FlovaDatastream(Client& client, const char* key) : client_(client), key_(key) {}

  const char* key() const { return key_; }
  flova::WriteResult write(const T& value) {
    return useResolved() ? cached_.write(value) : client_.flovaWrite(key_, value);
  }
  bool hasValue() const {
    return useResolved() ? cached_.hasValue() : client_.template flovaHasValue<T>(key_);
  }
  T value() const {
    return useResolved() ? cached_.value() : client_.template flovaValue<T>(key_);
  }
  flova::WriteResult report(const T& value,
                            flova::Origin origin = flova::Origin::SensorRead) {
    return useResolved() ? cached_.report(value, origin)
                         : client_.flovaReport(key_, value, origin);
  }
  FlovaDatastream& mode(flova::Mode value) {
    resolve().mode(value);
    return *this;
  }
  FlovaDatastream& offline(flova::OfflinePolicy value) {
    resolve().offline(value);
    return *this;
  }
  FlovaDatastream& scheduling(flova::Scheduling value) {
    resolve().scheduling(value);
    return *this;
  }
  FlovaDatastream& retention(const flova::HistoryRetentionPolicy& value) {
    resolve().retention(value);
    return *this;
  }
  FlovaDatastream& persist(flova::PersistencePolicy value, uint32_t intervalMs = 1000) {
    resolve().persist(value, intervalMs);
    return *this;
  }
  bool flush() { return resolve().flush(); }

  bool tryOnWrite(typename FlovaDatastreamBinding<T>::ResultHandler handler) {
    if (!handler) return false;
    if (flova::detail::UseResolvedDatastream<Client>::value)
      return resolve().tryOnWrite(handler);
    FlovaDatastreamBinding<T> binding;
    binding.kind = FlovaDatastreamHandlerKind::Result;
    binding.handler.result = handler;
    return registerLegacyWrite(binding);
  }
  bool tryOnWrite(typename FlovaDatastreamBinding<T>::ResultContextHandler handler,
                  void* context) {
    if (!handler) return false;
    if (flova::detail::UseResolvedDatastream<Client>::value)
      return resolve().tryOnWrite(handler, context);
    FlovaDatastreamBinding<T> binding;
    binding.kind = FlovaDatastreamHandlerKind::ResultWithContext;
    binding.handler.resultContext = handler;
    binding.context = context;
    return registerLegacyWrite(binding);
  }
  bool tryOnWrite(typename FlovaDatastreamBinding<T>::VoidHandler handler) {
    if (!handler) return false;
    if (flova::detail::UseResolvedDatastream<Client>::value)
      return resolve().tryOnWrite(handler);
    FlovaDatastreamBinding<T> binding;
    binding.kind = FlovaDatastreamHandlerKind::Void;
    binding.handler.voidHandler = handler;
    return registerLegacyWrite(binding);
  }
  bool tryOnWrite(typename FlovaDatastreamBinding<T>::VoidContextHandler handler,
                  void* context) {
    if (!handler) return false;
    if (flova::detail::UseResolvedDatastream<Client>::value)
      return resolve().tryOnWrite(handler, context);
    FlovaDatastreamBinding<T> binding;
    binding.kind = FlovaDatastreamHandlerKind::VoidWithContext;
    binding.handler.voidContext = handler;
    binding.context = context;
    return registerLegacyWrite(binding);
  }

  FlovaDatastream& onWrite(typename FlovaDatastreamBinding<T>::ResultHandler handler) {
    tryOnWrite(handler);
    return *this;
  }
  FlovaDatastream& onWrite(typename FlovaDatastreamBinding<T>::ResultContextHandler handler,
                          void* context) {
    tryOnWrite(handler, context);
    return *this;
  }
  FlovaDatastream& onWrite(typename FlovaDatastreamBinding<T>::VoidHandler handler) {
    tryOnWrite(handler);
    return *this;
  }
  FlovaDatastream& onWrite(typename FlovaDatastreamBinding<T>::VoidContextHandler handler,
                          void* context) {
    tryOnWrite(handler, context);
    return *this;
  }

 private:
  bool registerLegacyWrite(const FlovaDatastreamBinding<T>& binding) {
    // A downstream callback backend can borrow the member's stable address.
    const FlovaDatastreamBinding<T> previous = binding_;
    binding_ = binding;
    if (client_.flovaOnWrite(key_, FlovaDatastreamType<T>::value(), binding_)) return true;
    binding_ = previous;
    return false;
  }

  bool useResolved() const {
    return resolve().valid() || flova::detail::UseResolvedDatastream<Client>::value;
  }

  flova::Datastream<T>& resolve() const {
    // Slots remain stable for the runtime's lifetime. Resolve lazily: global
    // helper construction must never mount storage or register descriptors.
    if (!cached_.valid()) cached_ = client_.template flovaDatastream<T>(key_);
    return cached_;
  }
  Client& client_;
  const char* key_;
  mutable flova::Datastream<T> cached_;
  FlovaDatastreamBinding<T> binding_;
};

#define FLOVA_DATASTREAM(client, type, key) ((client).stream<type>(key))
#define FLOVA_WRITE(stream, value) ((stream).write(value))
#define FLOVA_READ(stream) ((stream).value())
#define FLOVA_REPORT(stream, ...) ((stream).report(__VA_ARGS__))
#define FLOVA_ON_WRITE(stream, ...) ((stream).onWrite(__VA_ARGS__))
#define FLOVA_HAS_VALUE(stream) ((stream).hasValue())
#define FLOVA_SETTING(client, type, key, default_value) ((client).setting<type>(key, default_value))
