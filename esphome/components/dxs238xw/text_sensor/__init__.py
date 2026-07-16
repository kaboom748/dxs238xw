import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_DXS238XW_ID, Dxs238xwComponent

DEPENDENCIES = ["dxs238xw"]

CONF_METER_STATE_DETAIL = "meter_state_detail"
CONF_DELAY_VALUE_REMAINING = "delay_value_remaining"
CONF_METER_ID = "meter_id"
CONF_LAST_ERROR = "last_error"

TYPES = {
    CONF_METER_STATE_DETAIL: text_sensor.text_sensor_schema(
        icon="mdi:message-alert",
    ),
    CONF_DELAY_VALUE_REMAINING: text_sensor.text_sensor_schema(
        icon="mdi:timer-sand",
    ),
    CONF_METER_ID: text_sensor.text_sensor_schema(
        icon="mdi:identifier",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_LAST_ERROR: text_sensor.text_sensor_schema(
        icon="mdi:alert-circle-outline",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
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
            var = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(parent, f"set_{type_}_text_sensor")(var))
