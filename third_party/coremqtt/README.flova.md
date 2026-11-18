# coreMQTT

`source/` and `LICENSE` originate from
https://github.com/FreeRTOS/coreMQTT/releases/tag/v5.0.2 (MIT).

Source archive: `https://codeload.github.com/FreeRTOS/coreMQTT/tar.gz/refs/tags/v5.0.2`
SHA-256: `498ae8e48ce1d20f041ada1acdd1d710f31c33f5b91d0de0fd98f8688d7ef260`.

The two `MQTT_Flova*CleanConnect` functions split submission and completion
for a poll-driven clean session. They reuse the library's MQTT v5 serializer,
CONNACK parser, negotiated flow limits and clean-session initialization.
Flova owns the small connection extension, build metadata, timeout configuration and transport adapter.
The upstream library owns MQTT framing, validation, keepalive and QoS state.
No FreeRTOS, Arduino, TLS or runtime allocator is required by the library.
