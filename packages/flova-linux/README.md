# Flova Linux

`flova-linux` is the native POSIX support package for Raspberry Pi and other
Linux hosts. It installs as a CMake package and exposes the same typed
datastream helpers used by the Arduino facades.

```cpp
#include <FlovaLinux.h>

FlovaLinux device;
auto relay = FLOVA_DATASTREAM(device, bool, "relay");

int main() {
  FLOVA_ON_WRITE(relay, [](bool value) { /* apply output */ });
  if (!device.begin()) return 1;
  for (;;) device.run();
}
```

`run()` advances transport and MQTT connection setup across calls. Durable
command checkpoints and application write handlers run synchronously in that
step. Allow for their latency when calling it from a poll or epoll loop.

```cmake
find_package(FlovaLinux CONFIG REQUIRED)
target_link_libraries(my_device PRIVATE Flova::Linux)
```

The package provides `FlovaLinux`, POSIX MQTT over TLS, `Clock`, `FileStorage`,
`Logger`, and `OtaUpdater`. The transport uses fixed receive and transmit
workspaces, OpenSSL certificate verification, and reconnect backoff.
`OtaUpdater` verifies the declared size and SHA-256, stages the artifact, and
exposes activation/confirmation/rollback operations for an external systemd
or equivalent supervisor.
Use one supervisor to stage and switch images in each OTA directory. A
verified staged image is executable by its owner. Activation and rollback
preserve the current image as a hard link before
atomically replacing its active path. Keep the OTA directory and active
path on the same filesystem. If a directory sync fails after publication,
the operation returns failure even though the new entry may already be
visible. FileStorage follows the same rule for durable record writes and
removals.

Direct Ethernet devices use the host's normal Linux network interface. The
application owns its event loop and hardware integration while `FlovaLinux`
owns the MQTT connection and device protocol.
