#include "dxs238xw_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dxs238xw {

static const char *const TAG = "dxs238xw.number";

void Dxs238xwNumber::control(float value) {
  // Avoid a pointless UART transaction when the value does not change
  if (this->has_state() && this->state == value) {
    ESP_LOGD(TAG, "Value unchanged (%.1f), nothing sent", value);
    return;
  }
  this->parent_->set_number_value(this->entity_id_, value);
}

}  // namespace dxs238xw
}  // namespace esphome
