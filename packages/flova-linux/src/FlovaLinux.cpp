#include <FlovaLinux.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <openssl/evp.h>

namespace {

uint64_t monotonicMs() {
  struct timespec value = {};
  clock_gettime(CLOCK_MONOTONIC, &value);
  return static_cast<uint64_t>(value.tv_sec) * 1000ULL +
         static_cast<uint64_t>(value.tv_nsec) / 1000000ULL;
}

bool safeComponent(const char* value, bool storageKey = false) {
  if (!value || !value[0]) return false;
  for (const char* cursor = value; *cursor; ++cursor) {
    if (!(('a' <= *cursor && *cursor <= 'z') ||
          ('A' <= *cursor && *cursor <= 'Z') ||
          ('0' <= *cursor && *cursor <= '9') || *cursor == '_' ||
          *cursor == '-' || *cursor == '.' || (storageKey && *cursor == ':')))
      return false;
  }
  return strcmp(value, ".") != 0 && strcmp(value, "..") != 0;
}

bool syncDirectory(const char* path) {
  const int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) return false;
  int result;
  do {
    result = fsync(fd);
  } while (result < 0 && errno == EINTR);
  close(fd);
  return result == 0;
}

bool syncParentDirectory(const char* path) {
  const char* slash = strrchr(path, '/');
  if (!slash) return syncDirectory(".");
  if (slash == path) return syncDirectory("/");

  char parent[512];
  const size_t length = static_cast<size_t>(slash - path);
  if (length >= sizeof(parent)) return false;
  memcpy(parent, path, length);
  parent[length] = 0;
  return syncDirectory(parent);
}

bool makeDirectoryEntry(const char* path) {
  if (::mkdir(path, 0750) != 0) return errno == EEXIST;
  if (syncParentDirectory(path)) return true;
  // A later begin() should retry creation if the new entry could not be synced.
  rmdir(path);
  return false;
}

bool makeDirectory(const char* path) {
  if (!path || !path[0]) return false;
  char buffer[512] = {};
  if (strlen(path) >= sizeof(buffer)) return false;
  strcpy(buffer, path);
  for (char* cursor = buffer + 1; *cursor; ++cursor) {
    if (*cursor != '/') continue;
    *cursor = 0;
    if (!makeDirectoryEntry(buffer)) return false;
    *cursor = '/';
  }
  if (!makeDirectoryEntry(buffer)) return false;
  struct stat info = {};
  return stat(buffer, &info) == 0 && S_ISDIR(info.st_mode);
}

bool replaceActiveImage(const char* candidate, const char* active, const char* backup) {
  struct stat info;
  if (stat(candidate, &info) != 0 || !S_ISREG(info.st_mode)) return false;
  if (lstat(active, &info) == 0) {
    char pending[392];
    const int length = snprintf(pending, sizeof(pending), "%s.tmp", backup);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(pending)) return false;
    if (unlink(pending) != 0 && errno != ENOENT) return false;
    // Preserve the active inode while publishing its backup. The supervisor's
    // path always resolves to the old or new image, including on rename failure.
    if (linkat(AT_FDCWD, active, AT_FDCWD, pending, AT_SYMLINK_FOLLOW) != 0) return false;
    if (rename(pending, backup) != 0) {
      unlink(pending);
      return false;
    }
    // Linux leaves both names alone when they already refer to the same inode.
    if (unlink(pending) != 0 && errno != ENOENT) return false;
    if (!syncParentDirectory(backup)) return false;
  } else if (errno != ENOENT) {
    return false;
  }

  if (rename(candidate, active) != 0) return false;
  const bool activeSynced = syncParentDirectory(active);
  const bool candidateSynced = syncParentDirectory(candidate);
  return activeSynced && candidateSynced;
}

bool copyFile(const char* source, const char* destination, uint32_t expected,
              uint32_t* copied, char* digestHex, size_t digestCapacity) {
  int input = open(source, O_RDONLY | O_CLOEXEC);
  if (input < 0) return false;
  int output = open(destination, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
                    0640);
  if (output < 0) {
    close(input);
    return false;
  }
  uint8_t buffer[4096];
  uint32_t total = 0;
  EVP_MD_CTX* digest = EVP_MD_CTX_new();
  if (!digest || EVP_DigestInit_ex(digest, EVP_sha256(), nullptr) != 1) {
    if (digest) EVP_MD_CTX_free(digest);
    close(output);
    close(input);
    return false;
  }
  bool ok = true;
  for (;;) {
    const ssize_t count = read(input, buffer, sizeof(buffer));
    if (count == 0) break;
    if (count < 0 || total > UINT32_MAX - static_cast<uint32_t>(count) ||
        (expected && total + static_cast<uint32_t>(count) > expected) ||
        write(output, buffer, static_cast<size_t>(count)) != count) {
      ok = false;
      break;
    }
    if (EVP_DigestUpdate(digest, buffer, static_cast<size_t>(count)) != 1) {
      ok = false;
      break;
    }
    total += static_cast<uint32_t>(count);
  }
  if (expected && total != expected) ok = false;
  if (copied) *copied = total;
  if (ok) {
    struct stat info;
    ok = fstat(output, &info) == 0 && fchmod(output, (info.st_mode & 0777) | S_IXUSR) == 0;
  }
  if (fsync(output) != 0) ok = false;
  close(output);
  close(input);
  if (ok && digestHex && digestCapacity >= 65) {
    uint8_t digestBytes[EVP_MAX_MD_SIZE] = {};
    unsigned digestLength = 0;
    if (EVP_DigestFinal_ex(digest, digestBytes, &digestLength) != 1 ||
        digestLength != 32) {
      ok = false;
    }
    for (size_t i = 0; i < sizeof(digestBytes); ++i)
      if (i < digestLength) snprintf(digestHex + i * 2, 3, "%02x", digestBytes[i]);
    if (digestLength == 32) digestHex[64] = 0;
  }
  EVP_MD_CTX_free(digest);
  return ok;
}

}  // namespace

namespace flova_linux {

uint64_t Clock::milliseconds() const { return monotonicMs(); }

bool Clock::utcValid() const { return utcBaseMs_ != 0; }

uint64_t Clock::utcMilliseconds() const {
  return utcValid() ? utcBaseMs_ + (monotonicMs() - monotonicBaseMs_) : 0;
}

void Clock::setUtc(uint64_t value, uint64_t) {
  utcBaseMs_ = value;
  monotonicBaseMs_ = monotonicMs();
}

FileStorage::FileStorage(const char* directory) : directory_{} {
  if (directory) {
    strncpy(directory_, directory, sizeof(directory_) - 1);
    directory_[sizeof(directory_) - 1] = 0;
  }
}

bool FileStorage::begin() { return directory_[0] && makeDirectory(directory_); }

bool FileStorage::pathFor(const char* key, char* output, size_t capacity) const {
  // Runtime records use colon-separated namespaces. They are single POSIX
  // filename components; OTA version names retain the stricter validation.
  if (!safeComponent(key, true) || !output ||
      snprintf(output, capacity, "%s/%s", directory_, key) >=
          static_cast<int>(capacity))
    return false;
  return true;
}

bool FileStorage::read(const char* key, void* output, size_t size) {
  char path[sizeof(directory_) + 128] = {};
  if (!pathFor(key, path, sizeof(path)) || !output) return false;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  const ssize_t count = ::read(fd, output, size);
  uint8_t extra = 0;
  const bool ok = count == static_cast<ssize_t>(size) &&
                 ::read(fd, &extra, sizeof(extra)) == 0;
  close(fd);
  return ok;
}

bool FileStorage::write(const char* key, const void* value, size_t size) {
  char path[sizeof(directory_) + 128] = {};
  char pending[sizeof(path) + 8] = {};
  if (!pathFor(key, path, sizeof(path)) || !value ||
      snprintf(pending, sizeof(pending), "%s.tmp", path) >=
          static_cast<int>(sizeof(pending)))
    return false;
  int fd = open(pending, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
  if (fd < 0) return false;
  const ssize_t written = ::write(fd, value, size);
  const bool ok = written == static_cast<ssize_t>(size) && fsync(fd) == 0;
  close(fd);
  if (!ok || rename(pending, path) != 0) {
    unlink(pending);
    return false;
  }
  return syncDirectory(directory_);
}

bool FileStorage::remove(const char* key) {
  char path[sizeof(directory_) + 128] = {};
  if (!pathFor(key, path, sizeof(path))) return false;
  if (unlink(path) != 0 && errno != ENOENT) return false;
  return syncDirectory(directory_);
}

bool FileStorage::clear() {
  // Runtime keys are intentionally explicit and bounded. Unknown files remain
  // untouched so applications can share the directory safely.
  static const char* const keys[] = {"session", "session.pending", "config",
                                     "prov_pending", "ota_pending"};
  bool ok = true;
  for (const char* key : keys) ok = remove(key) && ok;
  return ok;
}

flova::StorageCapabilities FileStorage::capabilities() const {
  flova::StorageCapabilities value;
  value.usableBytes = 0;
  value.availableBytes = 0;
  value.maxRecordBytes = 16384;
  value.eraseBlockBytes = 1;
  value.writeGranularity = 1;
  value.persistent = true;
  value.wearSensitive = false;
  return value;
}

void Logger::log(const char* message) {
  if (message) fprintf(stderr, "%s\n", message);
}

OtaUpdater::OtaUpdater(const char* root, const char* activeLink)
    : root_{}, activeLink_{} {
  if (root) strncpy(root_, root, sizeof(root_) - 1);
  if (activeLink) strncpy(activeLink_, activeLink, sizeof(activeLink_) - 1);
}

bool OtaUpdater::pathFor(const char* suffix, const char* version, char* output,
                         size_t capacity) const {
  if (!safeComponent(version) || !output ||
      snprintf(output, capacity, "%s/%s-%s", root_, suffix, version) >=
          static_cast<int>(capacity))
    return false;
  return true;
}

OtaStageResult OtaUpdater::stage(const char* sourcePath, const char* version,
                                 uint32_t expectedBytes,
                                 const char* expectedSha256) {
  if (!sourcePath || !safeComponent(version) || !expectedBytes ||
      !expectedSha256 || strlen(expectedSha256) != 64)
    return OtaStageResult::InvalidOffer;
  if (!makeDirectory(root_)) return OtaStageResult::StorageFailure;
  char destination[sizeof(root_) + 128] = {};
  if (!pathFor("staged", version, destination, sizeof(destination)))
    return OtaStageResult::InvalidOffer;
  char pending[sizeof(destination) + 8];
  const int length = snprintf(pending, sizeof(pending), "%s/.stage.pending", root_);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(pending))
    return OtaStageResult::InvalidOffer;
  uint32_t copied = 0;
  char digest[65] = {};
  if (!copyFile(sourcePath, pending, expectedBytes, &copied, digest,
                sizeof(digest))) {
    unlink(pending);
    return copied != expectedBytes ? OtaStageResult::SizeMismatch
                                   : OtaStageResult::DownloadUnavailable;
  }
  if (strcasecmp(digest, expectedSha256) != 0) {
    unlink(pending);
    return OtaStageResult::HashMismatch;
  }
  if (rename(pending, destination) != 0) {
    unlink(pending);
    return OtaStageResult::StorageFailure;
  }
  if (!syncDirectory(root_)) return OtaStageResult::StorageFailure;
  return OtaStageResult::Staged;
}

bool OtaUpdater::activate(const char* version) {
  char staged[sizeof(root_) + 128] = {};
  if (!pathFor("staged", version, staged, sizeof(staged))) return false;
  char previous[sizeof(root_) + 128] = {};
  if (snprintf(previous, sizeof(previous), "%s/previous", root_) >=
      static_cast<int>(sizeof(previous)))
    return false;
  return replaceActiveImage(staged, activeLink_, previous);
}

bool OtaUpdater::confirm(const char* version) {
  if (!safeComponent(version) || access(activeLink_, F_OK) != 0) return false;
  char confirmed[sizeof(root_) + 128] = {};
  char pending[sizeof(confirmed) + 8] = {};
  if (snprintf(confirmed, sizeof(confirmed), "%s/confirmed", root_) >=
          static_cast<int>(sizeof(confirmed)) ||
      snprintf(pending, sizeof(pending), "%s.tmp", confirmed) >=
          static_cast<int>(sizeof(pending)))
    return false;
  int fd = open(pending, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
  if (fd < 0) return false;
  const size_t length = strlen(version);
  const bool ok = write(fd, version, length) == static_cast<ssize_t>(length) &&
                  fsync(fd) == 0;
  close(fd);
  if (!ok || rename(pending, confirmed) != 0) {
    unlink(pending);
    return false;
  }
  return syncDirectory(root_);
}

bool OtaUpdater::rollback() {
  char previous[sizeof(root_) + 128] = {};
  if (snprintf(previous, sizeof(previous), "%s/previous", root_) >=
      static_cast<int>(sizeof(previous)))
    return false;
  char failed[sizeof(root_) + 128] = {};
  if (snprintf(failed, sizeof(failed), "%s/failed", root_) >=
      static_cast<int>(sizeof(failed)))
    return false;
  return replaceActiveImage(previous, activeLink_, failed);
}

}  // namespace flova_linux
