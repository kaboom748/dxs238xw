#include "common.h"

namespace esphome::dxs238xw::testing {

void push_u16(std::vector<uint8_t> &v, uint16_t x) {
  v.push_back(x >> 8);
  v.push_back(x & 0xFF);
}

void push_u24(std::vector<uint8_t> &v, uint32_t x) {
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x & 0xFF);
}

void push_u32(std::vector<uint8_t> &v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x & 0xFF);
}

std::vector<uint8_t> finish_frame(std::vector<uint8_t> f) {
  f[1] = f.size() + 1;
  uint16_t crc = 0;
  for (auto b : f)
    crc += b;
  f.push_back(crc & 0xFF);
  return f;
}

std::vector<uint8_t> meter_state_frame(bool relay_on, uint16_t delay_remaining, bool delay_state, uint8_t warn_code) {
  std::vector<uint8_t> f = {HEKR_HEADER, 0x00, HEKR_TYPE_RECEIVE, 0x00, HEKR_CMD_RECEIVE_METER_STATE};
  f.push_back(3);                    // phase_count
  f.push_back(relay_on ? 1 : 0);     // meter_state
  push_u32(f, 123456);               // total_energy
  f.push_back(warn_code);            // 11: 1 = over voltage, 2 = under voltage
  f.insert(f.end(), 3, 0);           // 12..14
  f.push_back(0);                    // 15: over current
  push_u16(f, delay_remaining);      // 16..17
  f.push_back(delay_state ? 1 : 0);  // 18
  f.push_back(0);                    // 19: end purchase
  return finish_frame(f);
}

std::vector<uint8_t> measurement_frame(uint32_t raw_current_1, uint32_t raw_current_2, uint32_t raw_power_total,
                                       uint32_t raw_power_1, uint32_t total_energy, uint32_t import_energy,
                                       uint32_t export_energy) {
  std::vector<uint8_t> f = {HEKR_HEADER, 0x00, HEKR_TYPE_RECEIVE, 0x00, HEKR_CMD_RECEIVE_MEASUREMENT};
  push_u24(f, raw_current_1);
  push_u24(f, raw_current_2);
  push_u24(f, 0);
  push_u16(f, 2301);  // 230.1 V
  push_u16(f, 2298);
  push_u16(f, 2305);
  for (int i = 0; i < 4; i++)
    push_u24(f, 0);  // reactive power
  push_u24(f, raw_power_total);
  push_u24(f, raw_power_1);
  push_u24(f, 0);
  push_u24(f, 0);
  push_u16(f, 998);
  push_u16(f, 997);
  push_u16(f, 0);
  push_u16(f, 0);
  push_u16(f, 5001);  // 50.01 Hz
  push_u32(f, total_energy);
  push_u32(f, import_energy);
  push_u32(f, export_energy);
  return finish_frame(f);
}

// ===========================================================================
// Sign correction
//
// The meter encodes negative (export) values as offset binary. Without the
// correction, -0.5 A reads as 1000.5 A and -0.5 kW reads as 100.5 kW.
// ===========================================================================
TEST(Dxs238xwParsing, MeasurementSignCorrection) {
  MockDxs238xw hub;
  sensor::Sensor current_1, current_2, power_total, power_1, voltage_1, frequency;

  hub.set_current_phase_1_sensor(&current_1);
  hub.set_current_phase_2_sensor(&current_2);
  hub.set_active_power_total_sensor(&power_total);
  hub.set_active_power_phase_1_sensor(&power_1);
  hub.set_voltage_phase_1_sensor(&voltage_1);
  hub.set_frequency_sensor(&frequency);

  hub.feed(measurement_frame(1000500, 2500, 1005000, 1005000, 123456, 200000, 76544));

  EXPECT_FLOAT_EQ(current_1.state, -0.5f);  // export
  EXPECT_FLOAT_EQ(current_2.state, 2.5f);   // import, untouched
  EXPECT_FLOAT_EQ(power_total.state, -0.5f);
  EXPECT_FLOAT_EQ(power_1.state, -0.5f);

  // Voltage and frequency must never be sign corrected.
  EXPECT_FLOAT_EQ(voltage_1.state, 230.1f);
  EXPECT_FLOAT_EQ(frequency.state, 50.01f);
}

// ===========================================================================
// Energies
//
// The meter accumulates reverse energy into the total, so export is a plain
// positive accumulator and total = |import| + |export|.
// ===========================================================================
TEST(Dxs238xwParsing, MeasurementEnergies) {
  MockDxs238xw hub;
  sensor::Sensor total, import_energy, export_energy;

  hub.set_total_energy_sensor(&total);
  hub.set_import_active_energy_sensor(&import_energy);
  hub.set_export_active_energy_sensor(&export_energy);

  hub.feed(measurement_frame(0, 0, 0, 0, 276544, 200000, 76544));

  EXPECT_FLOAT_EQ(import_energy.state, 2000.00f);
  EXPECT_FLOAT_EQ(export_energy.state, 765.44f);  // positive
  EXPECT_FLOAT_EQ(total.state, 2765.44f);
  EXPECT_FLOAT_EQ(total.state, import_energy.state + export_energy.state);
}

TEST(Dxs238xwParsing, DerivedSensors) {
  MockDxs238xw hub;
  sensor::Sensor contract, price;

  hub.set_contract_total_energy_sensor(&contract);
  hub.set_total_energy_price_sensor(&price);
  hub.set_starting(1000.0f);
  hub.set_price(0.25f);

  hub.feed(measurement_frame(0, 0, 0, 0, 123456, 200000, 76544));

  EXPECT_FLOAT_EQ(contract.state, 2234.56f);  // starting_kwh + total
  EXPECT_FLOAT_EQ(price.state, 308.64f);      // price_kwh * total
}

TEST(Dxs238xwParsing, MeterId) {
  MockDxs238xw hub;
  text_sensor::TextSensor meter_id;
  hub.set_meter_id_text_sensor(&meter_id);

  hub.feed(
      finish_frame({HEKR_HEADER, 0x00, HEKR_TYPE_RECEIVE, 0x00, HEKR_CMD_RECEIVE_METER_ID, 20, 24, 11, 12, 34, 56}));

  EXPECT_EQ(meter_id.state, "202411 123456");
}

TEST(Dxs238xwParsing, TruncatedFrameIsRejected) {
  MockDxs238xw hub;
  sensor::Sensor current_1;
  hub.set_current_phase_1_sensor(&current_1);

  hub.feed(measurement_frame(2500, 0, 0, 0, 0, 0, 0));
  const float before = current_1.state;

  hub.feed(finish_frame({HEKR_HEADER, 0x00, HEKR_TYPE_RECEIVE, 0x00, HEKR_CMD_RECEIVE_MEASUREMENT, 1, 2, 3}));

  EXPECT_FLOAT_EQ(current_1.state, before);
}

TEST(Dxs238xwParsing, MeterStateDetail) {
  MockDxs238xw hub;
  text_sensor::TextSensor detail;
  sensor::Sensor phase_count;
  binary_sensor::BinarySensor relay;

  hub.set_meter_state_detail_text_sensor(&detail);
  hub.set_phase_count_sensor(&phase_count);
  hub.set_meter_state_binary_sensor(&relay);

  hub.feed(meter_state_frame(false, 0, false, /*warn_code=*/1));

  EXPECT_FLOAT_EQ(phase_count.state, 3.0f);
  EXPECT_FALSE(relay.state);
  EXPECT_EQ(detail.state, "INACTIVE - Over Voltage");
}

// ===========================================================================
// End of delay
//
// The meter's own firmware drives the relay when the timer elapses - it may
// switch it ON. ESPHome must not fight it. rodgon81 forces the relay off here.
// ===========================================================================
TEST(Dxs238xwDelay, MeterTurnsRelayOnSendsNothing) {
  MockDxs238xw hub;
  text_sensor::TextSensor detail;
  hub.set_meter_state_detail_text_sensor(&detail);
  hub.reset_state();

  // Timer elapsed and the meter switched the relay back on by itself.
  hub.feed(meter_state_frame(/*relay_on=*/true, /*delay_remaining=*/0, /*delay_state=*/true));

  EXPECT_EQ(hub.queued(), 0);
  EXPECT_FALSE(hub.end_delay_flag());
  EXPECT_EQ(detail.state, "ACTIVE");
}

TEST(Dxs238xwDelay, ExpiryWithRelayOffIsReported) {
  MockDxs238xw hub;
  text_sensor::TextSensor detail;
  hub.set_meter_state_detail_text_sensor(&detail);
  hub.reset_state();

  hub.feed(meter_state_frame(false, 2, true));
  hub.feed(meter_state_frame(false, 0, true));

  EXPECT_EQ(hub.queued(), 0);
  EXPECT_TRUE(hub.end_delay_flag());
  EXPECT_EQ(detail.state, "INACTIVE - End Delay");
}

// The meter never clears delay_state, so a level test would stay true forever
// and blame the delay for every later switch-off.
TEST(Dxs238xwDelay, FlagUsesTransitionNotLevel) {
  MockDxs238xw hub;
  text_sensor::TextSensor detail;
  hub.set_meter_state_detail_text_sensor(&detail);
  hub.reset_state();

  hub.feed(meter_state_frame(false, 5, true));
  EXPECT_FALSE(hub.end_delay_flag());

  hub.feed(meter_state_frame(false, 0, true));
  EXPECT_TRUE(hub.end_delay_flag());

  hub.feed(meter_state_frame(true, 0, true));
  EXPECT_FALSE(hub.end_delay_flag());

  // Much later: manual switch-off, byte still armed at 0.
  hub.force_by_user();
  hub.feed(meter_state_frame(false, 0, true));
  EXPECT_FALSE(hub.end_delay_flag());
  EXPECT_EQ(detail.state, "INACTIVE - By User");
}

// The switch reflects "is a delay running?", not the raw armed byte.
TEST(Dxs238xwDelay, SwitchReflectsRunningNotArmedByte) {
  MockDxs238xw hub;
  MockSwitch delay_switch;
  hub.set_delay_state_switch(&delay_switch);

  hub.reset_state();
  hub.feed(meter_state_frame(false, 30, true));
  EXPECT_TRUE(delay_switch.state);

  hub.reset_state();
  hub.feed(meter_state_frame(true, 0, true));
  EXPECT_FALSE(delay_switch.state);
  EXPECT_EQ(hub.queued(), 0);

  hub.reset_state();
  hub.feed(meter_state_frame(true, 0, false));
  EXPECT_FALSE(delay_switch.state);

  hub.reset_state();
  hub.feed(meter_state_frame(true, 60, true));
  EXPECT_TRUE(delay_switch.state);
}

TEST(Dxs238xwDelay, ForceOffIsOptInAndFiresOnce) {
  MockDxs238xw hub;
  hub.set_delay_end_action(DELAY_END_FORCE_OFF);
  hub.reset_state();

  hub.feed(meter_state_frame(false, 2, true));
  hub.feed(meter_state_frame(true, 0, true));

  // SET_METER_STATE(off) then SET_DELAY(off)
  EXPECT_EQ(hub.queued(), 2);
  EXPECT_EQ(hub.first_cmd(), HEKR_CMD_SEND_SET_METER_STATE);
  EXPECT_EQ(hub.first_payload0(), 0);

  const uint8_t before = hub.queued();
  hub.feed(meter_state_frame(true, 0, true));
  EXPECT_EQ(hub.queued(), before);
}

}  // namespace esphome::dxs238xw::testing
