#pragma once

#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/dxs238xw/dxs238xw.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::dxs238xw::testing {

/// Minimal switch used to observe what the hub publishes.
class MockSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override { this->publish_state(state); }
};

/// Exposes the hub internals the tests need to inspect.
class MockDxs238xw : public Dxs238xwComponent {
 public:
  /// Inject a frame straight into the parser, bypassing the UART.
  void feed(const std::vector<uint8_t> &frame) {
    for (size_t i = 0; i < frame.size() && i < SM_MAX_BYTE_MSG_BUFFER; i++)
      this->buf_[i] = frame[i];
    this->parse_frame_();
  }

  void set_price(float v) { this->price_kwh_val_ = v; }
  void set_starting(float v) { this->starting_kwh_val_ = v; }

  uint8_t queued() const { return this->q_count_; }
  uint8_t first_cmd() const { return this->q_count_ ? this->queue_[this->q_head_].send : 0; }
  uint8_t first_payload0() const { return this->q_count_ ? this->queue_[this->q_head_].payload[0] : 0xFF; }
  bool end_delay_flag() const { return this->warn_end_delay_; }
  void force_by_user() { this->warn_by_user_ = true; }

  void reset_state() {
    this->q_count_ = 0;
    this->q_head_ = 0;
    this->q_tail_ = 0;
    this->warn_end_delay_ = false;
    this->warn_by_user_ = false;
    this->first_meter_state_ = false;
    this->delay_action_done_ = false;
    this->prev_delay_remaining_ = 0;
  }
};

// --- frame builders -------------------------------------------------------
void push_u16(std::vector<uint8_t> &v, uint16_t x);
void push_u24(std::vector<uint8_t> &v, uint32_t x);
void push_u32(std::vector<uint8_t> &v, uint32_t x);

/// Fill in the length byte and append the checksum.
std::vector<uint8_t> finish_frame(std::vector<uint8_t> f);

/// Build a 0x01 METER_STATE frame.
std::vector<uint8_t> meter_state_frame(bool relay_on, uint16_t delay_remaining, bool delay_state,
                                       uint8_t warn_code = 0);

/// Build a 0x0B MEASUREMENT frame. Raw values are the on-the-wire encoding.
std::vector<uint8_t> measurement_frame(uint32_t raw_current_1, uint32_t raw_current_2, uint32_t raw_power_total,
                                       uint32_t raw_power_1, uint32_t total_energy, uint32_t import_energy,
                                       uint32_t export_energy);

}  // namespace esphome::dxs238xw::testing
