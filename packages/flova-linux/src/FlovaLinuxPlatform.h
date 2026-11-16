#pragma once

// Private POSIX/TLS transport shared by the Linux facade and its host tests.
#include <FlovaArduinoPlatform.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <thread>

namespace {

class LinuxPlatform final : public FlovaArduinoPlatform {
 public:
  LinuxPlatform() = default;
  LinuxPlatform(const LinuxPlatform&) = delete;
  LinuxPlatform& operator=(const LinuxPlatform&) = delete;
  ~LinuxPlatform() override { closeLink(); }

  bool connected() override { return tlsReady_; }
  int available() override { fillRx(); return static_cast<int>((rxTail_ + sizeof(rx_) - rxHead_) % sizeof(rx_)); }
  int read() override { return readByte(); }
  bool linkClosed() const override { return socket_ < 0; }
  bool beginLink() override {
    SSL_library_init();
    SSL_load_error_strings();
    return true;
  }

  bool startLink(const char* host, uint16_t port) override {
    closeLink();
    if (!host || !*host || !port) return false;
    strncpy(host_, host, sizeof(host_) - 1);
    // Keep one worker slot across disconnects. A stalled system lookup cannot
    // block the caller or accumulate more workers on reconnect.
    if (resolver_ && !resolver_->done.load()) return false;
    resolver_ = std::make_shared<Resolver>();
    const std::shared_ptr<Resolver> result = resolver_;
    memcpy(result->host, host_, sizeof(host_));
    resolverActive_ = true;
    std::thread([result, port]() {
      char service[8] = {};
      snprintf(service, sizeof(service), "%u", static_cast<unsigned>(port));
      struct addrinfo hints = {};
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_family = AF_UNSPEC;
      struct addrinfo* addresses = nullptr;
      if (getaddrinfo(result->host, service, &hints, &addresses) != 0 || !addresses) {
        result->failed = true;
      } else {
        memcpy(&result->address, addresses->ai_addr, addresses->ai_addrlen);
        result->addressLength = static_cast<socklen_t>(addresses->ai_addrlen);
        result->addressFamily = addresses->ai_family;
        result->addressType = addresses->ai_socktype;
        result->addressProtocol = addresses->ai_protocol;
        freeaddrinfo(addresses);
      }
      // Publish only after all result fields are ready. The captured ownership
      // also allows the transport to be destroyed before getaddrinfo returns.
      result->done.store(true);
    }).detach();
    return true;
  }

  FlovaLinkOpenStatus pollLink() override {
    if (!resolverActive_) return FlovaLinkOpenStatus::Failed;
    if (!resolver_->done.load()) return FlovaLinkOpenStatus::InProgress;
    if (resolver_->failed) return FlovaLinkOpenStatus::Failed;
    if (socket_ < 0 && resolver_->addressLength) {
      socket_ = socket(resolver_->addressFamily, resolver_->addressType,
                       resolver_->addressProtocol);
      if (socket_ < 0) return FlovaLinkOpenStatus::Failed;
      const int flags = fcntl(socket_, F_GETFL, 0);
      fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
      const int result = connect(socket_, reinterpret_cast<struct sockaddr*>(&resolver_->address),
                                  resolver_->addressLength);
      if (result != 0 && errno != EINPROGRESS) {
        closeLink();
        return FlovaLinkOpenStatus::Failed;
      }
      connecting_ = result != 0;
    }
    if (socket_ < 0) return FlovaLinkOpenStatus::Failed;
    if (connecting_) {
      int error = 0;
      socklen_t length = sizeof(error);
      if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &length) != 0)
        return FlovaLinkOpenStatus::InProgress;
      if (error == EINPROGRESS || error == EALREADY) return FlovaLinkOpenStatus::InProgress;
      if (error != 0) {
        closeLink();
        return FlovaLinkOpenStatus::Failed;
      }
      connecting_ = false;
    }
    if (!ssl_) {
      ctx_ = SSL_CTX_new(TLS_client_method());
      if (!ctx_) return FlovaLinkOpenStatus::Failed;
      SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
      SSL_CTX_set_default_verify_paths(ctx_);
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
      ssl_ = SSL_new(ctx_);
      if (!ssl_) return FlovaLinkOpenStatus::Failed;
      SSL_set_fd(ssl_, socket_);
      SSL_set_tlsext_host_name(ssl_, host_);
      SSL_set_verify(ssl_, SSL_VERIFY_PEER, nullptr);
      X509_VERIFY_PARAM* verify = SSL_get0_param(ssl_);
      if (!verify || X509_VERIFY_PARAM_set1_host(verify, host_, 0) != 1) {
        closeLink();
        return FlovaLinkOpenStatus::Failed;
      }
    }
    const int result = SSL_connect(ssl_);
    if (result == 1) {
      tlsReady_ = true;
      return FlovaLinkOpenStatus::Connected;
    }
    const int error = SSL_get_error(ssl_, result);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
      return FlovaLinkOpenStatus::InProgress;
    closeLink();
    return FlovaLinkOpenStatus::Failed;
  }

  void closeLink() override {
    resolverActive_ = false;
    if (ssl_) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (ctx_) {
      SSL_CTX_free(ctx_);
      ctx_ = nullptr;
    }
    if (socket_ >= 0) {
      close(socket_);
      socket_ = -1;
    }
    connecting_ = false;
    tlsReady_ = false;
    rxHead_ = rxTail_ = 0;
    txLength_ = txOffset_ = 0;
  }

  bool submitLinkWrite(const uint8_t* data, size_t length) override {
    if (!tlsReady_ || !data || !length || length > sizeof(tx_)) return false;
    if (txLength_ != txOffset_) return false;
    memcpy(tx_, data, length);
    txLength_ = length;
    txOffset_ = 0;
    return true;
  }

  bool serviceLinkWrite() override {
    if (!tlsReady_ || txOffset_ == txLength_) return true;
    const int result = SSL_write(ssl_, tx_ + txOffset_,
                                 static_cast<int>(txLength_ - txOffset_));
    if (result > 0) {
      txOffset_ += static_cast<size_t>(result);
      if (txOffset_ == txLength_) txLength_ = txOffset_ = 0;
      return true;
    }
    const int error = SSL_get_error(ssl_, result);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) return true;
    closeLink();
    return false;
  }

  bool linkWriteBusy() const override { return txLength_ != txOffset_; }
  flova::OtaInstallResult installOta(const FlovaLinkOtaOffer&) override {
    return flova::OtaInstallResult::ResourceUnavailable;
  }

  int readByte() {
    if (!available()) return -1;
    const uint8_t value = rx_[rxHead_];
    rxHead_ = (rxHead_ + 1) % sizeof(rx_);
    return value;
  }

 private:
  void fillRx() {
    if (!tlsReady_) return;
    while (true) {
      const size_t freeBytes = (rxHead_ + sizeof(rx_) - rxTail_ - 1) % sizeof(rx_);
      if (!freeBytes) return;
      const size_t contiguous = rxTail_ >= rxHead_ ? sizeof(rx_) - rxTail_ : rxHead_ - rxTail_ - 1;
      const size_t count = contiguous < freeBytes ? contiguous : freeBytes;
      const int result = SSL_read(ssl_, rx_ + rxTail_, static_cast<int>(count));
      if (result > 0) {
        rxTail_ = (rxTail_ + static_cast<size_t>(result)) % sizeof(rx_);
        continue;
      }
      const int error = SSL_get_error(ssl_, result);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) return;
      closeLink();
      return;
    }
  }

  int socket_ = -1;
  bool connecting_ = false;
  bool tlsReady_ = false;
  char host_[256] = {};
  struct Resolver {
    char host[256] = {};
    struct sockaddr_storage address = {};
    socklen_t addressLength = 0;
    int addressFamily = AF_UNSPEC;
    int addressType = SOCK_STREAM;
    int addressProtocol = 0;
    bool failed = false;
    std::atomic<bool> done{false};
  };
  std::shared_ptr<Resolver> resolver_;
  bool resolverActive_ = false;
  SSL_CTX* ctx_ = nullptr;
  SSL* ssl_ = nullptr;
  uint8_t rx_[16384] = {};
  size_t rxHead_ = 0;
  size_t rxTail_ = 0;
  uint8_t tx_[526] = {};
  size_t txLength_ = 0;
  size_t txOffset_ = 0;
};

}  // namespace
