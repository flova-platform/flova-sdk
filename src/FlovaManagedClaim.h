#pragma once

#include "FlovaProvisioningAdapter.h"
#include "FlovaProvisioningHandoff.h"
#include "FlovaWifiProvisioning.h"

namespace flova {
namespace detail {

// The board owns setup-channel resources. This module owns the durable claim
// ordering shared by the two SoftAP adapters and the client that resumes it.
struct ManagedClaim {
  enum class Submission { Invalid, Accepted, WifiWriteFailed, HandoffWriteFailed };
  enum class PendingWrite { Accepted, Invalid, WriteFailed, VerifyFailed, ClearErrorFailed };
  enum class Failure { Retry, FreshSetup, Stop };

  static Submission submit(Storage& storage, const WifiRuntimeData& wifi,
                           const ProvisioningHandoff& handoff,
                           FlovaProvisioningHandler handler, void* context) {
    if (!handler) return Submission::Invalid;
    if (!storage.write("wifi", &wifi, sizeof(wifi))) return Submission::WifiWriteFailed;
    switch (handler(context, handoff)) {
      case FlovaProvisioningResponse::Accepted: return Submission::Accepted;
      // The pending claim may already be on disk despite a failed read-back.
      // Keep its network credentials so a subsequent boot can verify it.
      case FlovaProvisioningResponse::StorageFailed: return Submission::HandoffWriteFailed;
      default:
        storage.remove("wifi");
        return Submission::Invalid;
    }
  }

  static bool read(Storage& storage, ProvisioningHandoffImage& image) {
    memset(&image, 0, sizeof(image));
    return storage.read("prov_pending", &image, sizeof(image)) && verifyProvisioningImage(image);
  }

  static bool discard(Storage& storage) { return storage.remove("prov_pending"); }

  static PendingWrite accept(Storage& storage, ProvisioningHandoffImage& image,
                             const ProvisioningHandoff& input, FlovaEntropySource& entropy) {
    memset(&image, 0, sizeof(image));
    if (!copyBounded(input.linkUrl, image.handoff.linkUrl, true) ||
        !copyBounded(input.token, image.handoff.token, true) ||
        !generateSecret(image.handoff.linkSecret, entropy)) return PendingWrite::Invalid;
    makeProvisioningImage(image.handoff, image);
    if (!storage.write("prov_pending", &image, sizeof(image))) return PendingWrite::WriteFailed;
    if (!read(storage, image)) return PendingWrite::VerifyFailed;
    return storage.remove("prov_error") ? PendingWrite::Accepted : PendingWrite::ClearErrorFailed;
  }

  static bool persistProgress(Storage& storage, ProvisioningHandoffImage& image) {
    const uint8_t attempts = image.attempts;
    const uint8_t inProgress = image.inProgress;
    char error[kProvisioningErrorBytes] = {};
    memcpy(error, image.lastError, sizeof(error) - 1);
    return storage.write("prov_pending", &image, sizeof(image)) && read(storage, image) &&
           image.attempts == attempts && image.inProgress == inProgress &&
           strcmp(image.lastError, error) == 0;
  }

  static Failure classify(const char* error) {
    if (error && (strstr(error, "cbor") || strncmp(error, "configuration_", 14) == 0))
      return Failure::Stop;
    return provisioningFailureNeedsSetup(error) ? Failure::FreshSetup : Failure::Retry;
  }
};

}  // namespace detail
}  // namespace flova
