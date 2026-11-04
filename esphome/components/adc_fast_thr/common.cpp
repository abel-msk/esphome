#include "common.h"

#include <limits>

namespace esphome::adc_fast_thr {

const LogString *sampling_mode_to_str(SamplingMode mode) {
  switch (mode) {
    case SamplingMode::AVG:
      return LOG_STR("average");
    case SamplingMode::MIN:
      return LOG_STR("minimum");
    case SamplingMode::MAX:
      return LOG_STR("maximum");
  }
  return LOG_STR("unknown");
}

template<typename T> Aggregator<T>::Aggregator(SamplingMode mode) {
  this->mode_ = mode;
  // Start from the largest value so the first sample always becomes the minimum.
  if (mode == SamplingMode::MIN) {
    this->aggr_ = std::numeric_limits<T>::max();
  }
}

template<typename T> void Aggregator<T>::add_sample(T value) {
  this->samples_ += 1;

  switch (this->mode_) {
    case SamplingMode::AVG:
      this->aggr_ += value;
      break;

    case SamplingMode::MIN:
      if (value < this->aggr_) {
        this->aggr_ = value;
      }
      break;

    case SamplingMode::MAX:
      if (value > this->aggr_) {
        this->aggr_ = value;
      }
      break;
  }
}

template<typename T> T Aggregator<T>::aggregate() {
  if (this->mode_ == SamplingMode::AVG) {
    if (this->samples_ == 0) {
      return this->aggr_;
    }

    return (this->aggr_ + (this->samples_ >> 1)) / this->samples_;  // NOLINT(clang-analyzer-core.DivideZero)
  }

  return this->aggr_;
}

template class Aggregator<uint32_t>;

}  // namespace esphome::adc_fast_thr
