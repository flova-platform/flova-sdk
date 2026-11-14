#pragma once

#include <stdio.h>
#include <string.h>

#include <FlovaDevice.h>
#include <FlovaConfigurationInstaller.h>

// Shared fixed-key A/B adapter. Board storage supplies atomic bounded binary
// reads/writes; this class owns generation metadata and record-slot naming.
// It never retains more than one persisted record in a local workspace.
class FlovaLinkConfigurationStorage : public flova::config::ConfigurationStorage {
 public:
  FlovaLinkConfigurationStorage(flova::Storage& storage, uint32_t maximumRecords)
      : storage_(storage), maximumRecords_(maximumRecords) {}

  bool activeGeneration(uint32_t& generation) const override {
    uint32_t previous = 0;
    generations(generation, previous);
    return true;
  }

  // Returns the two independently checksummed pointers in descending order.
  // Runtime restore can validate the newest bank and fall back to the previous
  // complete bank without guessing from parity.
  void generations(uint32_t& newest, uint32_t& previous) const {
    Pointer left = {}, right = {};
    const bool leftOk = loadPointer("flova_l_a", left);
    const bool rightOk = loadPointer("flova_l_b", right);
    newest = previous = 0;
    if (leftOk) newest = left.generation;
    if (rightOk) {
      if (right.generation > newest) {
        previous = newest;
        newest = right.generation;
      } else if (right.generation != newest) {
        previous = right.generation;
      }
    }
  }

  bool discardNewestGeneration() {
    uint32_t newest = 0;
    uint32_t previous = 0;
    generations(newest, previous);
    return discardGeneration(newest);
  }

  bool discardGeneration(uint32_t generation) {
    if (!generation) return false;
    (void)bankForGeneration(generation);
    bool success = true;
    Pointer pointer = {};
    if (loadPointer("flova_l_a", pointer) && pointer.generation == generation) {
      success = storage_.remove("flova_l_a") && success;
    }
    if (loadPointer("flova_l_b", pointer) && pointer.generation == generation) {
      success = storage_.remove("flova_l_b") && success;
    }
    StoredManifest pending = {};
    if (load("flova_l_p", pending) && pending.generation == generation)
      success = storage_.remove("flova_l_p") && success;
    const char* key = manifestKey(generation);
    if (key) success = storage_.remove(key) && success;
    cachedGeneration_ = 0;
    return success;
  }

  bool generationManifest(uint32_t generation,
                          flova::config::GenerationManifest& manifest) const override {
    if (!generation) return false;
    StoredManifest stored = {};
    return load(manifestKey(generation), stored) &&
           stored.generation == generation && unpack(stored, manifest);
  }

  bool pendingManifest(flova::config::GenerationManifest& manifest) const override {
    StoredManifest stored = {};
    return load("flova_l_p", stored) && unpack(stored, manifest);
  }

  bool beginInactive(const flova::config::GenerationManifest& manifest) override {
    if (!manifest.generation || manifest.recordCount > maximumRecords_ ||
        manifest.recordCount > kMaximumRecordSlots ||
        manifest.maximumRecordBytes > flova::config::kMaximumRecordBytes)
      return false;
    uint32_t active = 0;
    if (!activeGeneration(active) || (active && manifest.generation <= active)) return false;
    // A device may skip cloud generations. Parity is not an inactive bank:
    // generation 1 -> 3 must leave generation 1 recoverable until promotion.
    const uint8_t activeBank = active ? bankForGeneration(active) : 0;
    if (activeBank > 1) return false;
    // Older firmware could put its very first pointer in the other slot.
    // Normalize that pointer before reusing the inactive bank; promotion must
    // retain an independent pointer to the last active generation.
    if (active) {
      Pointer pointer = {};
      const char* key = activeBank ? "flova_l_b" : "flova_l_a";
      if (!loadPointer(key, pointer) || pointer.generation != active ||
          pointer.magic != kBankPointerMagic) {
        pointer.magic = kBankPointerMagic;
        pointer.generation = active;
        pointer.checksum = checksum(reinterpret_cast<const uint8_t*>(&pointer), offsetof(Pointer, checksum));
        if (!save(key, pointer)) return false;
      }
    }
    const uint8_t bank = active ? (activeBank ^ 1U) : (manifest.generation & 1U);
    StoredManifest stored = {};
    pack(manifest, stored);
    if (!save("flova_l_p", stored) || !save(bank ? "flova_l_1_m" : "flova_l_0_m", stored))
      return false;
    cachedGeneration_ = manifest.generation;
    cachedBank_ = bank;
    return true;
  }

  bool writeRecord(uint32_t generation, const flova::config::Record& record) override {
    if (!generation || record.generation != generation || record.sequence >= maximumRecords_ ||
        record.sequence >= kMaximumRecordSlots ||
        record.length > flova::config::kMaximumRecordBytes)
      return false;
    memset(&storedWorkspace_, 0, sizeof(storedWorkspace_));
    StoredRecord& stored = storedWorkspace_;
    stored.magic = kRecordMagic;
    stored.generation = record.generation;
    stored.sequence = record.sequence;
    stored.kind = static_cast<uint8_t>(record.kind);
    stored.length = record.length;
    memcpy(stored.body, record.body, record.length);
    stored.checksum = checksum(reinterpret_cast<const uint8_t*>(&stored),
                                offsetof(StoredRecord, checksum));
    return save(recordKey(generation, record.sequence), stored);
  }

  bool readRecord(uint32_t generation, uint32_t sequence,
                  flova::config::Record& record) const override {
    if (sequence >= maximumRecords_ || sequence >= kMaximumRecordSlots) return false;
    memset(&storedWorkspace_, 0, sizeof(storedWorkspace_));
    StoredRecord& stored = storedWorkspace_;
    if (!load(recordKey(generation, sequence), stored) || stored.magic != kRecordMagic ||
        stored.generation != generation || stored.sequence != sequence ||
        stored.length > flova::config::kMaximumRecordBytes ||
        stored.checksum != checksum(reinterpret_cast<const uint8_t*>(&stored),
                                    offsetof(StoredRecord, checksum)))
      return false;
    record.generation = stored.generation;
    record.sequence = stored.sequence;
    record.kind = static_cast<flova::config::RecordKind>(stored.kind);
    record.length = stored.length;
    memcpy(record.body, stored.body, stored.length);
    return true;
  }

  bool finalizeInactive(const flova::config::GenerationManifest& manifest) override {
    StoredManifest stored = {};
    pack(manifest, stored);
    stored.finalized = 1;
    stored.checksum = checksum(reinterpret_cast<const uint8_t*>(&stored),
                               offsetof(StoredManifest, checksum));
    return save("flova_l_p", stored) && save(manifestKey(manifest.generation), stored);
  }

  bool promoteGeneration(uint32_t generation) override {
    uint32_t active = 0;
    if (!activeGeneration(active) || generation < active) return false;
    Pointer pointer = {};
    pointer.magic = kBankPointerMagic;
    pointer.generation = generation;
    pointer.checksum = checksum(reinterpret_cast<const uint8_t*>(&pointer),
                                offsetof(Pointer, checksum));
    const uint8_t bank = bankForGeneration(generation);
    if (bank > 1) return false;
    const char* key = bank ? "flova_l_b" : "flova_l_a";
    return save(key, pointer);
  }

 private:
  static const uint32_t kManifestMagic = 0x46434D31UL;
  static const uint32_t kRecordMagic = 0x46435231UL;
  static const uint32_t kPointerMagic = 0x46435031UL;
  static const uint32_t kBankPointerMagic = 0x46435032UL;
  static const uint32_t kMaximumRecordSlots = 1000UL;

  struct StoredManifest {
    uint32_t magic;
    uint32_t generation;
    uint16_t schemaVersion;
    uint16_t maximumRecordBytes;
    uint32_t recordCount;
    uint8_t sha256[32];
    uint8_t finalized;
    uint8_t reserved[3];
    uint32_t checksum;
  };

  struct StoredRecord {
    uint32_t magic;
    uint32_t generation;
    uint32_t sequence;
    uint8_t kind;
    uint8_t reserved;
    uint16_t length;
    uint8_t body[flova::config::kMaximumRecordBytes];
    uint32_t checksum;
  };

  struct Pointer {
    uint32_t magic;
    uint32_t generation;
    uint32_t checksum;
  };

  static uint32_t checksum(const uint8_t* data, size_t length) {
    uint32_t value = 2166136261UL;
    for (size_t i = 0; i < length; ++i) {
      value ^= data[i];
      value *= 16777619UL;
    }
    return value;
  }

  void pack(const flova::config::GenerationManifest& input,
            StoredManifest& output) const {
    memset(&output, 0, sizeof(output));
    output.magic = kManifestMagic;
    output.generation = input.generation;
    output.schemaVersion = input.schemaVersion;
    output.maximumRecordBytes = input.maximumRecordBytes;
    output.recordCount = input.recordCount;
    memcpy(output.sha256, input.checksum.bytes, sizeof(output.sha256));
    output.finalized = input.finalized ? 1 : 0;
    output.checksum = checksum(reinterpret_cast<const uint8_t*>(&output),
                               offsetof(StoredManifest, checksum));
  }

  static bool unpack(const StoredManifest& input,
                     flova::config::GenerationManifest& output) {
    if (input.magic != kManifestMagic ||
        input.checksum != checksum(reinterpret_cast<const uint8_t*>(&input),
                                   offsetof(StoredManifest, checksum)))
      return false;
    output.generation = input.generation;
    output.schemaVersion = input.schemaVersion;
    output.maximumRecordBytes = input.maximumRecordBytes;
    output.recordCount = input.recordCount;
    memcpy(output.checksum.bytes, input.sha256, sizeof(output.checksum.bytes));
    output.finalized = input.finalized != 0;
    return true;
  }

  uint8_t bankForGeneration(uint32_t generation) const {
    // A verification pass rereads many records in one generation. Resolve its
    // physical bank once rather than adding manifest I/O to every record read.
    if (generation && cachedGeneration_ == generation) return cachedBank_;
    StoredManifest stored = {};
    flova::config::GenerationManifest manifest;
    for (uint8_t bank = 0; bank < 2; ++bank) {
      if (load(bank ? "flova_l_1_m" : "flova_l_0_m", stored) &&
          stored.generation == generation && unpack(stored, manifest)) {
        cachedGeneration_ = generation;
        cachedBank_ = bank;
        return bank;
      }
    }
    // V2 pointers retain the physical bank even if its manifest is damaged.
    // V1 records used generation parity regardless of their pointer slot.
    Pointer pointer = {};
    for (uint8_t bank = 0; bank < 2; ++bank) {
      if (loadPointer(bank ? "flova_l_b" : "flova_l_a", pointer) &&
          pointer.generation == generation) {
        cachedGeneration_ = generation;
        cachedBank_ = pointer.magic == kBankPointerMagic ? bank : (generation & 1U);
        return cachedBank_;
      }
    }
    return 2;
  }

  const char* manifestKey(uint32_t generation) const {
    const uint8_t bank = bankForGeneration(generation);
    if (bank > 1) return nullptr;
    manifestKey_[0] = 'f';
    manifestKey_[1] = 'l';
    manifestKey_[2] = 'o';
    manifestKey_[3] = 'v';
    manifestKey_[4] = 'a';
    manifestKey_[5] = '_';
    manifestKey_[6] = 'l';
    manifestKey_[7] = '_';
    manifestKey_[8] = bank ? '1' : '0';
    manifestKey_[9] = '_';
    manifestKey_[10] = 'm';
    manifestKey_[11] = 0;
    return manifestKey_;
  }

  const char* recordKey(uint32_t generation, uint32_t sequence) const {
    const uint8_t bank = bankForGeneration(generation);
    if (bank > 1) return nullptr;
    memcpy(recordKey_, "flova_l_0_r_000", sizeof(recordKey_));
    recordKey_[8] = bank ? '1' : '0';
    recordKey_[12] = static_cast<char>('0' + sequence / 100U);
    recordKey_[13] = static_cast<char>('0' + (sequence / 10U) % 10U);
    recordKey_[14] = static_cast<char>('0' + sequence % 10U);
    return recordKey_;
  }

  template <typename T>
  bool load(const char* key, T& output) const {
    return key && storage_.read(key, &output, sizeof(output));
  }

  template <typename T>
  bool save(const char* key, const T& value) {
    return key && storage_.write(key, &value, sizeof(value));
  }

  bool loadPointer(const char* key, Pointer& pointer) const {
    return load(key, pointer) &&
           (pointer.magic == kPointerMagic || pointer.magic == kBankPointerMagic) &&
           pointer.checksum == checksum(reinterpret_cast<const uint8_t*>(&pointer),
                                        offsetof(Pointer, checksum));
  }

  flova::Storage& storage_;
  uint32_t maximumRecords_;
  mutable uint32_t cachedGeneration_ = 0;
  mutable uint8_t cachedBank_ = 0;
  mutable StoredRecord storedWorkspace_ = {};
  mutable char manifestKey_[12] = {};
  mutable char recordKey_[16] = {};
};
