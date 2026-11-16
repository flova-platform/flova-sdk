# MQTT example migration

The old PubSubClient facade example used removed constructors and has been
retired. Flova Link already uses bounded CBOR over MQTT/TLS internally.
Use `examples/custom-arduino-client` with `FlovaEsp32` or
`FlovaEsp8266`; Wi-Fi and device credentials arrive through setup.
No application-owned MQTT client or hardcoded device secret is required.
