#include "dxs238xw.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw";

// =============================================================================
// Deduplicated publishing
//   - binary_sensor and switch already deduplicate internally (publish_dedup_)
//   - sensor, text_sensor and number do not, so we do it here
// =============================================================================
#ifdef USE_SENSOR
#define UPDATE_SENSOR(name, value) \
  if (this->name##_sensor_ != nullptr) { \
    const float v_ = (value); \
    if (!this->name##_sensor_->has_state() || this->name##_sensor_->get_raw_state() != v_) { \
      this->name##_sensor_->publish_state(v_); \
    } \
  }
#else
#define UPDATE_SENSOR(name, value)
#endif

#ifdef USE_TEXT_SENSOR
#define UPDATE_TEXT_SENSOR(name, value) \
  if (this->name##_text_sensor_ != nullptr) { \
    const std::string s_ = (value); \
    if (!this->name##_text_sensor_->has_state() || this->name##_text_sensor_->get_raw_state() != s_) { \
      this->name##_text_sensor_->publish_state(s_); \
    } \
  }
#else
#define UPDATE_TEXT_SENSOR(name, value)
#endif

#ifdef USE_BINARY_SENSOR
#define UPDATE_BINARY_SENSOR(name, value) \
  if (this->name##_binary_sensor_ != nullptr) { \
    this->name##_binary_sensor_->publish_state(value); \
  }
#else
#define UPDATE_BINARY_SENSOR(name, value)
#endif

#ifdef USE_NUMBER
#define UPDATE_NUMBER(name, value) \
  if (this->name##_number_ != nullptr) { \
    const float v_ = (value); \
    if (!this->name##_number_->has_state() || this->name##_number_->state != v_) { \
      this->name##_number_->publish_state(v_); \
    } \
  }
#else
#define UPDATE_NUMBER(name, value)
#endif

#ifdef USE_SWITCH
#define UPDATE_SWITCH(name, value) \
  if (this->name##_switch_ != nullptr) { \
    this->name##_switch_->publish_state(value); \
  }
#else
#define UPDATE_SWITCH(name, value)
#endif

// =============================================================================
// Local helpers
// =============================================================================
static void log_frame_(const char *prefix, const uint8_t *data, uint8_t len) {
  // Hex formatting is compiled out when the log level would never show it:
  // no point paying for it on every frame on an ESP8266.
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[3 * 32 + 1];
  const uint8_t n = len > 32 ? 32 : len;
  format_hex_pretty_to(hex, sizeof(hex), data, n);
  ESP_LOGV(TAG, "%s (%u B): %s%s", prefix, len, hex, len > n ? " ..." : "");
#else
  (void) prefix;
  (void) data;
  (void) len;
#endif
}

uint8_t Dxs238xwComponent::crc_of_(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0;
  for (uint8_t i = 0; i < len - 1; i++)
    crc += data[i];
  return crc & 0xFF;
}

std::string Dxs238xwComponent::delay_remaining_string_(uint16_t minutes) {
  // Formatted on a stack buffer: integer-to-text conversion plus string
  // concatenation would allocate on every frame, which fragments the heap of a
  // long-running device. The delay tops out at 24 h, so "1d 0h 0m" is the
  // widest possible output.
  const uint16_t days = minutes / 1440;
  const uint8_t hours = (minutes % 1440) / 60;
  const uint8_t mins = minutes % 60;

  char buf[24];
  if (days != 0) {
    snprintf(buf, sizeof(buf), "%ud %uh %um", days, hours, mins);
  } else if (hours != 0) {
    snprintf(buf, sizeof(buf), "%uh %um", hours, mins);
  } else {
    snprintf(buf, sizeof(buf), "%um", mins);
  }
  return buf;
}

// =============================================================================
// Lifecycle
// =============================================================================
void Dxs238xwComponent::setup() {
  // --- poll slots ---
  this->polls_[POLL_METER_STATE].send = HEKR_CMD_SEND_GET_METER_STATE;
  this->polls_[POLL_METER_STATE].receive = HEKR_CMD_RECEIVE_METER_STATE;
  this->polls_[POLL_LIMIT_PURCHASE].send = HEKR_CMD_SEND_GET_LIMIT_AND_PURCHASE;
  this->polls_[POLL_LIMIT_PURCHASE].receive = HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE;
  this->polls_[POLL_MEASUREMENT].send = HEKR_CMD_SEND_GET_MEASUREMENT;
  this->polls_[POLL_MEASUREMENT].receive = HEKR_CMD_RECEIVE_MEASUREMENT;
  // The measurement cadence is driven by update() (<=> update_interval), not
  // by an internal interval.
  this->polls_[POLL_MEASUREMENT].interval = 0;

  // Initial acquisition: everything is due on the first loop() pass. Same
  // effect as first_data_acquisition_() but without blocking setup().
  for (uint8_t i = 0; i < POLL_COUNT; i++)
    this->polls_[i].due = true;

  // --- preferences (named keys: they survive an entity rename) ---
  this->pref_delay_value_set_ =
      global_preferences->make_preference<uint32_t>(this->pref_hash_(SM_PREF_DELAY_VALUE_SET));
  this->pref_starting_kwh_ = global_preferences->make_preference<float>(this->pref_hash_(SM_PREF_STARTING_KWH));
  this->pref_price_kwh_ = global_preferences->make_preference<float>(this->pref_hash_(SM_PREF_PRICE_KWH));
  this->pref_purchase_value_ =
      global_preferences->make_preference<uint32_t>(this->pref_hash_(SM_PREF_ENERGY_PURCHASE_VALUE));
  this->pref_purchase_alarm_ =
      global_preferences->make_preference<uint32_t>(this->pref_hash_(SM_PREF_ENERGY_PURCHASE_ALARM));

  uint32_t u32;
  float f32;
  if (this->pref_delay_value_set_.load(&u32))
    this->delay_value_set_val_ = u32;
  if (this->pref_purchase_value_.load(&u32))
    this->purchase_value_val_ = u32;
  if (this->pref_purchase_alarm_.load(&u32))
    this->purchase_alarm_val_ = u32;
  if (this->pref_starting_kwh_.load(&f32))
    this->starting_kwh_val_ = f32;
  if (this->pref_price_kwh_.load(&f32))
    this->price_kwh_val_ = f32;

  UPDATE_NUMBER(delay_value_set, this->delay_value_set_val_)
  UPDATE_NUMBER(energy_purchase_value, this->purchase_value_val_)
  UPDATE_NUMBER(energy_purchase_alarm, this->purchase_alarm_val_)
  UPDATE_NUMBER(starting_kwh, this->starting_kwh_val_)
  UPDATE_NUMBER(price_kwh, this->price_kwh_val_)
  UPDATE_SENSOR(price_kwh, this->price_kwh_val_)

  // transaction_gap: never restored, takes the configured value at each boot
  UPDATE_NUMBER(transaction_gap, this->transaction_gap_)

  UPDATE_TEXT_SENSOR(last_error, std::string("No Errors"))

  // Periodic publishing of the diagnostic counters
  this->set_interval("diag", 1000, [this]() { this->publish_diagnostics_(); });
}

void Dxs238xwComponent::update() {
  // <=> rodgon81: GET_MEASUREMENT_DATA on every update_interval
  this->polls_[POLL_MEASUREMENT].due = true;
}

void Dxs238xwComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DXS238XW :");
  ESP_LOGCONFIG(TAG, "  Component version: %s", SM_COMPONENT_VERSION);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Meter state interval: %u ms", this->polls_[POLL_METER_STATE].interval);
  ESP_LOGCONFIG(TAG, "  Limit/purchase interval: %u ms", this->polls_[POLL_LIMIT_PURCHASE].interval);
  ESP_LOGCONFIG(TAG, "  Transaction gap: %u ms", this->transaction_gap_);
  ESP_LOGCONFIG(TAG, "  Consecutive error threshold: %u", this->error_threshold_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
  static const char *const DELAY_ACTIONS[] = {"none", "disarm", "force_off"};
  ESP_LOGCONFIG(TAG, "  Delay end action: %s", DELAY_ACTIONS[this->delay_end_action_]);
#endif
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

// =============================================================================
// State machine - non-blocking, runs on every loop() iteration
// =============================================================================
void Dxs238xwComponent::loop() {
  const uint32_t now = millis();

  // Mark the periodic polls that are due
  for (uint8_t i = 0; i < POLL_COUNT; i++) {
    if (this->polls_[i].interval != 0 && (now - this->polls_[i].last) >= this->polls_[i].interval)
      this->polls_[i].due = true;
  }

  switch (this->state_) {
    // -------------------------------------------------------------------------
    case ST_IDLE: {
      // 1) Unsolicited frame pushed by the meter (physical button, autonomous
      //    trip): listen to it instead of throwing it away.
      if (this->available() > 0) {
        this->idx_ = 0;
        this->start_ = now;
        this->state_ = ST_LISTEN;
        break;
      }

      // Minimum spacing between two transactions
      if ((now - this->last_tx_end_) < this->transaction_gap_)
        break;

      // 2) Raw frame (hex_message) - highest priority
      if (this->raw_pending_) {
        this->raw_pending_ = false;
        this->raw_send_ = true;
        this->cmd_in_flight_ = true;
        this->cmd_by_user_ = false;
        this->next_cmd_ = this->raw_frame_[4];
        this->next_payload_size_ = 0;
        this->expected_ = SM_CMD_ANY;
        this->state_ = ST_SEND;
        break;
      }

      // 3) Pending SET command in the FIFO
      if (this->q_count_ > 0) {
        const SmCommand &c = this->queue_[this->q_head_];
        this->next_cmd_ = c.send;
        this->expected_ = c.receive;
        this->next_payload_size_ = c.size;
        memcpy(this->next_payload_, c.payload, c.size);
        this->cmd_by_user_ = c.by_user;
        this->q_head_ = (this->q_head_ + 1) % SM_QUEUE_DEPTH;
        this->q_count_--;
        this->cmd_in_flight_ = true;
        this->state_ = ST_SEND;
        break;
      }

      // 4) Serial number: only once at boot
      if (!this->meter_id_done_ && this->meter_id_tries_ < 3) {
        this->meter_id_tries_++;
        this->next_cmd_ = HEKR_CMD_SEND_GET_METER_ID;
        this->expected_ = HEKR_CMD_RECEIVE_METER_ID;
        this->next_payload_size_ = 0;
        this->cmd_in_flight_ = false;
        this->cmd_by_user_ = false;
        this->state_ = ST_SEND;
        break;
      }

      // 5) Fair round-robin over the periodic GETs that are due
      for (uint8_t i = 0; i < POLL_COUNT; i++) {
        const uint8_t s = (this->poll_idx_ + i) % POLL_COUNT;
        if (!this->polls_[s].due)
          continue;

        this->next_cmd_ = this->polls_[s].send;
        this->expected_ = this->polls_[s].receive;
        this->next_payload_size_ = 0;
        this->cmd_in_flight_ = false;
        this->cmd_by_user_ = false;
        this->polls_[s].due = false;
        this->polls_[s].last = now;
        this->poll_idx_ = (s + 1) % POLL_COUNT;
        this->state_ = ST_SEND;
        break;
      }
      break;
    }

    // -------------------------------------------------------------------------
    case ST_SEND: {
      uint8_t frame[6 + SM_MAX_PAYLOAD];
      const uint8_t *out;
      uint8_t len;

      if (this->raw_send_) {
        // Already-formed raw frame (hex_message)
        this->raw_send_ = false;
        out = this->raw_frame_;
        len = this->raw_len_;
        this->raw_len_ = 0;
      } else {
        len = 6 + this->next_payload_size_;
        frame[0] = HEKR_HEADER;
        frame[1] = len;
        frame[2] = HEKR_TYPE_SEND;
        frame[3] = this->seq_++;
        frame[4] = this->next_cmd_;
        for (uint8_t i = 0; i < this->next_payload_size_; i++)
          frame[5 + i] = this->next_payload_[i];
        frame[len - 1] = crc_of_(frame, len);
        out = frame;
      }

      this->purge_rx_();
      this->write_array(out, len);
      this->flush();

      log_frame_("TX", out, len);

      this->last_sent_len_ = len;
      this->idx_ = 0;
      this->start_ = now;
      this->state_ = ST_CONFIRM;
      break;
    }

    // -------------------------------------------------------------------------
    case ST_CONFIRM: {
      // The meter echoes back the exact frame that was sent (TYPE = 0x02)
      const uint8_t r = this->pump_frame_(HEKR_TYPE_SEND);

      if (r == FRAME_CRC) {
        this->end_transaction_(false, ERR_CRC);
      } else if (r == FRAME_OK) {
        if (this->buf_[1] == this->last_sent_len_ && this->buf_[4] == this->next_cmd_) {
          this->idx_ = 0;
          this->start_ = now;
          this->state_ = ST_RESPONSE;
        } else {
          this->idx_ = 0;
          this->end_transaction_(false, ERR_CONFIRM_INVALID);
        }
      } else if ((now - this->start_) > SM_MAX_MILLIS_TO_CONFIRM) {
        this->end_transaction_(false, ERR_CONFIRM_TIMEOUT);
      }
      break;
    }

    // -------------------------------------------------------------------------
    case ST_RESPONSE: {
      uint8_t r;
      while ((r = this->pump_frame_(HEKR_TYPE_RECEIVE)) != FRAME_NONE) {
        if (r == FRAME_CRC) {
          this->idx_ = 0;
          this->end_transaction_(false, ERR_CRC);
          return;
        }
        if (this->expected_ == SM_CMD_ANY || this->buf_[4] == this->expected_) {
          this->state_ = ST_PARSE;
          break;
        }
        // Valid frame, but not the one we asked for: it still carries useful
        // data, so use it and keep waiting for ours.
        ESP_LOGV(TAG, "Using interleaved frame 0x%02X", this->buf_[4]);
        this->parse_frame_();
        this->idx_ = 0;
      }

      if (this->state_ == ST_RESPONSE && (now - this->start_) > SM_MAX_MILLIS_TO_RESPONSE) {
        this->idx_ = 0;
        this->end_transaction_(false, ERR_RESPONSE_TIMEOUT);
      }
      break;
    }

    // -------------------------------------------------------------------------
    case ST_PARSE: {
      log_frame_("RX", this->buf_, this->buf_[1]);
      this->parse_frame_();
      this->idx_ = 0;
      this->stat_success_++;
      this->end_transaction_(true);
      break;
    }

    // -------------------------------------------------------------------------
    case ST_LISTEN: {
      // Unsolicited frame: counts as neither success nor error, it is a bonus.
      const uint8_t r = this->pump_frame_(HEKR_TYPE_RECEIVE);

      if (r == FRAME_OK) {
        ESP_LOGD(TAG, "Unsolicited frame received: cmd 0x%02X", this->buf_[4]);
        log_frame_("Unsolicited", this->buf_, this->buf_[1]);
        this->parse_frame_();
        this->idx_ = 0;
        this->state_ = ST_IDLE;
      } else if (r == FRAME_CRC || (now - this->start_) > SM_MAX_MILLIS_TO_LISTEN) {
        this->idx_ = 0;
        this->purge_rx_();
        this->state_ = ST_IDLE;
      }
      break;
    }

    default:
      this->state_ = ST_IDLE;
      break;
  }
}

// =============================================================================
// Frame pump - non-blocking, resynchronises on the header
// =============================================================================
uint8_t Dxs238xwComponent::pump_frame_(uint8_t type_message) {
  uint8_t b;

  while (this->available() > 0) {
    this->read_byte(&b);

    // Resynchronise: any byte outside a frame is dropped
    if (this->idx_ == 0) {
      if (b != HEKR_HEADER)
        continue;
      this->buf_[0] = b;
      this->idx_ = 1;
      continue;
    }

    if (this->idx_ >= SM_MAX_BYTE_MSG_BUFFER) {
      this->idx_ = 0;
      continue;
    }

    this->buf_[this->idx_] = b;
    this->idx_++;

    // Announced length is nonsense => false start, resynchronise
    if (this->idx_ == 2 && (this->buf_[1] < 6 || this->buf_[1] > SM_MAX_BYTE_MSG_BUFFER)) {
      this->idx_ = 0;
      continue;
    }
    // Unexpected message type => false start
    if (this->idx_ == 3 && this->buf_[2] != type_message) {
      this->idx_ = 0;
      continue;
    }

    if (this->idx_ >= 6 && this->idx_ >= this->buf_[1])
      return (crc_of_(this->buf_, this->buf_[1]) == this->buf_[this->buf_[1] - 1]) ? FRAME_OK : FRAME_CRC;
  }

  return FRAME_NONE;
}

void Dxs238xwComponent::purge_rx_() {
  uint8_t trash;
  while (this->available() > 0)
    this->read_byte(&trash);
}

void Dxs238xwComponent::end_transaction_(bool success, SmError error) {
  if (success) {
    this->consecutive_errors_ = 0;
    if (this->status_error_set_) {
      this->status_clear_error();
      this->status_error_set_ = false;
      ESP_LOGI(TAG, "Communication with the meter restored");
    }
  } else {
    this->report_error_(error);
    this->consecutive_errors_++;
    if (!this->status_error_set_ && this->consecutive_errors_ >= this->error_threshold_) {
      this->status_set_error(LOG_STR("Meter is not responding"));
      this->status_error_set_ = true;
    }
  }

  if (!success) {
    // Bus state is unknown: start from a clean buffer rather than let stray
    // bytes trigger a pointless 200 ms listen.
    this->idx_ = 0;
    this->purge_rx_();
  }

  this->cmd_in_flight_ = false;
  this->cmd_by_user_ = false;
  this->raw_send_ = false;
  this->last_tx_end_ = millis();
  this->state_ = ST_IDLE;
}

void Dxs238xwComponent::report_error_(SmError error) {
  const char *msg;
  switch (error) {
    case ERR_NONE:
      return;
    case ERR_CONFIRM_TIMEOUT:
      msg = "Communication (Confirmation Message) - Timed out";
      this->stat_confirm_timeout_++;
      break;
    case ERR_CONFIRM_INVALID:
      msg = "Communication (Confirmation Message) - Wrong Message";
      this->stat_confirm_timeout_++;
      break;
    case ERR_RESPONSE_TIMEOUT:
      msg = "Communication (Answer Message) - Timed out";
      this->stat_response_timeout_++;
      break;
    case ERR_CRC:
      msg = "CRC check failed";
      this->stat_crc_error_++;
      break;
    case ERR_INPUT_DATA:
      msg = "Input data - Wrong Message";
      break;
    case ERR_QUEUE_FULL:
      msg = "Command queue full";
      break;
    case ERR_NOT_READY:
      msg = "Meter limits not read yet";
      break;
    default:
      msg = "Unknown error";
      break;
  }

  ESP_LOGW(TAG, "Error: %s", msg);
  UPDATE_TEXT_SENSOR(last_error, std::string(msg))
}

void Dxs238xwComponent::publish_diagnostics_() {
  UPDATE_SENSOR(diag_success, this->stat_success_)
  UPDATE_SENSOR(diag_confirm_timeout, this->stat_confirm_timeout_)
  UPDATE_SENSOR(diag_response_timeout, this->stat_response_timeout_)
  UPDATE_SENSOR(diag_crc_error, this->stat_crc_error_)
}

// =============================================================================
// Command queue (FIFO)
// =============================================================================
bool Dxs238xwComponent::queue_command_(uint8_t send, uint8_t receive, const uint8_t *data, uint8_t size, bool by_user) {
  if (size > SM_MAX_PAYLOAD) {
    this->report_error_(ERR_INPUT_DATA);
    return false;
  }
  if (this->q_count_ >= SM_QUEUE_DEPTH) {
    this->report_error_(ERR_QUEUE_FULL);
    return false;
  }

  SmCommand &c = this->queue_[this->q_tail_];
  c.send = send;
  c.receive = receive;
  c.size = size;
  c.by_user = by_user;
  for (uint8_t i = 0; i < size; i++)
    c.payload[i] = data[i];

  this->q_tail_ = (this->q_tail_ + 1) % SM_QUEUE_DEPTH;
  this->q_count_++;

  ESP_LOGD(TAG, "Command queued: 0x%02X (queue: %u/%u)", send, this->q_count_, SM_QUEUE_DEPTH);
  return true;
}

void Dxs238xwComponent::queue_set_limit_() {
  const uint16_t cur = (uint16_t) lroundf(this->max_current_limit_val_ * 100.0f);
  uint8_t d[6];
  d[0] = cur >> 8;
  d[1] = cur & 0xFF;
  d[2] = this->max_voltage_limit_val_ >> 8;
  d[3] = this->max_voltage_limit_val_ & 0xFF;
  d[4] = this->min_voltage_limit_val_ >> 8;
  d[5] = this->min_voltage_limit_val_ & 0xFF;
  this->queue_command_(HEKR_CMD_SEND_SET_LIMIT, HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE, d, 6);
}

void Dxs238xwComponent::queue_set_purchase_(bool state) {
  const uint32_t value = state ? this->purchase_value_val_ * 100 : 0;
  const uint32_t alarm = state ? this->purchase_alarm_val_ * 100 : 0;

  uint8_t d[9];
  d[0] = value >> 24;
  d[1] = value >> 16;
  d[2] = value >> 8;
  d[3] = value & 0xFF;
  d[4] = alarm >> 24;
  d[5] = alarm >> 16;
  d[6] = alarm >> 8;
  d[7] = alarm & 0xFF;
  d[8] = state ? 1 : 0;
  this->queue_command_(HEKR_CMD_SEND_SET_PURCHASE, HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE, d, 9);
}

void Dxs238xwComponent::queue_set_delay_(bool state) {
  const uint16_t minutes = state ? (uint16_t) this->delay_value_set_val_ : 0;
  uint8_t d[3];
  d[0] = minutes >> 8;
  d[1] = minutes & 0xFF;
  d[2] = state ? 1 : 0;
  this->queue_command_(HEKR_CMD_SEND_SET_DELAY, HEKR_CMD_RECEIVE_METER_STATE, d, 3);
}

void Dxs238xwComponent::set_meter_state_(bool state) {
  const uint8_t d = state ? 1 : 0;
  // by_user = true: only an explicitly requested switch-off feeds the
  // "Off by User" warning (an end-of-delay switch-off must not).
  this->queue_command_(HEKR_CMD_SEND_SET_METER_STATE, HEKR_CMD_RECEIVE_METER_STATE, &d, 1, !state);
}

// =============================================================================
// Action: arbitrary hexadecimal frame
// =============================================================================
void Dxs238xwComponent::hex_message(std::string message, bool check_crc) {
  ESP_LOGD(TAG, "hex_message: %s", message.c_str());

  char clean[SM_MAX_HEX_MSG_LENGTH];
  size_t n = 0;

  for (const char c : message) {
    if (c == '.' || c == ':' || c == ' ' || c == '-' || c == ',')
      continue;
    if (n >= sizeof(clean)) {
      this->report_error_(ERR_INPUT_DATA);
      return;
    }
    clean[n++] = c;
  }

  if (n == 0 || (n % 2) != 0 || (n / 2) < 6 || (n / 2) > SM_MAX_BYTE_MSG_BUFFER) {
    this->report_error_(ERR_INPUT_DATA);
    return;
  }

  const uint8_t len = n / 2;
  if (parse_hex(clean, n, this->raw_frame_, len) != n) {
    this->report_error_(ERR_INPUT_DATA);
    return;
  }

  if (check_crc) {
    if (crc_of_(this->raw_frame_, len) != this->raw_frame_[len - 1]) {
      ESP_LOGW(TAG, "hex_message: supplied CRC is wrong");
      this->report_error_(ERR_CRC);
      return;
    }
  } else {
    this->raw_frame_[len - 1] = crc_of_(this->raw_frame_, len);
  }

  this->raw_len_ = len;
  this->raw_pending_ = true;
}

// =============================================================================
// Entity callbacks
// =============================================================================
void Dxs238xwComponent::set_number_value(SmIdEntity entity, float value) {
  switch (entity) {
    case NUMBER_MAX_CURRENT_LIMIT: {
      if (!this->limits_valid_) {
        this->report_error_(ERR_NOT_READY);
        UPDATE_NUMBER(max_current_limit, this->max_current_limit_val_)
        break;
      }
      this->max_current_limit_val_ = value;
      this->queue_set_limit_();
      UPDATE_NUMBER(max_current_limit, this->max_current_limit_val_)
      break;
    }

    case NUMBER_MAX_VOLTAGE_LIMIT: {
      const uint16_t v = (uint16_t) lroundf(value);
      if (!this->limits_valid_) {
        this->report_error_(ERR_NOT_READY);
      } else if (v > this->min_voltage_limit_val_) {
        this->max_voltage_limit_val_ = v;
        this->queue_set_limit_();
      } else {
        ESP_LOGW(TAG, "max_voltage_limit: %u must stay above min_voltage_limit (%u)", v, this->min_voltage_limit_val_);
        this->report_error_(ERR_INPUT_DATA);
      }
      UPDATE_NUMBER(max_voltage_limit, this->max_voltage_limit_val_)
      break;
    }

    case NUMBER_MIN_VOLTAGE_LIMIT: {
      const uint16_t v = (uint16_t) lroundf(value);
      if (!this->limits_valid_) {
        this->report_error_(ERR_NOT_READY);
      } else if (v < this->max_voltage_limit_val_) {
        this->min_voltage_limit_val_ = v;
        this->queue_set_limit_();
      } else {
        ESP_LOGW(TAG, "min_voltage_limit: %u must stay below max_voltage_limit (%u)", v, this->max_voltage_limit_val_);
        this->report_error_(ERR_INPUT_DATA);
      }
      UPDATE_NUMBER(min_voltage_limit, this->min_voltage_limit_val_)
      break;
    }

    case NUMBER_ENERGY_PURCHASE_VALUE: {
      this->purchase_value_val_ = (uint32_t) lroundf(value);
      this->pref_purchase_value_.save(&this->purchase_value_val_);
      // The meter only reloads its balance after an OFF transition.
      if (this->purchase_state_val_) {
        this->queue_set_purchase_(false);
        this->queue_set_purchase_(true);
      }
      UPDATE_NUMBER(energy_purchase_value, this->purchase_value_val_)
      break;
    }

    case NUMBER_ENERGY_PURCHASE_ALARM: {
      this->purchase_alarm_val_ = (uint32_t) lroundf(value);
      this->pref_purchase_alarm_.save(&this->purchase_alarm_val_);
      if (this->purchase_state_val_) {
        this->queue_set_purchase_(false);
        this->queue_set_purchase_(true);
      }
      UPDATE_NUMBER(energy_purchase_alarm, this->purchase_alarm_val_)
      break;
    }

    case NUMBER_DELAY_VALUE_SET: {
      this->delay_value_set_val_ = (uint32_t) lroundf(value);
      this->pref_delay_value_set_.save(&this->delay_value_set_val_);
      if (this->delay_state_val_)
        this->queue_set_delay_(true);
      UPDATE_NUMBER(delay_value_set, this->delay_value_set_val_)
      break;
    }

    case NUMBER_STARTING_KWH: {
      this->starting_kwh_val_ = roundf(value * 10.0f) / 10.0f;
      this->pref_starting_kwh_.save(&this->starting_kwh_val_);
      UPDATE_NUMBER(starting_kwh, this->starting_kwh_val_)
      this->publish_derived_();
      break;
    }

    case NUMBER_PRICE_KWH: {
      this->price_kwh_val_ = roundf(value * 10.0f) / 10.0f;
      this->pref_price_kwh_.save(&this->price_kwh_val_);
      UPDATE_NUMBER(price_kwh, this->price_kwh_val_)
      UPDATE_SENSOR(price_kwh, this->price_kwh_val_)
      this->publish_derived_();
      break;
    }

    case NUMBER_TRANSACTION_GAP: {
      this->transaction_gap_ = (uint32_t) lroundf(value);
      UPDATE_NUMBER(transaction_gap, this->transaction_gap_)
      break;
    }

    default:
      ESP_LOGE(TAG, "ID %u is not a known NUMBER", (uint8_t) entity);
      break;
  }
}

void Dxs238xwComponent::set_switch_value(SmIdEntity entity, bool value) {
  switch (entity) {
    case SWITCH_ENERGY_PURCHASE_STATE:
      this->queue_set_purchase_(value);
      break;
    case SWITCH_METER_STATE:
      this->set_meter_state_(value);
      break;
    case SWITCH_DELAY_STATE:
      this->queue_set_delay_(value);
      break;
    default:
      ESP_LOGE(TAG, "ID %u is not a known SWITCH", (uint8_t) entity);
      break;
  }
}

void Dxs238xwComponent::set_button_value(SmIdEntity entity) {
  switch (entity) {
    case BUTTON_RESET_DATA:
      // Clear the energy purchase before resetting (rodgon81 behaviour)
      this->queue_set_purchase_(false);
      this->queue_command_(HEKR_CMD_SEND_SET_RESET, HEKR_CMD_RECEIVE_MEASUREMENT);
      break;
    default:
      ESP_LOGE(TAG, "ID %u is not a known BUTTON", (uint8_t) entity);
      break;
  }
}

// =============================================================================
// Parsing
// =============================================================================
void Dxs238xwComponent::parse_frame_() {
  switch (this->buf_[4]) {
    case HEKR_CMD_RECEIVE_METER_STATE:
      this->parse_meter_state_();
      break;
    case HEKR_CMD_RECEIVE_LIMIT_AND_PURCHASE:
      this->parse_limit_and_purchase_();
      break;
    case HEKR_CMD_RECEIVE_MEASUREMENT:
      this->parse_measurement_();
      break;
    case HEKR_CMD_RECEIVE_METER_ID:
      this->parse_meter_id_();
      break;
    default:
      ESP_LOGW(TAG, "Unknown response command: 0x%02X", this->buf_[4]);
      break;
  }
}

void Dxs238xwComponent::parse_meter_state_() {
  const uint8_t *b = this->buf_;

  if (b[1] < 20) {
    ESP_LOGW(TAG, "METER_STATE frame too short (%u B), ignored", b[1]);
    return;
  }

  const uint8_t phase_count = b[5];
  const bool meter_state = b[6] != 0;
  const float total_energy = get_u32_(b + 7) * 0.01f;
  const uint8_t warn_code = b[11];
  const bool over_current = b[15] != 0;
  const uint16_t delay_remaining = get_u16_(b + 16);
  const bool delay_state = b[18] != 0;
  const bool end_purchase = (b[1] == 21) ? (b[19] != 0) : false;

  // The delay timer has elapsed. This is an OBSERVATION about the meter, not
  // a command: its own firmware already drives the relay at this point.
  const bool delay_expired = (delay_remaining == 0 && delay_state);

  // The meter does NOT clear delay_state once the timer elapses: the byte
  // stays armed with remaining = 0 even though the delay is over. The switch
  // must reflect reality ("is a delay running?"), not the raw byte.
  const bool delay_running = delay_state && (delay_remaining > 0);

  // Because the meter leaves the byte armed forever, delay_expired stays true
  // forever. It therefore cannot mean "off BECAUSE OF the delay": we need the
  // TRANSITION to zero, not the steady state. Otherwise a manual switch-off six
  // months later would still be blamed on the delay.
  const bool delay_just_expired = delay_expired && (this->prev_delay_remaining_ > 0);
  this->prev_delay_remaining_ = delay_remaining;

  // Sticky warnings: only cleared when the meter switches back on
  if (meter_state) {
    this->warn_over_voltage_ = false;
    this->warn_under_voltage_ = false;
    this->warn_over_current_ = false;
    this->warn_end_purchase_ = false;
    this->warn_end_delay_ = false;
    this->warn_by_user_ = false;
  } else {
    if (this->cmd_in_flight_ && this->cmd_by_user_) {
      // This frame is the confirmation of OUR switch-off command
      this->warn_by_user_ = true;
    }
    // "off_by_end_delay" may only be true if the meter is ACTUALLY off, and
    // only at the exact moment the delay elapses. This is rodgon81's mistake:
    // he raises the flag without looking at meter_state, then uses it as a
    // trigger -> he switches off a relay the meter has just switched on.
    if (delay_just_expired)
      this->warn_end_delay_ = true;
  }

  if (!this->warn_over_voltage_)
    this->warn_over_voltage_ = (warn_code == 1);
  if (!this->warn_under_voltage_)
    this->warn_under_voltage_ = (warn_code == 2);
  if (!this->warn_over_current_)
    this->warn_over_current_ = over_current;
  if (!this->warn_end_purchase_)
    this->warn_end_purchase_ = end_purchase;

  this->phase_count_val_ = phase_count;
  this->meter_state_val_ = meter_state;
  this->delay_state_val_ = delay_running;
  this->delay_value_remaining_ = delay_remaining;
  this->total_energy_val_ = total_energy;

  // --- End of delay: by default ESPHome does not interfere ---
  // The delay_action_done_ latch guarantees a single action per expiry, even
  // if several frames arrive before the command has been sent.
  if (!delay_expired)
    this->delay_action_done_ = false;

  if (this->delay_end_action_ != DELAY_END_NONE) {
    if (delay_expired && !this->delay_action_done_) {
      this->delay_action_done_ = true;

      if (this->delay_end_action_ == DELAY_END_FORCE_OFF && meter_state) {
        ESP_LOGD(TAG, "End of delay: forcing relay off (delay_end_action: force_off)");
        const uint8_t off = 0;
        // by_user = false: the user is not the one switching off.
        this->queue_command_(HEKR_CMD_SEND_SET_METER_STATE, HEKR_CMD_RECEIVE_METER_STATE, &off, 1, false);
      }

      ESP_LOGD(TAG, "End of delay: disarming the timer");
      this->queue_set_delay_(false);
    }

    // Timer still armed after an ESP reboot
    if (this->first_meter_state_ && delay_state && !delay_expired) {
      ESP_LOGD(TAG, "Timer still armed at boot: disarming");
      this->queue_set_delay_(false);
    }
  }
  this->first_meter_state_ = false;

  // --- Publishing ---
  UPDATE_BINARY_SENSOR(warning_off_by_over_voltage, this->warn_over_voltage_)
  UPDATE_BINARY_SENSOR(warning_off_by_under_voltage, this->warn_under_voltage_)
  UPDATE_BINARY_SENSOR(warning_off_by_over_current, this->warn_over_current_)
  UPDATE_BINARY_SENSOR(warning_off_by_end_purchase, this->warn_end_purchase_)
  UPDATE_BINARY_SENSOR(warning_off_by_end_delay, this->warn_end_delay_)
  UPDATE_BINARY_SENSOR(warning_off_by_user, this->warn_by_user_)
  UPDATE_BINARY_SENSOR(meter_state, meter_state)

  UPDATE_SWITCH(meter_state, meter_state)
  UPDATE_SWITCH(delay_state, delay_running)

  UPDATE_SENSOR(phase_count, phase_count)
  UPDATE_TEXT_SENSOR(delay_value_remaining, delay_remaining_string_(delay_remaining))

  this->publish_derived_();
  this->publish_meter_state_detail_();
}

void Dxs238xwComponent::parse_limit_and_purchase_() {
  const uint8_t *b = this->buf_;

  if (b[1] < 12) {
    ESP_LOGW(TAG, "LIMIT_AND_PURCHASE frame too short (%u B), ignored", b[1]);
    return;
  }

  this->max_voltage_limit_val_ = get_u16_(b + 5);
  this->min_voltage_limit_val_ = get_u16_(b + 7);
  this->max_current_limit_val_ = get_u16_(b + 9) * 0.01f;
  this->limits_valid_ = true;

  UPDATE_NUMBER(max_voltage_limit, this->max_voltage_limit_val_)
  UPDATE_NUMBER(min_voltage_limit, this->min_voltage_limit_val_)
  UPDATE_NUMBER(max_current_limit, this->max_current_limit_val_)

  if (b[1] == 25) {
    this->purchase_state_val_ = b[23] != 0;
    // energy_purchase_value / _alarm are not read back: the meter does not
    // report them reliably, they come from the preferences instead.
    this->purchase_balance_ = get_u32_(b + 15) * 0.01f;

    this->warn_purchase_alarm_ =
        (this->purchase_balance_ <= (float) this->purchase_alarm_val_) && this->purchase_state_val_;

    UPDATE_SENSOR(energy_purchase_balance, this->purchase_balance_)
    UPDATE_SWITCH(energy_purchase_state, this->purchase_state_val_)
    UPDATE_BINARY_SENSOR(warning_purchase_alarm, this->warn_purchase_alarm_)
    this->publish_derived_();
  }
}

void Dxs238xwComponent::parse_measurement_() {
  const uint8_t *b = this->buf_;

  if (b[1] < 67) {
    ESP_LOGW(TAG, "MEASUREMENT frame too short (%u B), ignored", b[1]);
    return;
  }

  // apply_sign_() works on the raw integer: see SM_SIGN_OFFSET_RAW.
  UPDATE_SENSOR(current_phase_1, apply_sign_(get_u24_(b + 5), 0.001f))
  UPDATE_SENSOR(current_phase_2, apply_sign_(get_u24_(b + 8), 0.001f))
  UPDATE_SENSOR(current_phase_3, apply_sign_(get_u24_(b + 11), 0.001f))

  // Voltage, power factor and frequency are never sign encoded.
  UPDATE_SENSOR(voltage_phase_1, get_u16_(b + 14) * 0.1f)
  UPDATE_SENSOR(voltage_phase_2, get_u16_(b + 16) * 0.1f)
  UPDATE_SENSOR(voltage_phase_3, get_u16_(b + 18) * 0.1f)

  UPDATE_SENSOR(reactive_power_total, apply_sign_(get_u24_(b + 20), 0.0001f))
  UPDATE_SENSOR(reactive_power_phase_1, apply_sign_(get_u24_(b + 23), 0.0001f))
  UPDATE_SENSOR(reactive_power_phase_2, apply_sign_(get_u24_(b + 26), 0.0001f))
  UPDATE_SENSOR(reactive_power_phase_3, apply_sign_(get_u24_(b + 29), 0.0001f))

  UPDATE_SENSOR(active_power_total, apply_sign_(get_u24_(b + 32), 0.0001f))
  UPDATE_SENSOR(active_power_phase_1, apply_sign_(get_u24_(b + 35), 0.0001f))
  UPDATE_SENSOR(active_power_phase_2, apply_sign_(get_u24_(b + 38), 0.0001f))
  UPDATE_SENSOR(active_power_phase_3, apply_sign_(get_u24_(b + 41), 0.0001f))

  UPDATE_SENSOR(power_factor_total, get_u16_(b + 44) * 0.001f)
  UPDATE_SENSOR(power_factor_phase_1, get_u16_(b + 46) * 0.001f)
  UPDATE_SENSOR(power_factor_phase_2, get_u16_(b + 48) * 0.001f)
  UPDATE_SENSOR(power_factor_phase_3, get_u16_(b + 50) * 0.001f)

  UPDATE_SENSOR(frequency, get_u16_(b + 52) * 0.01f)

  // The meter accumulates reverse energy INTO the total, so this only grows
  // (DTS238-7 manual, 3.1): total = |import| + |export|.
  this->total_energy_val_ = get_u32_(b + 54) * 0.01f;
  UPDATE_SENSOR(total_energy, this->total_energy_val_)
  UPDATE_SENSOR(import_active_energy, get_u32_(b + 58) * 0.01f)

  // Published POSITIVE and increasing, unlike rodgon81 and v1. The datasheet
  // describes it as "the reverse active energy, such as solar power
  // generation": a plain accumulator. This is also the only form compatible
  // with total_increasing and the Home Assistant energy dashboard.
  // Use a `multiply: -1` filter to get the old sign back.
  UPDATE_SENSOR(export_active_energy, get_u32_(b + 62) * 0.01f)

  this->publish_derived_();
}

void Dxs238xwComponent::parse_meter_id_() {
  if (this->buf_[1] < 12) {
    ESP_LOGW(TAG, "METER_ID frame too short (%u B), ignored", this->buf_[1]);
    return;
  }

  char serial[24];
  snprintf(serial, sizeof(serial), "%02u%02u%02u %02u%02u%02u", this->buf_[5], this->buf_[6], this->buf_[7],
           this->buf_[8], this->buf_[9], this->buf_[10]);
  UPDATE_TEXT_SENSOR(meter_id, std::string(serial))
  this->meter_id_done_ = true;
}

// =============================================================================
// Derived values
// =============================================================================
void Dxs238xwComponent::publish_derived_() {
  UPDATE_SENSOR(contract_total_energy, this->starting_kwh_val_ + this->total_energy_val_)
  UPDATE_SENSOR(total_energy_price, this->price_kwh_val_ * this->total_energy_val_)
  UPDATE_SENSOR(energy_purchase_price, this->price_kwh_val_ * this->purchase_balance_)
}

void Dxs238xwComponent::publish_meter_state_detail_() {
  SmMeterStateDetail d;

  if (this->meter_state_val_) {
    d = DETAIL_POWER_OK;
  } else if (this->warn_over_current_) {
    d = DETAIL_OVER_CURRENT;
  } else if (this->warn_over_voltage_) {
    d = DETAIL_OVER_VOLTAGE;
  } else if (this->warn_under_voltage_) {
    d = DETAIL_UNDER_VOLTAGE;
  } else if (this->warn_end_delay_) {
    d = DETAIL_END_DELAY;
  } else if (this->warn_end_purchase_) {
    d = DETAIL_END_PURCHASE;
  } else if (this->warn_by_user_) {
    d = DETAIL_BY_USER;
  } else {
    d = DETAIL_UNKNOWN;
  }

  if (d == this->detail_)
    return;
  this->detail_ = d;

  const char *s;
  switch (d) {
    case DETAIL_POWER_OK:
      s = "ACTIVE";
      break;
    case DETAIL_OVER_CURRENT:
      s = "INACTIVE - Over Current";
      break;
    case DETAIL_OVER_VOLTAGE:
      s = "INACTIVE - Over Voltage";
      break;
    case DETAIL_UNDER_VOLTAGE:
      s = "INACTIVE - Under Voltage";
      break;
    case DETAIL_END_DELAY:
      s = "INACTIVE - End Delay";
      break;
    case DETAIL_END_PURCHASE:
      s = "INACTIVE - End Purchase";
      break;
    case DETAIL_BY_USER:
      s = "INACTIVE - By User";
      break;
    default:
      s = "INACTIVE - Unknown";
      break;
  }

  UPDATE_TEXT_SENSOR(meter_state_detail, std::string(s))
}

}  // namespace dxs238xw
}  // namespace esphome
