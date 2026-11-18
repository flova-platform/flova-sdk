#include <FlovaLinux.h>

#include <errno.h>
#include <fcntl.h>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>

#define CHECK(condition)                                              \
  do {                                                                \
    if (!(condition)) {                                               \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      abort();                                                        \
    }                                                                 \
  } while (false)

static_assert(!std::is_copy_constructible<FlovaLinux>::value, "Linux facade owns its runtime");
static_assert(!std::is_copy_assignable<FlovaLinux>::value, "Linux facade owns its runtime");

static bool failDirectorySync;
static unsigned directorySyncs;
static const char* rejectRenameTo;

extern "C" int __real_fsync(int fd);
extern "C" int __wrap_fsync(int fd) {
  struct stat info;
  if (fstat(fd, &info) == 0 && S_ISDIR(info.st_mode)) {
    ++directorySyncs;
    if (failDirectorySync) {
      errno = EIO;
      return -1;
    }
  }
  return __real_fsync(fd);
}

extern "C" int __real_rename(const char* source, const char* destination);
extern "C" int __wrap_rename(const char* source, const char* destination) {
  if (rejectRenameTo && strcmp(destination, rejectRenameTo) == 0) {
    errno = EIO;
    return -1;
  }
  return __real_rename(source, destination);
}

int main() {
  const char* temporaryRoot = getenv("TMPDIR");
  if (!temporaryRoot || !temporaryRoot[0]) temporaryRoot = "./tmp";
  CHECK(mkdir(temporaryRoot, 0700) == 0 || errno == EEXIST);
  char directory[512] = {};
  const int written = snprintf(directory, sizeof(directory), "%s/flova-linux-test-XXXXXX", temporaryRoot);
  CHECK(written > 0 && static_cast<size_t>(written) < sizeof(directory));
  CHECK(mkdtemp(directory));
  flova_linux::FileStorage storage(directory);
  CHECK(storage.begin());

  const char value[] = "state";
  CHECK(storage.write("record", value, sizeof(value)));
  char restored[sizeof(value)] = {};
  CHECK(storage.read("record", restored, sizeof(restored)));
  CHECK(memcmp(value, restored, sizeof(value)) == 0);
  CHECK(!storage.write("../escape", value, sizeof(value)));

  const unsigned beforeSyncs = directorySyncs;
  failDirectorySync = true;
  CHECK(!storage.write("checkpoint", value, sizeof(value)));
  CHECK(directorySyncs == beforeSyncs + 1);
  CHECK(storage.read("checkpoint", restored, sizeof(restored)));
  failDirectorySync = false;
  CHECK(storage.write("checkpoint", value, sizeof(value)));
  failDirectorySync = true;
  CHECK(!storage.remove("checkpoint"));
  failDirectorySync = false;
  CHECK(storage.remove("checkpoint"));

  // Exercise the runtime's actual names, not just application-friendly keys.
  for (const char* key : {"cmd:0", "dsid:65534", "history:0", "stream:0"}) {
    CHECK(storage.write(key, value, sizeof(value)));
    CHECK(storage.read(key, restored, sizeof(restored)));
    CHECK(memcmp(value, restored, sizeof(value)) == 0);
    CHECK(storage.remove(key));
  }
  for (const char* key : {"../cmd:0", "cmd:0/escape", "..", ".", ""})
    CHECK(!storage.write(key, value, sizeof(value)));

  char fresh[sizeof(directory) + 16] = {};
  snprintf(fresh, sizeof(fresh), "%s/fresh", directory);
  {
    FlovaLinux::Config config = {"mqtts://localhost:8883", "a28cbe21-fd34-4cf6-8f28-8264e90192c7",
                                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", fresh};
    FlovaLinux device(config);
    CHECK(device.begin());
    struct stat info = {};
    CHECK(stat(fresh, &info) == 0 && S_ISDIR(info.st_mode));
  }
  CHECK(rmdir(fresh) == 0);
  char filePath[sizeof(directory) + 16] = {};
  snprintf(filePath, sizeof(filePath), "%s/record", directory);
  flova_linux::FileStorage invalidDirectory(filePath);
  CHECK(!invalidDirectory.begin()); // EEXIST alone does not establish a directory.
  {
    FlovaLinux::Config config = {"mqtts://localhost:8883", "a28cbe21-fd34-4cf6-8f28-8264e90192c7",
                                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", ""};
    FlovaLinux device(config);
    CHECK(!device.begin());
    CHECK(!strcmp(device.lastError(), "storage_begin_failed"));
    CHECK(!device.running());
  }

  char source[sizeof(directory) + 16] = {};
  snprintf(source, sizeof(source), "%s/source", directory);
  int fd = open(source, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  CHECK(fd >= 0);
  const char payload[] = "hello\n";
  CHECK(write(fd, payload, sizeof(payload) - 1) ==
         static_cast<ssize_t>(sizeof(payload) - 1));
  close(fd);

  char otaRoot[sizeof(directory) + 16] = {};
  snprintf(otaRoot, sizeof(otaRoot), "%s/ota", directory);
  char active[sizeof(directory) + 24] = {};
  snprintf(active, sizeof(active), "%s/current", otaRoot);
  flova_linux::OtaUpdater updater(otaRoot, active);
  CHECK(updater.stage(source, "1.0.0", sizeof(payload) - 1,
                       "0000000000000000000000000000000000000000000000000000000000000000") ==
         flova_linux::OtaStageResult::HashMismatch);
  CHECK(updater.stage(source, "1.0.0", sizeof(payload) - 1,
                       "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") ==
         flova_linux::OtaStageResult::Staged);
  CHECK(updater.stage(source, "bad:version", sizeof(payload) - 1,
                       "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") ==
         flova_linux::OtaStageResult::InvalidOffer);
  CHECK(updater.activate("1.0.0"));
  CHECK(updater.confirm("1.0.0"));
  struct stat original, current;
  CHECK(stat(active, &original) == 0 && (original.st_mode & S_IXUSR));
  CHECK(!updater.activate("missing"));
  CHECK(stat(active, &current) == 0 && current.st_ino == original.st_ino);
  CHECK(!updater.rollback());
  CHECK(stat(active, &current) == 0 && current.st_ino == original.st_ino);

  CHECK(updater.stage(source, "2.0.0", sizeof(payload) - 1,
                       "5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") ==
         flova_linux::OtaStageResult::Staged);
  CHECK(updater.stage(source, "2.0.0", sizeof(payload) - 1,
                       "0000000000000000000000000000000000000000000000000000000000000000") ==
         flova_linux::OtaStageResult::HashMismatch);
  failDirectorySync = true;
  CHECK(!updater.activate("2.0.0"));
  CHECK(stat(active, &current) == 0 && current.st_ino == original.st_ino);
  failDirectorySync = false;
  rejectRenameTo = active;
  CHECK(!updater.activate("2.0.0"));
  CHECK(stat(active, &current) == 0 && current.st_ino == original.st_ino);
  rejectRenameTo = nullptr;
  CHECK(updater.activate("2.0.0"));
  CHECK(stat(active, &current) == 0 && current.st_ino != original.st_ino);
  const ino_t upgradedInode = current.st_ino;
  rejectRenameTo = active;
  CHECK(!updater.rollback());
  CHECK(stat(active, &current) == 0 && current.st_ino == upgradedInode);
  rejectRenameTo = nullptr;
  CHECK(updater.rollback());
  CHECK(stat(active, &current) == 0 && current.st_ino == original.st_ino);

  for (const char* name : {"current", "failed", "confirmed"}) {
    char otaFile[sizeof(otaRoot) + 16];
    snprintf(otaFile, sizeof(otaFile), "%s/%s", otaRoot, name);
    CHECK(unlink(otaFile) == 0);
  }
  CHECK(rmdir(otaRoot) == 0);
  CHECK(unlink(source) == 0);
  CHECK(storage.remove("record"));
  CHECK(storage.clear());
  CHECK(rmdir(directory) == 0);
  return 0;
}
