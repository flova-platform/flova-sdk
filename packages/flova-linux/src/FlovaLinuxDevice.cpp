#include <FlovaLinux.h>

#include <FlovaLinuxArduinoCompat.h>

#include <FlovaArduinoPlatform.h>
#include <FlovaEntropy.h>
#include <adapters/ArduinoFlovaLink.h>

#include "FlovaLinuxPlatform.h"

#include <openssl/rand.h>
#include <cstdlib>

namespace {

class Entropy final : public FlovaEntropySource {
 public:
  uint8_t byte() override {
    uint8_t value = 0;
    if (RAND_bytes(&value, sizeof(value)) == 1) return value;
    return static_cast<uint8_t>(millis() ^ reinterpret_cast<uintptr_t>(this));
  }
};
}  // namespace

struct FlovaLinux::Impl::State {
  explicit State(const Config& input)
      : config(input),
        storage(input.stateDirectory),
        device(link, storage, clock, logger) {}

  Config config;
  Entropy entropy;
  LinuxPlatform platform;
  ArduinoFlovaLink link{platform, entropy};
  flova_linux::FileStorage storage;
  flova_linux::Clock clock;
  flova_linux::Logger logger;
  flova::Device device;
  bool started = false;
  const char* error = "";
};

FlovaLinux::Impl::Impl(const Config& config) : state_(new State(config)) {}
FlovaLinux::Impl::~Impl() { delete state_; }

bool FlovaLinux::Impl::begin() {
  State& state = *state_;
  if (!state.config.url || !state.config.deviceId || !state.config.secret ||
      !state.link.configure(state.config.url, state.config.deviceId,
                            state.config.secret)) {
    state.error = "invalid_linux_device_config";
    return false;
  }
  state.link.setConnectionAllowed(true);
  if (!state.device.begin()) {
    state.error = "device_begin_failed";
    return false;
  }
  state.started = true;
  return true;
}

void FlovaLinux::Impl::run() {
  if (state_->started) state_->device.run();
}

bool FlovaLinux::Impl::connected() const { return state_->link.connected(); }
bool FlovaLinux::Impl::ready() const { return state_->device.ready(); }
bool FlovaLinux::Impl::running() const { return state_->started; }
const char* FlovaLinux::Impl::lastError() const { return state_->error; }
flova::Device& FlovaLinux::Impl::device() { return state_->device; }

template <typename T>
flova::WriteResult FlovaLinux::Impl::write(const char* key, const T& value) {
  return state_->device.datastream<T>(key).write(value);
}

template <typename T>
bool FlovaLinux::Impl::hasValue(const char* key) const {
  return const_cast<flova::Device&>(state_->device).datastream<T>(key).hasValue();
}

template <typename T>
T FlovaLinux::Impl::value(const char* key) const {
  return const_cast<flova::Device&>(state_->device).datastream<T>(key).value();
}

template <typename T>
flova::WriteResult FlovaLinux::Impl::report(const char* key, const T& value,
                                            flova::Origin origin) {
  return state_->device.datastream<T>(key).report(value, origin);
}

template <typename T>
bool FlovaLinux::Impl::onWrite(const char* key, flova::ValueType,
                               FlovaDatastreamBinding<T>& binding) {
  auto stream = state_->device.datastream<T>(key);
  switch (binding.kind) {
    case FlovaDatastreamHandlerKind::Result:
      stream.onWrite(binding.handler.result);
      return true;
    case FlovaDatastreamHandlerKind::ResultWithContext:
      stream.onWrite(binding.handler.resultContext, binding.context);
      return true;
    case FlovaDatastreamHandlerKind::Void:
      stream.onWrite(binding.handler.voidHandler);
      return true;
    case FlovaDatastreamHandlerKind::VoidWithContext:
      stream.onWrite(binding.handler.voidContext, binding.context);
      return true;
    default:
      return false;
  }
}

template flova::WriteResult FlovaLinux::Impl::write<bool>(const char*, const bool&);
template flova::WriteResult FlovaLinux::Impl::write<int64_t>(const char*, const int64_t&);
template flova::WriteResult FlovaLinux::Impl::write<float>(const char*, const float&);
template flova::WriteResult FlovaLinux::Impl::write<double>(const char*, const double&);
template flova::WriteResult FlovaLinux::Impl::write<flova::Text>(const char*, const flova::Text&);
template bool FlovaLinux::Impl::hasValue<bool>(const char*) const;
template bool FlovaLinux::Impl::hasValue<int64_t>(const char*) const;
template bool FlovaLinux::Impl::hasValue<float>(const char*) const;
template bool FlovaLinux::Impl::hasValue<double>(const char*) const;
template bool FlovaLinux::Impl::hasValue<flova::Text>(const char*) const;
template bool FlovaLinux::Impl::value<bool>(const char*) const;
template int64_t FlovaLinux::Impl::value<int64_t>(const char*) const;
template float FlovaLinux::Impl::value<float>(const char*) const;
template double FlovaLinux::Impl::value<double>(const char*) const;
template flova::Text FlovaLinux::Impl::value<flova::Text>(const char*) const;
template flova::WriteResult FlovaLinux::Impl::report<bool>(const char*, const bool&, flova::Origin);
template flova::WriteResult FlovaLinux::Impl::report<int64_t>(const char*, const int64_t&, flova::Origin);
template flova::WriteResult FlovaLinux::Impl::report<float>(const char*, const float&, flova::Origin);
template flova::WriteResult FlovaLinux::Impl::report<double>(const char*, const double&, flova::Origin);
template flova::WriteResult FlovaLinux::Impl::report<flova::Text>(const char*, const flova::Text&, flova::Origin);
template bool FlovaLinux::Impl::onWrite<bool>(const char*, flova::ValueType, FlovaDatastreamBinding<bool>&);
template bool FlovaLinux::Impl::onWrite<int64_t>(const char*, flova::ValueType, FlovaDatastreamBinding<int64_t>&);
template bool FlovaLinux::Impl::onWrite<float>(const char*, flova::ValueType, FlovaDatastreamBinding<float>&);
template bool FlovaLinux::Impl::onWrite<double>(const char*, flova::ValueType, FlovaDatastreamBinding<double>&);
template bool FlovaLinux::Impl::onWrite<flova::Text>(const char*, flova::ValueType, FlovaDatastreamBinding<flova::Text>&);

FlovaLinux::FlovaLinux()
    : impl_(new Impl({getenv("FLOVA_LINK_URL"), getenv("FLOVA_DEVICE_ID"),
                      getenv("FLOVA_DEVICE_SECRET"),
                      getenv("FLOVA_STATE_DIRECTORY") ?: "/var/lib/flova/device"})) {}
FlovaLinux::FlovaLinux(const Config& config) : impl_(new Impl(config)) {}
FlovaLinux::~FlovaLinux() { delete impl_; }
bool FlovaLinux::begin() { return impl_->begin(); }
void FlovaLinux::run() { impl_->run(); }
bool FlovaLinux::connected() const { return impl_->connected(); }
bool FlovaLinux::ready() const { return impl_->ready(); }
bool FlovaLinux::running() const { return impl_->running(); }
const char* FlovaLinux::lastError() const { return impl_->lastError(); }
