#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

namespace esphome {
namespace dxs238xw {

static const char *const SM_COMPONENT_VERSION = "2.0.0";

// =============================================================================
// HEKR protocol
// =============================================================================
static const uint8_t HEKR_HEADER = 0x48;

static const uint8_t HEKR_TYPE_RECEIVE = 0x01;
static const uint8_t HEKR_TYPE_SEND = 0x02;

static const uint8_t HEKR_CMD_SEND_GET_METER_STATE = 0x00;
static const uint8_t HEKR_CMD_RECEIVE_METER_STATE = 0x01;
static const uint8_t HEKR_CMD_SEND_GET_LIMIT_AND_PURCHASE = 0x02;
static const uint8_t HEKR_CMD_SEND_SET_LIMIT = 0x03;
static const uint8_t HEKR_CMD_SEND_SET_RESET = 0x05;
static const uint8_t HEKR_CMD_SEND_GET_METER_ID = 0x06;
static const uint8_t HEKR_CMD_RECEIVE_METER_ID = 0x07;
static const uint8_t HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE = 0x08;
static const uint8_t HEKR_CMD_SEND_SET_METER_STATE = 0x09;
static const uint8_t HEKR_CMD_SEND_GET_MEASUREMENT = 0x0A;
static const uint8_t HEKR_CMD_RECEIVE_MEASUREMENT = 0x0B;
static const uint8_t HEKR_CMD_SEND_SET_DELAY = 0x0C;
static const uint8_t HEKR_CMD_SEND_SET_PURCHASE = 0x0D;

// Wildcard response command (hex_message: we cannot know what comes back)
static const uint8_t SM_CMD_ANY = 0xFF;

// =============================================================================
// Sizes and timings
// =============================================================================
static const uint8_t SM_MAX_BYTE_MSG_BUFFER = 96;
static const uint8_t SM_MAX_HEX_MSG_LENGTH = 255;

static const uint16_t SM_MAX_MILLIS_TO_CONFIRM = 200;
static const uint16_t SM_MAX_MILLIS_TO_RESPONSE = 1000;
static const uint16_t SM_MAX_MILLIS_TO_LISTEN = 200;

static const uint8_t SM_QUEUE_DEPTH = 8;
static const uint8_t SM_MAX_PAYLOAD = 9;

// The meter encodes negative (export) values as offset binary on 24 bits with
// an offset of 1,000,000 raw units - which is 1000.0 A at 0.001 A/LSB and
// 100.0 kW at 0.0001 kW/LSB, hence the single constant.
//
// The correction must be applied to the RAW integer, never to the scaled float:
// 1000.0f - 1000.5f loses every bit of the result (the float spacing at
// magnitude 1000 is 6.1e-5), while 1000500 - 1000000 is exact.
static const uint32_t SM_SIGN_OFFSET_RAW = 1000000;

// =============================================================================
// Preference keys (named: they survive an entity rename)
// =============================================================================
static const char *const SM_PREF_DELAY_VALUE_SET = "dxs238xw_delay_value_set";
static const char *const SM_PREF_STARTING_KWH = "dxs238xw_starting_kwh";
static const char *const SM_PREF_PRICE_KWH = "dxs238xw_price_kwh";
static const char *const SM_PREF_ENERGY_PURCHASE_VALUE = "dxs238xw_energy_purchase_value";
static const char *const SM_PREF_ENERGY_PURCHASE_ALARM = "dxs238xw_energy_purchase_alarm";

// =============================================================================
// Enums
// =============================================================================
enum SmIdEntity : uint8_t {
  ID_NULL = 0,

  NUMBER_MAX_CURRENT_LIMIT = 11,
  NUMBER_MAX_VOLTAGE_LIMIT = 12,
  NUMBER_MIN_VOLTAGE_LIMIT = 13,
  NUMBER_ENERGY_PURCHASE_VALUE = 14,
  NUMBER_ENERGY_PURCHASE_ALARM = 15,
  NUMBER_DELAY_VALUE_SET = 16,
  NUMBER_STARTING_KWH = 17,
  NUMBER_PRICE_KWH = 18,
  NUMBER_TRANSACTION_GAP = 19,

  SWITCH_ENERGY_PURCHASE_STATE = 21,
  SWITCH_METER_STATE = 22,
  SWITCH_DELAY_STATE = 23,

  BUTTON_RESET_DATA = 31,
};

enum SmMeterStateDetail : uint8_t {
  DETAIL_UNSET = 0,
  DETAIL_POWER_OK,
  DETAIL_OVER_CURRENT,
  DETAIL_OVER_VOLTAGE,
  DETAIL_UNDER_VOLTAGE,
  DETAIL_END_DELAY,
  DETAIL_END_PURCHASE,
  DETAIL_BY_USER,
  DETAIL_UNKNOWN,
};

// What to do when the delay timer elapses.
// The meter's own firmware already drives the relay at that point, so by
// default ESPHome only observes.
enum SmDelayEndAction : uint8_t {
  DELAY_END_NONE = 0,   // do not interfere (default)
  DELAY_END_DISARM,     // disarm the timer, leave the relay alone
  DELAY_END_FORCE_OFF,  // force the relay off (rodgon81 behaviour)
};

enum SmError : uint8_t {
  ERR_NONE = 0,
  ERR_CONFIRM_TIMEOUT,
  ERR_CONFIRM_INVALID,
  ERR_RESPONSE_TIMEOUT,
  ERR_CRC,
  ERR_INPUT_DATA,
  ERR_QUEUE_FULL,
  ERR_NOT_READY,
};

// State machine states
enum SmState : uint8_t {
  ST_IDLE = 0,
  ST_SEND,
  ST_CONFIRM,
  ST_RESPONSE,
  ST_PARSE,
  ST_LISTEN,
};

// Frame pump result
enum SmFrame : uint8_t {
  FRAME_NONE = 0,
  FRAME_OK,
  FRAME_CRC,
};

// Periodic poll slots (fair round-robin)
enum SmPoll : uint8_t {
  POLL_METER_STATE = 0,
  POLL_LIMIT_PURCHASE,
  POLL_MEASUREMENT,
  POLL_COUNT,
};

// =============================================================================
// Structures
// =============================================================================
struct SmCommand {
  uint8_t send;
  uint8_t receive;
  uint8_t payload[SM_MAX_PAYLOAD];
  uint8_t size;
  bool by_user;  // switch-off explicitly requested by the user
};

struct SmPollSlot {
  uint8_t send;
  uint8_t receive;
  uint32_t interval;
  uint32_t last;
  bool due;
};

class Dxs238xwComponent;

// =============================================================================
// Optional entity macros (conditional compilation)
// =============================================================================
#ifdef USE_SENSOR
#define DXS238XW_SENSOR(name) \
 protected: \
  sensor::Sensor *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *s) { this->name##_sensor_ = s; }
#else
#define DXS238XW_SENSOR(name)
#endif

#ifdef USE_BINARY_SENSOR
#define DXS238XW_BINARY_SENSOR(name) \
 protected: \
  binary_sensor::BinarySensor *name##_binary_sensor_{nullptr}; \
\
 public: \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->name##_binary_sensor_ = s; }
#else
#define DXS238XW_BINARY_SENSOR(name)
#endif

#ifdef USE_TEXT_SENSOR
#define DXS238XW_TEXT_SENSOR(name) \
 protected: \
  text_sensor::TextSensor *name##_text_sensor_{nullptr}; \
\
 public: \
  void set_##name##_text_sensor(text_sensor::TextSensor *s) { this->name##_text_sensor_ = s; }
#else
#define DXS238XW_TEXT_SENSOR(name)
#endif

#ifdef USE_NUMBER
#define DXS238XW_NUMBER(name) \
 protected: \
  number::Number *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(number::Number *n) { this->name##_number_ = n; }
#else
#define DXS238XW_NUMBER(name)
#endif

#ifdef USE_SWITCH
#define DXS238XW_SWITCH(name) \
 protected: \
  switch_::Switch *name##_switch_{nullptr}; \
\
 public: \
  void set_##name##_switch(switch_::Switch *s) { this->name##_switch_ = s; }
#else
#define DXS238XW_SWITCH(name)
#endif

#ifdef USE_BUTTON
#define DXS238XW_BUTTON(name) \
 protected: \
  button::Button *name##_button_{nullptr}; \
\
 public: \
  void set_##name##_button(button::Button *b) { this->name##_button_ = b; }
#else
#define DXS238XW_BUTTON(name)
#endif

// =============================================================================
// Hub
// =============================================================================
class Dxs238xwComponent : public PollingComponent, public uart::UARTDevice {
  DXS238XW_SENSOR(current_phase_1)
  DXS238XW_SENSOR(current_phase_2)
  DXS238XW_SENSOR(current_phase_3)
  DXS238XW_SENSOR(voltage_phase_1)
  DXS238XW_SENSOR(voltage_phase_2)
  DXS238XW_SENSOR(voltage_phase_3)
  DXS238XW_SENSOR(reactive_power_total)
  DXS238XW_SENSOR(reactive_power_phase_1)
  DXS238XW_SENSOR(reactive_power_phase_2)
  DXS238XW_SENSOR(reactive_power_phase_3)
  DXS238XW_SENSOR(active_power_total)
  DXS238XW_SENSOR(active_power_phase_1)
  DXS238XW_SENSOR(active_power_phase_2)
  DXS238XW_SENSOR(active_power_phase_3)
  DXS238XW_SENSOR(power_factor_total)
  DXS238XW_SENSOR(power_factor_phase_1)
  DXS238XW_SENSOR(power_factor_phase_2)
  DXS238XW_SENSOR(power_factor_phase_3)
  DXS238XW_SENSOR(frequency)
  DXS238XW_SENSOR(import_active_energy)
  DXS238XW_SENSOR(export_active_energy)
  DXS238XW_SENSOR(total_energy)
  DXS238XW_SENSOR(energy_purchase_balance)
  DXS238XW_SENSOR(phase_count)
  DXS238XW_SENSOR(energy_purchase_price)
  DXS238XW_SENSOR(total_energy_price)
  DXS238XW_SENSOR(contract_total_energy)
  DXS238XW_SENSOR(price_kwh)
  DXS238XW_SENSOR(diag_success)
  DXS238XW_SENSOR(diag_confirm_timeout)
  DXS238XW_SENSOR(diag_response_timeout)
  DXS238XW_SENSOR(diag_crc_error)

  DXS238XW_BINARY_SENSOR(warning_off_by_over_voltage)
  DXS238XW_BINARY_SENSOR(warning_off_by_under_voltage)
  DXS238XW_BINARY_SENSOR(warning_off_by_over_current)
  DXS238XW_BINARY_SENSOR(warning_off_by_end_purchase)
  DXS238XW_BINARY_SENSOR(warning_off_by_end_delay)
  DXS238XW_BINARY_SENSOR(warning_off_by_user)
  DXS238XW_BINARY_SENSOR(warning_purchase_alarm)
  DXS238XW_BINARY_SENSOR(meter_state)

  DXS238XW_TEXT_SENSOR(meter_state_detail)
  DXS238XW_TEXT_SENSOR(delay_value_remaining)
  DXS238XW_TEXT_SENSOR(meter_id)
  DXS238XW_TEXT_SENSOR(last_error)

  DXS238XW_NUMBER(max_current_limit)
  DXS238XW_NUMBER(max_voltage_limit)
  DXS238XW_NUMBER(min_voltage_limit)
  DXS238XW_NUMBER(energy_purchase_value)
  DXS238XW_NUMBER(energy_purchase_alarm)
  DXS238XW_NUMBER(delay_value_set)
  DXS238XW_NUMBER(starting_kwh)
  DXS238XW_NUMBER(price_kwh)
  DXS238XW_NUMBER(transaction_gap)

  DXS238XW_SWITCH(energy_purchase_state)
  DXS238XW_SWITCH(meter_state)
  DXS238XW_SWITCH(delay_state)

  DXS238XW_BUTTON(reset_data)

 public:
  Dxs238xwComponent() = default;

  // --- configuration (from __init__.py) ---
  void set_preference_seed(uint32_t seed) { this->pref_seed_ = seed; }
  void set_meter_state_interval(uint32_t v) { this->polls_[POLL_METER_STATE].interval = v; }
  void set_limit_purchase_interval(uint32_t v) { this->polls_[POLL_LIMIT_PURCHASE].interval = v; }
  void set_default_transaction_gap(uint32_t v) { this->transaction_gap_ = v; }
  void set_error_threshold(uint32_t v) { this->error_threshold_ = v; }
  void set_delay_end_action(SmDelayEndAction v) { this->delay_end_action_ = v; }

  // --- lifecycle ---
  void setup() override;
  void loop() override;    // non-blocking state machine
  void update() override;  // <=> update_interval: measurement cadence
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // --- public API (YAML actions and lambdas) ---
  void meter_state_on() { this->set_meter_state_(true); }
  void meter_state_off() { this->set_meter_state_(false); }
  void meter_state_toggle() { this->set_meter_state_(!this->meter_state_val_); }
  void hex_message(std::string message, bool check_crc = true);

  // --- entity callbacks ---
  void set_number_value(SmIdEntity entity, float value);
  void set_switch_value(SmIdEntity entity, bool value);
  void set_button_value(SmIdEntity entity);

 protected:
  // --- command queue (FIFO) ---
  bool queue_command_(uint8_t send, uint8_t receive, const uint8_t *data = nullptr, uint8_t size = 0,
                      bool by_user = false);
  void queue_set_limit_();
  void queue_set_purchase_(bool state);
  void queue_set_delay_(bool state);
  void set_meter_state_(bool state);

  // --- state machine ---
  uint8_t pump_frame_(uint8_t type_message);
  void end_transaction_(bool success, SmError error = ERR_NONE);
  void purge_rx_();
  static uint8_t crc_of_(const uint8_t *data, uint8_t len);

  /// Decode an offset-binary reading into a signed value. Exact by construction.
  static float apply_sign_(uint32_t raw, float scale) {
    if (raw >= SM_SIGN_OFFSET_RAW)
      return -static_cast<float>(raw - SM_SIGN_OFFSET_RAW) * scale;
    return static_cast<float>(raw) * scale;
  }

  static uint16_t get_u16_(const uint8_t *b) { return (static_cast<uint16_t>(b[0]) << 8) | b[1]; }
  static uint32_t get_u24_(const uint8_t *b) {
    return (static_cast<uint32_t>(b[0]) << 16) | (static_cast<uint32_t>(b[1]) << 8) | b[2];
  }
  static uint32_t get_u32_(const uint8_t *b) {
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) | b[3];
  }

  // --- parsing and publishing ---
  void parse_frame_();
  void parse_meter_state_();
  void parse_limit_and_purchase_();
  void parse_measurement_();
  void parse_meter_id_();
  void publish_meter_state_detail_();
  void publish_derived_();
  void publish_diagnostics_();
  void report_error_(SmError error);

  // --- preferences ---
  uint32_t pref_hash_(const char *name) const { return fnv1_hash(name) ^ this->pref_seed_; }
  static std::string delay_remaining_string_(uint16_t minutes);

  // ---------------------------------------------------------------------------
  // State machine
  // ---------------------------------------------------------------------------
  SmState state_{ST_IDLE};
  uint8_t seq_{0};
  uint8_t buf_[SM_MAX_BYTE_MSG_BUFFER]{};
  uint8_t idx_{0};
  uint32_t start_{0};        // timestamp of entry into the current state
  uint32_t last_tx_end_{0};  // end of the last transaction (for transaction_gap)
  uint8_t last_sent_len_{0};
  uint8_t expected_{0};

  // Command currently being sent
  uint8_t next_cmd_{0};
  uint8_t next_payload_[SM_MAX_PAYLOAD]{};
  uint8_t next_payload_size_{0};
  bool cmd_in_flight_{false};
  bool cmd_by_user_{false};

  // FIFO
  SmCommand queue_[SM_QUEUE_DEPTH]{};
  uint8_t q_head_{0};
  uint8_t q_tail_{0};
  uint8_t q_count_{0};

  // Raw frame (hex_message): kept out of the FIFO, payload too large
  uint8_t raw_frame_[SM_MAX_BYTE_MSG_BUFFER]{};
  uint8_t raw_len_{0};
  bool raw_pending_{false};
  bool raw_send_{false};

  // Polling
  SmPollSlot polls_[POLL_COUNT]{};
  uint8_t poll_idx_{0};
  bool meter_id_done_{false};
  uint8_t meter_id_tries_{0};

  // ---------------------------------------------------------------------------
  // Meter data (the hub owns it, entities are views onto it)
  // ---------------------------------------------------------------------------
  bool meter_state_val_{false};
  bool delay_state_val_{false};
  bool purchase_state_val_{false};
  uint8_t phase_count_val_{0};
  uint16_t delay_value_remaining_{0};
  float total_energy_val_{0};
  float purchase_balance_{0};

  // Limits (read from the meter)
  bool limits_valid_{false};
  float max_current_limit_val_{0};
  uint16_t max_voltage_limit_val_{0};
  uint16_t min_voltage_limit_val_{0};

  // Persisted values
  uint32_t delay_value_set_val_{1440};
  uint32_t purchase_value_val_{0};
  uint32_t purchase_alarm_val_{0};
  float starting_kwh_val_{0};
  float price_kwh_val_{0};

  // Sticky warnings
  bool warn_over_voltage_{false};
  bool warn_under_voltage_{false};
  bool warn_over_current_{false};
  bool warn_end_purchase_{false};
  bool warn_end_delay_{false};
  bool warn_by_user_{false};
  bool warn_purchase_alarm_{false};
  SmMeterStateDetail detail_{DETAIL_UNSET};
  bool first_meter_state_{true};
  bool delay_action_done_{false};
  uint16_t prev_delay_remaining_{0};

  // ---------------------------------------------------------------------------
  // Diagnostics
  // ---------------------------------------------------------------------------
  uint32_t stat_success_{0};
  uint32_t stat_confirm_timeout_{0};
  uint32_t stat_response_timeout_{0};
  uint32_t stat_crc_error_{0};
  uint32_t consecutive_errors_{0};
  uint32_t error_threshold_{5};
  bool status_error_set_{false};

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------
  uint32_t transaction_gap_{500};
  uint32_t pref_seed_{0};
  SmDelayEndAction delay_end_action_{DELAY_END_NONE};

  ESPPreferenceObject pref_delay_value_set_;
  ESPPreferenceObject pref_starting_kwh_;
  ESPPreferenceObject pref_price_kwh_;
  ESPPreferenceObject pref_purchase_value_;
  ESPPreferenceObject pref_purchase_alarm_;
};

}  // namespace dxs238xw
}  // namespace esphome
