#pragma once

#include "../dxs238xw.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace dxs238xw {

class Dxs238xwSwitch : public switch_::Switch {
 public:
  void set_dxs238xw_parent(Dxs238xwComponent *parent) { this->parent_ = parent; }
  void set_entity_id(SmIdEntity entity_id) { this->entity_id_ = entity_id; }

 protected:
  void write_state(bool state) override;

  Dxs238xwComponent *parent_{nullptr};
  SmIdEntity entity_id_{ID_NULL};
};

}  // namespace dxs238xw
}  // namespace esphome
