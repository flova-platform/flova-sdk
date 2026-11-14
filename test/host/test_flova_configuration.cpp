#include "FlovaConfiguration.h"
#include "FlovaLinkConfigurationStorage.h"
#include "FlovaEsp32StorageKey.h"
#include "adapters/ArduinoFlovaManualHardware.h"

#include <assert.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

class NvsMappedStorage final : public flova::Storage {
 public:
  bool read(const char* key, void* out, size_t size) override {
    const auto it = records.find(key);
    if (it == records.end() || it->second.size() != size) return false;
    memcpy(out, it->second.data(), size);
    return true;
  }
  bool write(const char* key, const void* value, size_t size) override {
    assert(writeCount < sizeof(writes) / sizeof(writes[0]));
    if (!flova::makeNvsStorageKey(key, writes[writeCount],
                                  sizeof(writes[writeCount])))
      return false;
    ++writeCount;
    const auto* bytes = static_cast<const uint8_t*>(value);
    records[key] = std::vector<uint8_t>(bytes, bytes + size);
    return true;
  }
  bool remove(const char* key) override {
    char physical[16] = {};
    return flova::makeNvsStorageKey(key, physical, sizeof(physical));
  }

  std::map<std::string, std::vector<uint8_t>> records;
  char writes[8][16] = {};
  size_t writeCount = 0;
};

class RecordStorage final : public flova::Storage {
 public:
  bool read(const char* key, void* out, size_t size) override {
    const auto it = records.find(key);
    if (it == records.end() || it->second.size() != size) return false;
    memcpy(out, it->second.data(), size);
    return true;
  }
  bool write(const char* key, const void* data, size_t size) override {
    const auto* bytes = static_cast<const uint8_t*>(data);
    records[key] = std::vector<uint8_t>(bytes, bytes + size);
    return true;
  }
  bool remove(const char* key) override { records.erase(key); return true; }
  std::map<std::string, std::vector<uint8_t>> records;
};

static void skippedGenerationPreservesActiveBank() {
  RecordStorage backend;
  FlovaLinkConfigurationStorage storage(backend, 4);
  flova::config::GenerationManifest manifest;
  manifest.schemaVersion = 1;
  manifest.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  manifest.recordCount = 1;
  flova::config::Record record;
  record.length = 1;
  record.body[0] = 0x42;
  for (uint32_t generation : {1U, 3U, 4U, 6U}) {
    manifest.generation = generation;
    manifest.finalized = false;
    uint32_t active = 0;
    assert(storage.activeGeneration(active));
    assert(storage.beginInactive(manifest));
    if (active) {
      // Reconstruct the adapter as after a reset during transfer.
      FlovaLinkConfigurationStorage restored(backend, 4);
      flova::config::GenerationManifest previous;
      flova::config::Record saved;
      assert(restored.generationManifest(active, previous));
      assert(previous.generation == active && previous.finalized);
      assert(restored.readRecord(active, 0, saved));
      assert(saved.generation == active && saved.body[0] == 0x42);
    }
    record.generation = generation;
    assert(storage.writeRecord(generation, record));
    assert(storage.finalizeInactive(manifest));
    assert(storage.promoteGeneration(generation));
    uint32_t newest = 0, previous = 0;
    storage.generations(newest, previous);
    assert(newest == generation && previous == active);
  }
  flova::config::GenerationManifest missing;
  assert(!storage.generationManifest(2, missing));
  backend.records["flova_l_0_m"][0] ^= 1;
  FlovaLinkConfigurationStorage recovered(backend, 4);
  assert(!recovered.generationManifest(6, missing));
  assert(recovered.discardGeneration(6));
  assert(recovered.generationManifest(4, missing));
  assert(missing.generation == 4 && missing.finalized);
  assert(recovered.discardGeneration(999));
  assert(recovered.generationManifest(4, missing));
}

static void legacyFirstEvenGenerationRemainsRecoverable() {
  RecordStorage backend;
  FlovaLinkConfigurationStorage storage(backend, 4);
  flova::config::GenerationManifest manifest;
  manifest.generation = 2;
  manifest.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  manifest.recordCount = 1;
  flova::config::Record record;
  record.generation = 2;
  record.length = 1;
  record.body[0] = 0x42;
  assert(storage.beginInactive(manifest));
  assert(storage.writeRecord(2, record));
  assert(storage.finalizeInactive(manifest));
  assert(storage.promoteGeneration(2));
  // Exact V1 pointer representation: first promotion always chose slot B.
  uint32_t pointer[3] = {0x46435031UL, 2, 2166136261UL};
  const auto* bytes = reinterpret_cast<const uint8_t*>(pointer);
  for (size_t i = 0; i < 8; ++i) pointer[2] = (pointer[2] ^ bytes[i]) * 16777619UL;
  assert(backend.write("flova_l_b", pointer, sizeof(pointer)));
  backend.remove("flova_l_a");
  FlovaLinkConfigurationStorage migrated(backend, 4);
  manifest.generation = 4;
  assert(migrated.beginInactive(manifest));
  flova::config::Record previous;
  assert(migrated.readRecord(2, 0, previous));
  assert(previous.body[0] == 0x42);
  record.generation = 4;
  assert(migrated.writeRecord(4, record));
  assert(migrated.finalizeInactive(manifest));
  assert(migrated.promoteGeneration(4));
  uint32_t newest = 0, old = 0;
  migrated.generations(newest, old);
  assert(newest == 4 && old == 2);
}

int main() {
  skippedGenerationPreservesActiveBank();
  legacyFirstEvenGenerationRemainsRecoverable();
  const uint8_t uuid[16] = {
      0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  char text[37] = {};

  assert(flova::formatUuidText(uuid, text, sizeof(text)));
  assert(strcmp(text, "00112233-4455-6677-8899-aabbccddeeff") == 0);
  assert(!flova::formatUuidText(uuid, text, sizeof(text) - 1));

  flova::ProvisioningHandoff handoff("wss://engine.example/api/device-link",
                                     "short-lived-token");
  assert(strcmp(handoff.token, "short-lived-token") == 0);

  flova::ProvisioningState state = {};
  assert(flova::copyBounded(handoff.linkUrl, state.linkUrl, true));
  assert(flova::copyBounded(handoff.token, state.token, true));
  assert(flova::copyBounded("generated-secret", state.linkSecret, true));
  flova::ProvisioningHandoffImage image = {};
  flova::makeProvisioningImage(state, image);
  assert(flova::verifyProvisioningImage(image));
  image.handoff.token[0] = 'x';
  assert(!flova::verifyProvisioningImage(image));

  assert(flova::provisioningBootMode(false, true, true) ==
         flova::ProvisioningBootMode::InterruptedBootstrap);
  assert(flova::provisioningBootMode(true, true, true) ==
         flova::ProvisioningBootMode::Runtime);
  assert(flova::terminalProvisioningError("invalid_provision_token"));
  assert(flova::terminalProvisioningError("provision_token_expired"));
  assert(flova::terminalProvisioningError(
      "provision_token_attempts_exceeded"));
  assert(flova::terminalProvisioningError("provisioning_secret_mismatch"));
  assert(!flova::terminalProvisioningError("bootstrap_timeout"));
  assert(!flova::provisioningFailureNeedsSetup("bootstrap_timeout"));
  assert(!flova::provisioningFailureNeedsSetup("network_timeout"));
  assert(flova::provisioningFailureNeedsSetup("provision_token_expired"));
  assert(flova::provisioningFailureNeedsSetup("invalid_provision_token"));

  char storageKey[16] = {};
  assert(flova::makeNvsStorageKey("history.meta", storageKey,
                                  sizeof(storageKey)));
  assert(strcmp(storageKey, "hm") == 0);
  assert(flova::makeNvsStorageKey("history:63", storageKey,
                                  sizeof(storageKey)));
  assert(strcmp(storageKey, "h63") == 0);
  assert(flova::makeNvsStorageKey("dsid:65535", storageKey,
                                  sizeof(storageKey)));
  assert(strcmp(storageKey, "d65535") == 0);
  assert(flova::makeNvsStorageKey("flova_l_1_r_127", storageKey,
                                  sizeof(storageKey)));
  assert(strcmp(storageKey, "r1_127") == 0);
  assert(!flova::makeNvsStorageKey("history:256", storageKey,
                                 sizeof(storageKey)));
  assert(flova::makeNvsStorageKey("cmd:0", storageKey, sizeof(storageKey)) && !strcmp(storageKey, "c0"));
  assert(flova::makeNvsStorageKey("cmd:255", storageKey, sizeof(storageKey)) && !strcmp(storageKey, "c255"));
  assert(!flova::makeNvsStorageKey("cmd:256", storageKey, sizeof(storageKey)));
  assert(!flova::makeNvsStorageKey("cmd:-1", storageKey, sizeof(storageKey)));
  assert(!flova::makeNvsStorageKey("unknown", storageKey,
                                   sizeof(storageKey)));

  ArduinoFlovaManualHardware manualHardware;
  const flova::HardwareCapabilities manualCapabilities =
      manualHardware.capabilities();
  assert(!manualCapabilities.automaticMapping);
  assert(manualCapabilities.inputSlots == 0);
  assert(manualCapabilities.outputSlots == 0);
  flova::config::Unit mappedUnit = {};
  mappedUnit.kind = flova::config::UnitKind::Datastream;
  mappedUnit.data.datastream.hasMapping = true;
  assert(manualHardware.validate(mappedUnit));
  assert(manualHardware.apply(mappedUnit));

  flova::config::Unit applicationUnit = {};
  applicationUnit.kind = flova::config::UnitKind::Datastream;
  assert(manualHardware.validate(applicationUnit));
  assert(manualHardware.apply(applicationUnit));

  NvsMappedStorage mapped;
  FlovaLinkConfigurationStorage configurationStorage(mapped, 4);
  flova::config::GenerationManifest manifest;
  manifest.generation = 1;
  manifest.schemaVersion = 1;
  manifest.maximumRecordBytes = flova::config::kMaximumRecordBytes;
  manifest.recordCount = 1;
  assert(configurationStorage.beginInactive(manifest));

  flova::config::Record record;
  record.generation = 1;
  record.sequence = 0;
  record.length = 1;
  record.body[0] = 0x42;
  assert(configurationStorage.writeRecord(1, record));
  assert(configurationStorage.finalizeInactive(manifest));
  assert(configurationStorage.promoteGeneration(1));
  assert(mapped.writeCount == 6);
  assert(strcmp(mapped.writes[0], "cp") == 0);
  assert(strcmp(mapped.writes[1], "m1") == 0);
  assert(strcmp(mapped.writes[2], "r1_000") == 0);
  assert(strcmp(mapped.writes[3], "cp") == 0);
  assert(strcmp(mapped.writes[4], "m1") == 0);
  assert(strcmp(mapped.writes[5], "cb") == 0);
  return 0;
}
