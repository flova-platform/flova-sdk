#include <FlovaLinux.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>

static_assert(!std::is_copy_constructible<FlovaLinux>::value, "Linux facade owns its runtime");
static_assert(!std::is_copy_assignable<FlovaLinux>::value, "Linux facade owns its runtime");

int main() {
  const char* temporaryRoot = getenv("TMPDIR");
  if (!temporaryRoot || !temporaryRoot[0]) temporaryRoot = "./tmp";
  assert(mkdir(temporaryRoot, 0700) == 0 || errno == EEXIST);
  char directory[512] = {};
  const int written = snprintf(directory, sizeof(directory), "%s/flova-linux-test-XXXXXX", temporaryRoot);
  assert(written > 0 && static_cast<size_t>(written) < sizeof(directory));
  assert(mkdtemp(directory));
  flova_linux::FileStorage storage(directory);
  assert(storage.begin());

  const char value[] = "state";
  assert(storage.write("record", value, sizeof(value)));
  char restored[sizeof(value)] = {};
  assert(storage.read("record", restored, sizeof(restored)));
  assert(memcmp(value, restored, sizeof(value)) == 0);
  assert(!storage.write("../escape", value, sizeof(value)));

  // Exercise the runtime's actual names, not just application-friendly keys.
  for (const char* key : {"cmd:0", "dsid:65534", "history:0", "stream:0"}) {
    assert(storage.write(key, value, sizeof(value)));
    assert(storage.read(key, restored, sizeof(restored)));
    assert(memcmp(value, restored, sizeof(value)) == 0);
    assert(storage.remove(key));
  }
  for (const char* key : {"../cmd:0", "cmd:0/escape", "..", ".", ""})
    assert(!storage.write(key, value, sizeof(value)));

  char fresh[sizeof(directory) + 16] = {};
  snprintf(fresh, sizeof(fresh), "%s/fresh", directory);
  {
    FlovaLinux::Config config = {"mqtts://localhost:8883", "a28cbe21-fd34-4cf6-8f28-8264e90192c7",
                                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", fresh};
    FlovaLinux device(config);
    assert(device.begin());
    struct stat info = {};
    assert(stat(fresh, &info) == 0 && S_ISDIR(info.st_mode));
  }
  assert(rmdir(fresh) == 0);
  char filePath[sizeof(directory) + 16] = {};
  snprintf(filePath, sizeof(filePath), "%s/record", directory);
  flova_linux::FileStorage invalidDirectory(filePath);
  assert(!invalidDirectory.begin()); // EEXIST alone does not establish a directory.
  {
    FlovaLinux::Config config = {"mqtts://localhost:8883", "a28cbe21-fd34-4cf6-8f28-8264e90192c7",
                                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", ""};
    FlovaLinux device(config);
    assert(!device.begin());
    assert(!strcmp(device.lastError(), "storage_begin_failed"));
    assert(!device.running());
  }

  char source[sizeof(directory) + 16] = {};
  snprintf(source, sizeof(source), "%s/source", directory);
  int fd = open(source, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  assert(fd >= 0);
  const char payload[] = "hello\n";
  assert(write(fd, payload, sizeof(payload) - 1) ==
         static_cast<ssize_t>(sizeof(payload) - 1));
  close(fd);

  char otaRoot[sizeof(directory) + 16] = {};
  snprintf(otaRoot, sizeof(otaRoot), "%s/ota", directory);
  char active[sizeof(directory) + 24] = {};
  snprintf(active, sizeof(active), "%s/current", otaRoot);
  flova_linux::OtaUpdater updater(otaRoot, active);
  assert(updater.stage(source, "1.0.0", sizeof(payload) - 1,
                       "0000000000000000000000000000000000000000000000000000000000000000") ==
         flova_linux::OtaStageResult::HashMismatch);
  assert(updater.stage(source, "1.0.0", sizeof(payload) - 1,
                       "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") ==
         flova_linux::OtaStageResult::Staged);
  assert(updater.stage(source, "bad:version", sizeof(payload) - 1,
                       "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") ==
         flova_linux::OtaStageResult::InvalidOffer);
  assert(updater.activate("1.0.0"));
  assert(updater.confirm("1.0.0"));

  unlink(source);
  storage.clear();
  rmdir(directory);
  return 0;
}
