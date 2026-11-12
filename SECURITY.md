# Security

Do not post device secrets, Wi-Fi passwords, provisioning tokens, or private
keys in issues or logs. Report exploitable vulnerabilities through GitHub's
private vulnerability reporting when available, rather than a public issue.

## Provisioning

SoftAP setup passwords are optional: `FLOVA_SETUP_PASSWORD` defaults to
`nullptr`, which starts an open setup AP. Nearby users can join it and submit
provisioning settings; plain HTTP on an open AP does not protect Wi-Fi
credentials or provisioning tokens from nearby observers. For production,
use a device-unique 8–63 character WPA password. Pass it to the universal/
provisioning constructor or define `FLOVA_SETUP_PASSWORD` before including
SDK headers. Supplied passwords outside that length range are rejected.

BLE setup requires device-unique proof of possession
(1–64 characters), passed to its constructor or defined through
`FLOVA_SETUP_PROOF_OF_POSSESSION`. Missing BLE credentials fail setup closed;
configured devices can still restore their saved sessions.

`FLOVA_ALLOW_UNAUTHENTICATED_PROVISIONING=1` allows BLE setup without proof of
possession for local development only; open SoftAP does not need this flag.
Do not ship this flag or a shared setup password/proof.
Keep the provisioning window physically controlled.

## OTA

OTA uses certificate-verified HTTPS and checks image size and SHA-256 against
the authenticated Device Link offer. ESP8266 withholds the final image byte
until the digest matches, because its Arduino `Update.end(false)` commits a
complete image rather than aborting it.

These checks are not independent release-signature verification or secure
boot. A compromised authorized control plane can offer a different image
and digest. Production deployments needing protection against that threat
must provision a separate signing trust root and enforce signed images.

## Remaining deployment controls

- Enforce reviewed changes and required CI checks through a team-approved
  branch/ruleset policy. No branch-protection policy is imposed by this change.
- ESP8266 LittleFS records are not encrypted by the SDK. Treat physical flash
  access as access to stored credentials; configure supported hardware
  security controls on platforms that provide them.
- Use secret scanning and push protection. A dependency alert count of zero
  is not proof of coverage for PlatformIO, Arduino, or vendored C/C++ code.
- Validate provisioning and rejected OTA activation on physical boards before
  shipping; host tests and compilation do not prove hardware acceptance.
