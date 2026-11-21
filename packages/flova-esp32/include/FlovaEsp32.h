#pragma once

#include <esp_system.h>

#include <FlovaEsp32BuildProfile.h>
#include <FlovaArduino.h>
#include <FlovaCustomCode.h>
#include <FlovaTlsRoots.h>
#include <FlovaEsp32Platform.h>
#include <FlovaEsp32Services.h>
#include <FlovaWifiProvisioning.h>
#include <FlovaEsp32Provisioning.h>
#include <adapters/ArduinoFlovaLink.h>
#include <adapters/ArduinoFlovaManualHardware.h>

class FlovaEsp32Entropy : public FlovaEntropySource {
 public:
  uint8_t byte() override { return static_cast<uint8_t>(esp_random()); }
};

class FlovaEsp32;
class FlovaEsp32WithLink;
template <>
struct flova::detail::UseResolvedDatastream<FlovaEsp32> : std::true_type {};
template <>
struct flova::detail::UseResolvedDatastream<FlovaEsp32WithLink> : std::true_type {};

namespace flova {
namespace detail {

class FlovaEsp32OwnedLink {
 public:
  explicit FlovaEsp32OwnedLink(FlovaEsp32Entropy& entropy) : link_(platform_, entropy) {}
  FlovaClientLink& link() { return link_; }

 private:
  FlovaEsp32Platform platform_;
  ArduinoFlovaLink link_;
};

template <typename Facade, typename Transport>
class ManagedEsp32 {
 protected:
  template <typename... Args>
  explicit ManagedEsp32(const char* setupPassword, Args&... args)
      : transport_(entropy_, args...), provisioning_(storage_, setupPassword),
        network_(storage_), identity_("custom_arduino_esp32"),
        client_(transport_.link(), provisioning_, network_, tlsClock_, identity_,
                storage_, clock_, logger_, entropy_, hardware_), recovery_(storage_, network_) {
    client_.setRestartHandler(scheduleManagedRestart, this);
  }

 public:
  bool begin() { return client_.begin(true); }
  bool logEvent(const char* code, const char* message = "") {
    return client_.logEvent(code, message);
  }
  void run() {
    client_.run();
    recovery_.restartIfDue(millis());
    recovery_.serviceSetup(client_, provisioning_, clock_);
  }
  bool startWifiSetup() { return client_.beginNetworkSetup(); }
  bool cancelWifiSetup() {
    if (!client_.networkSetupActive()) return false;
    if (recovery_.state() == FlovaWifiRecovery::State::Idle) {
      client_.endNetworkSetup(); return true;
    }
    return recovery_.cancel();
  }
  bool wifiSetupActive() const { return client_.networkSetupActive(); }
  const char* wifiSetupError() const {
    return recovery_.error()[0] ? recovery_.error() : client_.lastError();
  }
  bool connected() const { return client_.connected(); }
  template <typename T>
  bool report(const char* key, const T& value) { return client_.datastream<T>(key).report(value).accepted(); }
  template <typename T>
  flova::Datastream<T> flovaDatastream(const char* key) {
    return client_.datastream<T>(key);
  }
  template <typename T>
  flova::WriteResult flovaWrite(const char* key, const T& value) {
    return client_.datastream<T>(key).write(value);
  }
  template <typename T>
  bool flovaHasValue(const char* key) const {
    return const_cast<ManagedEsp32*>(this)->client_.datastream<T>(key).hasValue();
  }
  template <typename T>
  T flovaValue(const char* key) const {
    return const_cast<ManagedEsp32*>(this)->client_.datastream<T>(key).value();
  }
  template <typename T>
  flova::WriteResult flovaReport(const char* key, const T& value,
                                 flova::Origin origin = flova::Origin::SensorRead) {
    return client_.datastream<T>(key).report(value, origin);
  }
  template <typename T>
  FlovaDatastream<T, Facade> stream(const char* key) {
    return FlovaDatastream<T, Facade>(static_cast<Facade&>(*this), key);
  }
  template <typename T>
  bool flovaOnWrite(const char* key, flova::ValueType valueType,
                    FlovaDatastreamBinding<T>& binding) {
    (void)valueType;
    return binding.attach(client_.datastream<T>(key));
  }
  template <typename T>
  bool flovaMode(const char* key, flova::Mode value) {
    return client_.datastream<T>(key).tryMode(value);
  }
  template <typename T>
  bool flovaOffline(const char* key, flova::OfflinePolicy value) {
    return client_.datastream<T>(key).tryOffline(value);
  }
  template <typename T>
  bool flovaRetention(const char* key, const flova::HistoryRetentionPolicy& value) {
    return client_.datastream<T>(key).tryRetention(value);
  }
  template <typename T>
  bool flovaPersist(const char* key, flova::PersistencePolicy value, uint32_t intervalMs = 1000) {
    return client_.datastream<T>(key).tryPersist(value, intervalMs);
  }
  FlovaProvisioningResponse provision(const flova::ProvisioningHandoff& input) {
    return client_.provision(input);
  }

  bool startProvisioning() { return client_.startProvisioning(); }
  bool provisioning() const { return client_.provisioning(); }
  FlovaLifecycle lifecycle() const { return client_.lifecycle(); }
  bool networkConnected() const { return client_.networkConnected(); }
  bool tlsReady() const { return client_.tlsReady(); }
  bool runtimeReady() const { return client_.runtimeReady(); }
  bool ready() const { return client_.ready(); }
  FlovaPowerDownStatus prepareForPowerDown() {
    return client_.prepareForPowerDown();
  }
  const char* lastError() const { return client_.lastError(); }
  const flova::Diagnostics& diagnostics() const { return client_.diagnostics(); }
  flova::Device& device() { return client_.device(); }
  void status(FlovaStatusSnapshot& output) const { client_.status(output); }
  void setStatusListener(FlovaStatusListener listener,
                         void* context = nullptr) {
    client_.setStatusListener(listener, context);
  }
  bool setFirmwareTarget(const char* target) { return client_.setFirmwareTarget(target); }
  void enableOta(bool enabled = true) { client_.setOtaEnabled(enabled); }
  void setOtaProfile(FlovaOtaStrategy strategy, const char* bootLayoutVersion,
                     bool rollbackCapable = false) {
    client_.setOtaProfile(strategy, bootLayoutVersion, rollbackCapable);
  }
  void setRestartHandler(FlovaRestartHandler handler, void* context = nullptr) {
    client_.setRestartHandler(handler, context);
  }
  bool restartRequired() const { return client_.restartRequired(); }
  FlovaRestartReason restartReason() const { return client_.restartReason(); }
  bool factoryReset() { return client_.factoryReset(); }

  template <typename T>
  flova::Datastream<T> datastream(const char* key) {
    return client_.datastream<T>(key);
  }
  template <typename T>
  flova::Setting<T> setting(const char* key, const T& defaultValue) {
    return client_.setting<T>(key, defaultValue);
  }

 private:
  static void scheduleManagedRestart(void* context, FlovaRestartReason) {
    static_cast<ManagedEsp32*>(context)->recovery_.requestRestart(millis());
  }
  FlovaEsp32Entropy entropy_;
  Transport transport_;
  FlovaEsp32Storage storage_;
  ArduinoFlovaClock clock_;
  ArduinoFlovaLogger logger_;
  ArduinoFlovaManualHardware hardware_;
  FlovaEsp32ProvisioningWithWifiRecovery provisioning_;
  FlovaEsp32StoredNetwork network_;
  ArduinoFlovaUtcBootstrap<WiFiUDP> tlsClock_;
  FlovaEsp32Identity identity_;
  FlovaClient client_;
  FlovaEsp32WifiRecovery recovery_;
};

}  // namespace detail
}  // namespace flova

class FlovaEsp32 final
    : public flova::detail::ManagedEsp32<FlovaEsp32, flova::detail::FlovaEsp32OwnedLink> {
 public:
  explicit FlovaEsp32(const char* setupPassword = FLOVA_SETUP_PASSWORD)
      : ManagedEsp32(setupPassword) {}
};

class FlovaEsp32WithLink final
    : public flova::detail::ManagedEsp32<FlovaEsp32WithLink, flova::detail::BorrowedManagedLink> {
 public:
  explicit FlovaEsp32WithLink(FlovaClientLink& link, const char* setupPassword = FLOVA_SETUP_PASSWORD)
      : ManagedEsp32(setupPassword, link) {}
};
