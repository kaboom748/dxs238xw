import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_DXS238XW_ID, Dxs238xwComponent

DEPENDENCIES = ["dxs238xw"]

CONF_WARNING_OFF_BY_OVER_VOLTAGE = "warning_off_by_over_voltage"
CONF_WARNING_OFF_BY_UNDER_VOLTAGE = "warning_off_by_under_voltage"
CONF_WARNING_OFF_BY_OVER_CURRENT = "warning_off_by_over_current"
CONF_WARNING_OFF_BY_END_PURCHASE = "warning_off_by_end_purchase"
CONF_WARNING_OFF_BY_END_DELAY = "warning_off_by_end_delay"
CONF_WARNING_OFF_BY_USER = "warning_off_by_user"
CONF_WARNING_PURCHASE_ALARM = "warning_purchase_alarm"
CONF_METER_STATE = "meter_state"


def _warning():
    return binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


TYPES = {
    CONF_WARNING_OFF_BY_OVER_VOLTAGE: _warning(),
    CONF_WARNING_OFF_BY_UNDER_VOLTAGE: _warning(),
    CONF_WARNING_OFF_BY_OVER_CURRENT: _warning(),
    CONF_WARNING_OFF_BY_END_PURCHASE: _warning(),
    CONF_WARNING_OFF_BY_END_DELAY: _warning(),
    CONF_WARNING_OFF_BY_USER: _warning(),
    CONF_WARNING_PURCHASE_ALARM: _warning(),
    CONF_METER_STATE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_POWER,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DXS238XW_ID): cv.use_id(Dxs238xwComponent),
        **{cv.Optional(type_): schema for type_, schema in TYPES.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type_ in TYPES:
        if conf := config.get(type_):
            var = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(parent, f"set_{type_}_binary_sensor")(var))
