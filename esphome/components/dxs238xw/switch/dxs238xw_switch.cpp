#include "dxs238xw_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw.switch";

void Dxs238xwSwitch::write_state(bool state) {
  // optimistic: false - the published state comes solely from the meter
  if (this->has_state() && this->state == state) {
    ESP_LOGD(TAG, "State unchanged (%s), nothing sent", ONOFF(state));
    return;
  }
  this->parent_->set_switch_value(this->entity_id_, state);
}

}  // namespace dxs238xw
}  // namespace esphome
