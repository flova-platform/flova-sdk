// Exercise the private transport without adding an application-facing test API.
#include "../../packages/flova-linux/src/FlovaLinuxPlatform.h"

#include <assert.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::mutex resolverMutex;
std::condition_variable resolverCondition;
bool resolverBlocked = false;
bool resolverEntered = false;
bool resolverReturned = false;
std::string resolvedHost;
std::vector<uint8_t> received;
size_t receivedOffset = 0;

void blockResolver() {
  std::lock_guard<std::mutex> lock(resolverMutex);
  resolverBlocked = true;
  resolverEntered = resolverReturned = false;
}

void waitForResolver(bool& flag) {
  std::unique_lock<std::mutex> lock(resolverMutex);
  const bool completed = resolverCondition.wait_for(
      lock, std::chrono::seconds(2), [&flag]() { return flag; });
  assert(completed);
}

void releaseResolver() {
  std::lock_guard<std::mutex> lock(resolverMutex);
  resolverBlocked = false;
  resolverCondition.notify_all();
}

template <typename Predicate>
void waitUntil(Predicate ready) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!ready()) {
    assert(std::chrono::steady_clock::now() < deadline);
    std::this_thread::yield();
  }
}

void connectPlatform(LinuxPlatform& platform) {
  assert(platform.startLink("test.invalid", 8883));
  waitUntil([&]() {
    const auto status = platform.pollLink();
    assert(status != FlovaLinkOpenStatus::Failed);
    return status == FlovaLinkOpenStatus::Connected;
  });
}
}  // namespace

extern "C" int __real_getaddrinfo(const char*, const char*, const struct addrinfo*,
                                   struct addrinfo**);
extern "C" int __wrap_getaddrinfo(const char* host, const char* service,
                                   const struct addrinfo* hints,
                                   struct addrinfo** output) {
  {
    std::unique_lock<std::mutex> lock(resolverMutex);
    resolverEntered = true;
    resolverCondition.notify_all();
    resolverCondition.wait(lock, []() { return !resolverBlocked; });
    resolvedHost = host;
  }
  const int result = !strcmp(host, "missing.invalid") ? EAI_NONAME
      : __real_getaddrinfo("127.0.0.1", service, hints, output);
  {
    std::lock_guard<std::mutex> lock(resolverMutex);
    resolverReturned = true;
    resolverCondition.notify_all();
  }
  return result;
}

// TLS and socket I/O are controlled; DNS worker and ring-buffer code are real.
extern "C" int __wrap_connect(int, const struct sockaddr*, socklen_t) { return 0; }
extern "C" int __wrap_SSL_connect(SSL*) { return 1; }
extern "C" int __wrap_SSL_shutdown(SSL*) { return 1; }
extern "C" int __wrap_SSL_get_error(const SSL*, int) { return SSL_ERROR_WANT_READ; }
extern "C" int __wrap_SSL_read(SSL*, void* output, int capacity) {
  if (receivedOffset == received.size()) return -1;
  const size_t count = std::min(static_cast<size_t>(capacity),
                                received.size() - receivedOffset);
  memcpy(output, received.data() + receivedOffset, count);
  receivedOffset += count;
  return static_cast<int>(count);
}

int main(int argc, char** argv) {
  if (argc == 1 || !strcmp(argv[1], "receive")) {
    LinuxPlatform platform;
    connectPlatform(platform);
    // Exact capacity previously wrapped both indexes to zero and hid all bytes.
    received.resize(16384);
    for (size_t i = 0; i < received.size(); ++i)
      received[i] = static_cast<uint8_t>(i);
    assert(platform.available() == 16383);
    for (size_t i = 0; i < received.size(); ++i)
      assert(platform.read() == static_cast<uint8_t>(i));
    assert(platform.available() == 0);

    // Refill across multiple wraps, retaining the reserved byte and FIFO order.
    receivedOffset = 0;
    received.resize(2 * 16384 + 17);
    for (size_t i = 0; i < received.size(); ++i)
      received[i] = static_cast<uint8_t>(i + 7);
    for (size_t i = 0; i < received.size(); ++i)
      assert(platform.read() == static_cast<uint8_t>(i + 7));
    assert(platform.read() == -1);
  }
  if (argc == 1 || !strcmp(argv[1], "resolver")) {
    LinuxPlatform platform;
    blockResolver();
    assert(platform.startLink("old.invalid", 8883));
    waitForResolver(resolverEntered);
    assert(platform.pollLink() == FlovaLinkOpenStatus::InProgress);
    platform.closeLink(); // Must return while getaddrinfo is still blocked.
    assert(platform.pollLink() == FlovaLinkOpenStatus::Failed);
    // Repeated reconnects cannot spawn more workers while the slot is occupied.
    for (int i = 0; i < 100; ++i)
      assert(!platform.startLink("new.invalid", 8883));
    releaseResolver();
    waitForResolver(resolverReturned);
    assert(resolvedHost == "old.invalid");
    assert(platform.pollLink() == FlovaLinkOpenStatus::Failed);
    waitUntil([&]() { return platform.startLink("new.invalid", 8883); });
    waitUntil([&]() {
      const auto status = platform.pollLink();
      assert(status != FlovaLinkOpenStatus::Failed);
      return status == FlovaLinkOpenStatus::Connected;
    });
    assert(resolvedHost == "new.invalid");
    assert(platform.startLink("missing.invalid", 8883));
    waitUntil([&]() {
      return platform.pollLink() == FlovaLinkOpenStatus::Failed;
    });
    assert(!platform.connected());
    connectPlatform(platform); // A failed lookup also releases the worker slot.
  }
  if (argc == 1 || !strcmp(argv[1], "resolver")) {
    blockResolver();
    LinuxPlatform* platform = new LinuxPlatform;
    assert(platform->startLink("destroyed.invalid", 8883));
    waitForResolver(resolverEntered);
    delete platform; // Worker must own its data after transport destruction.
    releaseResolver();
    waitForResolver(resolverReturned);
  }
}
