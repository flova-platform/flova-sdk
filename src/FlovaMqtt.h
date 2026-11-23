#pragma once

#include <Arduino.h>
#include <FlovaArduinoPlatform.h>
#include <core_mqtt.h>
#include <stdio.h>
#include <string.h>

// Full coreMQTT client, not an MQTT codec/state-machine reimplementation.
// All MQTT storage is caller-owned. Transport callbacks never apply hardware
// work; a received frame is borrowed only for the duration of FrameHandler.
class FlovaMqtt final {
 public:
  static const size_t kFrameBytes = 512;
  static const size_t kPacketBytes = 640;
  static const size_t kInflight = 4;
  typedef bool (*FrameHandler)(void*, const uint8_t*, size_t);

  explicit FlovaMqtt(FlovaArduinoPlatform& platform) : platform_(platform) {}
  FlovaMqtt(const FlovaMqtt&) = delete;
  FlovaMqtt& operator=(const FlovaMqtt&) = delete;

  // TLS is already open and verified by the board. poll() completes the session.
  bool connect(const char* principal, const char* password, const char* epoch,
               FrameHandler handler, void* context) {
    reset();
    if (!principal || !password || !epoch || strlen(principal) > 66 ||
        strlen(password) > 128 || strlen(epoch) != 16) return false;
    const int up = snprintf(up_, sizeof(up_), "flova/v2/%s/%s/up", principal, epoch);
    const int down = snprintf(down_, sizeof(down_), "flova/v2/%s/%s/down", principal, epoch);
    const int will = snprintf(will_, sizeof(will_), "flova/v2/%s/%s/offline", principal, epoch);
    const int id = snprintf(clientId_, sizeof(clientId_), "%s:%s", principal, epoch);
    if (up < 0 || static_cast<size_t>(up) >= sizeof(up_) || down < 0 ||
        static_cast<size_t>(down) >= sizeof(down_) || will < 0 ||
        static_cast<size_t>(will) >= sizeof(will_) || id < 0 ||
        static_cast<size_t>(id) >= sizeof(clientId_)) return false;
    handler_ = handler;
    context_ = context;
    TransportInterface_t transport = {};
    transport.pNetworkContext = reinterpret_cast<NetworkContext_t*>(this);
    transport.send = send;
    transport.recv = receive;
    MQTTFixedBuffer_t buffer = {rx_, sizeof(rx_)};
    if (MQTT_Init(&mqtt_, &transport, clock, event, &buffer) != MQTTSuccess ||
        MQTT_InitStatefulQoS(&mqtt_, outgoing_, kInflight, incoming_, kInflight,
                            ackProperties_, sizeof(ackProperties_)) != MQTTSuccess)
      return false;
    MQTTConnectInfo_t info = {};
    info.cleanSession = true;
    info.pClientIdentifier = clientId_;
    info.clientIdentifierLength = static_cast<uint16_t>(strlen(clientId_));
    info.keepAliveSeconds = 45;
    info.pUserName = principal;
    info.userNameLength = static_cast<uint16_t>(strlen(principal));
    info.pPassword = password;
    info.passwordLength = static_cast<uint16_t>(strlen(password));
    MQTTPublishInfo_t willInfo = {};
    willInfo.qos = MQTTQoS1;
    willInfo.pTopicName = will_;
    willInfo.topicNameLength = static_cast<uint16_t>(strlen(will_));
    lastStatus_ = MQTT_FlovaStartCleanConnect(&mqtt_, &info, &willInfo);
    if (lastStatus_ != MQTTSuccess) return fail();
    awaitingConnack_ = true;
    openedAt_ = millis();
    return flush() || fail();
  }

  bool poll() {
    if (failed_ || !platform_.connected() || !flush()) return fail();
    if (awaitingConnack_) {
      if (millis() - openedAt_ >= 3000UL) {
        lastStatus_ = MQTTNoDataAvailable;
        return fail();
      }
      if (txLength_ || platform_.linkWriteBusy()) return true;
      return pollConnack();
    }
    if (!connected_) return fail();
    if (!subscribed_ && millis() - openedAt_ >= 10000UL) return fail();
    for (size_t i = 0; i < kInflight; ++i)
      if (packetIds_[i] && millis() - sentAt_[i] >= 15000UL) return fail();
    if (txLength_ || platform_.linkWriteBusy()) return true;
    const MQTTStatus_t result = MQTT_ProcessLoop(&mqtt_);
    if ((result != MQTTSuccess && result != MQTTNeedMoreBytes) || failed_ || !flush()) return fail();
    return true;
  }

  bool publish(const uint8_t* frame, size_t length) {
    if (!ready() || busy() || !frame || !length || length > kFrameBytes) return false;
    size_t slot = 0;
    const size_t limit = mqtt_.outgoingPublishRecordMaxCount;
    while (slot < limit && packetIds_[slot]) ++slot;
    if (slot == limit) return false;
    MQTTPublishInfo_t message = {};
    message.qos = MQTTQoS1;
    message.pTopicName = up_;
    message.topicNameLength = static_cast<uint16_t>(strlen(up_));
    message.pPayload = frame;
    message.payloadLength = length;
    const uint16_t id = MQTT_GetPacketId(&mqtt_);
    if (MQTT_Publish(&mqtt_, &message, id, nullptr) != MQTTSuccess) return fail();
    packetIds_[slot] = id;
    sentAt_[slot] = millis();
    return flush() || fail();
  }

  bool connected() const { return connected_ && !failed_; }
  MQTTStatus_t lastStatus() const { return lastStatus_; }
  bool credentialsRejected() const {
    return lastStatus_ == MQTTServerRefused &&
           (rx_[1] == MQTT_REASON_CONNACK_BAD_USER_NAME_OR_PASSWORD ||
            rx_[1] == MQTT_REASON_CONNACK_NOT_AUTHORIZED);
  }
  bool ready() const { return connected() && subscribed_; }
  bool busy() const {
    if (txLength_ || platform_.linkWriteBusy()) return true;
    for (size_t i = 0; i < mqtt_.outgoingPublishRecordMaxCount; ++i) if (!packetIds_[i]) return false;
    return true;
  }
  void reset() {
    lastStatus_ = MQTTSuccess;
    connected_ = subscribed_ = failed_ = awaitingConnack_ = false;
    connackTypeSeen_ = connackLengthComplete_ = false;
    connackLengthBytes_ = 0;
    connackLength_ = connackRead_ = 0;
    txLength_ = txOffset_ = 0;
    memset(packetIds_, 0, sizeof(packetIds_));
    memset(outgoing_, 0, sizeof(outgoing_));
    memset(incoming_, 0, sizeof(incoming_));
  }

 private:
  static uint32_t clock() { return millis(); }
  bool fail() { failed_ = true; return false; }

  bool badConnack() {
    lastStatus_ = MQTTBadResponse;
    return fail();
  }

  bool pollConnack() {
    // Only the fixed header is assembled here. coreMQTT validates the body
    // once it is complete. Leave the following packet for its normal loop.
    for (size_t i = 0; i < 64 && platform_.available() > 0; ++i) {
      const int byte = platform_.read();
      if (byte < 0) {
        lastStatus_ = MQTTRecvFailed;
        return fail();
      }
      if (!connackTypeSeen_) {
        if (byte != MQTT_PACKET_TYPE_CONNACK) return badConnack();
        connackTypeSeen_ = true;
      } else if (!connackLengthComplete_) {
        const size_t part = static_cast<size_t>(byte & 0x7f);
        if (connackLengthBytes_ >= 2) return badConnack();
        connackLength_ += part << (7 * connackLengthBytes_);
        if (connackLength_ >= sizeof(rx_)) return badConnack();
        if (byte & 0x80) {
          ++connackLengthBytes_;
          continue;
        }
        if ((!part && connackLengthBytes_) || connackLength_ < 3 ||
            connackLength_ + connackLengthBytes_ + 2 > sizeof(rx_)) return badConnack();
        connackLengthComplete_ = true;
      } else {
        rx_[connackRead_++] = static_cast<uint8_t>(byte);
        if (connackRead_ == connackLength_) return subscribe();
      }
    }
    return true;
  }

  bool subscribe() {
    MQTTPacketInfo_t packet = {};
    packet.type = MQTT_PACKET_TYPE_CONNACK;
    packet.remainingLength = connackLength_;
    packet.pRemainingData = rx_;
    lastStatus_ = MQTT_FlovaFinishCleanConnect(&mqtt_, &packet);
    if (lastStatus_ != MQTTSuccess) return fail();
    mqtt_.lastPacketRxTime = millis();

    MQTTSubscribeInfo_t subscription = {};
    subscription.qos = MQTTQoS1;
    subscription.pTopicFilter = down_;
    subscription.topicFilterLength = static_cast<uint16_t>(strlen(down_));
    subscriptionId_ = MQTT_GetPacketId(&mqtt_);
    lastStatus_ = MQTT_Subscribe(&mqtt_, &subscription, 1, subscriptionId_, nullptr);
    if (lastStatus_ != MQTTSuccess || !flush()) return fail();
    awaitingConnack_ = false;
    connected_ = true;
    openedAt_ = millis();
    return true;
  }

  // Platform submissions copy their input. Keep unsent bytes in owned storage,
  // splitting at 512 bytes so existing board TX budgets need not grow.
  bool flush() {
    if (!platform_.serviceLinkWrite()) return false;
    if (!txLength_ || platform_.linkWriteBusy()) return true;
    const size_t left = txLength_ - txOffset_;
    const size_t count = left > 512 ? 512 : left;
    if (!platform_.submitLinkWrite(tx_ + txOffset_, count)) return false;
    txOffset_ += count;
    if (txOffset_ == txLength_) txLength_ = txOffset_ = 0;
    return platform_.serviceLinkWrite();
  }

  static int32_t send(NetworkContext_t* network, const void* bytes, size_t length) {
    FlovaMqtt& self = *reinterpret_cast<FlovaMqtt*>(network);
    if (length > sizeof(self.tx_) - self.txLength_) return -1;
    memcpy(self.tx_ + self.txLength_, bytes, length);
    self.txLength_ += length;
    return static_cast<int32_t>(length);
  }

  static int32_t receive(NetworkContext_t* network, void* bytes, size_t length) {
    FlovaMqtt& self = *reinterpret_cast<FlovaMqtt*>(network);
    if (!self.flush() || !self.platform_.connected()) return -1;
    // Bound bytes and therefore complete MQTT packets processed per SDK tick.
    const size_t limit = length > 64 ? 64 : length;
    uint8_t* output = static_cast<uint8_t*>(bytes);
    size_t count = 0;
    while (count < limit && self.platform_.available() > 0) {
      const int byte = self.platform_.read();
      if (byte < 0) return -1;
      output[count++] = static_cast<uint8_t>(byte);
    }
    return static_cast<int32_t>(count);
  }

  static bool event(MQTTContext_t* mqtt, MQTTPacketInfo_t* packet,
                    MQTTDeserializedInfo_t* decoded, MQTTSuccessFailReasonCode_t*,
                    MQTTPropBuilder_t*, MQTTPropBuilder_t*) {
    FlovaMqtt& self = *reinterpret_cast<FlovaMqtt*>(mqtt->transportInterface.pNetworkContext);
    if ((packet->type & 0xf0U) == MQTT_PACKET_TYPE_PUBLISH) {
      const MQTTPublishInfo_t* message = decoded->pPublishInfo;
      if (!message || message->retain || message->topicNameLength != strlen(self.down_) ||
          memcmp(message->pTopicName, self.down_, message->topicNameLength) ||
          !message->payloadLength || message->payloadLength > kFrameBytes || !self.handler_ ||
          !self.handler_(self.context_, static_cast<const uint8_t*>(message->pPayload), message->payloadLength))
        return self.fail();
    } else if (packet->type == MQTT_PACKET_TYPE_SUBACK) {
      uint8_t* codes = nullptr;
      size_t count = 0;
      if (decoded->packetIdentifier != self.subscriptionId_ ||
          MQTT_GetSubAckStatusCodes(packet, &codes, &count) != MQTTSuccess || count != 1 || codes[0] != 1)
        return self.fail();
      self.subscribed_ = true;
    } else if (packet->type == MQTT_PACKET_TYPE_PUBACK) {
      if (decoded->pReasonCode && decoded->pReasonCode->reasonCodeLength &&
          decoded->pReasonCode->reasonCode[0] >= 0x80U) return self.fail();
      for (size_t i = 0; i < kInflight; ++i)
        if (self.packetIds_[i] == decoded->packetIdentifier) self.packetIds_[i] = 0;
    }
    return true;
  }

  FlovaArduinoPlatform& platform_;
  MQTTContext_t mqtt_ = {};
  MQTTStatus_t lastStatus_ = MQTTSuccess;
  MQTTPubAckInfo_t outgoing_[kInflight] = {};
  MQTTPubAckInfo_t incoming_[kInflight] = {};
  uint8_t ackProperties_[8] = {};
  uint8_t rx_[kPacketBytes] = {};
  uint8_t tx_[kPacketBytes] = {};
  size_t txLength_ = 0, txOffset_ = 0;
  uint16_t packetIds_[kInflight] = {};
  uint32_t sentAt_[kInflight] = {};
  uint16_t subscriptionId_ = 0;
  uint32_t openedAt_ = 0;
  size_t connackLength_ = 0, connackRead_ = 0;
  uint8_t connackLengthBytes_ = 0;
  char up_[112] = {}, down_[112] = {}, will_[112] = {}, clientId_[85] = {};
  FrameHandler handler_ = nullptr;
  void* context_ = nullptr;
  bool connected_ = false, subscribed_ = false, failed_ = false, awaitingConnack_ = false;
  bool connackTypeSeen_ = false, connackLengthComplete_ = false;
};
