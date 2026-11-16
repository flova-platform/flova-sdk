# Custom Arduino phone provisioning

This example adds Flova provisioning routes to an application-owned Arduino
web server. The SDK does not start, stop, or replace that server and does not
change the application's Wi-Fi mode.

The example explicitly composes `FlovaClient` with board storage, observed
networking, TLS clock, identity, Link, and manual hardware services. It starts
its own Wi-Fi and registers its own bounded handoff routes:

- `GET /status`
- `POST /provision`

The setup channel sends the Engine Link URL and activation token. The device
generates its secret locally, persists the handoff, and bootstraps over the
already-managed network without a reboot. BLE, serial, cellular, factory, or
other channel code can instead call
`flova.provision(ProvisioningHandoff(...))`. The standard `FlovaEsp32` and `FlovaEsp8266` classes provide managed SoftAP
setup for beginner sketches.

This is also the model for alternate channels. A BLE, serial, cellular,
Ethernet, or factory adapter can decode its own bounded input and call:

```cpp
flova::ProvisioningHandoff handoff(linkUrl, shortLivedToken);
client.provision(handoff);
```

The handoff contains only the authoritative MQTT/TLS Link URL and short-lived
token. The device generates its own secret. Wi-Fi credentials and network
ownership stay with the application or universal composition.

Build for both supported Arduino targets:

```sh
pio run -e custom-arduino-provisioning-esp32
pio run -e custom-arduino-provisioning-esp8266
```

The application may call `client.startProvisioning()` from its own factory
reset button. This clears Flova-private identity and waits for another handoff;
it does not alter network or server state.
