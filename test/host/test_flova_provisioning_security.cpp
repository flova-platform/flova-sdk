#include <assert.h>
#include <string.h>
#include <FlovaProvisioningSecurity.h>

int main() {
  const bool development = FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING == 1;
  assert(flova::validSetupPassword(nullptr));
  assert(flova::validSetupPassword(FLOVA_SETUP_PASSWORD));
  assert(!flova::validSetupPassword(""));
  assert(!flova::validSetupPassword("1234567"));
  assert(flova::validSetupPassword("12345678"));
  char text[66];
  memset(text, 'x', sizeof(text));
  text[63] = 0;
  assert(flova::validSetupPassword(text));
  text[63] = 'x'; text[64] = 0;
  assert(!flova::validSetupPassword(text));
  assert(flova::validSetupProofOfPossession(text));
  text[64] = 'x'; text[65] = 0;
  assert(!flova::validSetupProofOfPossession(text));
  assert(flova::validSetupProofOfPossession(nullptr) == development);
  assert(flova::validSetupProofOfPossession("") == development);
  assert(flova::validSetupProofOfPossession("device-unique-proof"));
}
