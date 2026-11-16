#include <FlovaWifiRecovery.h>
#include <assert.h>
#include <map>
#include <string>
#include <vector>

class Storage final : public flova::Storage {
 public:
  bool read(const char* key, void* out, size_t size) override {
    auto found = records.find(key);
    if (found == records.end() || found->second.size() != size) return false;
    memcpy(out, found->second.data(), size);
    if (corruptRead == key) static_cast<char*>(out)[0] ^= 1;
    return true;
  }
  bool write(const char* key, const void* data, size_t size) override {
    if (failWrite == key) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    records[key] = std::vector<uint8_t>(bytes, bytes + size);
    return true;
  }
  bool remove(const char* key) override {
    if (failRemove == key) return false;
    records.erase(key); return true;
  }
  std::map<std::string, std::vector<uint8_t>> records;
  std::string failWrite, failRemove, corruptRead;
};
class Network final : public FlovaWifiNetworkRuntime {
 public:
  bool begin() override { ++restores; return restoreOk; }
  bool connectWifi(const flova::WifiRuntimeData&) override { ++attempts; return startOk; }
  bool connected() const override { return online; }
  const char* connectionError() const override { return "wifi_auth_failed"; }
  bool online = false, startOk = true, restoreOk = true;
  unsigned attempts = 0, restores = 0;
};
static flova::WifiRuntimeData credentials(const char* ssid) {
  flova::WifiRuntimeData data = {};
  strcpy(data.ssid, ssid); strcpy(data.password, "password"); return data;
}
int main() {
  // Credentials-only recovery rejects claim tokens, duplicates and oversize data.
  flova::ProvisioningHandoff handoff;
  flova::WifiRuntimeData parsed = {};
  const char* json = "{\"wifi_ssid\":\"new\",\"wifi_password\":\"password\"}";
  assert(flova::parseWifiProvisioningHandoff(json, strlen(json), handoff, &parsed, true));
  const char* bad = "{\"wifi_ssid\":\"new\",\"token\":\"secret\"}";
  assert(!flova::parseWifiProvisioningHandoff(bad, strlen(bad), handoff, &parsed, true));
  bad = "{\"wifi_ssid\":\"a\",\"wifi_ssid\":\"b\"}";
  assert(!flova::parseWifiProvisioningHandoff(bad, strlen(bad), handoff, &parsed, true));
  const auto previous = credentials("previous"), next = credentials("next");
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    Storage storage; Network network;
    assert(flova::writeWifiVerified(storage, "wifi", previous));
    const char config[] = "identity and config";
    assert(storage.write("config", config, sizeof(config)));
    auto originalConfig = storage.records["config"];
    FlovaWifiRecovery recovery(storage, network);
    assert(!recovery.submit(next));
    recovery.begin(); assert(recovery.submit(next));
    recovery.run(0xfffffff0U);
    assert(network.attempts == 1);
    if (scenario == 0) {
      recovery.run(30000); // rollover-safe timeout; no writes or loss of old network
      assert(!strcmp(recovery.error(), "wifi_auth_failed"));
      assert(recovery.canSubmit());
      assert(recovery.cancel()); assert(recovery.finish());
      assert(network.restores == 1);
    } else {
      network.online = true;
      if (scenario == 2) storage.failWrite = "wifi";
      if (scenario == 3) storage.failWrite = "wifi_trial";
      if (scenario == 4) storage.corruptRead = "wifi_trial";
      if (scenario == 5) storage.failRemove = "wifi_trial";
      if (scenario == 6) storage.corruptRead = "wifi";
      recovery.run(100);
      if (scenario == 1) {
        assert(recovery.state() == FlovaWifiRecovery::State::Committed);
        assert(recovery.finish());
        assert(!storage.records.count("wifi_trial"));
      } else {
        assert(recovery.state() == FlovaWifiRecovery::State::Awaiting);
        assert(recovery.error()[0]);
      }
    }
    assert(storage.records["config"] == originalConfig);
    // Simulate reboot with all I/O working. Any unfinished promotion rolls back.
    storage.failWrite.clear(); storage.failRemove.clear(); storage.corruptRead.clear();
    assert(flova::restoreWifiRecovery(storage));
    flova::WifiRuntimeData saved = {};
    assert(storage.read("wifi", &saved, sizeof(saved)));
    assert(flova::sameWifi(saved, scenario == 1 ? next : previous));
  }
  // Crash after candidate write, before promotion marker removal.
  Storage storage;
  assert(flova::writeWifiVerified(storage, "wifi_trial", previous));
  assert(flova::writeWifiVerified(storage, "wifi", next));
  assert(flova::restoreWifiRecovery(storage));
  flova::WifiRuntimeData saved = {};
  assert(storage.read("wifi", &saved, sizeof(saved)) && flova::sameWifi(saved, previous));
  return 0;
}
