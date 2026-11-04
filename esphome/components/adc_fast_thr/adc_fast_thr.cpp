#include "adc_fast_thr.h"
#include "esphome/core/log.h"
#include "esp_adc/adc_continuous.h"
#include <cinttypes>

namespace esphome {
namespace adc_fast_thr {

ESPHOME_LOG_TAG(TAG, "adc_fast_thr");

static constexpr int32_t ADC_MAX_RAW = 4095;
static constexpr float UNCALIBRATED_FULL_SCALE_V = 3.3f;

// adc_oneshot_unit_handle_t ADCFastThr::shared_adc_handles[2] = {nullptr, nullptr};

const LogString *attenuation_to_str(adc_atten_t attenuation) {
  switch (attenuation) {
    case ADC_ATTEN_DB_0:
      return LOG_STR("0 dB");
    case ADC_ATTEN_DB_2_5:
      return LOG_STR("2.5 dB");
    case ADC_ATTEN_DB_6:
      return LOG_STR("6 dB");
    case ADC_ATTEN_DB_12_COMPAT:
      return LOG_STR("12 dB");
    default:
      return LOG_STR("Unknown Attenuation");
  }
};

// *******************************************************************************************
//
//   Threshold callback
//
// https://esp32.com/viewtopic.php?t=40186
bool IRAM_ATTR ADCFastThr::on_monitor_high_(adc_monitor_handle_t monitor_handle,
                                            const adc_monitor_evt_data_t *event_data, void *user_data) {
  static_cast<ADCFastThr *>(user_data)->monitor_state_ = MonitorState::MONITOR_STATE_ABOVE_HIGH;
  return false;
}
bool IRAM_ATTR ADCFastThr::on_monitor_low_(adc_monitor_handle_t monitor_handle,
                                           const adc_monitor_evt_data_t *event_data, void *user_data) {
  static_cast<ADCFastThr *>(user_data)->monitor_state_ = MonitorState::MONITOR_STATE_BELOW_LOW;
  return false;
}

// *******************************************************************************************
//
//   On buffer overflow callback
//
// https://controllerstech.com/esp32-9-how-to-use-adc-part2/
static bool IRAM_ATTR conv_done(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                void *user_data) {
  BaseType_t mustYield = pdFALSE;
  // Notify that ADC continuous driver has done enough number of conversions
  //  vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
  return (mustYield == pdTRUE);
}

// *******************************************************************************************
//
//    ADCFastThr  component class
//

void ADCFastThr::dump_config() {
  ESP_LOGCONFIG(TAG, "ADC Fast Threshold");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG,
                "  Sampling mode: %s\n"
                "  Attenuation: %s\n"
                "  Voltage divider: %.3f\n"
                "  High threshold: %.2f V (raw %" PRId32 ")\n"
                "  Low threshold: %.2f V (raw %" PRId32 ")",
                LOG_STR_ARG(sampling_mode_to_str(this->sampling_mode_)),
                LOG_STR_ARG(attenuation_to_str(this->attenuation_)), this->voltage_divider_, this->high_threshold_,
                this->adc_threshold_.h_threshold, this->low_threshold_, this->adc_threshold_.l_threshold);
  LOG_UPDATE_INTERVAL(this);
}

// *******************************************************************************************
//
//    Setup
//
void ADCFastThr::setup() {
  int adc_rc = 0;

  // ------------------------------------------------------------------------------------------
  //
  //   ADC Continuous Initialization
  //

  adc_handle_ = NULL;  // ?????
  // -------------------------------------------------------------
  // ЭТАП 1: Инициализация и запуск АЦП в непрерывном режиме (DMA)
  // -------------------------------------------------------------
  adc_continuous_handle_cfg_t adc_handle_config_ = {
      .max_store_buf_size = 1024,
      .conv_frame_size = 256,
  };

  // Создаем дескриптор непрерывного режима
  adc_rc = adc_continuous_new_handle(&adc_handle_config_, &adc_handle_);  //  SET adc_handle_
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Register ADC handle: %s", esp_err_to_name(adc_rc));
    this->mark_failed();
    return;
  }

  adc_continuous_config_t adc_config_ = {
      .sample_freq_hz = 20 * 1000, /*!< The expected ADC sampling frequency in Hz. Please refer to `soc/soc_caps.h` to
                                      know available sampling frequency range*/
      // .conv_mode = ADC_CONV_SINGLE_UNIT_1,  ///< ADC DMA conversion mode, see `adc_digi_convert_mode_t`.
      // .format,                              ///< ADC DMA conversion output format, see `adc_digi_output_format_t`.

      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
  };

  ///  Generate empty pattern
  adc_digi_pattern_config_t adc_pattern_[SOC_ADC_PATT_LEN_MAX] = {0};  //  Mem for all available channels
  adc_config_.pattern_num = 1;                                         /// One pattern struct per channel
  adc_pattern_[0].atten = this->attenuation_;
  adc_pattern_[0].channel = this->channel_;
  adc_pattern_[0].unit = this->adc_unit_;
  adc_pattern_[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
  adc_config_.adc_pattern = adc_pattern_;  ///< List of configs for each ADC channel that will be used

  adc_rc = adc_continuous_config(adc_handle_, &adc_config_);
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Error register ADC monitor handle: %s", esp_err_to_name(adc_rc));
    this->mark_failed();
    this->uninit();
    return;
  }
  this->setup_flags_.handle_init_complete = true;

  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = conv_done,
  };
  adc_rc = adc_continuous_register_event_callbacks(adc_handle_, &cbs, &this->raw_);  ///   Конвертация изменений

  // ------------------------------------------------------------------------------------------
  //
  //   Initialize ADC calibration
  //

  if (this->calibration_handle_ == nullptr) {
    adc_cali_handle_t handle = nullptr;

#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
    // RISC-V variants (except C2) and S3 use curve fitting calibration
    adc_cali_curve_fitting_config_t cali_config = {};  // Zero initialize first
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.chan = this->channel_;
#endif  // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    cali_config.unit_id = this->adc_unit_;
    cali_config.atten = this->attenuation_;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    adc_rc = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (adc_rc == ESP_OK) {
      this->calibration_handle_ = handle;
      this->setup_flags_.calibration_complete = true;
      ESP_LOGV(TAG, "Using curve fitting calibration");
    } else {
      ESP_LOGW(TAG, "Curve fitting calibration failed with error %d, will use uncalibrated readings", adc_rc);
      this->setup_flags_.calibration_complete = false;
    }
#else  // ESP32, ESP32-S2, and ESP32-C2 use line fitting calibration
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = this->adc_unit_,
        .atten = this->attenuation_,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
#if !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32C2)
        .default_vref = 1100,  // Default reference voltage in mV
#endif  // !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32C2)
    };
    adc_rc = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (adc_rc == ESP_OK) {
      this->calibration_handle_ = handle;
      this->setup_flags_.calibration_complete = true;
      ESP_LOGV(TAG, "Using line fitting calibration");
    } else {
      ESP_LOGW(TAG, "Line fitting calibration failed with error %d, will use uncalibrated readings", adc_rc);
      this->setup_flags_.calibration_complete = false;
    }
#endif  // ESP32C3 || ESP32C5 || ESP32C6 || ESP32C61 || ESP32H2 || ESP32P4 || ESP32S3
  }

  // ------------------------------------------------------------------------------------------
  //
  //   Configure the ADC monitor
  //
  const int32_t high_raw = this->voltage_to_raw_(this->high_threshold_);
  const int32_t low_raw = this->voltage_to_raw_(this->low_threshold_);
  if (high_raw < 0 || low_raw < 0) {
    this->mark_failed(LOG_STR("Threshold outside the ADC range"));
    this->uninit();
    return;
  }

  this->adc_threshold_ = {
      .adc_unit = this->adc_unit_,  // Specify the ADC unit to monitor
      .channel = this->channel_,    // Specify the ADC channel to monitor
      .h_threshold = high_raw,
      .l_threshold = low_raw,
  };

  // Create the ADC monitor
  adc_rc = adc_new_continuous_monitor(adc_handle_, &adc_threshold_, &adc_monitor_handle_);
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Error register ADC monitor handle: %s", esp_err_to_name(adc_rc));
    this->mark_failed();
    this->uninit();
    return;
  }

  // Register the callback function
  adc_monitor_evt_cbs_t zero_crossing_cbs_ = {
      .on_over_high_thresh = on_monitor_high_,
      .on_below_low_thresh = on_monitor_low_,
  };

  adc_rc = adc_continuous_monitor_register_event_callbacks(adc_monitor_handle_, &zero_crossing_cbs_, this);
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Error register callback %s", esp_err_to_name(adc_rc));
    this->mark_failed();
    this->uninit();
    return;
  }

  adc_rc = adc_continuous_monitor_enable(adc_monitor_handle_);
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Error monitor enable: %s", esp_err_to_name(adc_rc));
    adc_rc = adc_continuous_monitor_disable(adc_monitor_handle_);
    this->mark_failed();
    this->uninit();
    return;
  }

  // https://github.com/espressif/esp-idf/blob/master/components/esp_adc/adc_continuous.c
  adc_rc = adc_continuous_start(adc_handle_);
  if (adc_rc != ESP_OK) {
    ESP_LOGE(TAG, "Error continuous start: %s", esp_err_to_name(adc_rc));
    adc_rc = adc_continuous_monitor_disable(adc_monitor_handle_);
    this->mark_failed();
    this->uninit();
    return;
  }
  this->setup_flags_.config_complete = true;

  this->setup_flags_.init_complete = true;
}

// *******************************************************************************************
//
//   Read sample
//
void ADCFastThr::set_sample_count(uint8_t sample_count) {
  if (sample_count != 0) {
    this->sample_count_ = sample_count;
  }
}

// *******************************************************************************************
//
//   Read sample
//
float ADCFastThr::sample() {
  if (this->autorange_) {
    // return this->sample_autorange_();
    return 0.0f;
  } else {
    return this->sample_fixed_attenuation_();
  }
}
// *******************************************************************************************
//
//   Check the threshold interrupt state and write to the log if it changed
//
void ADCFastThr::loop() {
  const MonitorState state = this->monitor_state_;
  if (state == this->logged_monitor_state_) {
    return;
  }
  this->logged_monitor_state_ = state;
  if (state == MonitorState::MONITOR_STATE_ABOVE_HIGH) {
    ESP_LOGI(TAG, "Threshold crossed: above %.2f V", this->high_threshold_);
  } else {
    ESP_LOGI(TAG, "Threshold crossed: below %.2f V", this->low_threshold_);
  }
}
// *******************************************************************************************
//
//   Read the current voltage and print it to the log
//
void ADCFastThr::update() {
  float value_v = this->sample();
  ESP_LOGV(TAG, "'%s': Voltage=%.4fV", LOG_STR_ARG(this->get_log_name()), value_v);
  this->publish_state(value_v);
}

// *******************************************************************************************
//
//   Read with known attenuation
//

float ADCFastThr::sample_fixed_attenuation_() {
  // Using the read and parse function
  auto aggr = Aggregator<uint32_t>(this->sampling_mode_);
  adc_continuous_data_t parsed_data[64];  // User specifies maximum number of samples
  uint32_t num_samples = 0;
  // float final_value = 0.0;

  esp_err_t adc_rc = adc_continuous_read_parse(adc_handle_, parsed_data, 64, &num_samples, 100);
  if (adc_rc == ESP_OK) {
    this->sample_count_ = num_samples;
    for (int i = 0; i < num_samples; i++) {
      if ((parsed_data[i].valid) && (parsed_data[i].unit == this->adc_unit_) &&
          (parsed_data[i].channel == this->channel_)) {
        aggr.add_sample(parsed_data[i].raw_data);
        ESP_LOGV(TAG, "ADC%d, Channel: %d, Value: %" PRIu32, parsed_data[i].unit + 1, parsed_data[i].channel,
                 parsed_data[i].raw_data);
      }
    }
  } else {
    ESP_LOGE(TAG, "Error on read and parse sample %s", esp_err_to_name(adc_rc));
    return -0.0f;
  }

  uint32_t final_value = aggr.aggregate();

  if (this->output_raw_) {
    return final_value;
  }

  if (this->calibration_handle_ != nullptr) {
    int voltage_mv;
    adc_rc = adc_cali_raw_to_voltage(this->calibration_handle_, final_value, &voltage_mv);
    if (adc_rc == ESP_OK) {
      return voltage_mv / 1000.0f * this->voltage_divider_;
    } else {
      ESP_LOGW(TAG, "ADC calibration conversion failed with error %s, disabling calibration. ",
               esp_err_to_name(adc_rc));
      if (this->calibration_handle_ != nullptr) {
#if USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32C5 || USE_ESP32_VARIANT_ESP32C6 || \
    USE_ESP32_VARIANT_ESP32C61 || USE_ESP32_VARIANT_ESP32H2 || USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S3
        adc_cali_delete_scheme_curve_fitting(this->calibration_handle_);
#else   // Other ESP32 variants use line fitting calibration
        adc_cali_delete_scheme_line_fitting(this->calibration_handle_);
#endif  // ESP32C3 || ESP32C5 || ESP32C6 || ESP32C61 || ESP32H2 || ESP32P4 || ESP32S3
        this->calibration_handle_ = nullptr;
      }
    }
  }

  return final_value * UNCALIBRATED_FULL_SCALE_V / ADC_MAX_RAW * this->voltage_divider_;
}

// *******************************************************************************************
//
//   Convert a threshold at the divider input to a raw ADC value
//
int32_t ADCFastThr::voltage_to_raw_(float voltage) {
  const float pin_v = voltage / this->voltage_divider_;
  if (this->calibration_handle_ == nullptr) {
    const int32_t raw = static_cast<int32_t>(pin_v * ADC_MAX_RAW / UNCALIBRATED_FULL_SCALE_V + 0.5f);
    return raw <= ADC_MAX_RAW ? raw : -1;
  }

  // The calibration only converts raw to voltage, so search for the first raw value that reaches the target.
  const float pin_mv = pin_v * 1000.0f;
  int voltage_mv = 0;
  int32_t low = 0;
  int32_t high = ADC_MAX_RAW;
  while (low < high) {
    const int32_t middle = (low + high) / 2;
    if (adc_cali_raw_to_voltage(this->calibration_handle_, middle, &voltage_mv) != ESP_OK) {
      return -1;
    }
    if (voltage_mv < pin_mv) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  if (adc_cali_raw_to_voltage(this->calibration_handle_, low, &voltage_mv) != ESP_OK || voltage_mv < pin_mv) {
    return -1;
  }
  return low;
}

// *******************************************************************************************
//
//
//
void ADCFastThr::uninit() {
  if (adc_handle_ != nullptr) {
    adc_continuous_deinit(adc_handle_);
  }
}

}  // namespace adc_fast_thr
}  // namespace esphome
