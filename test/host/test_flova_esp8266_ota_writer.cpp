#include <assert.h>
#include <stdint.h>
#include <initializer_list>
#include <FlovaEsp8266OtaWriter.h>

// Models the ESP8266 core: end(false) activates any fully written image.
struct Updater {
  size_t size;
  size_t written = 0;
  bool activated = false;
  bool failWrite = false;
  bool running = true;
  explicit Updater(size_t bytes) : size(bytes) {}
  size_t write(uint8_t*, size_t length) {
    if (failWrite || !running) return 0;
    written += length;
    return length;
  }
  bool end(bool) {
    activated = running && written == size;
    running = false;
    return activated;
  }
};

int main() {
  uint8_t data[512] = {};
  for (size_t size : {size_t(1), size_t(511), size_t(512), size_t(513), size_t(1024)}) {
    for (bool verified : {false, true}) {
      Updater updater(size);
      flova::Esp8266OtaWriter<Updater> writer(updater, size);
      for (size_t remaining = size; remaining;) {
        const size_t count = remaining < sizeof(data) ? remaining : sizeof(data);
        assert(writer.write(data, count));
        remaining -= count;
      }
      assert(updater.written == size - 1);
      assert(writer.finish(verified) == verified);
      assert(updater.activated == verified);
    }
  }
  Updater partial(512);
  flova::Esp8266OtaWriter<Updater> partialWriter(partial, 512);
  assert(partialWriter.write(data, 128));
  partialWriter.abort();
  assert(!partial.activated && !partial.running);

  Updater failed(512);
  flova::Esp8266OtaWriter<Updater> failedWriter(failed, 512);
  failed.failWrite = true;
  assert(!failedWriter.write(data, 512));
  assert(!failedWriter.finish(true));
  assert(!failed.activated);

  Updater finalFailure(1);
  flova::Esp8266OtaWriter<Updater> finalWriter(finalFailure, 1);
  assert(finalWriter.write(data, 1));
  finalFailure.failWrite = true;
  assert(!finalWriter.finish(true));
  assert(!finalFailure.activated);
}
