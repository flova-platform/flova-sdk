#pragma once

#include <string.h>

// Development-only escape hatch for BLE setup without proof of possession.
#ifndef FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING
#define FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING 0
#endif

#ifndef FLOVA_SETUP_PASSWORD
// Optional: nullptr keeps the setup SoftAP open.
#define FLOVA_SETUP_PASSWORD nullptr
#endif
#ifndef FLOVA_SETUP_PROOF_OF_POSSESSION
#define FLOVA_SETUP_PROOF_OF_POSSESSION nullptr
#endif

namespace flova {

inline bool validSetupPassword(const char* password) {
  if (!password) return true;
  const size_t length = strlen(password);
  return length >= 8 && length <= 63;
}

inline bool validSetupProofOfPossession(const char* proof) {
  if (!proof || !proof[0])
    return FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING == 1;
  return strlen(proof) <= 64;
}

}  // namespace flova
