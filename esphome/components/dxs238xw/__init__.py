from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MESSAGE

CODEOWNERS = ["@kaboom748"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_DXS238XW_ID = "dxs238xw_id"
CONF_METER_STATE_INTERVAL = "meter_state_interval"
CONF_LIMIT_PURCHASE_INTERVAL = "limit_purchase_interval"
CONF_TRANSACTION_GAP = "transaction_gap"
CONF_ERROR_THRESHOLD = "error_threshold"
CONF_DELAY_END_ACTION = "delay_end_action"
CONF_CHECK_CRC = "check_crc"

dxs238xw_ns = cg.esphome_ns.namespace("dxs238xw")

Dxs238xwComponent = dxs238xw_ns.class_(
    "Dxs238xwComponent", cg.PollingComponent, uart.UARTDevice
)

SmIdEntity = dxs238xw_ns.enum("SmIdEntity")
SmDelayEndAction = dxs238xw_ns.enum("SmDelayEndAction")

# What to do when the delay timer elapses. The meter's own firmware already
# drives the relay at that point, so ESPHome only observes by default.
DELAY_END_ACTIONS = {
    "NONE": SmDelayEndAction.DELAY_END_NONE,
    "DISARM": SmDelayEndAction.DELAY_END_DISARM,
    "FORCE_OFF": SmDelayEndAction.DELAY_END_FORCE_OFF,
}

MeterStateOnAction = dxs238xw_ns.class_("MeterStateOnAction", automation.Action)
MeterStateOffAction = dxs238xw_ns.class_("MeterStateOffAction", automation.Action)
MeterStateToggleAction = dxs238xw_ns.class_("MeterStateToggleAction", automation.Action)
HexMessageAction = dxs238xw_ns.class_("HexMessageAction", automation.Action)


def _fnv1_hash(text: str) -> int:
    """Mirror of esphome::fnv1_hash() on the Python side (preference seed)."""
    hash_ = 2166136261
    for byte in text.encode("utf-8"):
        hash_ = (hash_ * 16777619) & 0xFFFFFFFF
        hash_ ^= byte
    return hash_


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Dxs238xwComponent),
            cv.Optional(
                CONF_METER_STATE_INTERVAL, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_LIMIT_PURCHASE_INTERVAL, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TRANSACTION_GAP, default="500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=20),
                    max=cv.TimePeriod(milliseconds=2000),
                ),
            ),
            cv.Optional(CONF_ERROR_THRESHOLD, default=5): cv.int_range(min=1, max=100),
            cv.Optional(CONF_DELAY_END_ACTION, default="NONE"): cv.enum(
                DELAY_END_ACTIONS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("3s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

# The meter requires 9600 8N1. The native validator also checks that the bus is
# not shared with another component and that tx_pin/rx_pin are declared.
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "dxs238xw",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_preference_seed(_fnv1_hash(str(config[CONF_ID]))))
    cg.add(var.set_meter_state_interval(config[CONF_METER_STATE_INTERVAL]))
    cg.add(var.set_limit_purchase_interval(config[CONF_LIMIT_PURCHASE_INTERVAL]))
    cg.add(var.set_default_transaction_gap(config[CONF_TRANSACTION_GAP]))
    cg.add(var.set_error_threshold(config[CONF_ERROR_THRESHOLD]))
    cg.add(var.set_delay_end_action(config[CONF_DELAY_END_ACTION]))


# =============================================================================
# Actions
# =============================================================================
SIMPLE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Dxs238xwComponent),
    }
)


# synchronous=True: play() queues the command and returns immediately, the
# actual transmission is done by loop(). play_next_() is never deferred.
@automation.register_action(
    "dxs238xw.meter_state_on",
    MeterStateOnAction,
    SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "dxs238xw.meter_state_off",
    MeterStateOffAction,
    SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "dxs238xw.meter_state_toggle",
    MeterStateToggleAction,
    SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
async def meter_state_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


HEX_MESSAGE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Dxs238xwComponent),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string_strict),
        cv.Optional(CONF_CHECK_CRC, default=True): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "dxs238xw.hex_message",
    HexMessageAction,
    HEX_MESSAGE_ACTION_SCHEMA,
    synchronous=True,
)
async def hex_message_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))

    template_ = await cg.templatable(config[CONF_CHECK_CRC], args, bool)
    cg.add(var.set_check_crc(template_))

    return var
