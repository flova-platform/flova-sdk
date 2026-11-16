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
static void verifySetupOwnership() {
  struct Client {
    bool ready = false;
    unsigned ends = 0;
    bool networkSetupReady() const { return ready; }
    void endNetworkSetup() { ready = false; ++ends; }
  } client;
  struct Provisioning {
    bool startOk = true, stopOk = true;
    unsigned starts = 0, loops = 0, stops = 0;
    bool startWifiSetup(FlovaWifiRecovery&) { ++starts; return startOk; }
    void loop() { ++loops; }
    bool stopProvisioning() { ++stops; return stopOk; }
  } provisioning;
  struct Clock : flova::Clock {
    uint64_t milliseconds() const override { return 100; }
  } clock;
  Storage storage; Network network;
  FlovaWifiRecovery recovery(storage, network);
  recovery.serviceSetup(client, provisioning, clock);
  assert(provisioning.starts == 0); // Wait for the client's Link drain.
  client.ready = true;
  recovery.serviceSetup(client, provisioning, clock);
  assert(client.ends == 1 && network.restores == 1);
  assert(!strcmp(recovery.error(), "wifi_storage_failed"));
  const auto wifi = credentials("previous");
  assert(flova::writeWifiVerified(storage, "wifi", wifi));
  client.ready = true; provisioning.startOk = false;
  recovery.serviceSetup(client, provisioning, clock);
  assert(client.ends == 2 && network.restores == 2);
  assert(!strcmp(recovery.error(), "wifi_setup_start_failed"));
  client.ready = true; provisioning.startOk = true;
  recovery.serviceSetup(client, provisioning, clock);
  assert(recovery.state() == FlovaWifiRecovery::State::Awaiting);
  assert(recovery.cancel());
  provisioning.stopOk = false;
  recovery.serviceSetup(client, provisioning, clock);
  assert(recovery.state() == FlovaWifiRecovery::State::Finished);
  assert(client.ends == 2 && network.restores == 3);
  provisioning.stopOk = true;
  recovery.serviceSetup(client, provisioning, clock);
  assert(recovery.state() == FlovaWifiRecovery::State::Idle);
  assert(client.ends == 3 && network.restores == 3);
  client.ready = true;
  recovery.serviceSetup(client, provisioning, clock);
  assert(recovery.submit(credentials("next")));
  recovery.serviceSetup(client, provisioning, clock);
  network.online = true;
  recovery.serviceSetup(client, provisioning, clock);
  assert(client.ends == 4 && recovery.state() == FlovaWifiRecovery::State::Idle);
  flova::WifiRuntimeData saved = {};
  assert(storage.read("wifi", &saved, sizeof(saved)) && !strcmp(saved.ssid, "next"));
}
int main() {
  verifySetupOwnership();
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
