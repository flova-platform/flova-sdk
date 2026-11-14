#pragma once
#include <stdint.h>

class FlovaEntropySource {
 public:
  virtual ~FlovaEntropySource() {}
  virtual uint8_t byte() = 0;
};
