#include <adapters/ArduinoFlovaLink.h>
#include <assert.h>
#include <deque>
#include <string>
#include <vector>

HardwareSerial Serial;
static uint32_t nowMs;
uint32_t millis() { return nowMs; }
unsigned long micros() { return nowMs * 1000UL; }
void delay(unsigned long ms) { nowMs += ms; }
class Entropy final : public FlovaEntropySource {
 public: uint8_t byte() override { return next++; }
 private: uint8_t next = 1;
};

// Scripted broker fixtures. MQTT payloads are decoded using the upstream
// library; CBOR application frames always use the production SDK codec.
class Broker final : public FlovaArduinoPlatform {
 public:
  bool open = true, failOpen = false, failWrite = false;
  bool slowAcks = false;
  unsigned writeTicks = 0;
  uint16_t nextDownlinkId = 22;
  std::deque<uint8_t> incoming;
  std::vector<uint8_t> wire;
  std::vector<std::vector<uint8_t> > frames;
  std::string down;
  bool connected() override { return open; }
  int available() override { return incoming.size(); }
  int read() override {
    if (incoming.empty()) return -1;
    const int value = incoming.front(); incoming.pop_front(); return value;
  }
  bool linkClosed() const override { return !open; }
  bool startLink(const char*, uint16_t) override { open = true; return true; }
  FlovaLinkOpenStatus pollLink() override {
    return failOpen ? FlovaLinkOpenStatus::Failed : FlovaLinkOpenStatus::Connected;
  }
  const char* linkError() const override { return "insufficient_tls_heap"; }
  void closeLink() override { open = false; }
  bool linkWriteBusy() const override { return writeTicks != 0; }
  bool serviceLinkWrite() override { if (writeTicks) --writeTicks; return !failWrite; }
  bool submitLinkWrite(const uint8_t* bytes, size_t length) override {
    if (failWrite) return false;
    assert(length <= 512);
    assert(!writeTicks);
    if (slowAcks && bytes[0] == 0x40) writeTicks = 8;
    wire.insert(wire.end(), bytes, bytes + length);
    serviceBroker();
    return true;
  }
  flova::OtaInstallResult installOta(const FlovaLinkOtaOffer&) override {
    return flova::OtaInstallResult::DownloadFailed;
  }
  void feed(std::initializer_list<uint8_t> bytes) { incoming.insert(incoming.end(), bytes); }
  void serviceBroker() {
    while (wire.size() >= 2) {
      size_t length = 0, multiplier = 1, offset = 1;
      uint8_t byte;
      do {
        if (offset == wire.size()) return;
        byte = wire[offset++]; length += (byte & 127) * multiplier; multiplier *= 128;
        assert(offset <= 5);
      } while (byte & 128);
      if (length > wire.size() - offset) return;
      uint8_t* body = wire.data() + offset;
      if (wire[0] == 0x10) feed({0x20, 3, 0, 0, 0});
      else if (wire[0] == 0x82) {
        assert(length >= 6 && body[2] == 0);
        const size_t topicLength = body[3] * 256 + body[4];
        assert(topicLength + 6 == length);
        down.assign(reinterpret_cast<char*>(body + 5), topicLength);
        feed({0x90, 4, body[0], body[1], 0, 1});
      } else if ((wire[0] & 0xf0) == 0x30) {
        MQTTPacketInfo_t packet = {};
        packet.type = wire[0]; packet.remainingLength = length; packet.pRemainingData = body;
        MQTTPublishInfo_t message = {}; MQTTPropBuilder_t properties = {}; uint16_t id = 0;
        assert(MQTT_DeserializePublish(&packet, &id, &message, &properties, 640, 0) == MQTTSuccess);
        const uint8_t* frame = static_cast<const uint8_t*>(message.pPayload);
        frames.push_back(std::vector<uint8_t>(frame, frame + message.payloadLength));
        feed({0x40, 4, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id), 0, 0});
      } else if (wire[0] == 0xc0) feed({0xd0, 0});
      wire.erase(wire.begin(), wire.begin() + offset + length);
    }
  }
  template <typename T, typename Encoder>
  void sendFrame(uint8_t type, uint64_t id, const T& value, Encoder encode) {
    uint8_t frame[512] = {}; size_t length = 0;
    assert(encode(frame + 12, 500, &value, &length) == 0);
    assert(flova::link::encodeFrameHeader(frame, sizeof(frame), type, 0, id, length));
    length += 12;
    const size_t remaining = 2 + down.size() + 2 + 1 + length;
    incoming.push_back(0x32);
    size_t left = remaining;
    do { const uint8_t byte = left % 128; left /= 128; incoming.push_back(left ? byte | 128 : byte); } while (left);
    incoming.push_back(down.size() >> 8); incoming.push_back(down.size());
    incoming.insert(incoming.end(), down.begin(), down.end());
    incoming.push_back(nextDownlinkId >> 8); incoming.push_back(nextDownlinkId);
    ++nextDownlinkId;
    incoming.push_back(0);
    incoming.insert(incoming.end(), frame, frame + length);
  }
  flova::link::FrameView lastFrame(uint8_t type) {
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
      flova::link::FrameView frame = {};
      assert(flova::link::decodeWebSocketBinaryMessage(it->data(), it->size(), frame) == flova::link::FrameResult::Complete);
      if (frame.messageType == type) return frame;
    }
    assert(false); return flova::link::FrameView();
  }
};

static void tick(ArduinoDeviceLink& link) { for (unsigned i = 0; i < 32; ++i) link.loop(); }
static unsigned commands, bindings;
static void received(const FlovaLinkInboundMessage& message) {
  if (message.type == FlovaLinkMessageType::Command) ++commands;
  if (message.type == FlovaLinkMessageType::DatastreamBound) {
    assert(message.body.datastreamBound.count == 64);
    for (size_t i = 0; i < 64; ++i) assert(message.body.datastreamBound.ids[i] == i + 1);
    ++bindings;
  }
}
static void authenticate(Broker& broker, ArduinoDeviceLink& link) {
  assert(link.configure("mqtts://mqtt.example:8883"));
  assert(link.connect("00112233-4455-6677-8899-aabbccddeeff", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  tick(link);
  assert(broker.lastFrame(1).messageId == 0);
  struct auth_ok auth = {1700000000000ULL, 30000, 1};
  broker.sendFrame(2, 0, auth, cbor_encode_auth_ok);
  tick(link);
}

int main() {
  Entropy entropy;
  for (uint32_t generation = 6; generation <= 8; ++generation) {
    Broker broker; ArduinoDeviceLink link(broker, entropy);
    link.setConfigurationGeneration(7); link.setCallback(received); commands = 0;
    authenticate(broker, link); assert(link.connected());
    const uint8_t id[16] = {1}; struct command value = {};
    value.command_generation = generation; value.command_id = {id, sizeof(id)};
    value.command_compact_id = 1;
    value.command_typed_value_fields_m.typed_value_fields_choice = typed_value_fields_r::typed_value_fields_value_bool_type_l_c;
    value.command_typed_value_fields_m.typed_value_fields_value_bool_type_l_value_bool = true;
    value.command_correlation_id.correlation_id_choice = correlation_id_r::correlation_id_empty_id_m_c;
    value.command_correlation_id.correlation_id_empty_id_m = {id, 0};
    broker.sendFrame(0x20, 42, value, cbor_encode_command); tick(link);
    assert(commands == (generation == 7 ? 1U : 0U));
    assert(link.connected() == (generation == 7));
  }
  for (unsigned failure = 0; failure < 5; ++failure) {
    Broker broker; ArduinoDeviceLink link(broker, entropy);
    char keys[64][49] = {}; const char* names[64] = {};
    for (unsigned i = 0; i < 64; ++i) {
      memset(keys[i], 'x', 48); keys[i][0] = 'A' + i / 26; keys[i][1] = 'a' + i % 26; names[i] = keys[i];
    }
    assert(link.setDatastreamKeys(names, 64));
    link.setConfigurationGeneration(7); link.setCallback(received); bindings = 0;
    authenticate(broker, link);
    size_t offset = 0;
    while (offset < 64) {
      const auto frame = broker.lastFrame(9); struct datastream_bind request = {}; size_t consumed = 0;
      assert(cbor_decode_datastream_bind(frame.payload, frame.payloadLength, &request, &consumed) == 0);
      const size_t count = request.datastream_bind_binding_keys.datastream_binding_keys_tstr1_48_count;
      assert(count == (64 - offset > 9 ? 9 : 64 - offset));
      for (size_t i = 0; i < count; ++i) {
        const auto& key = request.datastream_bind_binding_keys.datastream_binding_keys_tstr1_48[i];
        assert(key.len == 48 && !memcmp(key.value, names[offset + i], 48));
      }
      if (offset && failure == 4) { nowMs += 15001; link.loop(); assert(broker.linkClosed()); break; }
      struct datastream_bound reply = {};
      reply.datastream_bound_bound_generation = offset && failure == 2 ? 8 : 7;
      reply.datastream_bound_bound_ids.datastream_bound_ids_compact_id_m_count = count;
      for (size_t i = 0; i < count; ++i) reply.datastream_bound_bound_ids.datastream_bound_ids_compact_id_m[i] = offset + i + 1;
      if (offset && failure == 3) reply.datastream_bound_bound_ids.datastream_bound_ids_compact_id_m[0] = 1;
      broker.sendFrame(10, frame.messageId + (offset && failure == 1 ? 1 : 0), reply, cbor_encode_datastream_bound);
      tick(link);
      if (offset && failure) { assert(broker.linkClosed()); break; }
      offset += count;
    }
    assert(bindings == (failure ? 0U : 1U)); assert(link.connected() == !failure);
  }
  {
    Broker broker; ArduinoDeviceLink link(broker, entropy);
    link.setConfigurationGeneration(7); link.setCallback(received); commands = 0;
    authenticate(broker, link);
    const uint8_t id[16] = {1}; struct command value = {};
    value.command_generation = 7; value.command_id = {id, sizeof(id)};
    value.command_compact_id = 1;
    value.command_typed_value_fields_m.typed_value_fields_choice = typed_value_fields_r::typed_value_fields_value_bool_type_l_c;
    value.command_correlation_id.correlation_id_choice = correlation_id_r::correlation_id_empty_id_m_c;
    value.command_correlation_id.correlation_id_empty_id_m = {id, 0};
    broker.slowAcks = true;
    for (unsigned i = 0; i < 3; ++i) broker.sendFrame(0x20, 42 + i, value, cbor_encode_command);
    tick(link); tick(link);
    assert(link.connected() && commands == 3);
  }
  {
    Broker broker; ArduinoDeviceLink link(broker, entropy);
    assert(!link.configure("wss://engine.example/api/device-link"));
    assert(!link.configure("mqtt://mqtt.example:1883"));
    assert(!link.configure("mqtts://mqtt.example:8883/arbitrary"));
    assert(link.configure("mqtts://mqtt.example:8883"));
    assert(link.connectBootstrap("ttttttttttttttttttttttttttttttttttttttttttt", "esp8266-test",
                                 "universal_esp8266", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
    tick(link); assert(link.connected()); assert(broker.lastFrame(6).messageId == 0);
    FlovaLinkConfigurationReport report = {}; report.messageId = 17; report.generation = 2;
    report.status = FlovaLinkResultStatus::Ok;
    assert(link.publishConfigurationReport(report)); tick(link);
    assert(broker.lastFrame(0x18).messageId == 17);
    link.beginDrain(); tick(link); assert(link.drainComplete() && !link.drainFailed());
  }
  {
    Broker broker; ArduinoFlovaLink link(broker, entropy);
    FlovaLinkConfigurationRecord record = {}, output = {};
    record.generation = 1; assert(link.queueConfigurationRecord(record));
    record.generation = 2; assert(link.queueConfigurationRecord(record));
    record.generation = 3; assert(!link.queueConfigurationRecord(record));
    assert(link.takeConfigurationRecord(output) && output.generation == 1);
    assert(link.queueConfigurationRecord(record));
    assert(link.takeConfigurationRecord(output) && output.generation == 2);
    assert(link.takeConfigurationRecord(output) && output.generation == 3);
  }
}
