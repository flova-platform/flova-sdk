#pragma once

#include <FlovaHardware.h>

// Custom applications own their pins. This policy keeps cloud configuration
// from silently claiming hardware that the application controls explicitly.
class ArduinoFlovaManualHardware final : public flova::Hardware {
 public:
  flova::HardwareCapabilities capabilities() const override {
    return flova::HardwareCapabilities();
  }

  void attach(flova::Device&) override {}

  // Mappings may exist in a template shared with universal firmware. They are
  // accepted for backward compatibility but never applied by this adapter.
  bool validate(const flova::config::Unit& unit) override {
    if (unit.kind != flova::config::UnitKind::Safety) return true;
    const uint8_t policy = static_cast<uint8_t>(unit.data.safety.policy);
    return policy <= 4 || (FLOVA_OFFLINE_GUARDRAIL_CAPACITY && policy >= 7);
  }

  bool apply(const flova::config::Unit&) override { return true; }

  void resetConfiguration() override {}
  void run() override {}
  void setConnected(bool) override {}
};
