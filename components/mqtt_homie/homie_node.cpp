#include "homie_client.h"
#include "homie_device.h"
#include "homie_node.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#define TAG "homie:node"

namespace esphome {
namespace mqtt_homie {

std::string HomieNodeBase::get_id() const { return GetEntityBase()->get_object_id(); }

std::string HomieNodeBase::get_name() const { return GetEntityBase()->get_name(); }

std::string HomieNodeBase::get_name(int64_t node_idx) const { return ""; }

std::string HomieNodeBase::get_type() const { return ""; }

bool HomieNodeBase::is_array() const { return false; }

std::pair<int64_t, int64_t> HomieNodeBase::array_range() const { return {0, 0}; }

std::map<std::string, std::string> HomieNodeBase::get_attributes() const {
    auto base = GetEntityBase();
    auto base_class = const_cast<EntityBase_DeviceClass*>(GetEntityBaseDeviceClass());
    return {
        {"icon", base->get_icon()},
        {"class", base_class ? base_class->get_device_class() : std::string()},
    };
  }


void HomieNodeBase::notify_property_changed(HomiePropertyBase *property) {
  if (device) {
    this->defer("send", [this, property]() {  //
      device->notify_node_changed(this, property);
    });
  }
}

}  // namespace mqtt_homie
}  // namespace esphome
