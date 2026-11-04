from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.adc import ESP32_VARIANT_ADC1_PIN_TO_CHANNEL, adc_unit_t
from esphome.components.esp32 import (
    VARIANT_ESP32C3,
    include_builtin_idf_component,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_NUMBER,
    CONF_PIN,
    CONF_RAW,
    CONF_VOLTAGE_DIVIDER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["esp32"]

adc_fast_thr_ns = cg.esphome_ns.namespace("adc_fast_thr")
ADCFastThr = adc_fast_thr_ns.class_("ADCFastThr", sensor.Sensor, cg.PollingComponent)

# The continuous mode setup in adc_fast_thr.cpp uses ADC1 only.
ADC1_PIN_TO_CHANNEL = ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[VARIANT_ESP32C3]

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "12db": cg.global_ns.ADC_ATTEN_DB_12,
}

CONF_HIGH_THRESHOLD = "high_threshold"
CONF_LOW_THRESHOLD = "low_threshold"
CONF_SAMPLING_MODE = "sampling_mode"

SAMPLING_MODES = adc_fast_thr_ns.enum("SamplingMode", is_class=True).options(
    "avg", "min", "max"
)

# Keep in sync with the initializers in adc_fast_thr.h
DEFAULT_ATTENUATION = "12db"
DEFAULT_SAMPLING_MODE = "avg"
DEFAULT_VOLTAGE_DIVIDER = 1.0

# The highest voltage a pin can be asked to measure. The limit for each attenuation is checked at setup.
MAX_PIN_VOLTAGE = 3.3

_threshold = cv.All(cv.voltage, cv.positive_float)


def validate_adc1_pin(config: ConfigType) -> ConfigType:
    if config[CONF_NUMBER] not in ADC1_PIN_TO_CHANNEL:
        raise cv.Invalid(
            f"ESP32-C3 ADC1 is only available on GPIO{min(ADC1_PIN_TO_CHANNEL)}"
            f" to GPIO{max(ADC1_PIN_TO_CHANNEL)}"
        )
    return config


def validate_thresholds(config: ConfigType) -> ConfigType:
    if config[CONF_LOW_THRESHOLD] >= config[CONF_HIGH_THRESHOLD]:
        raise cv.Invalid(f"{CONF_LOW_THRESHOLD} must be below {CONF_HIGH_THRESHOLD}")
    pin_voltage = config[CONF_HIGH_THRESHOLD] / config[CONF_VOLTAGE_DIVIDER]
    if pin_voltage > MAX_PIN_VOLTAGE:
        raise cv.Invalid(
            f"{CONF_HIGH_THRESHOLD} means {pin_voltage:.2f}V at the pin, the limit is"
            f" {MAX_PIN_VOLTAGE}V. Set {CONF_VOLTAGE_DIVIDER} to the ratio of the"
            " voltage divider"
        )
    return config


CONFIG_SCHEMA = cv.All(
    only_on_variant(supported=[VARIANT_ESP32C3]),
    sensor.sensor_schema(
        ADCFastThr,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): cv.All(
                pins.internal_gpio_input_pin_schema, validate_adc1_pin
            ),
            cv.Optional(CONF_RAW, default=False): cv.boolean,
            cv.Optional(CONF_ATTENUATION, default=DEFAULT_ATTENUATION): cv.enum(
                ATTENUATION_MODES, lower=True
            ),
            cv.Optional(CONF_SAMPLING_MODE, default=DEFAULT_SAMPLING_MODE): cv.enum(
                SAMPLING_MODES, lower=True
            ),
            cv.Required(CONF_HIGH_THRESHOLD): _threshold,
            cv.Required(CONF_LOW_THRESHOLD): _threshold,
            cv.Optional(
                CONF_VOLTAGE_DIVIDER, default=DEFAULT_VOLTAGE_DIVIDER
            ): cv.float_range(min=1.0),
        }
    )
    .extend(cv.polling_component_schema("10s")),
    validate_thresholds,
)


async def to_code(config: ConfigType) -> None:
    # Re-enable ESP-IDF's ADC driver (excluded by default to save compile time)
    include_builtin_idf_component("esp_adc")

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.add_pin(var.set_pin, config[CONF_PIN])

    channel = ADC1_PIN_TO_CHANNEL[config[CONF_PIN][CONF_NUMBER]]
    cg.add(var.set_channel(adc_unit_t.ADC_UNIT_1, channel))

    # Skip the setters when the config matches the C++ initializers.
    if config[CONF_RAW]:
        cg.add(var.set_output_raw(True))
    if (attenuation := config[CONF_ATTENUATION]) != DEFAULT_ATTENUATION:
        cg.add(var.set_attenuation(attenuation))
    if (sampling_mode := config[CONF_SAMPLING_MODE]) != DEFAULT_SAMPLING_MODE:
        cg.add(var.set_sampling_mode(sampling_mode))
    if (voltage_divider := config[CONF_VOLTAGE_DIVIDER]) != DEFAULT_VOLTAGE_DIVIDER:
        cg.add(var.set_voltage_divider(voltage_divider))
    cg.add(var.set_high_threshold(config[CONF_HIGH_THRESHOLD]))
    cg.add(var.set_low_threshold(config[CONF_LOW_THRESHOLD]))
