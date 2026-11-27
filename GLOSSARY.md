# Flova SDK glossary

- **Offline guardrail:** A bounded policy for a writable datastream. It can require fresh physical input, limit how long an output runs, and put the output into its safe state when a rule trips.
- **Command outcome:** The original result of a command, retained so a retry can replay that result instead of repeating hardware work. The durable ledger verifies an intent before the command takes effect.
- **Managed claim:** The provisioning handoff from setup to an Engine-assigned device identity. The SDK can resume a verified pending claim after reset. Wi-Fi-only replacement does not change the identity or start a new claim.
- **Device Link session:** One attempt to connect over MQTT/TLS and establish bootstrap or authenticated runtime communication. Runtime datastream writes wait for binding when binding is required.
