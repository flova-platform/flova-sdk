#pragma once

#include <FlovaDevice.h>
#include <FlovaRuntimeServices.h>
#include <FlovaWifiProvisioning.h>

// Explicitly enabled by managed board compositions. These records use the
// existing board-local WifiRuntimeData layout; no cloud identity is modified.
namespace flova {
inline bool sameWifi(const WifiRuntimeData& a, const WifiRuntimeData& b) {
  return validWifiRuntimeData(a) && validWifiRuntimeData(b) &&
         strcmp(a.ssid, b.ssid) == 0 && strcmp(a.password, b.password) == 0;
}
inline bool writeWifiVerified(Storage& storage, const char* key,
                              const WifiRuntimeData& value) {
  WifiRuntimeData verified = {};
  return storage.write(key, &value, sizeof(value)) &&
         storage.read(key, &verified, sizeof(verified)) && sameWifi(value, verified);
}
// A durable rollback marker remains until candidate promotion is verified.
// Boot interrupted before marker removal restores the old network.
inline bool restoreWifiRecovery(Storage& storage) {
  WifiRuntimeData previous = {};
  if (!storage.read("wifi_trial", &previous, sizeof(previous))) return true;
  return validWifiRuntimeData(previous) &&
         writeWifiVerified(storage, "wifi", previous) && storage.remove("wifi_trial");
}
}  // namespace flova

class FlovaWifiNetworkRuntime : public FlovaNetworkRuntime {
 public:
  virtual bool connectWifi(const flova::WifiRuntimeData&) = 0;
};

class FlovaWifiRecovery {
 public:
  enum class State : uint8_t { Idle, Awaiting, Queued, Associating, Committed, Cancelled, Finished };
  FlovaWifiRecovery(flova::Storage& storage, FlovaWifiNetworkRuntime& network)
      : storage_(storage), network_(network) {}
  bool begin() {
    if (!storage_.read("wifi", &previous_, sizeof(previous_)) ||
        !flova::validWifiRuntimeData(previous_)) {
      error_ = "wifi_storage_failed"; return false;
    }
    state_ = State::Awaiting; error_ = ""; return true;
  }
  bool submit(const flova::WifiRuntimeData& wifi) {
    if (!canSubmit() || !flova::validWifiRuntimeData(wifi)) return false;
    error_ = "";
    candidate_ = wifi;
    state_ = State::Queued;
    return true;
  }
  bool cancel() {
    if (state_ == State::Idle || state_ == State::Committed || state_ == State::Finished) return false;
    state_ = State::Cancelled;
    return true;
  }
  void run(uint32_t now) {
    if (state_ == State::Queued) {
      startedAt_ = now;
      state_ = State::Associating;
      if (!network_.connectWifi(candidate_)) fail("wifi_start_failed");
      return;
    }
    if (state_ != State::Associating) return;
    if (!network_.connected()) {
      if (static_cast<uint32_t>(now - startedAt_) >= 30000UL)
        fail(network_.connectionError());
      return;
    }
    if (flova::sameWifi(previous_, candidate_)) { state_ = State::Committed; return; }
    if (!flova::writeWifiVerified(storage_, "wifi_trial", previous_)) {
      fail("wifi_storage_failed"); restoreFailed_ = true; return;
    }
    if (!flova::writeWifiVerified(storage_, "wifi", candidate_) ||
        !storage_.remove("wifi_trial")) {
      // Do not permit another candidate to overwrite an unresolved rollback.
      if (!flova::restoreWifiRecovery(storage_)) {
        fail("wifi_restore_failed");
        restoreFailed_ = true;
      } else fail("wifi_storage_failed");
      return;
    }
    state_ = State::Committed;
  }
  bool finish() {
    if (state_ == State::Finished) return true;
    if (state_ != State::Committed && state_ != State::Cancelled) return false;
    if (state_ == State::Cancelled && !network_.begin()) {
      fail("wifi_restore_failed"); restoreFailed_ = true; return false;
    }
    state_ = State::Finished;
    return true;
  }
  // Both managed ESP facades use this ordering: drain Link first, then own
  // the setup channel until credential promotion/cancellation has finished.
  template <typename Client, typename Provisioning>
  void serviceSetup(Client& client, Provisioning& provisioning,
                    const flova::Clock& clock) {
    if (!client.networkSetupReady()) return;
    if (state_ == State::Idle) {
      const bool ready = begin();
      if (!ready || !provisioning.startWifiSetup(*this)) {
        reset(ready ? "wifi_setup_start_failed" : error());
        if (!network_.begin()) reset("wifi_restore_failed");
        client.endNetworkSetup();
        return;
      }
    }
    provisioning.loop();
    run(static_cast<uint32_t>(clock.milliseconds()));
    if (state_ == State::Committed || state_ == State::Cancelled ||
        state_ == State::Finished) {
      if (finish() && provisioning.stopProvisioning()) {
        reset();
        client.endNetworkSetup();
      }
    }
  }
  void reset(const char* error = nullptr) {
    if (error) error_ = error;
    state_ = State::Idle;
    candidate_ = flova::WifiRuntimeData{};
    previous_ = flova::WifiRuntimeData{};
    restoreFailed_ = false;
  }
  void requestRestart(uint32_t now) { restartAt_ = now ? now : 1; }
  bool restartDue(uint32_t now) const {
    return restartAt_ && static_cast<uint32_t>(now - restartAt_) >= 1000UL;
  }
  State state() const { return state_; }
  const char* error() const { return error_; }
  bool canSubmit() const { return state_ == State::Awaiting && !restoreFailed_; }
 private:
  void fail(const char* error) { error_ = error; state_ = State::Awaiting; }
  flova::Storage& storage_;
  FlovaWifiNetworkRuntime& network_;
  flova::WifiRuntimeData candidate_ = {};
  flova::WifiRuntimeData previous_ = {};
  State state_ = State::Idle;
  const char* error_ = "";
  uint32_t startedAt_ = 0;
  uint32_t restartAt_ = 0;
  bool restoreFailed_ = false;
};
