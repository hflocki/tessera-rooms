"""Rooms: a room on a tile with its switches, plugs and lights. This file stays this short:
smart_display.register_plugin() reads the id, version, tile memory and the screen's texts from the manifest and
translations/ beside this folder (docs/FIRMWARE_API.md)."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

rooms_ns = cg.esphome_ns.namespace("rooms")
RoomsPlugin = rooms_ns.class_("RoomsPlugin", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(RoomsPlugin)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
