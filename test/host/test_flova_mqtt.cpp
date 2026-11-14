#include <FlovaMqtt.h>
#include <assert.h>
#include <algorithm>
#include <deque>
#include <vector>
#include <string>

HardwareSerial Serial;
static uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
unsigned long micros() { return nowMs * 1000UL; }
void delay(unsigned long milliseconds) { nowMs += milliseconds; }

class Platform final : public FlovaArduinoPlatform {
 public:
  bool open = true, blocked = false, failWrite = false, answerConnect = true, oneInflight = false;
  std::deque<uint8_t> incoming;
  std::vector<std::vector<uint8_t> > writes;
  bool connected() override { return open; }
  int available() override { return static_cast<int>(incoming.size()); }
  int read() override {
    if (incoming.empty()) return -1;
    const int byte = incoming.front(); incoming.pop_front(); return byte;
  }
  bool linkClosed() const override { return !open; }
  bool startLink(const char*, uint16_t) override { open = true; return true; }
  FlovaLinkOpenStatus pollLink() override { return FlovaLinkOpenStatus::Connected; }
  void closeLink() override { open = false; }
  bool linkWriteBusy() const override { return blocked; }
  bool submitLinkWrite(const uint8_t* bytes, size_t length) override {
    assert(length <= 512);
    if (blocked || failWrite) return false;
    writes.push_back(std::vector<uint8_t>(bytes, bytes + length));
    if (bytes[0] == 0x10 && answerConnect) {
      if (oneInflight) push({0x20, 6, 0, 0, 3, 0x21, 0, 1});
      else push({0x20, 3, 0, 0, 0});
    }
    if (bytes[0] == 0x82) push({0x90, 4, 0, 1, 0, 1});
    return true;
  }
  bool serviceLinkWrite() override { return !failWrite; }
  flova::OtaInstallResult installOta(const FlovaLinkOtaOffer&) override {
    return flova::OtaInstallResult();
  }
  void push(std::initializer_list<uint8_t> bytes) { incoming.insert(incoming.end(), bytes); }
};

struct Received {
  size_t count = 0;
  std::vector<uint8_t> frame;
  bool accept = true;
  static bool onFrame(void* context, const uint8_t* bytes, size_t length) {
    Received& self = *static_cast<Received*>(context);
    ++self.count;
    self.frame.assign(bytes, bytes + length);
    return self.accept;
  }
};

static const char* principal = "d-a28cbe21-fd34-4cf6-8f28-8264e90192c7";
static const char* epoch = "0123456789abcdef";

static void connect(FlovaMqtt& mqtt, Received& received) {
  const bool ok = mqtt.connect(principal, "test-password", epoch, Received::onFrame, &received);
  if (!ok) fprintf(stderr, "MQTT connect status=%d\n", static_cast<int>(mqtt.lastStatus()));
  assert(ok);
  assert(!mqtt.ready());
  assert(mqtt.poll());
  assert(mqtt.ready());
}

// Wire fixtures, not a second production MQTT implementation.
static std::vector<uint8_t> publication(const std::string& topic,
                                      const std::vector<uint8_t>& payload,
                                      bool retained = false) {
  const size_t remaining = 2 + topic.size() + 2 + 1 + payload.size();
  std::vector<uint8_t> wire;
  wire.push_back(retained ? 0x33 : 0x32);
  size_t length = remaining;
  do {
    uint8_t byte = static_cast<uint8_t>(length % 128);
    length /= 128;
    wire.push_back(length ? byte | 0x80 : byte);
  } while (length);
  wire.push_back(static_cast<uint8_t>(topic.size() >> 8));
  wire.push_back(static_cast<uint8_t>(topic.size()));
  wire.insert(wire.end(), topic.begin(), topic.end());
  wire.push_back(0); wire.push_back(22); wire.push_back(0);
  wire.insert(wire.end(), payload.begin(), payload.end());
  return wire;
}

int main() {
  const std::string down = std::string("flova/v2/") + principal + "/" + epoch + "/down";
  {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    connect(mqtt, received);
    const std::vector<uint8_t> binary = {0, 0xff, 0x80, 0, 42};
    const std::vector<uint8_t> wire = publication(down, binary);
    // The actual library must retain arbitrarily fragmented headers/bodies.
    for (uint8_t byte : wire) {
      platform.incoming.push_back(byte);
      assert(mqtt.poll());
    }
    assert(received.count == 1 && received.frame == binary);
    assert(mqtt.publish(binary.data(), binary.size()));
    assert(!mqtt.publish(binary.data(), 513));
    const uint8_t ack[] = {0x40, 4, 0, 2, 0, 0};
    platform.incoming.insert(platform.incoming.end(), ack, ack + sizeof(ack));
    assert(mqtt.poll());
  }
  {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    connect(mqtt, received);
    const std::vector<uint8_t> payload(512, 0xa5);
    assert(mqtt.publish(payload.data(), payload.size()));
    assert(mqtt.busy()); // Entire MQTT packet exceeds the board's 512-byte submission.
    assert(mqtt.poll());
    assert(!mqtt.busy());
    const std::vector<uint8_t> wire = publication(down, payload);
    platform.incoming.insert(platform.incoming.end(), wire.begin(), wire.end());
    for (int i = 0; i < 16; ++i) assert(mqtt.poll());
    assert(received.count == 1 && received.frame == payload);
    nowMs += 15001;
    assert(!mqtt.poll()); // Missing PUBACK cannot accumulate unbounded work.
  }
  for (int invalid = 0; invalid < 4; ++invalid) {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    connect(mqtt, received);
    if (invalid == 3) received.accept = false;
    const std::vector<uint8_t> wire = publication(invalid == 0 ? down + "/other" : down,
      std::vector<uint8_t>(invalid == 1 ? 513 : 12, 0), invalid == 2);
    platform.incoming.insert(platform.incoming.end(), wire.begin(), wire.end());
    bool ok = true;
    for (int i = 0; i < 16 && ok; ++i) ok = mqtt.poll();
    assert(!ok && !mqtt.connected());
  }
  {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    connect(mqtt, received);
    platform.blocked = true;
    const uint8_t byte = 0;
    assert(mqtt.poll());
    assert(!mqtt.publish(&byte, 1));
    platform.blocked = false;
    platform.failWrite = true;
    assert(!mqtt.poll());
  }
  {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    platform.oneInflight = true;
    connect(mqtt, received);
    const uint8_t byte = 0;
    assert(mqtt.publish(&byte, 1));
    assert(mqtt.busy());
    assert(!mqtt.publish(&byte, 1));
    assert(mqtt.ready()); // Negotiated backpressure is not a transport error.
    platform.push({0x40, 4, 0, 2, 0, 0});
    assert(mqtt.poll());
    assert(!mqtt.busy());
    assert(mqtt.publish(&byte, 1));
  }
  {
    Platform platform; Received received; FlovaMqtt mqtt(platform);
    platform.answerConnect = false;
    assert(!mqtt.connect(principal, "password", epoch, Received::onFrame, &received));
  }
}
