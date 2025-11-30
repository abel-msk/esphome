# from esphome import automation, pins
from esphome import pins
import esphome.codegen as cg
from esphome.components import storage
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import VARIANT_ESP32, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    # CONF_DATA,
    CONF_ID,
    # CONF_OUTPUT,
    # CONF_PATH,
    # CONF_PULLDOWN,
    # CONF_PULLUP,
    PLATFORM_ESP32,
)
from esphome.core import CORE

CONF_SD_MMC_CARD_ID = "sd_mmc_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_POWER_CTRL_PIN = "power_ctrl_pin"

sdmmc_component_ns = cg.esphome_ns.namespace("sdmmc")
SdMmc = sdmmc_component_ns.class_("SdMMC", cg.PollingComponent, storage.FileProvider)

# Action
# SdMmcWriteFileAction = sdmmc_component_ns.class_("SdMmcWriteFileAction", automation.Action)
# SdMmcAppendFileAction = sdmmc_component_ns.class_("SdMmcAppendFileAction", automation.Action)
# SdMmcCreateDirectoryAction = sdmmc_component_ns.class_("SdMmcCreateDirectoryAction", automation.Action)
# SdMmcRemoveDirectoryAction = sdmmc_component_ns.class_("SdMmcRemoveDirectoryAction", automation.Action)
# SdMmcDeleteFileAction = sdmmc_component_ns.class_("SdMmcDeleteFileAction", automation.Action)


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("10s"))


async def to_code(config):
    if CORE.target_platform != PLATFORM_ESP32:
        raise NotImplementedError(
            "This component can be used only on ESP32 platform any variants."
        )
    variant = get_esp32_variant()
    if variant not in [VARIANT_ESP32, VARIANT_ESP32S3]:
        raise NotImplementedError(f"Unsupported variant {variant}")

    if CORE.using_arduino and CORE.is_esp32:
        cg.add_library("FS", None)
        cg.add_library("SD_MMC", None)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    if not config[CONF_MODE_1BIT]:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))


# SD_MMC_PATH_ACTION_SCHEMA = cv.Schema(
#     {
#         cv.GenerateID(): cv.use_id(SdMmc),
#         cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
#     }
# )

# SD_MMC_WRITE_FILE_ACTION_SCHEMA = cv.Schema(
#     {
#         cv.GenerateID(): cv.use_id(SdMmc),
#         cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
#         cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
#     }
# ).extend(SD_MMC_PATH_ACTION_SCHEMA)

# @automation.register_action(
#     "sd_mmc_card.write_file", SdMmcWriteFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
# )
# async def sd_mmc_write_file_to_code(config, action_id, template_arg, args):
#     parent = await cg.get_variable(config[CONF_ID])
#     var = cg.new_Pvariable(action_id, template_arg, parent)
#     path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
#     data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
#     cg.add(var.set_path(path_))
#     cg.add(var.set_data(data_))
#     return var


# @automation.register_action(
#     "sd_mmc_card.append_file", SdMmcAppendFileAction, SD_MMC_WRITE_FILE_ACTION_SCHEMA
# )
# async def sd_mmc_append_file_to_code(config, action_id, template_arg, args):
#     parent = await cg.get_variable(config[CONF_ID])
#     var = cg.new_Pvariable(action_id, template_arg, parent)
#     path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
#     data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
#     cg.add(var.set_path(path_))
#     cg.add(var.set_data(data_))
#     return var


# @automation.register_action(
#     "sd_mmc_card.create_directory", SdMmcCreateDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
# )
# async def sd_mmc_create_directory_to_code(config, action_id, template_arg, args):
#     parent = await cg.get_variable(config[CONF_ID])
#     var = cg.new_Pvariable(action_id, template_arg, parent)
#     path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
#     cg.add(var.set_path(path_))
#     return var


# @automation.register_action(
#     "sd_mmc_card.remove_directory", SdMmcRemoveDirectoryAction, SD_MMC_PATH_ACTION_SCHEMA
# )
# async def sd_mmc_remove_directory_to_code(config, action_id, template_arg, args):
#     parent = await cg.get_variable(config[CONF_ID])
#     var = cg.new_Pvariable(action_id, template_arg, parent)
#     path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
#     cg.add(var.set_path(path_))
#     return var


# @automation.register_action(
#     "sd_mmc_card.delete_file", SdMmcDeleteFileAction, SD_MMC_PATH_ACTION_SCHEMA
# )
# async def sd_mmc_delete_file_to_code(config, action_id, template_arg, args):
#     parent = await cg.get_variable(config[CONF_ID])
#     var = cg.new_Pvariable(action_id, template_arg, parent)
#     path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
#     cg.add(var.set_path(path_))
#     return var
