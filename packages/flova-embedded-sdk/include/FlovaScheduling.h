#pragma once

#include <stddef.h>
#include <stdint.h>

namespace flova {

// Execution ownership, not a request to run a second cloud/offline executor.
enum class Scheduling : uint8_t { Disabled = 0, CloudOnly = 1 };

// Borrowed by Link for the Device lifetime; never calls storage or hardware.
typedef bool (*SchedulingReader)(void*, size_t, uint16_t&, Scheduling&);

}  // namespace flova
