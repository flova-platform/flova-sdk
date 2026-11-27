#pragma once

#include <FlovaFlashLog.h>

#include <new>

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FlovaConfiguration.h>
#include <FlovaManagedClaim.h>
#include <FlovaProvisioningAdapter.h>
#include <FlovaProvisioningSecurity.h>
#include <FlovaSoftApPortal.h>
#include <FlovaWifiProvisioning.h>
#include <FlovaEsp8266Services.h>

template <bool WifiRecoveryEnabled>
class FlovaEsp8266ProvisioningBase : public FlovaProvisioningAdapter {
 public:
  explicit FlovaEsp8266ProvisioningBase(FlovaEsp8266Storage& storage,
                                    const char* setupPassword = FLOVA_SETUP_PASSWORD)
      : storage_(storage), setupPassword_(setupPassword) {}

  ~FlovaEsp8266ProvisioningBase() override { stopProvisioning(); }
  FlovaEsp8266ProvisioningBase(const FlovaEsp8266ProvisioningBase&) = delete;
  FlovaEsp8266ProvisioningBase& operator=(const FlovaEsp8266ProvisioningBase&) = delete;

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
    WiFi.disconnect(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    char ssid[32] = {};
    FLOVA_FORMAT(ssid, sizeof(ssid), "Flova-Setup-%06lx",
             static_cast<unsigned long>(ESP.getChipId()));
    if (!WiFi.softAP(ssid, setupPassword_, 1, false, 4)) { stopProvisioning(); return false; }
    setup_->server.begin();
    provisioning_ = true;
    return true;
  }

  bool stopProvisioning() override {
    const bool hadSetup = setup_ != nullptr;
    const bool hadAp = (WiFi.getMode() & WIFI_AP) != 0;
    if (!hadSetup && !hadAp) return true;
    if (hadSetup || hadAp) logHeap(0);
    provisioning_ = false;
    recovery_ = nullptr;
    if (setup_) { setup_->server.stop(); delete setup_; setup_ = nullptr; }
    if (hadSetup) logHeap(1);
    bool stopped = WiFi.softAPdisconnect(true);
    if (WiFi.getMode() & WIFI_AP) {
      yield();
      stopped = WiFi.mode(WIFI_STA) && stopped;
    }
    const bool apDisabled = (WiFi.getMode() & WIFI_AP) == 0;
    if (hadSetup || hadAp) logHeap(2);
    return stopped && apDisabled;
  }

  bool stopAfterNetworkConnected() const override { return true; }

 private:
  static void logHeap(uint8_t stage) {
    uint32_t freeBytes = 0;
    uint32_t largestBlock = 0;
    uint8_t fragmentation = 0;
    ESP.getHeapStats(&freeBytes, &largestBlock, &fragmentation);
    FLOVA_SERIAL_PRINTF_DEBUG(
        "[flova] provisioning heap stage=%u dram_free=%u dram_max=%u dram_frag=%u%% mode=%u\n",
        stage, freeBytes, largestBlock, fragmentation,
        static_cast<unsigned>(WiFi.getMode()));
  }

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
    using Claim = flova::detail::ManagedClaim;
    const auto result = Claim::submit(storage_, setup_->wifi, setup_->input, handler_, context_);
    if (result == Claim::Submission::WifiWriteFailed) {
      FLOVA_SERIAL_PRINTF_ERROR("[flova] provisioning storage_failed stage=wifi reason=%s\n",
                    storage_.lastError());
      setup_->server.send(500, "application/json", "{\"ok\":false,\"error\":\"storage_failed\"}");
      return;
    }
    if (result == Claim::Submission::Accepted) {
      setup_->server.send(202, "application/json", "{\"ok\":true,\"status\":\"accepted\"}");
    } else if (result == Claim::Submission::HandoffWriteFailed) {
      FLOVA_SERIAL_PRINTLN_ERROR("[flova] provisioning storage_failed stage=handoff");
      setup_->server.send(500, "application/json", "{\"ok\":false,\"error\":\"storage_failed\"}");
    } else {
      setup_->server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_handoff\"}");
    }
  }

  FlovaEsp8266Storage& storage_;
  const char* setupPassword_;
  FlovaWifiRecovery* recovery_ = nullptr;
  struct Setup {
    ESP8266WebServer server{80};
    flova::ProvisioningHandoff input;
    flova::WifiRuntimeData wifi = {};
  };
  Setup* setup_ = nullptr;
  FlovaProvisioningHandler handler_ = nullptr;
  void* context_ = nullptr;
  bool provisioning_ = false;
};

using FlovaEsp8266Provisioning = FlovaEsp8266ProvisioningBase<false>;
using FlovaEsp8266ProvisioningWithWifiRecovery = FlovaEsp8266ProvisioningBase<true>;
