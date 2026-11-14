#pragma once

// Socket writes are accepted into the adapter's bounded, owned transmit queue.
// No MQTT API may wait seconds for a congested socket or a publish ACK.
#define MQTT_SEND_TIMEOUT_MS 5U
#define MQTT_RECV_POLLING_TIMEOUT_MS 0U
#define MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT 1U
