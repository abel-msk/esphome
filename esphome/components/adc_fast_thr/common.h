#pragma once

#include <cstdint>

#include "esphome/core/log.h"

namespace esphome::adc_fast_thr {

enum class SamplingMode : uint8_t {
  AVG = 0,
  MIN = 1,
  MAX = 2,
};

const LogString *sampling_mode_to_str(SamplingMode mode);

/// Combines samples into one value. Instantiated for uint32_t only, in common.cpp.
template<typename T> class Aggregator {
 public:
  explicit Aggregator(SamplingMode mode);
  void add_sample(T value);
  T aggregate();

 protected:
  T aggr_{0};
  uint8_t samples_{0};
  SamplingMode mode_{SamplingMode::AVG};
};

}  // namespace esphome::adc_fast_thr
