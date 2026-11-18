#pragma once

#include <new>

#include <WebServer.h>
#include <WiFi.h>
#include <FlovaConfiguration.h>
#include <FlovaProvisioningAdapter.h>
#include <FlovaProvisioningSecurity.h>
#include <FlovaSoftApPortal.h>
#include <FlovaWifiProvisioning.h>
#include <FlovaEsp32Services.h>

template <bool WifiRecoveryEnabled>
class FlovaEsp32ProvisioningBase : public FlovaProvisioningAdapter {
 public:
  explicit FlovaEsp32ProvisioningBase(FlovaEsp32Storage& storage,
                                  const char* setupPassword = FLOVA_SETUP_PASSWORD)
      : storage_(storage), setupPassword_(setupPassword) {}

  ~FlovaEsp32ProvisioningBase() override { delete setup_; }
  FlovaEsp32ProvisioningBase(const FlovaEsp32ProvisioningBase&) = delete;
  FlovaEsp32ProvisioningBase& operator=(const FlovaEsp32ProvisioningBase&) = delete;

  bool begin(FlovaProvisioningHandler handler, void* context) override {
    handler_ = handler;
    context_ = context;
    return true;
  }
  void clearHandler() override { handler_ = nullptr; context_ = nullptr; }

  void loop() override {
    if (provisioning_) setup_->server.handleClient();
  }

  bool startWifiSetup(FlovaWifiRecovery& recovery) {
    recovery_ = &recovery;
    if (startProvisioning()) return true;
    stopProvisioning();
    return false;
  }

  bool startProvisioning() override {
    if (!flova::validSetupPassword(setupPassword_)) return false;
    FlovaWifiRecovery* recovery = recovery_;
    if (!stopProvisioning()) return false;
    recovery_ = recovery;
    if (!recovery_ && (!storage_.remove("wifi") || !storage_.remove("wifi_trial"))) return false;
    setup_ = new (std::nothrow) Setup;
    if (!setup_) return false;
    setup_->server.on("/setup", HTTP_GET, [this]() { handleSetup(); });
    setup_->server.on("/", HTTP_GET, [this]() { handleSetup(); });
    if (WifiRecoveryEnabled) {
      setup_->server.on("/wifi", HTTP_POST, [this]() {
        const String& body = setup_->server.arg("plain");
        if (!recovery_ || !recovery_->canSubmit() || body.length() >= 256 ||
            !flova::parseWifiProvisioningHandoff(body.c_str(), body.length(),
                setup_->input, &setup_->wifi, true) ||
            !recovery_->submit(setup_->wifi)) {
          setup_->server.send(400, "application/json", "{\"error\":\"invalid_wifi_settings\"}");
          return;
        }
        setup_->server.send(202, "application/json", "{\"status\":\"testing\"}");
      });
      setup_->server.on("/wifi/cancel", HTTP_POST, [this]() {
        if (recovery_) recovery_->cancel();
        setup_->server.send(202, "application/json", "{}");
      });
    }
    setup_->server.on("/status", HTTP_GET, [this]() { handleStatus(); });
    setup_->server.on("/provision", HTTP_POST, [this]() { handleProvision(); });
    WiFi.persistent(false);
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    char ssid[32] = {};
    snprintf(ssid, sizeof(ssid), "Flova-Setup-%08lx",
             static_cast<unsigned long>(ESP.getEfuseMac()));
    if (!WiFi.softAP(ssid, setupPassword_)) { stopProvisioning(); return false; }
    setup_->server.begin();
    provisioning_ = true;
    return true;
  }

  bool stopProvisioning() override {
    provisioning_ = false;
    recovery_ = nullptr;
    if (setup_) { setup_->server.stop(); delete setup_; setup_ = nullptr; }
    WiFi.softAPdisconnect(true);
    return true;
  }

  bool stopAfterNetworkConnected() const override { return true; }

 private:
  void handleSetup() {
    setup_->server.sendHeader("Cache-Control", "no-store");
    setup_->server.sendHeader("X-Frame-Options", "DENY");
    setup_->server.send_P(200, "text/html; charset=utf-8", WifiRecoveryEnabled && recovery_ ? flova::kWifiRecoveryPage : flova::kSoftApSetupPage);
  }

  void handleStatus() {
    if (WifiRecoveryEnabled && recovery_) {
      char response[160] = {};
      snprintf(response, sizeof(response), "{\"status\":\"wifi_setup\",\"testing\":%s,\"error\":\"%s\"}",
          recovery_->state() == FlovaWifiRecovery::State::Associating ||
          recovery_->state() == FlovaWifiRecovery::State::Queued ? "true" : "false",
          recovery_->error());
      setup_->server.sendHeader("Cache-Control", "no-store");
      setup_->server.send(200, "application/json", response);
      return;
    }
    char error[flova::kProvisioningErrorBytes] = {};
    char* body = reinterpret_cast<char*>(&setup_->input);
    const size_t bodyCapacity = sizeof(setup_->input);
    const bool hasError =
        storage_.read("prov_error", error, sizeof(error)) && error[0];
    snprintf(body, bodyCapacity,
             hasError
                 ? "{\"status\":\"setup_mode\",\"protocol\":\"flova-link-v1\",\"browser_handoff\":\"fragment-v1\",\"can_retry\":true,\"last_error_code\":\"%s\"}"
                 : "{\"status\":\"setup_mode\",\"protocol\":\"flova-link-v1\",\"browser_handoff\":\"fragment-v1\",\"can_retry\":true}",
             error);
    setup_->server.send(200, "application/json", body);
  }

  void handleProvision() {
    if (WifiRecoveryEnabled && recovery_) {
      setup_->server.send(409, "application/json", "{\"error\":\"wifi_only_setup\"}");
      return;
    }
    const String& body = setup_->server.arg("plain");
    if (!handler_ || body.length() >= 768 ||
        !flova::parseWifiProvisioningHandoff(body.c_str(), body.length(), setup_->input,
                                             &setup_->wifi)) {
      setup_->server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_handoff\"}");
      return;
    }
    if (!storage_.write("wifi", &setup_->wifi, sizeof(setup_->wifi))) {
      setup_->server.send(500, "application/json", "{\"ok\":false,\"error\":\"storage_failed\"}");
      return;
    }
    const FlovaProvisioningResponse result = handler_(context_, setup_->input);
    if (result == FlovaProvisioningResponse::Accepted) {
      setup_->server.send(202, "application/json", "{\"ok\":true,\"status\":\"accepted\"}");
    } else if (result == FlovaProvisioningResponse::StorageFailed) {
      setup_->server.send(500, "application/json", "{\"ok\":false,\"error\":\"storage_failed\"}");
    } else {
      storage_.remove("wifi");
      setup_->server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_handoff\"}");
    }
  }

  FlovaEsp32Storage& storage_;
  const char* setupPassword_;
  FlovaWifiRecovery* recovery_ = nullptr;
  struct Setup {
    WebServer server{80};
    flova::ProvisioningHandoff input;
    flova::WifiRuntimeData wifi = {};
  };
  Setup* setup_ = nullptr;
  FlovaProvisioningHandler handler_ = nullptr;
  void* context_ = nullptr;
  bool provisioning_ = false;
};

using FlovaEsp32Provisioning = FlovaEsp32ProvisioningBase<false>;
using FlovaEsp32ProvisioningWithWifiRecovery = FlovaEsp32ProvisioningBase<true>;
