#include "dxs238xw_button.h"

namespace esphome {
namespace dxs238xw {

void Dxs238xwButton::press_action() { this->parent_->set_button_value(this->entity_id_); }

}  // namespace dxs238xw
}  // namespace esphome
