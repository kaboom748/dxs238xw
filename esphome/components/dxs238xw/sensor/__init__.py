import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_EXPORT_ACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_IMPORT_ACTIVE_ENERGY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_MONETARY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_KILOVOLT_AMPS_REACTIVE,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
)

from .. import CONF_DXS238XW_ID, Dxs238xwComponent

DEPENDENCIES = ["dxs238xw"]

CONF_CURRENT_PHASE_1 = "current_phase_1"
CONF_CURRENT_PHASE_2 = "current_phase_2"
CONF_CURRENT_PHASE_3 = "current_phase_3"
CONF_VOLTAGE_PHASE_1 = "voltage_phase_1"
CONF_VOLTAGE_PHASE_2 = "voltage_phase_2"
CONF_VOLTAGE_PHASE_3 = "voltage_phase_3"
CONF_REACTIVE_POWER_TOTAL = "reactive_power_total"
CONF_REACTIVE_POWER_PHASE_1 = "reactive_power_phase_1"
CONF_REACTIVE_POWER_PHASE_2 = "reactive_power_phase_2"
CONF_REACTIVE_POWER_PHASE_3 = "reactive_power_phase_3"
CONF_ACTIVE_POWER_TOTAL = "active_power_total"
CONF_ACTIVE_POWER_PHASE_1 = "active_power_phase_1"
CONF_ACTIVE_POWER_PHASE_2 = "active_power_phase_2"
CONF_ACTIVE_POWER_PHASE_3 = "active_power_phase_3"
CONF_POWER_FACTOR_TOTAL = "power_factor_total"
CONF_POWER_FACTOR_PHASE_1 = "power_factor_phase_1"
CONF_POWER_FACTOR_PHASE_2 = "power_factor_phase_2"
CONF_POWER_FACTOR_PHASE_3 = "power_factor_phase_3"
CONF_TOTAL_ENERGY = "total_energy"
CONF_ENERGY_PURCHASE_BALANCE = "energy_purchase_balance"
CONF_PHASE_COUNT = "phase_count"
CONF_ENERGY_PURCHASE_PRICE = "energy_purchase_price"
CONF_TOTAL_ENERGY_PRICE = "total_energy_price"
CONF_CONTRACT_TOTAL_ENERGY = "contract_total_energy"
CONF_PRICE_KWH = "price_kwh"

CONF_DIAG_SUCCESS = "diag_success"
CONF_DIAG_CONFIRM_TIMEOUT = "diag_confirm_timeout"
CONF_DIAG_RESPONSE_TIMEOUT = "diag_response_timeout"
CONF_DIAG_CRC_ERROR = "diag_crc_error"


def _current(icon=None):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _voltage():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _reactive_power():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
        accuracy_decimals=4,
        device_class=DEVICE_CLASS_REACTIVE_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _active_power():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT,
        accuracy_decimals=4,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _power_factor():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _diag():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:counter",
    )


TYPES = {
    # --- currents (sign corrected: export shows as a negative value) ---
    CONF_CURRENT_PHASE_1: _current(),
    CONF_CURRENT_PHASE_2: _current(),
    CONF_CURRENT_PHASE_3: _current(),
    # --- voltages ---
    CONF_VOLTAGE_PHASE_1: _voltage(),
    CONF_VOLTAGE_PHASE_2: _voltage(),
    CONF_VOLTAGE_PHASE_3: _voltage(),
    # --- reactive power (sign corrected) ---
    CONF_REACTIVE_POWER_TOTAL: _reactive_power(),
    CONF_REACTIVE_POWER_PHASE_1: _reactive_power(),
    CONF_REACTIVE_POWER_PHASE_2: _reactive_power(),
    CONF_REACTIVE_POWER_PHASE_3: _reactive_power(),
    # --- active power (sign corrected) ---
    CONF_ACTIVE_POWER_TOTAL: _active_power(),
    CONF_ACTIVE_POWER_PHASE_1: _active_power(),
    CONF_ACTIVE_POWER_PHASE_2: _active_power(),
    CONF_ACTIVE_POWER_PHASE_3: _active_power(),
    # --- power factors ---
    CONF_POWER_FACTOR_TOTAL: _power_factor(),
    CONF_POWER_FACTOR_PHASE_1: _power_factor(),
    CONF_POWER_FACTOR_PHASE_2: _power_factor(),
    CONF_POWER_FACTOR_PHASE_3: _power_factor(),
    # --- frequency ---
    CONF_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # --- energies ---
    CONF_IMPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_EXPORT_ACTIVE_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    # The meter accumulates reverse energy INTO the total, so
    # total = |import| + |export| (DTS238-7 manual, 3.1 and APP section).
    # It only ever grows, and the reset button sets it back to zero, which is
    # exactly what total_increasing handles. rodgon81 uses `total` here.
    CONF_TOTAL_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    # --- energy purchase ---
    CONF_ENERGY_PURCHASE_BALANCE: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # --- misc ---
    CONF_PHASE_COUNT: sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:sine-wave",
    ),
    # --- pricing ---
    CONF_ENERGY_PURCHASE_PRICE: sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_MONETARY,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:cash",
    ),
    CONF_TOTAL_ENERGY_PRICE: sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_MONETARY,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:cash",
    ),
    # starting_kwh + total_energy. Unlike total_energy this carries a
    # user-adjustable offset that can jump either way, so `total` rather than
    # `total_increasing`: an offset decrease must not be read as a meter reset.
    CONF_CONTRACT_TOTAL_ENERGY: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL,
    ),
    CONF_PRICE_KWH: sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_MONETARY,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:cash",
    ),
    # --- serial link diagnostics ---
    CONF_DIAG_SUCCESS: _diag(),
    CONF_DIAG_CONFIRM_TIMEOUT: _diag(),
    CONF_DIAG_RESPONSE_TIMEOUT: _diag(),
    CONF_DIAG_CRC_ERROR: _diag(),
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
            var = await sensor.new_sensor(conf)
            cg.add(getattr(parent, f"set_{type_}_sensor")(var))
