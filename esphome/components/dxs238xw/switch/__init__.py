import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

from .. import CONF_DXS238XW_ID, Dxs238xwComponent, SmIdEntity, dxs238xw_ns

DEPENDENCIES = ["dxs238xw"]

# These switches mirror the meter's physical state, they are not a local
# setpoint: their state must come from the meter, never from a restore.
# With RESTORE_DEFAULT_OFF (rodgon81's choice) every boot would replay the saved
# state and send a command to the relay, overriding for instance a switch-off
# made with the physical button while the ESP was powered down.
# Can be overridden with `restore_mode:` in the YAML.
_RESTORE = "DISABLED"

Dxs238xwSwitch = dxs238xw_ns.class_("Dxs238xwSwitch", switch.Switch)

CONF_ENERGY_PURCHASE_STATE = "energy_purchase_state"
CONF_METER_STATE = "meter_state"
CONF_DELAY_STATE = "delay_state"

TYPES = {
    CONF_ENERGY_PURCHASE_STATE: (
        switch.switch_schema(
            Dxs238xwSwitch,
            icon="mdi:cash-multiple",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode=_RESTORE,
        ),
        SmIdEntity.SWITCH_ENERGY_PURCHASE_STATE,
    ),
    CONF_METER_STATE: (
        switch.switch_schema(
            Dxs238xwSwitch,
            icon="mdi:electric-switch",
            device_class=DEVICE_CLASS_SWITCH,
            default_restore_mode=_RESTORE,
        ),
        SmIdEntity.SWITCH_METER_STATE,
    ),
    CONF_DELAY_STATE: (
        switch.switch_schema(
            Dxs238xwSwitch,
            icon="mdi:timer-cog-outline",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode=_RESTORE,
        ),
        SmIdEntity.SWITCH_DELAY_STATE,
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
            var = await switch.new_switch(conf)
            cg.add(var.set_dxs238xw_parent(parent))
            cg.add(var.set_entity_id(entity_id))
            cg.add(getattr(parent, f"set_{type_}_switch")(var))
