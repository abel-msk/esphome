import esphome.codegen as cg
from esphome.components import fatfs

FatESP32_ns = cg.esphome_ns.namespace("fatfs_esp32")
FatESP32 = FatESP32_ns.class_("FatESP32", fatfs.FatFS)

CONFIG_SCHEMA = fatfs.fatfs_schema(FatESP32)


async def to_code(config):
    raise NotImplementedError(
        "This component is API extender for FATFS API. You need fatfs_esp32_... component with device driver."
    )
