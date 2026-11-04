#pragma once

#include <cmath>
#include <cstdint>

#include "common.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_monitor.h"
#include "freertos/FreeRTOS.h"
#include "hal/adc_types.h"

namespace esphome::adc_fast_thr {

static constexpr adc_atten_t ADC_ATTEN_DB_12_COMPAT = ADC_ATTEN_DB_12;

/// The threshold the input crossed last.
enum class MonitorState : uint8_t {
  MONITOR_STATE_UNKNOWN = 0,
  MONITOR_STATE_ABOVE_HIGH = 1,
  MONITOR_STATE_BELOW_LOW = 2,
};

/// Reads an ADC channel in continuous (DMA) mode and watches it with the hardware threshold monitor.
class ADCFastThr final : public sensor::Sensor, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_output_raw(bool output_raw) { this->output_raw_ = output_raw; }
  /// A value of 0 is ignored.
  void set_sample_count(uint8_t sample_count);
  void set_sampling_mode(SamplingMode sampling_mode) { this->sampling_mode_ = sampling_mode; }
  void set_attenuation(adc_atten_t attenuation) { this->attenuation_ = attenuation; }
  void set_channel(adc_unit_t unit, adc_channel_t channel) {
    this->adc_unit_ = unit;
    this->channel_ = channel;
  }
  void set_autorange(bool autorange) { this->autorange_ = autorange; }
  /// Input voltage divided by the voltage at the pin.
  void set_voltage_divider(float voltage_divider) { this->voltage_divider_ = voltage_divider; }
  /// Monitor thresholds in volts at the divider input. Set them before setup() runs.
  void set_high_threshold(float high_threshold) { this->high_threshold_ = high_threshold; }
  void set_low_threshold(float low_threshold) { this->low_threshold_ = low_threshold; }

  /// Read the samples waiting in the DMA buffer and combine them into one value.
  float sample();
  /// Release the continuous mode driver.
  void uninit();

 protected:
  // Interrupt handlers for the threshold monitor: they only store the state, because logging is not allowed there.
  static bool on_monitor_high_(adc_monitor_handle_t monitor_handle, const adc_monitor_evt_data_t *event_data,
                               void *user_data);
  static bool on_monitor_low_(adc_monitor_handle_t monitor_handle, const adc_monitor_evt_data_t *event_data,
                              void *user_data);
  float sample_fixed_attenuation_();
  /// Returns -1 when the ADC cannot reach this voltage with the current attenuation and divider.
  int32_t voltage_to_raw_(float voltage);

  volatile MonitorState monitor_state_{MonitorState::MONITOR_STATE_UNKNOWN};
  MonitorState logged_monitor_state_{MonitorState::MONITOR_STATE_UNKNOWN};

  InternalGPIOPin *pin_{nullptr};
  adc_continuous_handle_t adc_handle_{nullptr};
  adc_monitor_handle_t adc_monitor_handle_{nullptr};
  adc_cali_handle_t calibration_handle_{nullptr};
  adc_monitor_config_t adc_threshold_{};
  uint32_t raw_{0};
  float high_threshold_{NAN};
  float low_threshold_{NAN};
  float voltage_divider_{1.0f};
  adc_atten_t attenuation_{ADC_ATTEN_DB_12};
  adc_channel_t channel_{};
  adc_unit_t adc_unit_{};
  SamplingMode sampling_mode_{SamplingMode::AVG};
  uint8_t sample_count_{1};
  bool output_raw_{false};
  bool autorange_{false};
  struct SetupFlags {
    uint8_t init_complete : 1;
    uint8_t config_complete : 1;
    uint8_t handle_init_complete : 1;
    uint8_t calibration_complete : 1;
    uint8_t reserved : 4;
  } setup_flags_{};
};

}  // namespace esphome::adc_fast_thr
