import esphome.codegen as cg
from esphome.components import fatfs
import esphome.config_validation as cv
from esphome.const import PLATFORM_ESP32
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass

# IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@abel-msk"]

FatESP32_ns = cg.esphome_ns.namespace("fatfs_esp32")
FatESP32 = FatESP32_ns.class_("FatESP32", fatfs.FatFS)


def fatfs_esp32_schema(
    class_: MockObjClass,
) -> cv.Schema:
    return fatfs.fatfs_schema(class_)


async def new_esp32_driver(config, *args):
    if CORE.target_platform != PLATFORM_ESP32:
        raise NotImplementedError(
            "This component can be used only on ESP32 platform any variants."
        )
    return await fatfs.new_fatfs(config, *args)


CONFIG_SCHEMA = fatfs.fatfs_schema(FatESP32)


async def to_code(config):
    raise NotImplementedError(
        "This component is API extender for FATFS API. You need fatfs_esp32_... component with device driver."
    )
