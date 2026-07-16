import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_RESTART, ENTITY_CATEGORY_CONFIG

from .. import CONF_DXS238XW_ID, Dxs238xwComponent, SmIdEntity, dxs238xw_ns

DEPENDENCIES = ["dxs238xw"]

Dxs238xwButton = dxs238xw_ns.class_("Dxs238xwButton", button.Button)

CONF_RESET_DATA = "reset_data"

TYPES = {
    CONF_RESET_DATA: (
        button.button_schema(
            Dxs238xwButton,
            icon="mdi:database-remove",
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        SmIdEntity.BUTTON_RESET_DATA,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DXS238XW_ID): cv.use_id(Dxs238xwComponent),
        **{cv.Optional(type_): schema for type_, (schema, _) in TYPES.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type_, (_, entity_id) in TYPES.items():
        if conf := config.get(type_):
            var = await button.new_button(conf)
            cg.add(var.set_dxs238xw_parent(parent))
            cg.add(var.set_entity_id(entity_id))
            cg.add(getattr(parent, f"set_{type_}_button")(var))
