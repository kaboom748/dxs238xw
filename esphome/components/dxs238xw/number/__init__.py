import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_MILLISECOND,
    UNIT_MINUTE,
    UNIT_VOLT,
)

from .. import CONF_DXS238XW_ID, Dxs238xwComponent, SmIdEntity, dxs238xw_ns

DEPENDENCIES = ["dxs238xw"]

Dxs238xwNumber = dxs238xw_ns.class_("Dxs238xwNumber", number.Number)

CONF_MAX_CURRENT_LIMIT = "max_current_limit"
CONF_MAX_VOLTAGE_LIMIT = "max_voltage_limit"
CONF_MIN_VOLTAGE_LIMIT = "min_voltage_limit"
CONF_ENERGY_PURCHASE_VALUE = "energy_purchase_value"
CONF_ENERGY_PURCHASE_ALARM = "energy_purchase_alarm"
CONF_DELAY_VALUE_SET = "delay_value_set"
CONF_STARTING_KWH = "starting_kwh"
CONF_PRICE_KWH = "price_kwh"
CONF_TRANSACTION_GAP = "transaction_gap"

MODE_SCHEMA = {
    cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
}

# (schema, min, max, step, entity_id)
TYPES = {
    # Overload protection threshold. The datasheet gives 65 A as the factory
    # default and lists 1-63 A (or 1-80 A on the 80 A variant) as the settable
    # range, so an upper bound of 60 A - as used by rodgon81 - cannot even
    # represent the meter's own default. The wire format is hundredths of an
    # ampere in a uint16, so decimals are kept (rodgon81 rounds to an integer,
    # which is his own TODO).
    CONF_MAX_CURRENT_LIMIT: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:current-ac",
            entity_category=ENTITY_CATEGORY_CONFIG,
            device_class=DEVICE_CLASS_CURRENT,
            unit_of_measurement=UNIT_AMPERE,
        ).extend(MODE_SCHEMA),
        1.0,
        80.0,
        0.01,
        SmIdEntity.NUMBER_MAX_CURRENT_LIMIT,
    ),
    CONF_MAX_VOLTAGE_LIMIT: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:flash-triangle",
            entity_category=ENTITY_CATEGORY_CONFIG,
            device_class=DEVICE_CLASS_VOLTAGE,
            unit_of_measurement=UNIT_VOLT,
        ).extend(MODE_SCHEMA),
        80.0,
        300.0,
        1.0,
        SmIdEntity.NUMBER_MAX_VOLTAGE_LIMIT,
    ),
    CONF_MIN_VOLTAGE_LIMIT: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:flash-triangle-outline",
            entity_category=ENTITY_CATEGORY_CONFIG,
            device_class=DEVICE_CLASS_VOLTAGE,
            unit_of_measurement=UNIT_VOLT,
        ).extend(MODE_SCHEMA),
        80.0,
        300.0,
        1.0,
        SmIdEntity.NUMBER_MIN_VOLTAGE_LIMIT,
    ),
    CONF_ENERGY_PURCHASE_VALUE: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:cash-plus",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
        ).extend(MODE_SCHEMA),
        0.0,
        999999.0,
        1.0,
        SmIdEntity.NUMBER_ENERGY_PURCHASE_VALUE,
    ),
    CONF_ENERGY_PURCHASE_ALARM: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:cash-alert",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
        ).extend(MODE_SCHEMA),
        0.0,
        999999.0,
        1.0,
        SmIdEntity.NUMBER_ENERGY_PURCHASE_ALARM,
    ),
    CONF_DELAY_VALUE_SET: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:timer-cog",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_MINUTE,
        ).extend(MODE_SCHEMA),
        1.0,
        1440.0,
        1.0,
        SmIdEntity.NUMBER_DELAY_VALUE_SET,
    ),
    CONF_STARTING_KWH: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:counter",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
        ).extend(MODE_SCHEMA),
        0.0,
        999999.9,
        0.1,
        SmIdEntity.NUMBER_STARTING_KWH,
    ),
    CONF_PRICE_KWH: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:cash",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ).extend(MODE_SCHEMA),
        0.0,
        999999.9,
        0.1,
        SmIdEntity.NUMBER_PRICE_KWH,
    ),
    # Minimum spacing between two serial transactions. Never restored: takes
    # the hub's `transaction_gap:` value at each boot.
    CONF_TRANSACTION_GAP: (
        number.number_schema(
            Dxs238xwNumber,
            icon="mdi:timer-outline",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_MILLISECOND,
        ).extend(MODE_SCHEMA),
        20.0,
        2000.0,
        10.0,
        SmIdEntity.NUMBER_TRANSACTION_GAP,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DXS238XW_ID): cv.use_id(Dxs238xwComponent),
        **{cv.Optional(type_): schema for type_, (schema, _, _, _, _) in TYPES.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type_, (_, min_value, max_value, step, entity_id) in TYPES.items():
        if conf := config.get(type_):
            var = await number.new_number(
                conf, min_value=min_value, max_value=max_value, step=step
            )
            cg.add(var.set_dxs238xw_parent(parent))
            cg.add(var.set_entity_id(entity_id))
            cg.add(getattr(parent, f"set_{type_}_number")(var))
