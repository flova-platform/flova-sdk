#include <assert.h>
#include <initializer_list>
#include <map>
#include <string>
#include <string.h>
#include <vector>
#include <FlovaEsp32HardwarePolicy.h>
#include <FlovaEsp8266HardwarePolicy.h>
#include <adapters/ArduinoFlovaHardware.h>
#include <adapters/ArduinoFlovaManualHardware.h>
#include <adapters/ArduinoFlovaServices.h>
#include <FlovaOtaTimeout.h>

HardwareSerial Serial;
static uint32_t ticks;
uint32_t millis() { return ticks; }
void pinMode(uint8_t, uint8_t) {}
static uint8_t pinLevels[64];
static int pwmLevels[64];
void digitalWrite(uint8_t pin, uint8_t level) { pinLevels[pin] = level; }
int digitalRead(uint8_t pin) { return pinLevels[pin]; }
int analogRead(uint8_t) { return 0; }
void analogWrite(uint8_t pin, int value) { pwmLevels[pin] = value; }

class TestLink : public flova::Link {
 public:
  bool begin() override { return true; }
  bool connected() const override { return online; }
  uint32_t connectionEpoch() const override { return online ? 1 : 0; }
  bool send(const flova::Message&) override { return true; }
  bool bindDatastreams(const char* const*, size_t count, DatastreamId* ids) override {
    if (count != 1) return false;
    ids[0] = 1;
    return true;
  }
  void poll() override {}
  void setReceiver(flova::MessageReceiver receiver, void* context) override {
    receiver_ = receiver;
    context_ = context;
  }
  uint32_t messageNonce() const override { return 1; }
  void inject(const flova::Message& message) { receiver_(context_, message); }
  bool online = true;
 private:
  flova::MessageReceiver receiver_ = nullptr;
  void* context_ = nullptr;
};

class TestStorage : public flova::Storage {
 public:
  bool read(const char* key, void* output, size_t size) override {
    const auto found = records_.find(key);
    if (found == records_.end() || found->second.size() != size) return false;
    memcpy(output, found->second.data(), size);
    return true;
  }
  bool write(const char* key, const void* input, size_t size) override {
    const auto* bytes = static_cast<const uint8_t*>(input);
    records_[key].assign(bytes, bytes + size);
    return true;
  }
  bool remove(const char* key) override { return records_.erase(key) == 1; }
 private:
  std::map<std::string, std::vector<uint8_t>> records_;
};

class TestLogger : public flova::Logger {
 public:
  void log(const char*) override {}
};

static flova::config::Unit mapping(const char* key, DatastreamId id, uint16_t pin,
                                  flova::config::MappingKind kind) {
  flova::config::Unit unit = {};
  unit.kind = flova::config::UnitKind::Datastream;
  unit.data.datastream.id = id;
  strcpy(unit.data.datastream.key, key);
  unit.data.datastream.hasMapping = true;
  unit.data.datastream.mapping.kind = kind;
  unit.data.datastream.mapping.pin = pin;
  if (kind == flova::config::MappingKind::DigitalOutput) {
    unit.data.datastream.hasDefault = true;
    unit.data.datastream.defaultValue.kind = flova::config::ValueKind::Boolean;
    unit.data.datastream.mapping.hasMinimumOutputMs = true;
    unit.data.datastream.mapping.minimumOutputMs = 500;
  }
  return unit;
}

static void verifySharedPolicyThroughMapping(bool pwm) {
  TestLink link;
  TestStorage storage;
  TestLogger logger;
  ArduinoFlovaClock clock;
  flova::Device device(link, storage, clock, logger);
  ArduinoFlovaHardware hardware;
  hardware.attach(device);
  auto output = mapping("drive", 1, 5, pwm ? flova::config::MappingKind::PwmOutput
                                           : flova::config::MappingKind::DigitalOutput);
  if (pwm) output.data.datastream.valueType = 3;
  ticks = 1;
  assert(device.applyConfigurationUnit(output) && hardware.apply(output));
  assert(device.begin());

  flova::OfflineGuardrail policy;
  policy.safeValue = pwm ? flova::Value::from(20.0) : flova::Value::from(false);
  policy.maximumRunMs = 50;
  assert(device.installOfflineGuardrail(1, policy));
  device.run();
  uint64_t latch = 0;
  assert(device.offlineGuardrailStatus(1, latch) == flova::GuardrailStatus::Disarmed);
  clock.setUtc(200000, clock.milliseconds());
  flova::Message rearm;
  rearm.kind = flova::MessageKind::GuardrailRearm;
  rearm.datastreamId = 1;
  rearm.monotonic = latch;
  rearm.timestamp = link.connectionEpoch();
  rearm.expiresAtUtcMs = clock.utcMilliseconds() + 10000;
  flova::Value::copy(rearm.commandId, "rearm-drive");
  link.inject(rearm);
  assert(device.offlineGuardrailStatus(1, latch) == flova::GuardrailStatus::Armed);
  ticks += 600;
  assert(device.write(1, pwm ? flova::Value::from(100.0) : flova::Value::from(true)).accepted());
  assert(pwm ? pwmLevels[5] == 255 : pinLevels[5] == HIGH);
  ticks += 50;  // Fallback must not wait for the mapping's normal write interval.
  device.run();
  assert(pwm ? pwmLevels[5] == 51 : pinLevels[5] == LOW);
  assert(device.offlineGuardrailStatus(1, latch) == flova::GuardrailStatus::Tripped);
}

static void verifyRestoredDigitalPolicy(flova::config::SafetyPolicy policy, bool activeHigh) {
  TestLink link;
  TestStorage storage;
  TestLogger logger;
  ArduinoFlovaClock clock;
  flova::Device device(link, storage, clock, logger);
  ArduinoFlovaHardware hardware;
  hardware.attach(device);
  auto output = mapping("pump", 1, 5, flova::config::MappingKind::DigitalOutput);
  output.data.datastream.mapping.hasActiveHigh = true;
  output.data.datastream.mapping.activeHigh = activeHigh;
  const uint8_t on = activeHigh ? HIGH : LOW;
  const uint8_t off = activeHigh ? LOW : HIGH;
  pinLevels[5] = on;
  const auto input = mapping("guard", 2, 4, flova::config::MappingKind::DigitalInput);
  ticks = UINT32_MAX - 1500;
  assert(device.applyConfigurationUnit(output) && hardware.apply(output));
  assert(pinLevels[5] == off);
  assert(device.applyConfigurationUnit(input) && hardware.apply(input));

  flova::config::Unit config = {};
  config.kind = flova::config::UnitKind::Safety;
  config.data.safety.datastreamId = 1;
  config.data.safety.policy = policy;
  config.data.safety.hasTimeoutMs = true;
  config.data.safety.timeoutMs = 1000;
  config.data.safety.guardId = 2;
  assert(device.validateConfigurationUnit(config));
  assert(device.applyConfigurationUnit(config) && hardware.apply(config));
  device.serviceOfflineGuardrails();
  uint64_t latch = 0;
  assert(device.offlineGuardrailStatus(1, latch) == flova::GuardrailStatus::Disarmed);
  assert(pinLevels[5] == off && !device.datastream<bool>("pump").value());
  assert(!device.write(1, flova::Value::from(true)).accepted());
}

int main() {
  verifySharedPolicyThroughMapping(false);
  verifySharedPolicyThroughMapping(true);
  verifyRestoredDigitalPolicy(flova::config::SafetyPolicy::StopOnDisconnect, true);
  verifyRestoredDigitalPolicy(flova::config::SafetyPolicy::FinishBoundedRun, false);
  ArduinoFlovaClock clock;
  ticks = 0xfffffff0UL;
  assert(clock.milliseconds() == ticks);
  clock.setUtc(100000, 0);
  ticks = 20;
  assert(clock.milliseconds() == (1ULL << 32) + 20);
  assert(clock.utcMilliseconds() == 100036);
  ticks = 0xfffffff0UL; clock.milliseconds(); ticks = 30;
  assert(clock.milliseconds() == (2ULL << 32) + 30);
  assert(clock.utcMilliseconds() == 100000 + (1ULL << 32) + 46);
  const uint32_t start = 0xfffffff0UL;
  assert(!flovaOtaTimedOut(start + 100, start, start + 100, 1000));
  assert(flovaOtaTimedOut(start + 1100, start, start + 100, 1000));
  const uint32_t deadline = static_cast<uint32_t>(start + FLOVA_OTA_TOTAL_TIMEOUT_MS);
  assert(flovaOtaTimedOut(deadline, start, deadline, 1000));
  ticks = 0;
  uint16_t pin = 65535;
  uint8_t mode = 255;
  for (const char* reference : {"A0", "ADC0", "TOUT"}) {
    assert(flova::esp8266::resolvePin(reference, pin) && pin == A0);
    assert(flova::esp8266::validAnalogPin(pin));
    assert(!flova::esp8266::validDigitalPin(pin));
  }
  assert(flova::esp8266::resolvePin("GPIO0", pin) && pin == 0);
  assert(!flova::esp8266::validAnalogPin(pin));
  assert(flova::esp8266::inputMode(16, 2, mode) && mode == INPUT_PULLDOWN_16);
  assert(!flova::esp8266::inputMode(16, 1, mode));
  assert(!flova::esp8266::inputMode(5, 2, mode));
  assert(flova::esp32::resolvePin("ADC1_CH4", pin) && pin == 32);
  assert(flova::esp32::validAnalogPin(pin));
  assert(flova::esp32::resolvePin("ADC2_CH0", pin) && pin == 4);
  assert(!flova::esp32::validAnalogPin(pin)); // Wi-Fi owns ADC2.
  assert(!flova::esp32::resolvePin("A0", pin)); // No board-specific alias guessed.
  for (uint16_t invalid : {6, 11, 20, 24, 28, 31, 40, 255})
    assert(!flova::esp32::validInputPin(invalid));
  assert(!flova::esp32::validOutputPin(34));
  assert(flova::esp32::inputMode(34, 0, mode) && mode == INPUT);
  assert(!flova::esp32::inputMode(34, 1, mode));
  assert(flova::esp32::inputMode(32, 2, mode) && mode == INPUT_PULLDOWN);
  for (const char* invalid : {"GPIO", "GPIO-1", "GPIO65536", "GPIO1x", "UNKNOWN", "ADC1_CH8"})
    assert(!flova::esp32::resolvePin(invalid, pin));

  ArduinoFlovaHardware hardware(flova::esp8266::validDigitalPin,
      flova::esp8266::validDigitalPin, flova::esp8266::validAnalogPin,
      flova::esp8266::resolvePin, flova::esp8266::inputMode);
  flova::config::Unit unit = {};
  unit.kind = flova::config::UnitKind::Datastream;
  unit.data.datastream.hasMapping = true;
  unit.data.datastream.valueType = 2;
  unit.data.datastream.mapping.kind = flova::config::MappingKind::AnalogInput;
  strcpy(unit.data.datastream.mapping.pinReference, "TOUT");
  assert(hardware.resolve(unit) && unit.data.datastream.mapping.pin == A0);
  assert(hardware.validate(unit));
  unit.data.datastream.mapping.kind = flova::config::MappingKind::DigitalOutput;
  assert(!hardware.validate(unit));
  assert(!hardware.validateInputMode(16, 1));

  unit = {};
  unit.kind = flova::config::UnitKind::Datastream;
  assert(hardware.resolve(unit) && hardware.validate(unit));

  // A custom SDK application never resolves, rejects, or applies user-owned pins.
  ArduinoFlovaManualHardware manual;
  strcpy(unit.data.datastream.mapping.pinReference, "CUSTOM_PERIPHERAL");
  assert(!manual.capabilities().automaticMapping);
  assert(manual.resolve(unit) && manual.validate(unit) && manual.apply(unit));
}
