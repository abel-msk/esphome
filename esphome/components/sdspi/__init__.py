# from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi, storage
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@abel-msk"]
AUTO_LOAD = ["storage"]
DEPENDENCIES = ["spi"]

sdspi_ns = cg.esphome_ns.namespace("sdspi")
sdspi = sdspi_ns.class_("SDSPI", cg.Component, storage.RawStorage)


CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(sdspi)}).extend(
    spi.spi_device_schema(cs_pin_required=True, default_mode="MODE0")
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
