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

// Managed custom firmware: application-owned pins, SDK-owned connectivity.
class FlovaEsp32 final {
 public:
  explicit FlovaEsp32(const char* setupPassword = FLOVA_SETUP_PASSWORD)
      : linkPlatform_(), defaultLink_(linkPlatform_, entropy_), link_(defaultLink_),
        provisioning_(storage_, setupPassword), network_(storage_),
        identity_("custom_arduino_esp32"),
        client_(link_, provisioning_, network_, tlsClock_, identity_, storage_,
                clock_, logger_, entropy_, hardware_), recovery_(storage_, network_) {
    client_.setRestartHandler(scheduleManagedRestart, this);
  }

  // Inject a board transport; the default uses CBOR over MQTT/TLS.
  explicit FlovaEsp32(FlovaClientLink& transport)
      : linkPlatform_(), defaultLink_(linkPlatform_, entropy_), link_(transport),
        provisioning_(storage_, FLOVA_SETUP_PASSWORD), network_(storage_),
        identity_("custom_arduino_esp32"),
        client_(link_, provisioning_, network_, tlsClock_, identity_, storage_,
                clock_, logger_, entropy_, hardware_), recovery_(storage_, network_) {
    client_.setRestartHandler(scheduleManagedRestart, this);
  }

  bool begin() { return client_.begin(true); }
  void run() {
    client_.run();
    recovery_.restartIfDue(millis());
    if (!client_.networkSetupActive()) return;
    if (!client_.networkSetupReady()) return;
    if (recovery_.state() == FlovaWifiRecovery::State::Idle) {
      const bool recoveryReady = recovery_.begin();
      if (!recoveryReady || !provisioning_.startWifiSetup(recovery_)) {
        recovery_.reset(recoveryReady ? "wifi_setup_start_failed" : recovery_.error());
        if (!network_.begin()) recovery_.reset("wifi_restore_failed");
        client_.endNetworkSetup();
        return;
      }
    }
    provisioning_.loop();
    recovery_.run(millis());
    if (recovery_.state() == FlovaWifiRecovery::State::Committed ||
        recovery_.state() == FlovaWifiRecovery::State::Cancelled ||
        recovery_.state() == FlovaWifiRecovery::State::Finished) {
      if (recovery_.finish() && provisioning_.stopProvisioning()) {
        recovery_.reset();
        client_.endNetworkSetup();
      }
    }
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
    return const_cast<FlovaEsp32*>(this)->client_.datastream<T>(key).hasValue();
  }
  template <typename T>
  T flovaValue(const char* key) const {
    return const_cast<FlovaEsp32*>(this)->client_.datastream<T>(key).value();
  }
  template <typename T>
  flova::WriteResult flovaReport(const char* key, const T& value,
                                 flova::Origin origin = flova::Origin::SensorRead) {
    return client_.datastream<T>(key).report(value, origin);
  }
  template <typename T>
  FlovaDatastream<T, FlovaEsp32> stream(const char* key) {
    return FlovaDatastream<T, FlovaEsp32>(*this, key);
  }
  template <typename T>
  bool flovaOnWrite(const char* key, flova::ValueType valueType,
                    FlovaDatastreamBinding<T>& binding) {
    (void)valueType;
    auto stream = client_.datastream<T>(key);
    switch (binding.kind) {
      case FlovaDatastreamHandlerKind::Result:
        stream.onWrite(binding.handler.result);
        break;
      case FlovaDatastreamHandlerKind::ResultWithContext:
        stream.onWrite(binding.handler.resultContext, binding.context);
        break;
      case FlovaDatastreamHandlerKind::Void:
        stream.onWrite(binding.handler.voidHandler);
        break;
      case FlovaDatastreamHandlerKind::VoidWithContext:
        stream.onWrite(binding.handler.voidContext, binding.context);
        break;
      default:
        return false;
    }
    return true;
  }
  template <typename T>
  bool flovaMode(const char* key, flova::Mode value) {
    client_.datastream<T>(key).mode(value);
    return true;
  }
  template <typename T>
  bool flovaOffline(const char* key, flova::OfflinePolicy value) {
    client_.datastream<T>(key).offline(value);
    return true;
  }
  template <typename T>
  bool flovaRetention(const char* key, const flova::HistoryRetentionPolicy& value) {
    client_.datastream<T>(key).retention(value);
    return true;
  }
  template <typename T>
  bool flovaPersist(const char* key, flova::PersistencePolicy value, uint32_t intervalMs = 1000) {
    client_.datastream<T>(key).persist(value, intervalMs);
    return true;
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
    static_cast<FlovaEsp32*>(context)->recovery_.requestRestart(millis());
  }
  FlovaEsp32Entropy entropy_;
  FlovaEsp32Platform linkPlatform_;
  ArduinoFlovaLink defaultLink_;
  FlovaClientLink& link_;
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
